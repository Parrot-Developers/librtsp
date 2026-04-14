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

#define ULOG_TAG rtsp_client_ingest_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_client_ingest_test);

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

#define CONTROL_URL "stream=0"
#define STREAM_PORT 55004
#define STREAM_CHANNEL 0
#define CONTROL_PORT 55005
#define CONTROL_CHANNEL 1


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
	bool retried_announce;
	struct rtsp_client *client;
	const char *session_id;
	struct rtsp_url *url;
	char *url_str;
	char *addr;
	enum rtsp_lower_transport lower_transport;
	struct sdp_media *media;
};

static struct app s_app;


static void stop_app(struct app *app)
{
	if (!app || !app->loop)
		return;
	app->stopped = 1;
	pomp_loop_wakeup(app->loop);
}


static void sig_handler(int signum)
{
	ULOGI("signal %d(%s) received, stopping", signum, strsignal(signum));
	stop_app(&s_app);
	s_app.stopped = 1;
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


static void announce_req(struct app *app)
{
	int res = 0;
	struct sdp_session *session = NULL;
	char *sdp = NULL;

	ULOGI("request announce");

	session = sdp_session_new();
	if (session == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("sdp_session_new", -res);
		goto out;
	}
	session->session_id = 123456789;
	session->session_version = 1;
	session->server_addr = strdup("127.0.0.1");
	session->session_name = strdup("TestSession");
	session->connection_addr = strdup("0.0.0.0");
	session->control_url = strdup("*");
	session->start_mode = SDP_START_MODE_SENDONLY;
	session->tool = strdup("RTSP client ingest test");

	res = sdp_session_media_add(session, &app->media);
	if (res < 0) {
		ULOG_ERRNO("sdp_session_media_add", -res);
		goto out;
	}
	app->media->type = SDP_MEDIA_TYPE_VIDEO;
	app->media->media_title = strdup("DefaultVideo");
	app->media->connection_addr = strdup("239.255.42.1");
	app->media->control_url = strdup(CONTROL_URL);
	app->media->payload_type = 96;
	app->media->encoding_name = strdup("H264");
	app->media->clock_rate = 90000;

	res = sdp_description_write(session, &sdp);
	if (res < 0) {
		ULOG_ERRNO("sdp_description_write", -res);
		goto out;
	}

	res = rtsp_client_announce(app->client,
				   rtsp_url_get_path(app->url),
				   sdp,
				   header_ext,
				   1,
				   NULL,
				   RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_announce", -res);

out:
	if (session)
		sdp_session_destroy(session);
	free(sdp);
}


static void setup_req(struct app *app)
{
	int res = 0;
	bool is_interleaved =
		(app->lower_transport == RTSP_LOWER_TRANSPORT_TCP);
	uint16_t stream_val = is_interleaved ? STREAM_CHANNEL : STREAM_PORT;
	uint16_t control_val = is_interleaved ? CONTROL_CHANNEL : CONTROL_PORT;
	const char *label = is_interleaved ? "channel" : "port";

	ULOGI("request setup: url='%s' lower_transport='%s' "
	      "stream_%s=%d control_%s=%d",
	      CONTROL_URL,
	      rtsp_lower_transport_str(app->lower_transport),
	      label,
	      stream_val,
	      label,
	      control_val);

	res = rtsp_client_setup(app->client,
				app->url_str,
				CONTROL_URL,
				app->session_id,
				RTSP_DELIVERY_UNICAST,
				app->lower_transport,
				stream_val,
				control_val,
				RTSP_TRANSPORT_METHOD_RECORD,
				header_ext,
				1,
				NULL,
				RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_setup", -res);
}


static void record_req(struct app *app)
{
	int res = 0;
	struct rtsp_range range;

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;

	ULOGI("request record");

	res = rtsp_client_record(app->client,
				 app->session_id,
				 &range,
				 header_ext,
				 1,
				 NULL,
				 RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("rtsp_client_record", -res);
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


static void ready_to_send_cb(struct rtsp_client *client, void *userdata)
{
	ULOGI("%s", __func__);
}


static void interleaved_data_cb(struct rtsp_client *client,
				uint8_t channel,
				const uint8_t *data,
				size_t len,
				void *userdata)
{
	ULOGI("%s: channel=%u, len=%zu", __func__, channel, len);
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

	if (state == RTSP_CLIENT_CONN_STATE_DISCONNECTED)
		stop_app(app);
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
		ULOGE("%s: %s err=%d(%s)",
		      __func__,
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("%s: %s",
		      __func__,
		      rtsp_client_req_status_str(req_status));
		return;
	}

	ULOGI("%s: methods allowed: 0x%08X", __func__, methods);

	announce_req(userdata);
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
	ULOGI("%s", __func__);
}


static void announce_resp_cb(struct rtsp_client *client,
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
		if ((status == -EPERM) &&
		    (rtsp_url_get_user(app->url) != NULL) &&
		    !app->retried_announce) {
			/* Silent error and retry */
			app->retried_announce = true;
			announce_req(userdata);
			return;
		}
		ULOGE("%s: %s err=%d(%s)",
		      __func__,
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("%s: %s",
		      __func__,
		      rtsp_client_req_status_str(req_status));
		return;
	}

	setup_req(app);
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
		ULOGE("%s: %s err=%d(%s)",
		      __func__,
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		app->stopped = 1;
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("%s: %s",
		      __func__,
		      rtsp_client_req_status_str(req_status));
		app->stopped = 1;
		return;
	}

	app->session_id = strdup(session_id);

	const char *label = (app->lower_transport == RTSP_LOWER_TRANSPORT_TCP)
				    ? "channel"
				    : "port";

	ULOGI("%s: src_stream_%s=%" PRIu16 " src_control_%s=%" PRIu16
	      " ssrc_valid=%d ssrc=%" PRIu32,
	      __func__,
	      label,
	      src_stream_port,
	      label,
	      src_control_port,
	      ssrc_valid,
	      ssrc);

	record_req(app);
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
	ULOGI("%s", __func__);
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
	ULOGI("%s", __func__);
}


static void record_resp_cb(struct rtsp_client *client,
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
		ULOGE("%s: %s err=%d(%s)",
		      __func__,
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("%s: %s",
		      __func__,
		      rtsp_client_req_status_str(req_status));
		return;
	}

	if (app->lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
		uint8_t buf[] = "Hello World!";
		int err = rtsp_client_send_interleaved(
			client, 0, buf, sizeof(buf));
		if (err < 0)
			ULOG_ERRNO("rtsp_client_send_interleaved", -err);
	}

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
		ULOGE("%s: %s err=%d(%s)",
		      __func__,
		      rtsp_client_req_status_str(req_status),
		      -status,
		      strerror(-status));
		return;
	} else if (req_status != RTSP_CLIENT_REQ_STATUS_OK) {
		ULOGW("%s: %s",
		      __func__,
		      rtsp_client_req_status_str(req_status));
		return;
	}

	free((void *)app->session_id);
	app->session_id = NULL;

	ULOGI("%s", __func__);

	app->stopped = 1;
}


static void announce_cb(struct rtsp_client *client,
			const char *path,
			const struct rtsp_header_ext *ext,
			size_t ext_count,
			const char *sdp,
			void *userdata)
{
	ULOGI("%s: sdp:\n%s", __func__, sdp);
}


static const struct rtsp_client_cbs cbs = {
	.ready_to_send_cb = &ready_to_send_cb,
	.interleaved_data_cb = &interleaved_data_cb,
	.connection_state = &connection_state_cb,
	.session_removed = &session_removed_cb,
	.options_resp = &options_resp_cb,
	.describe_resp = &describe_resp_cb,
	.announce_resp = &announce_resp_cb,
	.setup_resp = &setup_resp_cb,
	.play_resp = &play_resp_cb,
	.pause_resp = &pause_resp_cb,
	.record_resp = &record_resp_cb,
	.teardown_resp = &teardown_resp_cb,
	.announce = &announce_cb,
};


static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	struct app *app = userdata;
	if (!app)
		return;

	teardown_req(app);
}


static void welcome(char *prog_name)
{
	printf("\n%s - Real Time Streaming Protocol library "
	       "client ingest test program\n"
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
	       "-t | --tcp                         Use TCP lower transport\n",
	       prog_name);
}


int main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int res = 0;
	int argidx = 0;
	char *url;

	memset(&s_app, 0, sizeof(s_app));

	s_app.lower_transport = RTSP_LOWER_TRANSPORT_UDP;

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
		} else if (strcmp(argv[argidx], "-t") == 0 ||
			   strcmp(argv[argidx], "--tcp") == 0) {
			s_app.lower_transport = RTSP_LOWER_TRANSPORT_TCP;
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

	res = rtsp_url_parse(url, &s_app.url);
	if (res < 0) {
		ULOG_ERRNO("rtsp_url_parse", -res);
		exit(EXIT_FAILURE);
	}

	if (rtsp_url_get_path(s_app.url) == NULL) {
		res = -EINVAL;
		ULOGE("missing path in URL");
		status = EXIT_FAILURE;
		goto cleanup;
	}

	res = rtsp_url_to_str(s_app.url, &s_app.url_str);
	if (res < 0) {
		ULOG_ERRNO("rtsp_url_to_str", -res);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	res = rtsp_url_to_str_no_path(s_app.url, &s_app.addr);
	if (res < 0) {
		ULOG_ERRNO("rtsp_url_to_str_no_path", -res);
		status = EXIT_FAILURE;
		goto cleanup;
	}

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
	printf("Connect client to URL '%s/%s'\n",
	       s_app.addr,
	       rtsp_url_get_path(s_app.url));
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

	rtsp_url_free(s_app.url);
	free(s_app.url_str);
	free(s_app.addr);

	printf("%s\n", (status == EXIT_SUCCESS) ? "Done!" : "Failed!");
	exit(status);
}
