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

#include "rtsp_priv.h"
#include "rtsp_test.h"

#include <rtsp/client.h>
#include <rtsp/server.h>


/* A fixed, unusual port to minimize the chance of colliding with a real
 * service on the CI/dev machine running this suite. */
#define TEST_LOOPBACK_PORT 18923


/*
 * Minimal client callback stubs. connection_state/session_removed/announce
 * are the 3 mandatory rtsp_client_cbs members; the rest are intentionally
 * left NULL in most tests below to exercise the per-method -ENOSYS guard.
 */

static void fake_client_connection_state(struct rtsp_client *client,
					 enum rtsp_client_conn_state state,
					 void *userdata)
{
	UNUSED(client);
	UNUSED(state);
	UNUSED(userdata);
}


static void fake_client_session_removed(struct rtsp_client *client,
					const char *session_id,
					int status,
					void *userdata)
{
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(status);
	UNUSED(userdata);
}


static void fake_client_announce(struct rtsp_client *client,
				 const char *content_base,
				 const struct rtsp_header_ext *ext,
				 size_t ext_count,
				 const char *sdp,
				 void *userdata)
{
	UNUSED(client);
	UNUSED(content_base);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(sdp);
	UNUSED(userdata);
}


static void fake_client_options_resp(struct rtsp_client *client,
				     enum rtsp_client_req_status req_status,
				     int status,
				     uint32_t methods,
				     const struct rtsp_header_ext *ext,
				     size_t ext_count,
				     void *userdata,
				     void *req_userdata)
{
	UNUSED(client);
	UNUSED(req_status);
	UNUSED(status);
	UNUSED(methods);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(userdata);
	UNUSED(req_userdata);
}


static void fill_mandatory_client_cbs(struct rtsp_client_cbs *cbs)
{
	memset(cbs, 0, sizeof(*cbs));
	cbs->connection_state = &fake_client_connection_state;
	cbs->session_removed = &fake_client_session_removed;
	cbs->announce = &fake_client_announce;
}


/*
 * Minimal server callback stubs. describe/setup/play/pause/teardown/
 * request_timeout are the 6 mandatory rtsp_server_cbs members.
 */

static void fake_server_describe(struct rtsp_server *server,
				 const char *server_address,
				 const char *path,
				 const struct rtsp_header_ext *ext,
				 size_t ext_count,
				 void *request_ctx,
				 void *userdata)
{
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(request_ctx);
	UNUSED(userdata);
}


static void fake_server_setup(struct rtsp_server *server,
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
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(request_ctx);
	UNUSED(media_ctx);
	UNUSED(delivery);
	UNUSED(lower_transport);
	UNUSED(src_address);
	UNUSED(dst_address);
	UNUSED(dst_stream_port);
	UNUSED(dst_control_port);
	UNUSED(userdata);
}


static void fake_server_play(struct rtsp_server *server,
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
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(request_ctx);
	UNUSED(media_ctx);
	UNUSED(range);
	UNUSED(scale);
	UNUSED(stream_userdata);
	UNUSED(userdata);
}


static void fake_server_pause(struct rtsp_server *server,
			      const char *session_id,
			      const struct rtsp_header_ext *ext,
			      size_t ext_count,
			      void *request_ctx,
			      void *media_ctx,
			      const struct rtsp_range *range,
			      void *stream_userdata,
			      void *userdata)
{
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(request_ctx);
	UNUSED(media_ctx);
	UNUSED(range);
	UNUSED(stream_userdata);
	UNUSED(userdata);
}


static void fake_server_teardown(struct rtsp_server *server,
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
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(reason);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(request_ctx);
	UNUSED(media_ctx);
	UNUSED(stream_userdata);
	UNUSED(userdata);
}


static void fake_server_request_timeout(struct rtsp_server *server,
					void *request_ctx,
					enum rtsp_method_type method,
					void *userdata)
{
	UNUSED(server);
	UNUSED(request_ctx);
	UNUSED(method);
	UNUSED(userdata);
}


static void fill_mandatory_server_cbs(struct rtsp_server_cbs *cbs)
{
	memset(cbs, 0, sizeof(*cbs));
	cbs->describe = &fake_server_describe;
	cbs->setup = &fake_server_setup;
	cbs->play = &fake_server_play;
	cbs->pause = &fake_server_pause;
	cbs->teardown = &fake_server_teardown;
	cbs->request_timeout = &fake_server_request_timeout;
}


/*
 * rtsp_client_new() argument validation
 */

static void test_rtsp_client_new_args(void)
{
	struct pomp_loop *loop;
	struct rtsp_client_cbs cbs;
	struct rtsp_client *client = NULL;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_client_cbs(&cbs);

	ret = rtsp_client_new(NULL, NULL, &cbs, NULL, &client);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(client);

	ret = rtsp_client_new(loop, NULL, NULL, NULL, &client);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_client_new(loop, NULL, &cbs, NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	{
		struct rtsp_client_cbs bad = cbs;
		bad.connection_state = NULL;
		client = NULL;
		ret = rtsp_client_new(loop, NULL, &bad, NULL, &client);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		CU_ASSERT_PTR_NULL(client);
	}
	{
		struct rtsp_client_cbs bad = cbs;
		bad.session_removed = NULL;
		client = NULL;
		ret = rtsp_client_new(loop, NULL, &bad, NULL, &client);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		CU_ASSERT_PTR_NULL(client);
	}
	{
		struct rtsp_client_cbs bad = cbs;
		bad.announce = NULL;
		client = NULL;
		ret = rtsp_client_new(loop, NULL, &bad, NULL, &client);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		CU_ASSERT_PTR_NULL(client);
	}

	/* OK: minimal valid construction; NULL software_name defaults
	 * internally, no DNS/socket I/O happens synchronously */
	client = NULL;
	ret = rtsp_client_new(loop, NULL, &cbs, NULL, &client);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(client);
	CU_ASSERT_PTR_NULL(rtsp_client_get_remote_url(client));

	/* Destroying right after construction, with no connect ever
	 * attempted, must be safe */
	ret = rtsp_client_destroy(client);
	CU_ASSERT_EQUAL(ret, 0);

	pomp_loop_destroy(loop);
}


/*
 * Every optional response callback left NULL: each request method must
 * fail with -ENOSYS for its own missing *_resp callback, checked before
 * the connection-state (-EPIPE) guard, so this needs no real connection.
 */
static void test_rtsp_client_enosys_matrix(void)
{
	struct pomp_loop *loop;
	struct rtsp_client_cbs cbs;
	struct rtsp_client *client = NULL;
	struct rtsp_range range;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_client_cbs(&cbs);

	ret = rtsp_client_new(loop, NULL, &cbs, NULL, &client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;

	ret = rtsp_client_options(client, NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_describe(client, "/stream", NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_announce(
		client, "/stream", "v=0\r\n", NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_setup(client,
				"rtsp://host/stream",
				"rtsp://host/stream/trackID=1",
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				5000,
				5001,
				RTSP_TRANSPORT_METHOD_PLAY,
				NULL,
				0,
				NULL,
				0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_play(
		client, "session1", &range, 1.0f, NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_pause(client, "session1", &range, NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_record(client, "session1", &range, NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_teardown(client, NULL, "session1", NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_destroy(client);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


static void test_rtsp_client_state_machine(void)
{
	struct pomp_loop *loop;
	struct rtsp_client_cbs cbs;
	struct rtsp_client *client = NULL;
	uint8_t data[4] = {1, 2, 3, 4};
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_client_cbs(&cbs);
	cbs.options_resp = &fake_client_options_resp;

	ret = rtsp_client_new(loop, NULL, &cbs, NULL, &client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* set_socket_* before any connect: succeeds, just stashes values */
	ret = rtsp_client_set_socket_rxbuf_size(client, 4096);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_client_set_socket_txbuf_size(client, 4096);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_client_set_socket_class_selector(client, 0);
	CU_ASSERT_EQUAL(ret, 0);

	/* disconnect while already DISCONNECTED */
	ret = rtsp_client_disconnect(client);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* a request while not connected, with its resp cb now set, hits
	 * -EPIPE instead of -ENOSYS */
	ret = rtsp_client_options(client, NULL, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, -EPIPE);

	/* send_interleaved bounds, checked before the -EPIPE guard */
	ret = rtsp_client_send_interleaved(NULL, 0, data, sizeof(data));
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_client_send_interleaved(client, 0, NULL, sizeof(data));
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_client_send_interleaved(client, 0, data, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_client_send_interleaved(
		client, 0, data, (size_t)UINT16_MAX + 1);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_client_send_interleaved(client, 0, data, sizeof(data));
	CU_ASSERT_EQUAL(ret, -EPIPE);

	/* connect with an unparsable URL propagates rtsp_url_parse's error,
	 * without touching conn_state */
	ret = rtsp_client_connect(client, "not-a-url");
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_client_disconnect(client);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* connect with a valid URL returns synchronously (moves to
	 * CONNECTING; DNS/socket I/O happens asynchronously) */
	ret = rtsp_client_connect(client, "rtsp://127.0.0.1:1/stream");
	CU_ASSERT_EQUAL(ret, 0);

	/* a second connect while CONNECTING */
	ret = rtsp_client_connect(client, "rtsp://127.0.0.1:1/stream");
	CU_ASSERT_EQUAL(ret, -EBUSY);

	/* disconnecting while still CONNECTING synchronously completes,
	 * without needing the loop pumped */
	ret = rtsp_client_disconnect(client);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_client_destroy(client);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


/*
 * rtsp_server_new() argument validation
 */

static void test_rtsp_server_new_args(void)
{
	struct pomp_loop *loop;
	struct rtsp_server_cbs cbs;
	struct rtsp_server *server = NULL;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_server_cbs(&cbs);

	ret = rtsp_server_new(NULL, 0, 0, 0, loop, &cbs, NULL, &server);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(server);

	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, NULL, &cbs, NULL, &server);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, NULL, NULL, &server);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, &cbs, NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

#define ASSERT_MANDATORY_SERVER_CB_MISSING(field)                              \
	do {                                                                   \
		struct rtsp_server_cbs bad = cbs;                              \
		bad.field = NULL;                                              \
		server = NULL;                                                 \
		ret = rtsp_server_new(NULL,                                    \
				      TEST_LOOPBACK_PORT,                      \
				      0,                                       \
				      0,                                       \
				      loop,                                    \
				      &bad,                                    \
				      NULL,                                    \
				      &server);                                \
		CU_ASSERT_EQUAL(ret, -EINVAL);                                 \
		CU_ASSERT_PTR_NULL(server);                                    \
	} while (0)

	ASSERT_MANDATORY_SERVER_CB_MISSING(describe);
	ASSERT_MANDATORY_SERVER_CB_MISSING(setup);
	ASSERT_MANDATORY_SERVER_CB_MISSING(play);
	ASSERT_MANDATORY_SERVER_CB_MISSING(pause);
	ASSERT_MANDATORY_SERVER_CB_MISSING(teardown);
	ASSERT_MANDATORY_SERVER_CB_MISSING(request_timeout);

#undef ASSERT_MANDATORY_SERVER_CB_MISSING

	/* OK: minimal valid construction. This really binds/listens on
	 * TEST_LOOPBACK_PORT synchronously. */
	server = NULL;
	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, &cbs, NULL, &server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(server);

	/* Binding the same port again while the first server still holds
	 * it fails synchronously (no loop pump needed) */
	{
		struct rtsp_server *server2 = NULL;
		ret = rtsp_server_new(NULL,
				      TEST_LOOPBACK_PORT,
				      0,
				      0,
				      loop,
				      &cbs,
				      NULL,
				      &server2);
		CU_ASSERT_TRUE(ret < 0);
		CU_ASSERT_PTR_NULL(server2);
	}

	ret = rtsp_server_destroy(server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


static void test_rtsp_server_send_interleaved_args(void)
{
	struct pomp_loop *loop;
	struct rtsp_server_cbs cbs;
	struct rtsp_server *server = NULL;
	uint8_t data[4] = {1, 2, 3, 4};
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_server_cbs(&cbs);
	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, &cbs, NULL, &server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	ret = rtsp_server_send_interleaved(NULL, 0, data, sizeof(data));
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_send_interleaved(server, 0, NULL, sizeof(data));
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_send_interleaved(server, 0, data, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_send_interleaved(
		server, 0, data, (size_t)UINT16_MAX + 1);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_server_destroy(server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


static void test_rtsp_server_announce_args(void)
{
	struct pomp_loop *loop;
	struct rtsp_server_cbs cbs;
	struct rtsp_server *server = NULL;
	char *huge;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_server_cbs(&cbs);
	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, &cbs, NULL, &server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	ret = rtsp_server_announce(
		NULL, (char *)"rtsp://x/y", NULL, 0, (char *)"v=0\r\n");
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_announce(server, NULL, NULL, 0, (char *)"v=0\r\n");
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_announce(
		server, (char *)"", NULL, 0, (char *)"v=0\r\n");
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_announce(server, (char *)"rtsp://x/y", NULL, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_announce(
		server, (char *)"rtsp://x/y", NULL, 0, (char *)"");
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Session description at/over RTSP_SESSION_DESCRIPTION_MAX_LEN */
	huge = malloc(RTSP_SESSION_DESCRIPTION_MAX_LEN + 16);
	CU_ASSERT_PTR_NOT_NULL_FATAL(huge);
	memset(huge, 'a', RTSP_SESSION_DESCRIPTION_MAX_LEN + 15);
	huge[RTSP_SESSION_DESCRIPTION_MAX_LEN + 15] = '\0';
	ret = rtsp_server_announce(server, (char *)"rtsp://x/y", NULL, 0, huge);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	free(huge);

	ret = rtsp_server_destroy(server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


static void test_rtsp_server_force_teardown_args(void)
{
	struct pomp_loop *loop;
	struct rtsp_server_cbs cbs;
	struct rtsp_server *server = NULL;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_server_cbs(&cbs);
	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, &cbs, NULL, &server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	ret = rtsp_server_force_teardown(NULL, "s1", NULL, NULL, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_force_teardown(server, NULL, NULL, NULL, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* A syntactically valid but unknown session_id: no session was ever
	 * created on this fresh server */
	ret = rtsp_server_force_teardown(
		server, "unknown-session", NULL, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	ret = rtsp_server_destroy(server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


/*
 * request_ctx is looked up by pointer value only (rtsp_server_pending_
 * request_find()), never dereferenced before that lookup succeeds - so a
 * bogus, never-issued, non-NULL request_ctx safely yields -ENOENT.
 * reply_to_describe/reply_to_announce don't take a media_ctx at all, which
 * is what makes this safe to test without a real completed request; the
 * other five reply_to_* functions dereference media_ctx unconditionally
 * right after their NULL check, so a fabricated media_ctx is NOT safe to
 * test here (would crash) - only a real loopback handshake can exercise
 * their equivalent lookup-miss path safely.
 */
static void test_rtsp_server_reply_bogus_request_ctx(void)
{
	struct pomp_loop *loop;
	struct rtsp_server_cbs cbs;
	struct rtsp_server *server = NULL;
	int bogus_marker;
	void *bogus_ctx = &bogus_marker;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);
	fill_mandatory_server_cbs(&cbs);
	ret = rtsp_server_new(
		NULL, TEST_LOOPBACK_PORT, 0, 0, loop, &cbs, NULL, &server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	ret = rtsp_server_reply_to_describe(
		NULL, bogus_ctx, 200, NULL, 0, (char *)"v=0\r\n");
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_reply_to_describe(
		server, NULL, 200, NULL, 0, (char *)"v=0\r\n");
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_reply_to_describe(
		server, bogus_ctx, 200, NULL, 0, (char *)"v=0\r\n");
	CU_ASSERT_EQUAL(ret, -ENOENT);

	ret = rtsp_server_reply_to_announce(NULL, bogus_ctx, 200, NULL, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_reply_to_announce(server, NULL, 200, NULL, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_server_reply_to_announce(server, bogus_ctx, 200, NULL, 0);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	ret = rtsp_server_destroy(server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(loop);
}


/*
 * Real client<->server handshake on a single, shared, single-threaded
 * pomp_loop (modeled on tools/rtsp_server_test.c and
 * tools/rtsp_client_test.c). Neither side needs real hardware or a real
 * media stream: librtsp does not validate SDP content or resource paths
 * itself, so the server's DESCRIBE reply is a hand-written literal SDP
 * string and the client derives its SETUP resource URL directly from the
 * content_base it receives, without needing a real SDP parser (libsdp).
 */

#define TEST_LOOPBACK_SDP                                                      \
	"v=0\r\n"                                                              \
	"o=- 0 0 IN IP4 127.0.0.1\r\n"                                         \
	"s=-\r\n"                                                              \
	"t=0 0\r\n"                                                            \
	"m=video 0 RTP/AVP 96\r\n"                                             \
	"a=control:trackID=1\r\n"


struct loopback_ctx {
	struct pomp_loop *loop;
	struct rtsp_server *server;
	struct rtsp_client *client;

	bool connected;
	int connected_count;
	bool disconnected;
	bool session_removed;

	bool options_done;
	enum rtsp_client_req_status options_status;
	int options_code;

	bool describe_done;
	enum rtsp_client_req_status describe_status;
	int describe_code;
	char content_base[256];

	bool setup_done;
	enum rtsp_client_req_status setup_status;
	int setup_code;
	char session_id[64];
	uint16_t src_stream_port;
	uint16_t src_control_port;

	bool play_done;
	enum rtsp_client_req_status play_status;
	int play_code;
	bool play_rtp_info_seq_valid;
	uint16_t play_rtp_info_seq;
	bool play_rtp_info_rtptime_valid;
	uint32_t play_rtp_info_rtptime;

	bool pause_done;
	enum rtsp_client_req_status pause_status;
	int pause_code;

	bool teardown_done;
	enum rtsp_client_req_status teardown_status;
	int teardown_code;

	bool server_got_teardown;
	enum rtsp_server_teardown_reason last_teardown_reason;

	bool client_got_forced_teardown;

	bool server_request_timeout_fired;
	enum rtsp_method_type timed_out_method;

	bool client_got_interleaved_data;
	uint8_t client_interleaved_channel;
	uint8_t client_interleaved_data[32];
	size_t client_interleaved_len;

	bool server_got_interleaved_data;
	uint8_t server_interleaved_channel;
	uint8_t server_interleaved_data[32];
	size_t server_interleaved_len;

	bool client_got_announce;
	char announce_content_base[256];
	char announce_sdp[512];

	bool client_resp_timeout_fired;

	bool client_socket_cb_fired;
	bool server_socket_cb_fired;

	int describe_call_count;
};


/* Elapsed-time-bounded polling: an iteration-count budget silently turns
 * into a per-event budget whenever the polled events can fire back-to-back
 * near-instantly, so this bounds by real time instead (see CLAUDE.md). */
static bool
loopback_pump_until(struct pomp_loop *loop, const bool *flag, int timeout_ms)
{
	struct timespec start;
	time_get_monotonic(&start);
	while (!*flag) {
		pomp_loop_wait_and_process(loop, 20);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)timeout_ms * 1000)
			break;
	}
	return *flag;
}


/* Pumps for a fixed duration, never used as a substitute for a real
 * completion predicate - only to give an already-in-flight message (e.g.
 * a client's auto-reply to a server-pushed request) a chance to actually
 * reach the other side before the test proceeds to tear things down. */
static void loopback_pump_for(struct pomp_loop *loop, int duration_ms)
{
	struct timespec start;
	time_get_monotonic(&start);
	while (true) {
		pomp_loop_wait_and_process(loop, 20);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)duration_ms * 1000)
			break;
	}
}


#define LOOPBACK_TIMEOUT_MS 2000


static void loopback_server_describe(struct rtsp_server *server,
				     const char *server_address,
				     const char *path,
				     const struct rtsp_header_ext *ext,
				     size_t ext_count,
				     void *request_ctx,
				     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);

	ret = rtsp_server_reply_to_describe(ctx->server,
					    request_ctx,
					    0,
					    NULL,
					    0,
					    (char *)TEST_LOOPBACK_SDP);
	CU_ASSERT_EQUAL(ret, 0);
}


static void loopback_server_setup(struct rtsp_server *server,
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
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(delivery);
	UNUSED(lower_transport);
	UNUSED(src_address);
	UNUSED(dst_address);
	UNUSED(dst_stream_port);
	UNUSED(dst_control_port);

	ret = rtsp_server_reply_to_setup(ctx->server,
					 request_ctx,
					 media_ctx,
					 0,
					 6000,
					 6001,
					 1,
					 0x12345678,
					 NULL,
					 0,
					 NULL);
	CU_ASSERT_EQUAL(ret, 0);
}


static void loopback_server_play(struct rtsp_server *server,
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
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(scale);
	UNUSED(stream_userdata);

	ret = rtsp_server_reply_to_play(ctx->server,
					request_ctx,
					media_ctx,
					0,
					range,
					1.0f,
					0,
					0,
					0,
					0,
					NULL,
					0);
	CU_ASSERT_EQUAL(ret, 0);
}


static void loopback_server_pause(struct rtsp_server *server,
				  const char *session_id,
				  const struct rtsp_header_ext *ext,
				  size_t ext_count,
				  void *request_ctx,
				  void *media_ctx,
				  const struct rtsp_range *range,
				  void *stream_userdata,
				  void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(stream_userdata);

	ret = rtsp_server_reply_to_pause(
		ctx->server, request_ctx, media_ctx, 0, range, NULL, 0);
	CU_ASSERT_EQUAL(ret, 0);
}


static void loopback_server_teardown(struct rtsp_server *server,
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
	struct loopback_ctx *ctx = userdata;
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(stream_userdata);

	ctx->server_got_teardown = true;
	ctx->last_teardown_reason = reason;

	/* This callback also fires for a server-initiated teardown (e.g.
	 * via rtsp_server_force_teardown(), reason FORCED_TEARDOWN or
	 * SESSION_TIMEOUT), as an informational notification only - there
	 * is no real pending client request to reply to in that case, and
	 * request_ctx is not usable with rtsp_server_reply_to_teardown().
	 * Only a genuine client-initiated TEARDOWN request must be
	 * answered. */
	if (reason == RTSP_SERVER_TEARDOWN_REASON_CLIENT_REQUEST) {
		int ret = rtsp_server_reply_to_teardown(
			ctx->server, request_ctx, media_ctx, 0, NULL, 0);
		CU_ASSERT_EQUAL(ret, 0);
	}
}


static void loopback_server_request_timeout(struct rtsp_server *server,
					    void *request_ctx,
					    enum rtsp_method_type method,
					    void *userdata)
{
	UNUSED(server);
	UNUSED(request_ctx);
	UNUSED(method);
	UNUSED(userdata);
	/* Should never fire in this test: every request is answered
	 * synchronously, within the same callback that received it. */
	CU_FAIL("unexpected server-side request timeout");
}


static void loopback_client_connection_state(struct rtsp_client *client,
					     enum rtsp_client_conn_state state,
					     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	if (state == RTSP_CLIENT_CONN_STATE_CONNECTED) {
		ctx->connected = true;
		ctx->connected_count++;
	} else if (state == RTSP_CLIENT_CONN_STATE_DISCONNECTED) {
		ctx->disconnected = true;
	}
}


static void loopback_client_session_removed(struct rtsp_client *client,
					    const char *session_id,
					    int status,
					    void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(status);
	ctx->session_removed = true;
}


static void loopback_client_options_resp(struct rtsp_client *client,
					 enum rtsp_client_req_status req_status,
					 int status,
					 uint32_t methods,
					 const struct rtsp_header_ext *ext,
					 size_t ext_count,
					 void *userdata,
					 void *req_userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(methods);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->options_status = req_status;
	ctx->options_code = status;
	ctx->options_done = true;
}


static void
loopback_client_describe_resp(struct rtsp_client *client,
			      enum rtsp_client_req_status req_status,
			      int status,
			      const char *content_base,
			      const struct rtsp_header_ext *ext,
			      size_t ext_count,
			      const char *sdp,
			      void *userdata,
			      void *req_userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(sdp);
	UNUSED(req_userdata);
	ctx->describe_status = req_status;
	ctx->describe_code = status;
	if (content_base != NULL) {
		snprintf(ctx->content_base,
			 sizeof(ctx->content_base),
			 "%s",
			 content_base);
	}
	ctx->describe_done = true;
}


static void loopback_client_setup_resp(struct rtsp_client *client,
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
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(ssrc_valid);
	UNUSED(ssrc);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->setup_status = req_status;
	ctx->setup_code = status;
	ctx->src_stream_port = src_stream_port;
	ctx->src_control_port = src_control_port;
	if (session_id != NULL)
		snprintf(ctx->session_id,
			 sizeof(ctx->session_id),
			 "%s",
			 session_id);
	ctx->setup_done = true;
}


static void loopback_client_play_resp(struct rtsp_client *client,
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
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(range);
	UNUSED(scale);
	UNUSED(seq_valid);
	UNUSED(seq);
	UNUSED(rtptime_valid);
	UNUSED(rtptime);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->play_status = req_status;
	ctx->play_code = status;
	ctx->play_done = true;
}


static void loopback_client_pause_resp(struct rtsp_client *client,
				       const char *session_id,
				       enum rtsp_client_req_status req_status,
				       int status,
				       const struct rtsp_range *range,
				       const struct rtsp_header_ext *ext,
				       size_t ext_count,
				       void *userdata,
				       void *req_userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(range);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->pause_status = req_status;
	ctx->pause_code = status;
	ctx->pause_done = true;
}


static void
loopback_client_teardown_resp(struct rtsp_client *client,
			      const char *session_id,
			      enum rtsp_client_req_status req_status,
			      int status,
			      const struct rtsp_header_ext *ext,
			      size_t ext_count,
			      void *userdata,
			      void *req_userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->teardown_status = req_status;
	ctx->teardown_code = status;
	ctx->teardown_done = true;
}


/* Same as loopback_server_play, but replies with a valid RTP-Info (seq +
 * rtptime), covering rtsp_server_reply_to_play()'s RTP-Info header
 * generation branch that loopback_server_play (seq_valid=rtptime_valid=0)
 * never reaches. */
static void
loopback_server_play_with_rtp_info(struct rtsp_server *server,
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
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(scale);
	UNUSED(stream_userdata);

	ret = rtsp_server_reply_to_play(ctx->server,
					request_ctx,
					media_ctx,
					0,
					range,
					1.0f,
					1,
					1000,
					1,
					90000,
					NULL,
					0);
	CU_ASSERT_EQUAL(ret, 0);
}


static void loopback_client_play_resp_capture_rtp_info(
	struct rtsp_client *client,
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
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(range);
	UNUSED(scale);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->play_status = req_status;
	ctx->play_code = status;
	ctx->play_rtp_info_seq_valid = seq_valid;
	ctx->play_rtp_info_seq = seq;
	ctx->play_rtp_info_rtptime_valid = rtptime_valid;
	ctx->play_rtp_info_rtptime = rtptime;
	ctx->play_done = true;
}


static void test_rtsp_loopback_full_handshake(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.server);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.options_resp = &loopback_client_options_resp;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp;
	client_cbs.pause_resp = &loopback_client_pause_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.client);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)TEST_LOOPBACK_PORT);
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}

	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* OPTIONS */
	ret = rtsp_client_options(
		ctx.client, NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.options_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.options_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.options_code, 0);

	/* DESCRIBE */
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.describe_code, 0);
	CU_ASSERT_TRUE(strlen(ctx.content_base) > 0);

	/* SETUP: the resource URL is derived from content_base, matching
	 * the track this synthetic SDP declares (trackID=1). librtsp does
	 * not itself validate this path server-side in this test. */
	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.setup_code, 0);
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);
	CU_ASSERT_EQUAL(ctx.src_stream_port, (uint16_t)6000);
	CU_ASSERT_EQUAL(ctx.src_control_port, (uint16_t)6001);

	/* PLAY */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_play(ctx.client,
			       ctx.session_id,
			       &range,
			       1.0f,
			       NULL,
			       0,
			       NULL,
			       LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.play_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.play_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.play_code, 0);

	/* PAUSE */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	ret = rtsp_client_pause(ctx.client,
				ctx.session_id,
				&range,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.pause_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.pause_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.pause_code, 0);

	/* TEARDOWN */
	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.teardown_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.teardown_code, 0);
	CU_ASSERT_TRUE(ctx.server_got_teardown);

	/* The server removes the session after a successful teardown,
	 * notifying the client */
	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.session_removed, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * rtsp_server_force_teardown() on a real, established session: reuses the
 * same fake server/client callbacks and loopback_ctx as the full handshake
 * above, up through PLAY, then has the server unilaterally tear the session
 * down instead of the client requesting it.
 */

static void loopback_client_teardown_notify(struct rtsp_client *client,
					    const char *path,
					    const char *session_id,
					    const struct rtsp_header_ext *ext,
					    size_t ext_count,
					    void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	ctx->client_got_forced_teardown = true;
}


static void test_rtsp_loopback_force_teardown(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 1,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.teardown = &loopback_client_teardown_notify;
	client_cbs.options_resp = &loopback_client_options_resp;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 1));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_play(ctx.client,
			       ctx.session_id,
			       &range,
			       1.0f,
			       NULL,
			       0,
			       NULL,
			       LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.play_done, LOOPBACK_TIMEOUT_MS));

	/* The server unilaterally tears the session down */
	ret = rtsp_server_force_teardown(
		ctx.server, ctx.session_id, NULL, NULL, 0);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE(ctx.server_got_teardown);
	CU_ASSERT_EQUAL(ctx.last_teardown_reason,
			RTSP_SERVER_TEARDOWN_REASON_FORCED_TEARDOWN);

	CU_ASSERT_TRUE(loopback_pump_until(ctx.loop,
					   &ctx.client_got_forced_teardown,
					   LOOPBACK_TIMEOUT_MS));

	/* Let the client's auto-generated 200 OK reply to the pushed
	 * TEARDOWN actually reach and get processed by the server
	 * (rtsp_server_response_process()) before tearing the connection
	 * down. */
	loopback_pump_for(ctx.loop, 200);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * rtsp_client_cancel(): canceling a request completes it synchronously
 * (RTSP_CLIENT_REQ_STATUS_CANCELED), without waiting for the server's
 * (now-irrelevant) reply to actually arrive.
 */
static void test_rtsp_client_cancel_pending_request(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 2,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 2));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	ret = rtsp_client_cancel(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_CANCELED);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * ANNOUNCE/RECORD ingest path: the client publishes content to the server
 * (the reverse of DESCRIBE/PLAY), modeled on tools/rtsp_client_ingest_test.c
 * and tools/rtsp_server_ingest_test.c. Unlike DESCRIBE, the content_base
 * used for the follow-up SETUP is just the client's own connection URL -
 * there is no server-provided content_base to capture first.
 */

#define TEST_LOOPBACK_INGEST_SDP                                               \
	"v=0\r\n"                                                              \
	"o=- 0 0 IN IP4 127.0.0.1\r\n"                                         \
	"s=-\r\n"                                                              \
	"t=0 0\r\n"                                                            \
	"m=video 0 RTP/AVP 96\r\n"                                             \
	"a=control:trackID=1\r\n"


struct loopback_ingest_ctx {
	struct pomp_loop *loop;
	struct rtsp_server *server;
	struct rtsp_client *client;

	bool connected;
	bool disconnected;

	bool announce_done;
	enum rtsp_client_req_status announce_status;
	int announce_code;
	bool server_got_announce;

	bool setup_done;
	enum rtsp_client_req_status setup_status;
	char session_id[64];

	bool record_done;
	enum rtsp_client_req_status record_status;
	int record_code;
	bool server_got_record;

	bool teardown_done;
};


static void ingest_server_setup(struct rtsp_server *server,
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
	struct loopback_ingest_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(delivery);
	UNUSED(lower_transport);
	UNUSED(src_address);
	UNUSED(dst_address);
	UNUSED(dst_stream_port);
	UNUSED(dst_control_port);

	ret = rtsp_server_reply_to_setup(ctx->server,
					 request_ctx,
					 media_ctx,
					 0,
					 7000,
					 7001,
					 1,
					 0xabcdef01,
					 NULL,
					 0,
					 NULL);
	CU_ASSERT_EQUAL(ret, 0);
}


static void ingest_server_announce(struct rtsp_server *server,
				   const char *server_address,
				   const char *path,
				   const char *sdp,
				   const struct rtsp_header_ext *ext,
				   size_t ext_count,
				   void *request_ctx,
				   void *userdata)
{
	struct loopback_ingest_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);

	CU_ASSERT_PTR_NOT_NULL(sdp);
	ctx->server_got_announce = true;
	ret = rtsp_server_reply_to_announce(
		ctx->server, request_ctx, 0, NULL, 0);
	CU_ASSERT_EQUAL(ret, 0);
}


static void ingest_server_record(struct rtsp_server *server,
				 const char *session_id,
				 const struct rtsp_header_ext *ext,
				 size_t ext_count,
				 void *request_ctx,
				 void *media_ctx,
				 const struct rtsp_range *range,
				 void *stream_userdata,
				 void *userdata)
{
	struct loopback_ingest_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(stream_userdata);

	ctx->server_got_record = true;
	ret = rtsp_server_reply_to_record(
		ctx->server, request_ctx, media_ctx, 0, range, NULL, 0);
	CU_ASSERT_EQUAL(ret, 0);
}


static void ingest_server_teardown(struct rtsp_server *server,
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
	struct loopback_ingest_ctx *ctx = userdata;
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(stream_userdata);

	/* See loopback_server_teardown()'s comment: only a genuine
	 * client-initiated TEARDOWN request must be answered. */
	if (reason == RTSP_SERVER_TEARDOWN_REASON_CLIENT_REQUEST) {
		int ret = rtsp_server_reply_to_teardown(
			ctx->server, request_ctx, media_ctx, 0, NULL, 0);
		CU_ASSERT_EQUAL(ret, 0);
	}
}


static void ingest_client_connection_state(struct rtsp_client *client,
					   enum rtsp_client_conn_state state,
					   void *userdata)
{
	struct loopback_ingest_ctx *ctx = userdata;
	UNUSED(client);
	if (state == RTSP_CLIENT_CONN_STATE_CONNECTED)
		ctx->connected = true;
	else if (state == RTSP_CLIENT_CONN_STATE_DISCONNECTED)
		ctx->disconnected = true;
}


static void ingest_client_session_removed(struct rtsp_client *client,
					  const char *session_id,
					  int status,
					  void *userdata)
{
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(status);
	UNUSED(userdata);
}


static void ingest_client_announce(struct rtsp_client *client,
				   const char *content_base,
				   const struct rtsp_header_ext *ext,
				   size_t ext_count,
				   const char *sdp,
				   void *userdata)
{
	/* Incoming server-initiated announce: unused in this test, which
	 * only exercises the client-initiated rtsp_client_announce() */
	UNUSED(client);
	UNUSED(content_base);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(sdp);
	UNUSED(userdata);
}


static void ingest_client_announce_resp(struct rtsp_client *client,
					enum rtsp_client_req_status req_status,
					int status,
					const struct rtsp_header_ext *ext,
					size_t ext_count,
					void *userdata,
					void *req_userdata)
{
	struct loopback_ingest_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->announce_status = req_status;
	ctx->announce_code = status;
	ctx->announce_done = true;
}


static void ingest_client_setup_resp(struct rtsp_client *client,
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
	struct loopback_ingest_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(status);
	UNUSED(src_stream_port);
	UNUSED(src_control_port);
	UNUSED(ssrc_valid);
	UNUSED(ssrc);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->setup_status = req_status;
	if (session_id != NULL) {
		snprintf(ctx->session_id,
			 sizeof(ctx->session_id),
			 "%s",
			 session_id);
	}
	ctx->setup_done = true;
}


static void ingest_client_record_resp(struct rtsp_client *client,
				      const char *session_id,
				      enum rtsp_client_req_status req_status,
				      int status,
				      const struct rtsp_header_ext *ext,
				      size_t ext_count,
				      void *userdata,
				      void *req_userdata)
{
	struct loopback_ingest_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->record_status = req_status;
	ctx->record_code = status;
	ctx->record_done = true;
}


static void ingest_client_teardown_resp(struct rtsp_client *client,
					const char *session_id,
					enum rtsp_client_req_status req_status,
					int status,
					const struct rtsp_header_ext *ext,
					size_t ext_count,
					void *userdata,
					void *req_userdata)
{
	struct loopback_ingest_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(req_status);
	UNUSED(status);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->teardown_done = true;
}


static void test_rtsp_loopback_ingest(void)
{
	struct loopback_ingest_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char url[64];
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &fake_server_describe;
	server_cbs.setup = &ingest_server_setup;
	server_cbs.play = &fake_server_play;
	server_cbs.pause = &fake_server_pause;
	server_cbs.teardown = &ingest_server_teardown;
	server_cbs.request_timeout = &fake_server_request_timeout;
	server_cbs.announce = &ingest_server_announce;
	server_cbs.record = &ingest_server_record;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 3,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &ingest_client_connection_state;
	client_cbs.session_removed = &ingest_client_session_removed;
	client_cbs.announce = &ingest_client_announce;
	client_cbs.announce_resp = &ingest_client_announce_resp;
	client_cbs.setup_resp = &ingest_client_setup_resp;
	client_cbs.record_resp = &ingest_client_record_resp;
	client_cbs.teardown_resp = &ingest_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	snprintf(url,
		 sizeof(url),
		 "rtsp://127.0.0.1:%u/publish",
		 (unsigned int)(TEST_LOOPBACK_PORT + 3));
	ret = rtsp_client_connect(ctx.client, url);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* ANNOUNCE */
	ret = rtsp_client_announce(ctx.client,
				   "/publish",
				   TEST_LOOPBACK_INGEST_SDP,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.announce_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_TRUE(ctx.server_got_announce);
	CU_ASSERT_EQUAL(ctx.announce_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.announce_code, 0);

	/* SETUP for RECORD: content_base is just the connection URL, unlike
	 * the DESCRIBE/PLAY path where it comes from the server's reply */
	snprintf(resource_url, sizeof(resource_url), "%s/trackID=1", url);
	ret = rtsp_client_setup(ctx.client,
				url,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_RECORD,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);

	/* RECORD */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_record(ctx.client,
				 ctx.session_id,
				 &range,
				 NULL,
				 0,
				 NULL,
				 LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.record_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_TRUE(ctx.server_got_record);
	CU_ASSERT_EQUAL(ctx.record_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.record_code, 0);

	/* TEARDOWN */
	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Session timeout: with a session_timeout_ms under 1000, the value
 * advertised to the client (session->timeout_ms / 1000, in whole seconds)
 * truncates to 0, so the client falls back to its own, much longer,
 * default keep-alive pace instead of probing fast enough to keep the
 * session alive - letting the server's own per-session timer fire.
 */
#define TEST_LOOPBACK_SESSION_TIMEOUT_MS 300


static void test_rtsp_loopback_session_timeout(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 4,
			      0,
			      TEST_LOOPBACK_SESSION_TIMEOUT_MS,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.teardown = &loopback_client_teardown_notify;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 4));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));

	/* No further client activity: just wait for the server's own
	 * per-session timer to expire the session. Unlike
	 * rtsp_server_force_teardown(), a natural session timeout does not
	 * push any wire notification to the client - there is no reliable
	 * way to notify a peer that may itself be unresponsive - so only
	 * the server side observably reacts here (session removed,
	 * cbs.teardown fired with reason SESSION_TIMEOUT); the client
	 * finds out only later, e.g. the next time it references the
	 * session and gets an error back. */
	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.server_got_teardown, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.last_teardown_reason,
			RTSP_SERVER_TEARDOWN_REASON_SESSION_TIMEOUT);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Request timeout: the server's request-reply-timeout sweep runs on a
 * fixed 1-second periodic timer (rtsp_server_timer_cb() in rtsp_server.c),
 * checking every pending request's own deadline (reply_timeout_ms after it
 * was received) - so this needs a pump window comfortably over 1 second,
 * not just over reply_timeout_ms itself.
 */
#define TEST_LOOPBACK_REPLY_TIMEOUT_MS 200


static void loopback_server_pause_no_reply(struct rtsp_server *server,
					   const char *session_id,
					   const struct rtsp_header_ext *ext,
					   size_t ext_count,
					   void *request_ctx,
					   void *media_ctx,
					   const struct rtsp_range *range,
					   void *stream_userdata,
					   void *userdata)
{
	/* Deliberately never replies, to let the server's own
	 * reply_timeout_ms elapse for this request */
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(request_ctx);
	UNUSED(media_ctx);
	UNUSED(range);
	UNUSED(stream_userdata);
	UNUSED(userdata);
}


static void
loopback_server_request_timeout_capture(struct rtsp_server *server,
					void *request_ctx,
					enum rtsp_method_type method,
					void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(server);
	UNUSED(request_ctx);
	ctx->timed_out_method = method;
	ctx->server_request_timeout_fired = true;
}


static void test_rtsp_loopback_request_timeout(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause_no_reply;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout_capture;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 5,
			      TEST_LOOPBACK_REPLY_TIMEOUT_MS,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp;
	client_cbs.pause_resp = &loopback_client_pause_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 5));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_play(ctx.client,
			       ctx.session_id,
			       &range,
			       1.0f,
			       NULL,
			       0,
			       NULL,
			       LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.play_done, LOOPBACK_TIMEOUT_MS));

	/* PAUSE is deliberately never answered by the server */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	ret = rtsp_client_pause(ctx.client,
				ctx.session_id,
				&range,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS * 2);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	CU_ASSERT_TRUE(loopback_pump_until(ctx.loop,
					   &ctx.server_request_timeout_fired,
					   LOOPBACK_TIMEOUT_MS * 2));
	CU_ASSERT_EQUAL(ctx.timed_out_method, RTSP_METHOD_TYPE_PAUSE);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Server-side error replies: the application callback can decline a
 * request by passing a negative status to rtsp_server_reply_to_*(), which
 * skips straight to building an error response (RTSP status class > 2xx)
 * instead of requiring the normal success-path arguments. This exercises
 * the server's error-response-building code and the client's
 * RTSP_CLIENT_REQ_STATUS_FAILED handling, neither reachable from the
 * happy-path handshake test.
 */

static void
loopback_server_describe_conditional(struct rtsp_server *server,
				     const char *server_address,
				     const char *path,
				     const struct rtsp_header_ext *ext,
				     size_t ext_count,
				     void *request_ctx,
				     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int status = 0;
	int ret;
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(ext);
	UNUSED(ext_count);

	if ((path != NULL) && (strstr(path, "bad") != NULL))
		status = -ENOENT;

	ret = rtsp_server_reply_to_describe(
		ctx->server,
		request_ctx,
		status,
		NULL,
		0,
		(status == 0) ? (char *)TEST_LOOPBACK_SDP : NULL);
	CU_ASSERT_EQUAL(ret, 0);
}


static void
loopback_server_setup_conditional(struct rtsp_server *server,
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
	struct loopback_ctx *ctx = userdata;
	int status = 0;
	int ret;
	UNUSED(server);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(delivery);
	UNUSED(lower_transport);
	UNUSED(src_address);
	UNUSED(dst_address);
	UNUSED(dst_stream_port);
	UNUSED(dst_control_port);

	if ((path != NULL) && (strstr(path, "reject") != NULL))
		status = -ENOSYS;

	ret = rtsp_server_reply_to_setup(ctx->server,
					 request_ctx,
					 media_ctx,
					 status,
					 6000,
					 6001,
					 (status == 0) ? 1 : 0,
					 (status == 0) ? 0x12345678u : 0,
					 NULL,
					 0,
					 NULL);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_rtsp_loopback_server_errors(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe_conditional;
	server_cbs.setup = &loopback_server_setup_conditional;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 6,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 6));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* DESCRIBE a path the server declines: 404 Not Found */
	ret = rtsp_client_describe(
		ctx.client, "/bad", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_FAILED);
	CU_ASSERT_EQUAL(ctx.describe_code, -ENOENT);

	/* DESCRIBE again on a path the server accepts */
	ctx.describe_done = false;
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.describe_code, 0);
	CU_ASSERT_TRUE(strlen(ctx.content_base) > 0);

	/* SETUP a resource the server declines: 501 Not Implemented */
	snprintf(resource_url,
		 sizeof(resource_url),
		 "%sreject",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_FAILED);
	CU_ASSERT_EQUAL(ctx.setup_code, -ENOSYS);
	CU_ASSERT_TRUE(strlen(ctx.session_id) == 0);

	/* SETUP a resource the server accepts */
	ctx.setup_done = false;
	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);

	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * TCP interleaved transport: SETUP negotiates a pair of interleaved
 * channels over the shared RTSP/TCP connection instead of separate UDP
 * ports, then RTP/RTCP-like data is exchanged in both directions over that
 * same connection via rtsp_{client,server}_send_interleaved(). This is the
 * only way to exercise the client's interleaved-channel bookkeeping
 * (allocate/count/check_channel_unused, update_interleaved_channels,
 * channel_pair_cmp): the server's reply doesn't need any special handling
 * for TCP - rtsp_server_reply_to_setup() automatically echoes the
 * channel pair straight from the client's own request.
 */

static void loopback_client_interleaved_data(struct rtsp_client *client,
					     uint8_t channel,
					     const uint8_t *data,
					     size_t len,
					     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	ctx->client_got_interleaved_data = true;
	ctx->client_interleaved_channel = channel;
	ctx->client_interleaved_len = len;
	if (len <= sizeof(ctx->client_interleaved_data))
		memcpy(ctx->client_interleaved_data, data, len);
}


static void loopback_server_interleaved_data(struct rtsp_server *server,
					     struct pomp_conn *conn,
					     uint8_t channel,
					     const void *data,
					     size_t len,
					     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(server);
	UNUSED(conn);
	ctx->server_got_interleaved_data = true;
	ctx->server_interleaved_channel = channel;
	ctx->server_interleaved_len = len;
	if (len <= sizeof(ctx->server_interleaved_data))
		memcpy(ctx->server_interleaved_data, data, len);
}


static void test_rtsp_loopback_tcp_interleaved(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	static const uint8_t c2s_data[] = {0x01, 0x02, 0x03, 0x04};
	static const uint8_t s2c_data[] = {0xaa, 0xbb, 0xcc};
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;
	server_cbs.interleaved_data = &loopback_server_interleaved_data;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 7,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;
	client_cbs.interleaved_data_cb = &loopback_client_interleaved_data;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 7));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	/* SETUP over TCP: channels 0 (stream) and 1 (control), instead of
	 * real UDP ports */
	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_TCP,
				0,
				1,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);

	/* Client -> server, on channel 0 */
	ret = rtsp_client_send_interleaved(
		ctx.client, 0, c2s_data, sizeof(c2s_data));
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(
		loopback_pump_until(ctx.loop,
				    &ctx.server_got_interleaved_data,
				    LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.server_interleaved_channel, (uint8_t)0);
	CU_ASSERT_EQUAL(ctx.server_interleaved_len, sizeof(c2s_data));
	CU_ASSERT_EQUAL(
		memcmp(ctx.server_interleaved_data, c2s_data, sizeof(c2s_data)),
		0);

	/* Server -> client, on the same channel */
	ret = rtsp_server_send_interleaved(
		ctx.server, 0, s2c_data, sizeof(s2c_data));
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(
		loopback_pump_until(ctx.loop,
				    &ctx.client_got_interleaved_data,
				    LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.client_interleaved_channel, (uint8_t)0);
	CU_ASSERT_EQUAL(ctx.client_interleaved_len, sizeof(s2c_data));
	CU_ASSERT_EQUAL(
		memcmp(ctx.client_interleaved_data, s2c_data, sizeof(s2c_data)),
		0);

	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Socket tuning on a live connection, and rtsp_client_remove_session():
 * both untested via the fully-connected path in earlier tests
 * (test_rtsp_client_state_machine only exercises set_socket_* before any
 * connect).
 */

static void test_rtsp_loopback_live_socket_and_remove_session(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 8,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 8));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* Socket tuning on a real, live socket (unlike
	 * test_rtsp_client_state_machine, which only covers the
	 * not-yet-connected stash path) */
	ret = rtsp_client_set_socket_rxbuf_size(ctx.client, 8192);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_client_set_socket_txbuf_size(ctx.client, 8192);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_client_set_socket_class_selector(ctx.client, 0);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);

	/* Explicitly remove the session client-side, instead of going
	 * through a TEARDOWN request/response round trip */
	ret = rtsp_client_remove_session(ctx.client, ctx.session_id);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.session_removed, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Server-initiated ANNOUNCE (rtsp_server_announce()): the reverse
 * direction from rtsp_client_announce() - the server pushes updated
 * content to the client, which receives it as an incoming RTSP request on
 * cbs.announce and derives a content_base via client_uri_to_content_base(),
 * neither reachable from the client-initiated ingest test.
 */

static void loopback_client_announce_capture(struct rtsp_client *client,
					     const char *content_base,
					     const struct rtsp_header_ext *ext,
					     size_t ext_count,
					     const char *sdp,
					     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(ext);
	UNUSED(ext_count);
	if (content_base != NULL) {
		snprintf(ctx->announce_content_base,
			 sizeof(ctx->announce_content_base),
			 "%s",
			 content_base);
	}
	if (sdp != NULL) {
		snprintf(ctx->announce_sdp,
			 sizeof(ctx->announce_sdp),
			 "%s",
			 sdp);
	}
	ctx->client_got_announce = true;
}


static void test_rtsp_loopback_server_initiated_announce(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 9,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &loopback_client_announce_capture;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 9));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_server_announce(ctx.server,
				   (char *)"/live",
				   NULL,
				   0,
				   (char *)TEST_LOOPBACK_SDP);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.client_got_announce, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_TRUE(strlen(ctx.announce_content_base) > 0);
	CU_ASSERT_TRUE(strlen(ctx.announce_sdp) > 0);

	/* Let the client's auto-generated 200 OK reply to the pushed
	 * ANNOUNCE reach and get processed by the server
	 * (rtsp_server_response_process()) before disconnecting. */
	loopback_pump_for(ctx.loop, 200);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Client-side response timeout (rtsp_client_resp_timeout_cb()): the
 * mirror image of test_rtsp_loopback_request_timeout, from the client's
 * own perspective - a short per-call timeout_ms elapses before the
 * (deliberately silent) server ever replies.
 */

#define TEST_LOOPBACK_CLIENT_TIMEOUT_MS 300


static void loopback_client_pause_resp_timeout_capture(
	struct rtsp_client *client,
	const char *session_id,
	enum rtsp_client_req_status req_status,
	int status,
	const struct rtsp_range *range,
	const struct rtsp_header_ext *ext,
	size_t ext_count,
	void *userdata,
	void *req_userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(status);
	UNUSED(range);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->pause_status = req_status;
	if (req_status == RTSP_CLIENT_REQ_STATUS_TIMEOUT)
		ctx->client_resp_timeout_fired = true;
	ctx->pause_done = true;
}


static void test_rtsp_loopback_client_resp_timeout(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	/* Server-side reply_timeout_ms stays at its (1s-periodic-swept)
	 * default, comfortably slower than the client's own short
	 * per-call timeout below, so the client's own timer is what fires
	 * first here. The server's own periodic sweep is still expected to
	 * eventually flag this same never-answered PAUSE too (it shares
	 * the loop, which stays pumped past the client's own timeout while
	 * waiting for disconnect) - so this uses the capturing stub, not
	 * the CU_FAIL-asserting one meant for tests where that should
	 * never happen. */
	server_cbs.pause = &loopback_server_pause_no_reply;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout_capture;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 10,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp;
	client_cbs.pause_resp = &loopback_client_pause_resp_timeout_capture;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 10));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_play(ctx.client,
			       ctx.session_id,
			       &range,
			       1.0f,
			       NULL,
			       0,
			       NULL,
			       LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.play_done, LOOPBACK_TIMEOUT_MS));

	/* PAUSE with a short client-side timeout, against a server that
	 * never replies */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	ret = rtsp_client_pause(ctx.client,
				ctx.session_id,
				&range,
				NULL,
				0,
				NULL,
				TEST_LOOPBACK_CLIENT_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.pause_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_TRUE(ctx.client_resp_timeout_fired);
	CU_ASSERT_EQUAL(ctx.pause_status, RTSP_CLIENT_REQ_STATUS_TIMEOUT);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Client keep-alive (GET_PARAMETER probes), server-side: with no further
 * explicit client activity, the client library itself periodically probes
 * the session (at roughly 40% of the server-advertised session timeout,
 * per rtsp_client.c's send_keep_alive()) to keep it from expiring. This is
 * the only way to reach the server's GET_PARAMETER handling
 * (rtsp_server_get_parameter()) without the test driving it directly -
 * there is no public API to send a bare keep-alive on demand. Waiting past
 * the nominal session timeout and confirming the session is still usable
 * is an indirect but conclusive proof the server actually processed at
 * least one such probe (without it, the session would already be gone).
 */
#define TEST_LOOPBACK_KEEPALIVE_SESSION_TIMEOUT_MS 4000


static void test_rtsp_loopback_client_keep_alive(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 11,
			      0,
			      TEST_LOOPBACK_KEEPALIVE_SESSION_TIMEOUT_MS,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.pause_resp = &loopback_client_pause_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 11));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55004,
				55005,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));

	/* No further explicit client activity: just wait past the nominal
	 * session timeout and rely on the client's automatic keep-alive to
	 * have kept the session alive on the server. */
	loopback_pump_for(ctx.loop,
			  TEST_LOOPBACK_KEEPALIVE_SESSION_TIMEOUT_MS + 1500);
	CU_ASSERT_FALSE(ctx.server_got_teardown);

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	ret = rtsp_client_pause(ctx.client,
				ctx.session_id,
				&range,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.pause_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.pause_status, RTSP_CLIENT_REQ_STATUS_OK);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * Client-side auth header generation (generate_authorization_header()'s
 * client->auth != NULL branch, and update_auth_from_server()'s server !=
 * NULL branch): rtsp_server has no API to attach a WWW-Authenticate
 * header to any reply (rtsp_response_header_write() has no code path for
 * it at all, confirmed by inspection), so a genuine 401 challenge cannot
 * be issued through it. But header parsing on the wire does not care
 * whether a header line was classified by the sender as "known" or
 * "extension" - only the header NAME on the wire matters - so a reply's
 * generic 'ext' array (written verbatim as "key: value\r\n", with no
 * filtering) can carry a literal WWW-Authenticate line that the client's
 * parser will recognize and process exactly like a real challenge. This
 * is semantically unusual (a real server wouldn't send this on a 200 OK),
 * but it drives the exact same client-side code as a real one would.
 */

static void
loopback_server_describe_with_challenge(struct rtsp_server *server,
					const char *server_address,
					const char *path,
					const struct rtsp_header_ext *ext,
					size_t ext_count,
					void *request_ctx,
					void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int ret;
	struct rtsp_header_ext challenge = {
		.key = "WWW-Authenticate",
		.value = "Digest realm=\"test-realm\", nonce=\"abc123nonce\", "
			 "algorithm=MD5, qop=\"auth\"",
	};
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);

	ret = rtsp_server_reply_to_describe(ctx->server,
					    request_ctx,
					    0,
					    &challenge,
					    1,
					    (char *)TEST_LOOPBACK_SDP);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_rtsp_loopback_client_auth_header_generation(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe_with_challenge;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 12,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[96];
		snprintf(url,
			 sizeof(url),
			 "rtsp://testuser:testpass@127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 12));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* First DESCRIBE: the reply's smuggled WWW-Authenticate populates
	 * client->auth via update_auth_from_server(). */
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* Second DESCRIBE: generate_authorization_header() now takes its
	 * client->auth != NULL branch, computing a real Digest response
	 * from the URL's embedded credentials and the previously-received
	 * challenge, and attaches it to this request. */
	ctx.describe_done = false;
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_OK);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/*
 * DNS resolution failure/timeout (resolv_timer_cb()): connecting to a
 * hostname under the ".invalid" TLD, reserved by RFC 2606 to never
 * resolve, drives the client to either the resolver's own fast-failure
 * callback or, if nothing answers at all within
 * RTSP_CLIENT_RESOLV_TIMEOUT_MS (5s), resolv_timer_cb()'s own timeout
 * path - whichever it is depends on the resolver/sandbox's exact network
 * behavior, which this test does not control. Either way, the client
 * must end up back at DISCONNECTED, never CONNECTED. No real server is
 * needed for this test at all: the client uses none of the loopback
 * server's other fields, so its rtsp_server_cbs.request_ctx-based
 * plumbing is never engaged. */
#define TEST_LOOPBACK_RESOLV_WAIT_MS                                           \
	6500 /* > RTSP_CLIENT_RESOLV_TIMEOUT_MS                                \
	      */


static void test_rtsp_client_resolve_failure(void)
{
	struct loopback_ctx ctx;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	ret = rtsp_client_connect(
		ctx.client, "rtsp://this-host-does-not-exist.invalid/live");
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.disconnected, TEST_LOOPBACK_RESOLV_WAIT_MS));
	CU_ASSERT_FALSE(ctx.connected);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* SETUP+PLAY where the server replies with a valid RTP-Info (seq + rtptime)
 * -- covers rtsp_server_reply_to_play()'s RTP-Info header generation branch,
 * and pins down the value round-tripping correctly to the client's
 * play_resp callback. */
static void test_rtsp_loopback_play_rtp_info(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play_with_rtp_info;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 13,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp_capture_rtp_info;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 13));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55020,
				55021,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_play(ctx.client,
			       ctx.session_id,
			       &range,
			       1.0f,
			       NULL,
			       0,
			       NULL,
			       LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.play_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.play_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.play_code, 0);
	CU_ASSERT_TRUE(ctx.play_rtp_info_seq_valid);
	CU_ASSERT_EQUAL(ctx.play_rtp_info_seq, (uint16_t)1000);
	CU_ASSERT_TRUE(ctx.play_rtp_info_rtptime_valid);
	CU_ASSERT_EQUAL(ctx.play_rtp_info_rtptime, (uint32_t)90000);

	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* TEARDOWN targeting a track that doesn't exist in the session (e.g. after
 * a typo, or a stale cached track list): none of the session's media match
 * the requested path/prefix, so rtsp_server_teardown()'s
 * `list_is_empty(&request->medias)` branch fires (-ENOENT) before ever
 * touching the real media -- the session and its one real media must
 * survive this untouched, provable by tearing it down for real right after
 * with a normal, successful TEARDOWN. */
static void test_rtsp_loopback_teardown_no_match(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	char bogus_resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 14,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 14));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55022,
				55023,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* TEARDOWN a track that was never set up: no media matches, session
	 * and its real media must survive. Deliberately a top-level absolute
	 * path with no '/' of its own (not nested under content_base, unlike
	 * "trackID=1") -- rtsp_server_teardown()'s non-exact-match filtering
	 * only actually excludes non-matching media when is_prefix is true
	 * (i.e. the derived path itself has no internal '/'); a path with an
	 * embedded '/' but no exact match falls through the loop's filter
	 * unfiltered instead, which would accidentally tear down the real
	 * media rather than exercising the intended -ENOENT/no-match path. */
	snprintf(bogus_resource_url,
		 sizeof(bogus_resource_url),
		 "rtsp://127.0.0.1:%u/bogus-track",
		 (unsigned int)(TEST_LOOPBACK_PORT + 14));
	ret = rtsp_client_teardown(ctx.client,
				   bogus_resource_url,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	/* The server correctly rejects this with 404 (visible as "path
	 * 'bogus-track' not found" in the log), but rtsp_client's own
	 * teardown_request_complete() cannot locally correlate the response
	 * to any known media either (this resource_url matches neither a
	 * specific media nor the session's own content_base) and silently
	 * drops it without ever invoking teardown_resp() -- unlike every
	 * other error response exercised elsewhere in this suite, no
	 * client-side callback fires for this one at all, so there is
	 * nothing to wait on here beyond giving it a bounded window to (not)
	 * happen. */
	loopback_pump_for(ctx.loop, 300);
	CU_ASSERT_FALSE(ctx.teardown_done);
	CU_ASSERT_FALSE(ctx.session_removed);

	/* the real media is still there: a genuine teardown must still
	 * succeed */
	ctx.teardown_done = false;
	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.teardown_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.teardown_code, 0);
	CU_ASSERT_TRUE(ctx.server_got_teardown);

	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.session_removed, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* TEARDOWN using the exact same resource_url that was used for SETUP
 * (instead of NULL, which every other loopback test uses) -- covers
 * rtsp_server_teardown()'s exact-match `media_found = true` branch
 * (rtsp_server_session_media_find() succeeding), never reached by a
 * whole-session (NULL resource_url) or unmatched-prefix teardown. */
static void test_rtsp_loopback_teardown_exact_media_match(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 15,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 15));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55024,
				55025,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* tear down the exact same resource_url used for SETUP, instead of
	 * the whole-session NULL every other test uses */
	ret = rtsp_client_teardown(ctx.client,
				   resource_url,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.teardown_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.teardown_code, 0);
	CU_ASSERT_TRUE(ctx.server_got_teardown);

	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.session_removed, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* TEARDOWN with a resource_url that resolves to an empty path (just the
 * bare host:port, trailing slash, nothing after) -- covers
 * rtsp_server_teardown()'s `(path == NULL) || (path[0] == '\0')` branch
 * (is_prefix=true, prefix_len=0), distinct from every other loopback
 * teardown test's non-empty path (either the whole content_base "/live" or
 * a "trackID=..." suffix). */
static void test_rtsp_loopback_teardown_whole_session_empty_path(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	char empty_path_url[64];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 16,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 16));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55026,
				55027,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* absolute URL, trailing slash, no path segment at all -> empty path
	 * server-side */
	snprintf(empty_path_url,
		 sizeof(empty_path_url),
		 "rtsp://127.0.0.1:%u/",
		 (unsigned int)(TEST_LOOPBACK_PORT + 16));
	ret = rtsp_client_teardown(ctx.client,
				   empty_path_url,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	/* The server fully tears down and removes the session (confirmed via
	 * server_got_teardown, and independently by the "all media torn
	 * down, removing session" log line) -- but exactly like the
	 * no-match case above, rtsp_client's own teardown_request_complete()
	 * cannot locally correlate this empty path against the session's
	 * real (non-empty, "live"-based) content_base, so it silently drops
	 * the response without invoking teardown_resp() or updating its own
	 * local session tracking. This leaves the client's local session
	 * state stale (believing the session/media still exist when the
	 * server already destroyed them) -- characterized here rather than
	 * worked around, since a real caller only ever passes
	 * resource_url=NULL (matching content_base exactly, see the other
	 * two teardown tests above), never an absolute URL that happens to
	 * resolve to an empty path while content_base does not. */
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.server_got_teardown, LOOPBACK_TIMEOUT_MS));
	loopback_pump_for(ctx.loop, 300);
	CU_ASSERT_FALSE(ctx.teardown_done);
	CU_ASSERT_FALSE(ctx.session_removed);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* SETUP over TCP with client_stream_port == client_control_port == 0:
 * is_channel_pair_valid() treats a pair with rtp == rtcp as invalid (0 == 0
 * here), so rtsp_client_setup() cannot use it as an explicit request and
 * falls into allocate_interleaved_channels()'s "use next available
 * channels" loop instead -- distinct from every other TCP-interleaved
 * loopback test, which explicitly requests channels 0/1 and so only
 * exercises the explicit-pair validation branch. */
static void test_rtsp_loopback_tcp_interleaved_auto_channels(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;
	server_cbs.interleaved_data = &loopback_server_interleaved_data;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 17,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;
	client_cbs.interleaved_data_cb = &loopback_client_interleaved_data;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 17));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_TCP,
				0,
				0,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);

	/* allocate_interleaved_channels()'s "use next available channels"
	 * loop starts at channel 0, and nothing else has claimed any channel
	 * on this freshly created client, so it must have picked 0 (rtp) and
	 * 1 (rtcp) -- proven behaviorally by actually exchanging data on
	 * channel 0, the same way test_rtsp_loopback_tcp_interleaved()
	 * verifies its own explicitly-requested channel 0. */
	{
		static const uint8_t data[] = {0x11, 0x22, 0x33};

		ret = rtsp_client_send_interleaved(
			ctx.client, 0, data, sizeof(data));
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_TRUE_FATAL(
			loopback_pump_until(ctx.loop,
					    &ctx.server_got_interleaved_data,
					    LOOPBACK_TIMEOUT_MS));
		CU_ASSERT_EQUAL(ctx.server_interleaved_channel, (uint8_t)0);
		CU_ASSERT_EQUAL(ctx.server_interleaved_len, sizeof(data));
		CU_ASSERT_EQUAL(
			memcmp(ctx.server_interleaved_data, data, sizeof(data)),
			0);
	}

	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* rtsp_client_setup()/_play()/_pause()/_record()'s "make sure we know the
 * session" client-side guards (a bogus session_id never reaches the wire at
 * all), plus rtsp_client_setup()'s second guard (an existing, real session
 * id but a mismatched content_base -> -EINVAL). All purely local/synchronous
 * checks: no request is ever sent for any of these, so no server-side
 * involvement is needed beyond the one real SETUP used to obtain a genuine
 * session_id/content_base pair to probe against. */
static void test_rtsp_client_session_validation_errors(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 18,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp;
	client_cbs.pause_resp = &loopback_client_pause_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 18));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55030,
				55031,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* SETUP: bogus (never-seen) session id -> -ENOENT, no request sent */
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"bogus-session-id",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55032,
				55033,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* SETUP: real session id, but a content_base that does not match the
	 * session's own -> -EINVAL */
	ret = rtsp_client_setup(ctx.client,
				"rtsp://127.0.0.1:1/completely-different",
				resource_url,
				ctx.session_id,
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55034,
				55035,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* PLAY/PAUSE/RECORD: bogus session id -> -ENOENT, no request sent */
	{
		struct rtsp_range r;
		memset(&r, 0, sizeof(r));
		r.start.format = RTSP_TIME_FORMAT_NPT;
		r.start.npt.now = 1;
		r.stop.format = RTSP_TIME_FORMAT_NPT;
		r.stop.npt.infinity = 1;
		ret = rtsp_client_play(ctx.client,
				       "bogus-session-id",
				       &r,
				       1.0f,
				       NULL,
				       0,
				       NULL,
				       LOOPBACK_TIMEOUT_MS);
		CU_ASSERT_EQUAL(ret, -ENOENT);
	}

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	ret = rtsp_client_pause(ctx.client,
				"bogus-session-id",
				&range,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* rtsp_client_record()'s own "session not found" guard is identically
	 * shaped (confirmed by reading rtsp_client.c), but isn't exercised
	 * here: this is a normal playback (DESCRIBE/SETUP/PLAY) session, and
	 * the server never advertises RECORD as an allowed method for it, so
	 * the earlier methods_allowed guard rejects it with -ENOSYS before
	 * ever reaching the session lookup. RECORD only makes sense in an
	 * ingest (ANNOUNCE-based) session -- see test_rtsp_loopback_ingest. */

	/* none of the rejected calls above should have left a request
	 * pending or otherwise disturbed the real session */
	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.teardown_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.teardown_code, 0);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* Same trick as loopback_server_describe_with_challenge, but with a Basic
 * challenge instead of Digest -- covers generate_authorization_header()'s
 * RTSP_AUTH_TYPE_BASIC case, never reached by the Digest-only test above. */
static void
loopback_server_describe_with_basic_challenge(struct rtsp_server *server,
					      const char *server_address,
					      const char *path,
					      const struct rtsp_header_ext *ext,
					      size_t ext_count,
					      void *request_ctx,
					      void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int ret;
	struct rtsp_header_ext challenge = {
		.key = "WWW-Authenticate",
		.value = "Basic realm=\"test-realm\"",
	};
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);

	ret = rtsp_server_reply_to_describe(ctx->server,
					    request_ctx,
					    0,
					    &challenge,
					    1,
					    (char *)TEST_LOOPBACK_SDP);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_rtsp_loopback_client_basic_auth_header_generation(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe_with_basic_challenge;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 19,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[96];
		snprintf(url,
			 sizeof(url),
			 "rtsp://testuser:testpass@127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 19));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* First DESCRIBE: the reply's smuggled WWW-Authenticate populates
	 * client->auth (type Basic) via update_auth_from_server(). */
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* Second DESCRIBE: generate_authorization_header() now takes its
	 * RTSP_AUTH_TYPE_BASIC case, base64-encoding the URL's embedded
	 * credentials and attaching them to this request. */
	ctx.describe_done = false;
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_OK);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* TCP-interleaved SETUP followed by a TEARDOWN targeting that exact media
 * (not the whole-session NULL every other TCP-interleaved test uses) --
 * covers teardown_request_complete()'s `media != NULL` cleanup branch
 * (is_channel_pair_valid()/set_channel_pair_used()/
 * rtsp_client_session_media_remove()), never reached by a whole-session
 * teardown: rtsp_client_session_media_find() only returns a match for the
 * media's own exact path, never for the session's base content_base path
 * that a NULL resource_url produces. */
static void test_rtsp_loopback_tcp_interleaved_exact_teardown(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 20,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 20));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_TCP,
				0,
				1,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* tear down the exact media (its channel pair 0/1 must be released
	 * by teardown_request_complete()'s media != NULL branch) */
	ret = rtsp_client_teardown(ctx.client,
				   resource_url,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.teardown_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_EQUAL(ctx.teardown_code, 0);

	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.session_removed, LOOPBACK_TIMEOUT_MS));

	/* the released channels must be reusable: a second SETUP requesting
	 * the exact same 0/1 pair must succeed rather than being rejected as
	 * still in use -- this is what actually proves
	 * set_channel_pair_used(..., false) ran, not just that the code
	 * path executed without crashing */
	ctx.setup_done = false;
	memset(ctx.session_id, 0, sizeof(ctx.session_id));
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_TCP,
				0,
				1,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	ctx.teardown_done = false;
	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* RTSP_CLIENT_MAX_FAILED_REQUESTS (5) consecutive request timeouts, with no
 * successful request resetting the counter in between -- covers
 * request_complete()'s forced-reconnect branch (tskt_client_stop() +
 * tskt_client_connect() back to the same, still-listening server), never
 * reached by test_rtsp_loopback_client_resp_timeout's single timeout.
 * Confirmed via connected_count reaching 2: the initial connect, then the
 * forced reconnect's own real TCP handshake completing again. */
static void test_rtsp_loopback_client_repeated_timeout_reconnect(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_range range;
	char resource_url[320];
	struct timespec start;
	int ret;
	int i;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause_no_reply;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout_capture;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 21,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.play_resp = &loopback_client_play_resp;
	client_cbs.pause_resp = &loopback_client_pause_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 21));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.connected_count, 1);

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55040,
				55041,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	ret = rtsp_client_play(ctx.client,
			       ctx.session_id,
			       &range,
			       1.0f,
			       NULL,
			       0,
			       NULL,
			       LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.play_done, LOOPBACK_TIMEOUT_MS));

	/* 5 consecutive short-timeout PAUSE calls against a server that never
	 * replies: RTSP_CLIENT_MAX_FAILED_REQUESTS is reached on the 5th,
	 * forcing a reconnect */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.now = 1;
	for (i = 0; i < 5; i++) {
		ctx.pause_done = false;
		ret = rtsp_client_pause(
			ctx.client, ctx.session_id, &range, NULL, 0, NULL, 300);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
		CU_ASSERT_TRUE_FATAL(loopback_pump_until(
			ctx.loop, &ctx.pause_done, LOOPBACK_TIMEOUT_MS));
		CU_ASSERT_EQUAL(ctx.pause_status,
				RTSP_CLIENT_REQ_STATUS_TIMEOUT);
	}

	/* the forced reconnect targets the same still-listening server, so
	 * it must succeed: a genuine second CONNECTED transition */
	time_get_monotonic(&start);
	while (ctx.connected_count < 2) {
		pomp_loop_wait_and_process(ctx.loop, 20);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)LOOPBACK_TIMEOUT_MS * 1000)
			break;
	}
	CU_ASSERT_EQUAL(ctx.connected_count, 2);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* SETUP with a relative resource_url ("trackID=1", no scheme) instead of
 * the absolute "content_base + trackID=1" every other loopback test builds
 * -- covers format_request_uri()'s asprintf() concatenation branch, never
 * reached by an already-absolute control_url (which takes the "use
 * verbatim" branch instead). This is in fact the more common real-world
 * shape: SDP control attributes are typically relative. */
static void test_rtsp_loopback_setup_relative_resource_url(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 22,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;
	client_cbs.teardown_resp = &loopback_client_teardown_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 22));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	/* relative, not "%strackID=1" like every other test here --
	 * format_request_uri() must concatenate it onto content_base itself
	 */
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				"trackID=1",
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55042,
				55043,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);
	CU_ASSERT_TRUE(strlen(ctx.session_id) > 0);

	ret = rtsp_client_teardown(ctx.client,
				   NULL,
				   ctx.session_id,
				   NULL,
				   0,
				   NULL,
				   LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.teardown_done, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


static void loopback_client_socket_cb(int fd, void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(fd);
	ctx->client_socket_cb_fired = true;
}


static void loopback_server_socket_cb(int fd, void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	UNUSED(fd);
	ctx->server_socket_cb_fired = true;
}


/* Covers tskt_client_socket_created_cb()'s/the server-side equivalent's
 * `if (cbs.socket_cb) ...` branch -- never taken by any other loopback test,
 * since none of them set this optional callback. */
static void test_rtsp_loopback_socket_created_callbacks(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;
	server_cbs.socket_cb = &loopback_server_socket_cb;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 23,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.socket_cb = &loopback_client_socket_cb;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 23));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_TRUE(ctx.client_socket_cb_fired);
	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.server_socket_cb_fired, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* A buffer size this large should exceed the OS's net.core.rmem_max/
 * wmem_max ceiling, so the kernel silently clamps the actual value --
 * set_socket_rxbuf_size()/set_socket_txbuf_size()'s readback verification
 * then mismatches, hitting the "failed to set" -ENOSYS branch instead of a
 * hard error straight out of the setsockopt() call itself. */
static void test_rtsp_loopback_huge_socket_buffer_size(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 24,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 24));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_set_socket_rxbuf_size(ctx.client, 100 * 1024 * 1024);
	CU_ASSERT_EQUAL(ret, -ENOSYS);
	ret = rtsp_client_set_socket_txbuf_size(ctx.client, 100 * 1024 * 1024);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* First call: server replies to DESCRIBE with an empty session description.
 * Second call: server replies with one over RTSP_SESSION_DESCRIPTION_MAX_LEN
 * (UINT16_MAX). Covers both of rtsp_server_reply_to_describe()'s session
 * description validation branches -- an app-side misuse the server must
 * reject rather than send a malformed/oversized response for. */
static void loopback_server_describe_bad_sdp(struct rtsp_server *server,
					     const char *server_address,
					     const char *path,
					     const struct rtsp_header_ext *ext,
					     size_t ext_count,
					     void *request_ctx,
					     void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);

	ctx->describe_call_count++;

	if (ctx->describe_call_count == 1) {
		ret = rtsp_server_reply_to_describe(
			ctx->server, request_ctx, 0, NULL, 0, (char *)"");
	} else {
		static char huge_sdp[70000];
		memset(huge_sdp, 'a', sizeof(huge_sdp) - 1);
		huge_sdp[sizeof(huge_sdp) - 1] = '\0';
		ret = rtsp_server_reply_to_describe(
			ctx->server, request_ctx, 0, NULL, 0, huge_sdp);
	}
	CU_ASSERT_TRUE(ret < 0);
}


static void test_rtsp_loopback_describe_sdp_validation(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe_bad_sdp;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 25,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 25));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* first DESCRIBE: empty session description */
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_NOT_EQUAL(ctx.describe_code, 0);

	/* second DESCRIBE: oversized session description */
	ctx.describe_done = false;
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_NOT_EQUAL(ctx.describe_code, 0);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* Server replies to SETUP with source ports 0/0 -- covers
 * rtsp_server_reply_to_setup()'s port validation branch. */
static void
loopback_server_setup_invalid_ports(struct rtsp_server *server,
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
	struct loopback_ctx *ctx = userdata;
	int ret;
	UNUSED(server);
	UNUSED(path);
	UNUSED(session_id);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(delivery);
	UNUSED(lower_transport);
	UNUSED(src_address);
	UNUSED(dst_address);
	UNUSED(dst_stream_port);
	UNUSED(dst_control_port);

	ret = rtsp_server_reply_to_setup(ctx->server,
					 request_ctx,
					 media_ctx,
					 0,
					 0,
					 0,
					 1,
					 0x12345678,
					 NULL,
					 0,
					 NULL);
	CU_ASSERT_TRUE(ret < 0);
}


static void test_rtsp_loopback_setup_reply_invalid_ports(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup_invalid_ports;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 26,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 26));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55044,
				55045,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_NOT_EQUAL(ctx.setup_code, 0);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* Same silent-session-expiry technique as test_rtsp_loopback_session_timeout,
 * but instead of just observing the server-side removal, this issues a
 * second SETUP (adding a track to what the client still believes is a valid
 * session) once the session has already silently expired server-side --
 * covers rtsp_server_setup()'s "existing session" -> session-not-found
 * branch, unreachable via the public client API with a locally-bogus
 * session_id (rtsp_client_setup() validates that locally first and never
 * reaches the wire at all; see test_rtsp_client_session_validation_errors).
 */
static void test_rtsp_loopback_setup_existing_session_not_found(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	char resource_url[320];
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 28,
			      0,
			      TEST_LOOPBACK_SESSION_TIMEOUT_MS,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.teardown = &loopback_client_teardown_notify;
	client_cbs.describe_resp = &loopback_client_describe_resp;
	client_cbs.setup_resp = &loopback_client_setup_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[64];
		snprintf(url,
			 sizeof(url),
			 "rtsp://127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 28));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	snprintf(resource_url,
		 sizeof(resource_url),
		 "%strackID=1",
		 ctx.content_base);
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				"",
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55046,
				55047,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.setup_status, RTSP_CLIENT_REQ_STATUS_OK);

	/* let the server silently expire and remove the session */
	CU_ASSERT_TRUE(loopback_pump_until(
		ctx.loop, &ctx.server_got_teardown, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.last_teardown_reason,
			RTSP_SERVER_TEARDOWN_REASON_SESSION_TIMEOUT);

	/* the client still believes this session_id is valid: this SETUP
	 * passes rtsp_client_setup()'s own local check and reaches the
	 * wire, where the server can no longer find it */
	ctx.setup_done = false;
	ret = rtsp_client_setup(ctx.client,
				ctx.content_base,
				resource_url,
				ctx.session_id,
				RTSP_DELIVERY_UNICAST,
				RTSP_LOWER_TRANSPORT_UDP,
				55048,
				55049,
				RTSP_TRANSPORT_METHOD_UNKNOWN,
				NULL,
				0,
				NULL,
				LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.setup_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_NOT_EQUAL(ctx.setup_code, 0);

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* First DESCRIBE: the usual smuggling trick (status=0 + a smuggled
 * WWW-Authenticate, since rtsp_server_reply_to_describe() can't attach ext
 * headers to a genuine non-success status -- that routes through the
 * ext-less error_response() instead) populates client->auth/server_auth.
 * Second DESCRIBE (now carrying a real Digest Authorization header):
 * a genuine 401 with no WWW-Authenticate at all -- covers
 * request_complete()'s "401 without WWW-Authenticate, nonce probably
 * invalidated" branch, which clears the cached auth data. */
static void loopback_server_describe_challenge_then_bare_401(
	struct rtsp_server *server,
	const char *server_address,
	const char *path,
	const struct rtsp_header_ext *ext,
	size_t ext_count,
	void *request_ctx,
	void *userdata)
{
	struct loopback_ctx *ctx = userdata;
	int ret;
	struct rtsp_header_ext challenge = {
		.key = "WWW-Authenticate",
		.value = "Digest realm=\"test-realm\", nonce=\"abc123nonce\", "
			 "algorithm=MD5, qop=\"auth\"",
	};
	UNUSED(server);
	UNUSED(server_address);
	UNUSED(path);
	UNUSED(ext);
	UNUSED(ext_count);

	ctx->describe_call_count++;

	if (ctx->describe_call_count == 1) {
		ret = rtsp_server_reply_to_describe(ctx->server,
						    request_ctx,
						    0,
						    &challenge,
						    1,
						    (char *)TEST_LOOPBACK_SDP);
	} else {
		ret = rtsp_server_reply_to_describe(
			ctx->server,
			request_ctx,
			RTSP_STATUS_CODE_UNAUTHORIZED,
			NULL,
			0,
			NULL);
	}
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_rtsp_loopback_client_401_without_challenge_clears_auth(void)
{
	struct loopback_ctx ctx;
	struct rtsp_server_cbs server_cbs;
	struct rtsp_client_cbs client_cbs;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx.loop);

	memset(&server_cbs, 0, sizeof(server_cbs));
	server_cbs.describe = &loopback_server_describe_challenge_then_bare_401;
	server_cbs.setup = &loopback_server_setup;
	server_cbs.play = &loopback_server_play;
	server_cbs.pause = &loopback_server_pause;
	server_cbs.teardown = &loopback_server_teardown;
	server_cbs.request_timeout = &loopback_server_request_timeout;

	ret = rtsp_server_new(NULL,
			      TEST_LOOPBACK_PORT + 29,
			      0,
			      0,
			      ctx.loop,
			      &server_cbs,
			      &ctx,
			      &ctx.server);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &loopback_client_connection_state;
	client_cbs.session_removed = &loopback_client_session_removed;
	client_cbs.announce = &fake_client_announce;
	client_cbs.describe_resp = &loopback_client_describe_resp;

	ret = rtsp_client_new(ctx.loop, NULL, &client_cbs, &ctx, &ctx.client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	{
		char url[96];
		snprintf(url,
			 sizeof(url),
			 "rtsp://testuser:testpass@127.0.0.1:%u/live",
			 (unsigned int)(TEST_LOOPBACK_PORT + 29));
		ret = rtsp_client_connect(ctx.client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.connected, LOOPBACK_TIMEOUT_MS));

	/* first DESCRIBE: smuggled challenge populates client->auth */
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	/* second DESCRIBE: attaches a real Digest Authorization header
	 * (generate_authorization_header()'s client->auth != NULL branch),
	 * gets a bare 401 back with no challenge */
	ctx.describe_done = false;
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));
	CU_ASSERT_EQUAL(ctx.describe_status, RTSP_CLIENT_REQ_STATUS_FAILED);

	/* third DESCRIBE: client->auth was cleared by the bare 401, so this
	 * one carries no Authorization header at all -- proven indirectly
	 * by the exchange completing cleanly rather than hanging or
	 * crashing on stale auth state */
	ctx.describe_done = false;
	ret = rtsp_client_describe(
		ctx.client, "/live", NULL, 0, NULL, LOOPBACK_TIMEOUT_MS);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(loopback_pump_until(
		ctx.loop, &ctx.describe_done, LOOPBACK_TIMEOUT_MS));

	ret = rtsp_client_disconnect(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	loopback_pump_until(ctx.loop, &ctx.disconnected, LOOPBACK_TIMEOUT_MS);

	ret = rtsp_client_destroy(ctx.client);
	CU_ASSERT_EQUAL(ret, 0);
	ret = rtsp_server_destroy(ctx.server);
	CU_ASSERT_EQUAL(ret, 0);
	pomp_loop_destroy(ctx.loop);
}


/* clang-format off */
CU_TestInfo g_rtsp_test_loopback[] = {
	{FN("rtsp-loopback-client-new-args"), &test_rtsp_client_new_args},
	{FN("rtsp-loopback-client-enosys-matrix"),
	 &test_rtsp_client_enosys_matrix},
	{FN("rtsp-loopback-client-state-machine"),
	 &test_rtsp_client_state_machine},
	{FN("rtsp-loopback-server-new-args"), &test_rtsp_server_new_args},
	{FN("rtsp-loopback-server-send-interleaved-args"),
	 &test_rtsp_server_send_interleaved_args},
	{FN("rtsp-loopback-server-announce-args"),
	 &test_rtsp_server_announce_args},
	{FN("rtsp-loopback-server-force-teardown-args"),
	 &test_rtsp_server_force_teardown_args},
	{FN("rtsp-loopback-server-reply-bogus-request-ctx"),
	 &test_rtsp_server_reply_bogus_request_ctx},
	{FN("rtsp-loopback-full-handshake"),
	 &test_rtsp_loopback_full_handshake},
	{FN("rtsp-loopback-force-teardown"),
	 &test_rtsp_loopback_force_teardown},
	{FN("rtsp-loopback-client-cancel-pending-request"),
	 &test_rtsp_client_cancel_pending_request},
	{FN("rtsp-loopback-ingest"), &test_rtsp_loopback_ingest},
	{FN("rtsp-loopback-session-timeout"),
	 &test_rtsp_loopback_session_timeout},
	{FN("rtsp-loopback-request-timeout"),
	 &test_rtsp_loopback_request_timeout},
	{FN("rtsp-loopback-server-errors"), &test_rtsp_loopback_server_errors},
	{FN("rtsp-loopback-tcp-interleaved"),
	 &test_rtsp_loopback_tcp_interleaved},
	{FN("rtsp-loopback-live-socket-and-remove-session"),
	 &test_rtsp_loopback_live_socket_and_remove_session},
	{FN("rtsp-loopback-server-initiated-announce"),
	 &test_rtsp_loopback_server_initiated_announce},
	{FN("rtsp-loopback-client-resp-timeout"),
	 &test_rtsp_loopback_client_resp_timeout},
	{FN("rtsp-loopback-client-keep-alive"),
	 &test_rtsp_loopback_client_keep_alive},
	{FN("rtsp-loopback-client-auth-header-generation"),
	 &test_rtsp_loopback_client_auth_header_generation},
	{FN("rtsp-client-resolve-failure"), &test_rtsp_client_resolve_failure},
	{FN("rtsp-loopback-play-rtp-info"), &test_rtsp_loopback_play_rtp_info},
	{FN("rtsp-loopback-teardown-no-match"),
	 &test_rtsp_loopback_teardown_no_match},
	{FN("rtsp-loopback-teardown-exact-media-match"),
	 &test_rtsp_loopback_teardown_exact_media_match},
	{FN("rtsp-loopback-teardown-whole-session-empty-path"),
	 &test_rtsp_loopback_teardown_whole_session_empty_path},
	{FN("rtsp-loopback-tcp-interleaved-auto-channels"),
	 &test_rtsp_loopback_tcp_interleaved_auto_channels},
	{FN("rtsp-client-session-validation-errors"),
	 &test_rtsp_client_session_validation_errors},
	{FN("rtsp-loopback-client-basic-auth-header-generation"),
	 &test_rtsp_loopback_client_basic_auth_header_generation},
	{FN("rtsp-loopback-tcp-interleaved-exact-teardown"),
	 &test_rtsp_loopback_tcp_interleaved_exact_teardown},
	{FN("rtsp-loopback-client-repeated-timeout-reconnect"),
	 &test_rtsp_loopback_client_repeated_timeout_reconnect},
	{FN("rtsp-loopback-setup-relative-resource-url"),
	 &test_rtsp_loopback_setup_relative_resource_url},
	{FN("rtsp-loopback-socket-created-callbacks"),
	 &test_rtsp_loopback_socket_created_callbacks},
	{FN("rtsp-loopback-huge-socket-buffer-size"),
	 &test_rtsp_loopback_huge_socket_buffer_size},
	{FN("rtsp-loopback-describe-sdp-validation"),
	 &test_rtsp_loopback_describe_sdp_validation},
	{FN("rtsp-loopback-setup-reply-invalid-ports"),
	 &test_rtsp_loopback_setup_reply_invalid_ports},
	{FN("rtsp-loopback-setup-existing-session-not-found"),
	 &test_rtsp_loopback_setup_existing_session_not_found},
	{FN("rtsp-loopback-client-401-without-challenge-clears-auth"),
	 &test_rtsp_loopback_client_401_without_challenge_clears_auth},

	CU_TEST_INFO_NULL,
};
/* clang-format on */
