/**
 * Copyright (c) 2017 Parrot Drones SAS
 * Copyright (c) 2017 Aurelien Barre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ULOG_TAG rtsp_client_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_client_test);

#include <libpomp.h>
#include <libsdp.h>
#include <rtsp/client.h>

/* Win32 stubs */
#ifdef _WIN32
static inline const char *strsignal(int signum)
{
	return "??";
}
#endif /* _WIN32 */


static const struct rtsp_header_ext header_ext[] = {
	{
		.key = "X-com-parrot-test",
		.value = "client-test",
	},
};


struct app {
	struct pomp_loop *loop;
	struct pomp_timer *timer;
	int stopped;
	struct rtsp_client *client;
	const char *session_id;
	char *addr;
	char *path;
	struct {
		int cancel_enable;
		int pause_enable;
		int failed_enable;
	} tests;
};

static struct app s_app;


static void sig_handler(int signum)
{
	ULOGI("signal %d(%s) received, stopping", signum, strsignal(signum));
	s_app.stopped = 1;
	if (s_app.loop != NULL)
		pomp_loop_wakeup(s_app.loop);
}


static void options_req(struct app *app)
{
	int res = 0;

	ULOGI("request options");

	res = rtsp_client_options(app->client,
				  header_ext,
				  1,
				  NULL,
				  RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_options", -res);
}


static void describe_req(struct app *app)
{
	int res = 0;

	ULOGI("request description");

	res = rtsp_client_describe(app->client,
				   app->path,
				   header_ext,
				   1,
				   NULL,
				   RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_describe", -res);

	if (app->tests.cancel_enable) {
		app->tests.cancel_enable = 0;
		ULOGI("cancel request");
		res = rtsp_client_cancel(app->client);
		if (res < 0)
			ULOG_ERRNO("rtsp_client_cancel", -res);
	}
}


static void
setup_req(struct app *app, const char *content_base, struct sdp_media *media)
{
	int res = 0;
	const char *url = NULL;

	if (app->tests.failed_enable) {
		app->tests.failed_enable = 0;
		url = "fake";
	} else {
		url = media->control_url;
	}

	ULOGI("request setup: url='%s' stream_port=%d control_port=%d",
	      url,
	      media->dst_stream_port,
	      media->dst_control_port);

	res = rtsp_client_setup(app->client,
				content_base,
				url,
				app->session_id,
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				header_ext,
				1,
				NULL,
				RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_setup", -res);
}


static void play_req(struct app *app)
{
	int res = 0;
	struct rtsp_range range;

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;

	ULOGI("request play");

	res = rtsp_client_play(app->client,
			       app->session_id,
			       &range,
			       1.0,
			       header_ext,
			       1,
			       NULL,
			       RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_play", -res);
}


static void pause_req(struct app *app)
{
	int res = 0;
	struct rtsp_range range;

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;

	ULOGI("request pause");

	res = rtsp_client_pause(app->client,
				app->session_id,
				&range,
				header_ext,
				1,
				NULL,
				RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_play failed", -res);
}


static void teardown_req(struct app *app)
{
	int res = 0;

	ULOGI("request teardown");

	res = rtsp_client_teardown(app->client,
				   NULL,
				   app->session_id,
				   header_ext,
				   1,
				   NULL,
				   RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_teardown", -res);
}


static void connection_state_cb(struct rtsp_client *client,
				enum rtsp_client_conn_state state,
				void *userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	ULOGI("connection state: %s", rtsp_client_conn_state_str(state));

	if (state == RTSP_CLIENT_CONN_STATE_CONNECTED)
		options_req(app);
}


static void session_removed_cb(struct rtsp_client *client,
			       const char *session_id,
			       int status,
			       void *userdata)
{
	ULOGI("session %s removed, status=%d(%s)",
	      session_id,
	      -status,
	      strerror(-status));
}


static void options_resp_cb(struct rtsp_client *client,
			    enum rtsp_client_req_status req_status,
			    int status,
			    uint32_t methods,
			    const struct rtsp_header_ext *ext,
			    size_t ext_count,
			    void *userdata,
			    void *req_userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGE("options_resp: %s err=%d(%s)",
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("options_resp: %s",
		      rtsp_client_req_status_str(req_status));
		return;
	}

	ULOGI("options_resp: methods allowed: 0x%08X", methods);

	describe_req(userdata);
}


static void describe_resp_cb(struct rtsp_client *client,
			     enum rtsp_client_req_status req_status,
			     int status,
			     const char *content_base,
			     const struct rtsp_header_ext *ext,
			     size_t ext_count,
			     const char *sdp,
			     void *userdata,
			     void *req_userdata)
{
	int res;
	struct sdp_session *sdp_session = NULL;
	struct sdp_media *media = NULL;
	struct list_node *node = NULL;
	struct app *app = userdata;
	if (!app)
		return;

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGE("describe_resp: %s err=%d(%s)",
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status == RTSP_CLIENT_REQ_STATUS_CANCELED) {
		ULOGI("describe_resp: %s, retry",
		      rtsp_client_req_status_str(req_status));
		describe_req(app);
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("describe_resp: %s",
		      rtsp_client_req_status_str(req_status));
		return;
	}

	ULOGI("describe_resp: sdp:\n%s", sdp);

	res = sdp_description_read(sdp, &sdp_session);
	if (res < 0) {
		ULOG_ERRNO("sdp_description_read", -res);
		return;
	}
	if (sdp_session->deletion)
		ULOGW("sdp refers to a no longer existing session");

	node = list_first(&sdp_session->medias);
	if (node == NULL)
		goto out;

	media = list_entry(node, struct sdp_media, node);
	if (media == NULL)
		goto out;

	setup_req(app, content_base, media);

out:
	sdp_session_destroy(sdp_session);
}


static void setup_resp_cb(struct rtsp_client *client,
			  const char *session_id,
			  enum rtsp_client_req_status req_status,
			  int status,
			  uint16_t src_stream_port,
			  uint16_t src_control_port,
			  int ssrc_valid,
			  uint32_t ssrc,
			  const struct rtsp_header_ext *ext,
			  size_t ext_count,
			  void *userdata,
			  void *req_userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGE("setup_resp: %s err=%d(%s)",
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		app->stopped = 1;
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("setup_resp: %s", rtsp_client_req_status_str(req_status));
		app->stopped = 1;
		return;
	}

	app->session_id = strdup(session_id);

	ULOGI("setup_resp: src_stream_port=%" PRIu16
	      " src_control_port=%" PRIu16 " ssrc_valid=%d ssrc=%" PRIu32,
	      src_stream_port,
	      src_control_port,
	      ssrc_valid,
	      ssrc);

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGI("retry description after have failed");
		describe_req(app);
	}

	play_req(app);
}


static void play_resp_cb(struct rtsp_client *client,
			 const char *session_id,
			 enum rtsp_client_req_status req_status,
			 int status,
			 const struct rtsp_range *range,
			 float scale,
			 int seq_valid,
			 uint16_t seq,
			 int rtptime_valid,
			 uint32_t rtptime,
			 const struct rtsp_header_ext *ext,
			 size_t ext_count,
			 void *userdata,
			 void *req_userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGE("play_resp: %s err=%d(%s)",
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("play_resp: %s", rtsp_client_req_status_str(req_status));
		return;
	}

	ULOGI("play_resp: scale=%.2f seq_valid=%d seq=%" PRIu16
	      " rtptime_valid=%d rtptime=%" PRIu32,
	      scale,
	      seq_valid,
	      seq,
	      rtptime_valid,
	      rtptime);

	ULOGI("waiting for 5s...");
	pomp_timer_set(app->timer, 5000);
}


static void pause_resp_cb(struct rtsp_client *client,
			  const char *session_id,
			  enum rtsp_client_req_status req_status,
			  int status,
			  const struct rtsp_range *range,
			  const struct rtsp_header_ext *ext,
			  size_t ext_count,
			  void *userdata,
			  void *req_userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGE("pause_resp: %s err=%d(%s)",
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("pause_resp: %s", rtsp_client_req_status_str(req_status));
		return;
	}

	ULOGI("pause_resp");

	ULOGI("waiting for 5s...");
	pomp_timer_set(app->timer, 5000);
}


static void teardown_resp_cb(struct rtsp_client *client,
			     const char *session_id,
			     enum rtsp_client_req_status req_status,
			     int status,
			     const struct rtsp_header_ext *ext,
			     size_t ext_count,
			     void *userdata,
			     void *req_userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	if (req_status == RTSP_CLIENT_REQ_STATUS_FAILED) {
		ULOGE("teardown_resp: %s err=%d(%s)",
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("teardown_resp: %s",
		      rtsp_client_req_status_str(req_status));
		return;
	}

	free((void *)app->session_id);
	app->session_id = NULL;

	ULOGI("teardown_resp");

	app->stopped = 1;
}


static void announce_cb(struct rtsp_client *client,
			const char *path,
			const struct rtsp_header_ext *ext,
			size_t ext_count,
			const char *sdp,
			void *userdata)
{
	ULOGI("announce: sdp:\n%s", sdp);
}


static const struct rtsp_client_cbs cbs = {
	.connection_state = &connection_state_cb,
	.session_removed = &session_removed_cb,
	.options_resp = &options_resp_cb,
	.describe_resp = &describe_resp_cb,
	.setup_resp = &setup_resp_cb,
	.play_resp = &play_resp_cb,
	.pause_resp = &pause_resp_cb,
	.teardown_resp = &teardown_resp_cb,
	.announce = &announce_cb,
};


static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	if (app->tests.pause_enable) {
		app->tests.pause_enable = 0;
		pause_req(userdata);
	} else {
		teardown_req(app);
	}
}


static void welcome(char *prog_name)
{
	printf("\n%s - Real Time Streaming Protocol library "
	       "client test program\n"
	       "Copyright (c) 2017 Parrot Drones SAS\n"
	       "Copyright (c) 2017 Aurelien Barre\n\n",
	       prog_name);
}


static void usage(char *prog_name)
{
	printf("Usage: %s [<options>] <url>\n"
	       "\n"
	       "Options:\n"
	       "-h | --help                        Print this message\n"
	       "     --test-cancel                 Test request cancel\n"
	       "     --test-pause                  Test pause request\n"
	       "     --test-failed                 Test failed request\n",
	       prog_name);
}


int main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int res = 0;
	int argidx = 0;
	char *url = NULL;
	char *tmp;

	memset(&s_app, 0, sizeof(s_app));

	welcome(argv[0]);

	/* Parse parameters */
	for (argidx = 1; argidx < argc; argidx++) {
		if (argv[argidx][0] != '-') {
			/* End of options */
			break;
		} else if (strcmp(argv[argidx], "-h") == 0 ||
			   strcmp(argv[argidx], "--help") == 0) {
			/* Help */
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		} else if (strcmp(argv[argidx], "--test-cancel") == 0) {
			s_app.tests.cancel_enable = 1;
		} else if (strcmp(argv[argidx], "--test-pause") == 0) {
			s_app.tests.pause_enable = 1;
		} else if (strcmp(argv[argidx], "--test-failed") == 0) {
			s_app.tests.failed_enable = 1;
		}
	}

	/* URL */
	if (argc - argidx < 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	url = argv[argidx];
	if (url == NULL) {
		ULOGE("missing URL");
		exit(EXIT_FAILURE);
	}

	/* Split addr/path for URL */
	if (strncmp(url, "rtsp://", 7) != 0) {
		ULOGE("bad URL scheme, expected 'rtsp://'");
		exit(EXIT_FAILURE);
	}
	s_app.addr = url;
	tmp = strchr(&url[7], '/');
	if (tmp == NULL) {
		ULOGE("missing path in URL");
		exit(EXIT_FAILURE);
	}
	*tmp = '\0';
	s_app.path = tmp + 1;

	/* Setup signal handlers */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	/* Create loop */
	s_app.loop = pomp_loop_new();
	if (s_app.loop == NULL) {
		ULOG_ERRNO("pomp_loop_new", ENOMEM);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	s_app.timer = pomp_timer_new(s_app.loop, &timer_cb, &s_app);
	if (s_app.timer == NULL) {
		ULOG_ERRNO("pomp_timer_new", ENOMEM);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	/* Create RTSP client */
	res = rtsp_client_new(s_app.loop, NULL, &cbs, &s_app, &s_app.client);
	if (res < 0) {
		ULOG_ERRNO("rtsp_client_new", -res);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	/* Connect RTSP client */
	printf("Connect client to URL '%s/%s'\n", s_app.addr, s_app.path);
	res = rtsp_client_connect(s_app.client, s_app.addr);
	if (res < 0) {
		ULOG_ERRNO("rtsp_client_connect", -res);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	/* Run the loop */
	while (!s_app.stopped)
		pomp_loop_wait_and_process(s_app.loop, -1);

	res = pomp_timer_clear(s_app.timer);
	if (res < 0)
		ULOG_ERRNO("pomp_timer_clear", -res);

	/* Disconnect RTSP client */
	rtsp_client_disconnect(s_app.client);
	if (res < 0) {
		ULOG_ERRNO("rtsp_client_disconnect", -res);
		status = EXIT_FAILURE;
		goto cleanup;
	}

cleanup:
	/* Cleanup */
	res = pomp_timer_destroy(s_app.timer);
	if (res < 0)
		ULOG_ERRNO("pomp_timer_destroy", -res);

	res = rtsp_client_destroy(s_app.client);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_destroy", -res);

	res = pomp_loop_destroy(s_app.loop);
	if (res < 0)
		ULOG_ERRNO("pomp_loop_destroy", -res);

	printf("%s\n", (status == EXIT_SUCCESS) ? "Done!" : "Failed!");
	exit(status);
}
