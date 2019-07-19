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

#define ULOG_TAG rtsp_server_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_server_test);

#include <futils/random.h>
#include <libpomp.h>
#include <libsdp.h>
#include <rtsp/server.h>

/* Win32 stubs */
#ifdef _WIN32
static inline const char *strsignal(int signum)
{
	return "??";
}
#endif /* _WIN32 */


#define RESOURCE_PATH "live"
#define MEDIA1_PATH "stream=0"
#define MEDIA2_PATH "stream=1"


static int s_stopping;
struct pomp_loop *s_loop;
struct rtsp_server *s_server;


static void sighandler(int signum)
{
	ULOGI("signal %d(%s) received, stopping", signum, strsignal(signum));
	s_stopping = 1;
	if (s_loop)
		pomp_loop_wakeup(s_loop);
	signal(SIGINT, SIG_DFL);
}


static void describe_cb(struct rtsp_server *server,
			const char *server_address,
			const char *path,
			void *request_ctx,
			void *userdata)
{
	int ret = 0, err;
	struct sdp_session *session = NULL;
	struct sdp_media *media1 = NULL, *media2 = NULL;
	char *sdp = NULL;

	if ((server_address == NULL) || (server_address[0] == '\0')) {
		ULOGE("%s: invalid server address", __func__);
		ret = -EINVAL;
		goto out;
	}
	if ((path == NULL) || (path[0] == '\0')) {
		ULOGE("%s: invalid path", __func__);
		ret = -EINVAL;
		goto out;
	}
	if (strcmp(path, RESOURCE_PATH) != 0) {
		ULOGE("%s: not found", __func__);
		ret = -ENOENT;
		goto out;
	}

	session = sdp_session_new();
	if (session == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("sdp_session_new", -ret);
		goto out;
	}
	session->session_id = 123456789;
	session->session_version = 1;
	session->server_addr = strdup(server_address);
	session->session_name = strdup("TestSession");
	session->connection_addr = strdup("0.0.0.0");
	session->control_url = strdup("*");
	session->start_mode = SDP_START_MODE_RECVONLY;
	session->tool = strdup("RTSP server test");
	session->type = strdup("broadcast");

	ret = sdp_session_media_add(session, &media1);
	if (ret < 0) {
		ULOG_ERRNO("sdp_session_media_add", -ret);
		goto out;
	}
	media1->type = SDP_MEDIA_TYPE_VIDEO;
	media1->media_title = strdup("DefaultVideo");
	media1->connection_addr = strdup("239.255.42.1");
	media1->control_url = strdup(MEDIA1_PATH);
	media1->payload_type = 96;
	media1->encoding_name = strdup("H264");
	media1->clock_rate = 90000;

	ret = sdp_session_media_add(session, &media2);
	if (ret < 0) {
		ULOG_ERRNO("sdp_session_media_add", -ret);
		goto out;
	}
	media2->type = SDP_MEDIA_TYPE_VIDEO;
	media2->media_title = strdup("SecondVideo");
	media2->connection_addr = strdup("239.255.42.1");
	media2->control_url = strdup(MEDIA2_PATH);
	media2->payload_type = 96;
	media2->encoding_name = strdup("H264");
	media2->clock_rate = 90000;

	ret = sdp_description_write(session, &sdp);
	if (ret < 0) {
		ULOG_ERRNO("sdp_description_write", -ret);
		goto out;
	}

out:
	err = rtsp_server_reply_to_describe(server, request_ctx, ret, sdp);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_describe", -err);
	if (session)
		sdp_session_destroy(session);
	free(sdp);
}


static void setup_cb(struct rtsp_server *server,
		     const char *path,
		     const char *session_id,
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
	int ret = 0, err;
	uint32_t ssrc32 = 0;

	if ((path == NULL) || (path[0] == '\0')) {
		ULOGE("%s: invalid path", __func__);
		ret = -EINVAL;
		goto out;
	}
	if ((strcmp(path, RESOURCE_PATH "/" MEDIA1_PATH) != 0) &&
	    (strcmp(path, RESOURCE_PATH "/" MEDIA2_PATH) != 0)) {
		ULOGE("%s: not found", __func__);
		ret = -ENOENT;
		goto out;
	}
	if ((session_id == NULL) || (session_id[0] == '\0')) {
		ULOGE("%s: invalid session id", __func__);
		ret = -EINVAL;
		goto out;
	}
	if ((src_address == NULL) || (src_address[0] == '\0')) {
		ULOGE("%s: invalid destination address", __func__);
		ret = -EINVAL;
		goto out;
	}
	if ((dst_address == NULL) || (dst_address[0] == '\0')) {
		ULOGE("%s: invalid destination address", __func__);
		ret = -EINVAL;
		goto out;
	}
	if ((dst_stream_port == 0) || (dst_control_port == 0)) {
		ULOGE("%s: invalid client ports", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (delivery != RTSP_DELIVERY_UNICAST) {
		ULOGE("%s: unsupported delivery", __func__);
		ret = -ENOSYS;
		goto out;
	}
	if (lower_transport != RTSP_LOWER_TRANSPORT_UDP) {
		ULOGE("%s: unsupported lower transport", __func__);
		ret = -ENOSYS;
		goto out;
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
					 (void *)((intptr_t)ssrc32));
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_setup", -err);
}


static void play_cb(struct rtsp_server *server,
		    const char *session_id,
		    void *request_ctx,
		    void *media_ctx,
		    const struct rtsp_range *range,
		    float scale,
		    void *stream_userdata,
		    void *userdata)
{
	int ret = 0, err;
	struct rtsp_range _range;
	uint16_t seq16 = 0;
	uint32_t time32 = 0;

	memset(&_range, 0, sizeof(struct rtsp_range));

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

	if (range->start.format != RTSP_TIME_FORMAT_NPT) {
		ULOGE("%s: unsupported range format", __func__);
		ret = -ENOSYS;
		goto out;
	}
	if (scale == 0.)
		scale = 1.;
	_range = *range;

	ret = futils_random32(&time32);
	if (ret < 0) {
		ULOG_ERRNO("futils_random32", -ret);
		goto out;
	}
	ret = futils_random16(&seq16);
	if (ret < 0) {
		ULOG_ERRNO("futils_random16", -ret);
		goto out;
	}

out:
	err = rtsp_server_reply_to_play(server,
					request_ctx,
					media_ctx,
					ret,
					&_range,
					scale,
					1,
					seq16,
					1,
					time32);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_play", -err);
}


static void pause_cb(struct rtsp_server *server,
		     const char *session_id,
		     void *request_ctx,
		     void *media_ctx,
		     const struct rtsp_range *range,
		     void *stream_userdata,
		     void *userdata)
{
	int ret = 0, err;
	struct rtsp_range _range;

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

	_range = *range;

out:
	err = rtsp_server_reply_to_pause(
		server, request_ctx, media_ctx, ret, &_range);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_reply_to_pause", -err);
}


static void teardown_cb(struct rtsp_server *server,
			const char *session_id,
			enum rtsp_server_teardown_reason reason,
			void *request_ctx,
			void *media_ctx,
			void *stream_userdata,
			void *userdata)
{
	int ret = 0, err;

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

out:
	if (request_ctx != NULL) {
		err = rtsp_server_reply_to_teardown(
			server, request_ctx, media_ctx, ret);
		if (err < 0)
			ULOG_ERRNO("rtsp_server_reply_to_teardown", -err);
	}
}


static void request_timeout_cb(struct rtsp_server *server,
			       void *request_ctx,
			       enum rtsp_method_type method,
			       void *userdata)
{
	ULOGI("%s", __func__);
	return;
}


static const struct rtsp_server_cbs cbs = {
	.describe = &describe_cb,
	.setup = &setup_cb,
	.play = &play_cb,
	.pause = &pause_cb,
	.teardown = &teardown_cb,
	.request_timeout = &request_timeout_cb,
};


static void welcome(char *prog_name)
{
	printf("\n%s - Real Time Streaming Protocol library "
	       "server test program\n"
	       "Copyright (c) 2017 Parrot Drones SAS\n"
	       "Copyright (c) 2017 Aurelien Barre\n\n",
	       prog_name);
}


static void usage(char *prog_name)
{
	printf("Usage: %s <port>\n", prog_name);
}


int main(int argc, char **argv)
{
	int status = EXIT_SUCCESS, err;
	uint16_t port = 0;

	s_stopping = 0;
	s_loop = NULL;
	s_server = NULL;

	welcome(argv[0]);

	if (argc < 2) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[1]);

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

	printf("Server listening on port %d\n", port);
	printf("Connect to URL 'rtsp://<ip>:%d/%s'\n", port, RESOURCE_PATH);

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
