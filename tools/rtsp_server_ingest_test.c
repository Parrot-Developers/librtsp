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
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <futils/random.h>
#include <libpomp.h>
#include <libsdp.h>
#include <rtsp/server.h>

#define ULOG_TAG rtsp_server_ingest_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_server_ingest_test);


#define RESOURCE_PATH "live"
#define UNUSED(x) (void)(x)


static const struct rtsp_header_ext header_ext[] = {
	{
		.key = "X-com-parrot-test",
		.value = "server-ingest-test",
	},
};


static int s_stopping;
struct pomp_loop *s_loop;
struct rtsp_server *s_server;


static void sighandler(int signum)
{
	ULOGI("signal %d received, stopping", signum);
	s_stopping = 1;
	if (s_loop)
		pomp_loop_wakeup(s_loop);
	signal(SIGINT, SIG_DFL);
}


static void socket_cb(int fd, void *userdata)
{
	UNUSED(userdata);
	ULOGI("socket_cb called with fd=%d", fd);
}


static void announce_cb(struct rtsp_server *server,
			const char *server_address,
			const char *path,
			const char *sdp,
			const struct rtsp_header_ext *ext,
			size_t ext_count,
			void *request_ctx,
			void *userdata)
{
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);

	int ret = 0;
	struct sdp_session *sdp_session = NULL;

	ULOGI("ANNOUNCE: server_address=%s path=%s",
	      server_address ? server_address : "-",
	      path ? path : "-");

	if (sdp != NULL) {
		ULOGI("SDP description:\n%s", sdp);
		ret = sdp_description_read(sdp, &sdp_session);
		if (ret >= 0) {
			ULOGI("SDP parsed: session_name=%s session_id=%" PRIu64,
			      sdp_session->session_name
				      ? sdp_session->session_name
				      : "-",
			      sdp_session->session_id);
			sdp_session_destroy(sdp_session);
			ret = 0;
		} else {
			ULOG_ERRNO("sdp_description_read", -ret);
		}
	}

	ret = rtsp_server_reply_to_announce(
		server, request_ctx, ret, header_ext, 1);
	if (ret < 0)
		ULOG_ERRNO("rtsp_server_reply_to_announce", -ret);
}


static void setup_cb(struct rtsp_server *server,
		     const char *path,
		     const char *session_id,
		     const struct rtsp_header_ext *ext,
		     size_t ext_count,
		     void *request_ctx,
		     void *media_ctx,
		     enum rtsp_delivery delivery,
		     enum rtsp_lower_transport lower_transport,
		     const char *src_address,
		     const char *dst_address,
		     uint16_t dst_stream_port,
		     uint16_t dst_control_port,
		     void *userdata)
{
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);

	int ret = 0;
	int err;
	uint32_t ssrc32 = 0;

	if ((path == NULL) || (path[0] == '\0')) {
		ULOGE("%s: invalid path", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (delivery != RTSP_DELIVERY_UNICAST) {
		ULOGE("%s: unsupported delivery", __func__);
		ret = -ENOSYS;
		goto out;
	}

	if (lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
		ULOGI("SETUP (TCP): path=%s session_id=%s "
		      "stream_channel=%u control_channel=%u",
		      path,
		      session_id ? session_id : "(new)",
		      dst_stream_port,
		      dst_control_port);
	} else {
		ULOGI("SETUP (UDP): path=%s session_id=%s "
		      "dst_stream_port=%u dst_control_port=%u "
		      "src_address=%s dst_address=%s",
		      path,
		      session_id ? session_id : "(new)",
		      dst_stream_port,
		      dst_control_port,
		      src_address ? src_address : "-",
		      dst_address ? dst_address : "-");
	}

	ret = futils_random32(&ssrc32);
	if (ret < 0) {
		ULOG_ERRNO("futils_random32", -ret);
		goto out;
	}

out:
	err = rtsp_server_reply_to_setup(server,
					 request_ctx,
					 media_ctx,
					 ret,
					 5004,
					 5005,
					 1,
					 ssrc32,
					 header_ext,
					 1,
					 (void *)((intptr_t)ssrc32));
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_setup", -err);
}


static void record_cb(struct rtsp_server *server,
		      const char *session_id,
		      const struct rtsp_header_ext *ext,
		      size_t ext_count,
		      void *request_ctx,
		      void *media_ctx,
		      const struct rtsp_range *range,
		      void *stream_userdata,
		      void *userdata)
{
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);

	int ret = 0;
	int err;
	struct rtsp_range _range;

	memset(&_range, 0, sizeof(_range));

	if ((session_id == NULL) || (session_id[0] == '\0')) {
		ULOGE("%s: invalid session id", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (range == NULL) {
		ULOGE("%s: invalid range pointer", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (stream_userdata == NULL) {
		ULOGE("%s: invalid stream userdata pointer", __func__);
		ret = -EINVAL;
		goto out;
	}

	ULOGI("RECORD: session_id=%s range_start=%" PRIu64
	      " range_stop=%" PRIu64,
	      session_id,
	      range->start.npt.sec,
	      range->stop.npt.sec);

	_range = *range;

out:
	err = rtsp_server_reply_to_record(
		server, request_ctx, media_ctx, ret, &_range, header_ext, 1);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_record", -err);
}


static void pause_cb(struct rtsp_server *server,
		     const char *session_id,
		     const struct rtsp_header_ext *ext,
		     size_t ext_count,
		     void *request_ctx,
		     void *media_ctx,
		     const struct rtsp_range *range,
		     void *stream_userdata,
		     void *userdata)
{
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);

	int ret = 0;
	int err;
	struct rtsp_range _range;

	memset(&_range, 0, sizeof(_range));

	if ((session_id == NULL) || (session_id[0] == '\0')) {
		ULOGE("%s: invalid session id", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (range == NULL) {
		ULOGE("%s: invalid range pointer", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (stream_userdata == NULL) {
		ULOGE("%s: invalid stream userdata pointer", __func__);
		ret = -EINVAL;
		goto out;
	}

	ULOGI("PAUSE: session_id=%s", session_id);
	_range = *range;

out:
	err = rtsp_server_reply_to_pause(
		server, request_ctx, media_ctx, ret, &_range, header_ext, 1);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_pause", -err);
}


static void teardown_cb(struct rtsp_server *server,
			const char *path,
			const char *session_id,
			enum rtsp_server_teardown_reason reason,
			const struct rtsp_header_ext *ext,
			size_t ext_count,
			void *request_ctx,
			void *media_ctx,
			void *stream_userdata,
			void *userdata)
{
	UNUSED(path);
	UNUSED(reason);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);

	int ret = 0;
	int err;

	if ((session_id == NULL) || (session_id[0] == '\0')) {
		ULOGE("%s: invalid session id", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (stream_userdata == NULL) {
		ULOGE("%s: invalid stream userdata pointer", __func__);
		ret = -EINVAL;
		goto out;
	}

	ULOGI("TEARDOWN: session_id=%s reason=%d", session_id, reason);

out:
	if (request_ctx != NULL) {
		err = rtsp_server_reply_to_teardown(
			server, request_ctx, media_ctx, ret, header_ext, 1);
		if (err < 0)
			ULOG_ERRNO("rtsp_server_reply_to_teardown", -err);
	}
}


static void request_timeout_cb(struct rtsp_server *server,
			       void *request_ctx,
			       enum rtsp_method_type method,
			       void *userdata)
{
	UNUSED(server);
	UNUSED(request_ctx);
	UNUSED(method);
	UNUSED(userdata);

	ULOGI("%s", __func__);
}


static void interleaved_data_cb(struct rtsp_server *server,
				struct pomp_conn *conn,
				uint8_t channel,
				const void *data,
				size_t len,
				void *userdata)
{
	UNUSED(server);
	UNUSED(conn);
	UNUSED(userdata);

	char *str = malloc(len + 1);
	if (str != NULL) {
		memcpy(str, data, len);
		str[len] = '\0';
		ULOGI("INTERLEAVED DATA: channel=%u len=%zu data='%s'",
		      channel,
		      len,
		      str);
		free(str);
	} else {
		ULOGI("INTERLEAVED DATA: channel=%u len=%zu", channel, len);
	}
}


static void describe_cb(struct rtsp_server *server,
			const char *server_address,
			const char *path,
			const struct rtsp_header_ext *ext,
			size_t ext_count,
			void *request_ctx,
			void *userdata)
{
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);

	int err = rtsp_server_reply_to_describe(
		server, request_ctx, -ENOSYS, NULL, 0, NULL);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_describe", -err);
}


static void play_cb(struct rtsp_server *server,
		    const char *session_id,
		    const struct rtsp_header_ext *ext,
		    size_t ext_count,
		    void *request_ctx,
		    void *media_ctx,
		    const struct rtsp_range *range,
		    float scale,
		    void *stream_userdata,
		    void *userdata)
{
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(range);
	UNUSED(scale);
	UNUSED(stream_userdata);
	UNUSED(userdata);

	int err = rtsp_server_reply_to_play(server,
					    request_ctx,
					    media_ctx,
					    -ENOSYS,
					    range,
					    scale,
					    0,
					    0,
					    0,
					    0,
					    NULL,
					    0);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_play", -err);
}


static const struct rtsp_server_cbs cbs = {
	.socket_cb = &socket_cb,
	.describe = &describe_cb,
	.setup = &setup_cb,
	.play = &play_cb,
	.pause = &pause_cb,
	.teardown = &teardown_cb,
	.request_timeout = &request_timeout_cb,
	.announce = &announce_cb,
	.record = &record_cb,
	.interleaved_data = &interleaved_data_cb,
};


static void welcome(char *prog_name)
{
	printf("\n%s - Real Time Streaming Protocol library "
	       "server ingest test program\n"
	       "Copyright (c) 2017 Parrot Drones SAS\n"
	       "Copyright (c) 2017 Aurelien Barre\n\n",
	       prog_name);
}


static void usage(char *prog_name)
{
	printf("Usage: %s <port>\n", prog_name);
}


static void parse_port(const char *arg, uint16_t *port)
{
	int value = atoi(arg);

	if (value > 0 && value <= UINT16_MAX)
		*port = (uint16_t)value;
}


int main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int err;
	uint16_t port = 0;

	s_stopping = 0;
	s_loop = NULL;
	s_server = NULL;

	welcome(argv[0]);

	/* Parse parameters */
	int argidx = 1;
	while (argidx < argc) {
		if (strcmp(argv[argidx], "-p") == 0) {
			if (++argidx >= argc) {
				ULOGE("missing port after -p");
				exit(EXIT_FAILURE);
			}
			parse_port(argv[argidx++], &port);
		} else if (strcmp(argv[argidx], "-h") == 0 ||
			   strcmp(argv[argidx], "--help") == 0) {
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		} else {
			parse_port(argv[argidx++], &port);
		}
	}

	if (port == 0) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	signal(SIGINT, sighandler);

	s_loop = pomp_loop_new();
	if (!s_loop) {
		ULOGE("pomp_loop_new() failed");
		status = EXIT_FAILURE;
		goto cleanup;
	}

	err = rtsp_server_new(NULL, port, 0, 0, s_loop, &cbs, NULL, &s_server);
	if (err < 0) {
		ULOG_ERRNO("rtsp_server_new", -err);
		status = EXIT_FAILURE;
		goto cleanup;
	}

	printf("Ingest server listening on port %d\n", port);
	printf("Push stream URL: 'rtsp://<ip>:%d/%s'\n", port, RESOURCE_PATH);

	while (!s_stopping)
		pomp_loop_wait_and_process(s_loop, -1);

	printf("Server stopped\n");

cleanup:
	if (s_server) {
		err = rtsp_server_destroy(s_server);
		if (err)
			ULOG_ERRNO("rtsp_server_destroy", -err);
	}

	if (s_loop) {
		err = pomp_loop_destroy(s_loop);
		if (err)
			ULOG_ERRNO("pomp_loop_destroy", -err);
	}

	printf("%s\n", (status == EXIT_SUCCESS) ? "Done!" : "Failed!");
	exit(status);
}
