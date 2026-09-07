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

/*
 * TLS ("rtsps://") coverage for rtsp_client.c.
 *
 * rtsp_server.c has no TLS support at all (confirmed by inspection: zero
 * references to TLS/SSL anywhere in that file), so a real rtsp_server cannot
 * be used as the peer here. Instead, the "server" side in these tests is a
 * hand-built raw socket: a plain-TCP tskt_socket accepts the connection, then
 * a server-mode SSL_CTX wraps it via ttls_socket_new_with_ctx() (mirroring
 * packages/libtransport-tls/tests/ttls_test_socket.c's tls_connect_pair()
 * helper) and speaks raw RTSP bytes directly, rather than going through
 * rtsp_server's request/reply API which has no TLS hook to plug into.
 *
 * This still exercises the real, production rtsp_client TLS code path end to
 * end: rtsp_client_connect()'s lazy OPENSSL_init_ssl()/ttls_init()/
 * SSL_CTX_new() block, tskt_client_socket_upgrade_cb() (which calls the real
 * ttls_socket_new_with_ctx()), a genuine TLS handshake and an RTSP
 * request/response exchange over it, and rtsp_client_destroy()'s
 * SSL_CTX_free()/ttls_deinit() cleanup.
 *
 * Note rtsp_client never validates the peer certificate (no CA is ever
 * loaded into client->ssl_ctx, no SSL_CTX_set_verify() call anywhere in
 * rtsp_client.c) -- so the throwaway self-signed test certificate below
 * needs no matching CA on the client side at all.
 */

#include "rtsp_priv.h"
#include "rtsp_test.h"

#include <rtsp/client.h>

#include <unistd.h>

#include <futils/timetools.h>
#include <libpomp.h>

#include <openssl/ssl.h>
#include <transport-socket/tskt.h>
#include <transport-tls/ttls.h>
#include <transport-tls/ttls_utils.h>

#include "certs/rtsp_test_certs.h"


#define TLS_TEST_TIMEOUT_MS 3000
#define TLS_TEST_POLL_MS 20


/*
 * Small helpers
 */

/* Writes a NUL-terminated PEM buffer to a freshly created temp file so it can
 * be handed to ttls_ctx_use_certificate() as a plain filesystem path (it only
 * accepts a URI, not raw PEM data). Caller must remove(path_buf) when done. */
static int write_temp_pem(const char *pem, char *path_buf, size_t path_buf_len)
{
	int fd;
	ssize_t written;
	size_t len = strlen(pem);

	if (path_buf_len < 32)
		return -EINVAL;

	snprintf(path_buf, path_buf_len, "/tmp/rtsp_test_tls_XXXXXX");
	fd = mkstemp(path_buf);
	if (fd < 0)
		return -errno;

	written = write(fd, pem, len);
	close(fd);
	if (written < 0)
		return -errno;
	if ((size_t)written != len)
		return -EIO;

	return 0;
}


static bool wait_flag(struct pomp_loop *loop, const bool *flag, int timeout_ms)
{
	struct timespec start;
	time_get_monotonic(&start);
	while (!*flag) {
		pomp_loop_wait_and_process(loop, TLS_TEST_POLL_MS);
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


static bool wait_either_flag(struct pomp_loop *loop,
			     const bool *a,
			     const bool *b,
			     int timeout_ms)
{
	struct timespec start;
	time_get_monotonic(&start);
	while (!*a && !*b) {
		pomp_loop_wait_and_process(loop, TLS_TEST_POLL_MS);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)timeout_ms * 1000)
			break;
	}
	return *a || *b;
}


/* Accepts one connection on 'listener', pumping 'loop' while polling, since
 * accept() on a plain tskt_socket is not itself event-driven here. */
static bool wait_accept(struct pomp_loop *loop,
			struct tskt_socket *listener,
			struct tskt_socket **ret_accepted,
			int timeout_ms)
{
	struct timespec start;
	char addr[64];
	uint16_t port;
	int res;

	time_get_monotonic(&start);
	do {
		res = tskt_socket_accept(
			listener, addr, sizeof(addr), &port, ret_accepted);
		if (res == 0)
			return true;
		pomp_loop_wait_and_process(loop, TLS_TEST_POLL_MS);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)timeout_ms * 1000)
			break;
	} while (1);

	return false;
}


/* Reads from 'sock' (pumping 'loop' between attempts, so both the TLS
 * handshake and the actual request bytes can progress) until a full RTSP
 * request (terminated by a blank line) has accumulated in 'buf', or timeout.
 */
static bool wait_tls_request(struct pomp_loop *loop,
			     struct tskt_socket *sock,
			     char *buf,
			     size_t buf_cap,
			     int timeout_ms)
{
	struct timespec start;
	size_t len = 0;

	buf[0] = '\0';
	time_get_monotonic(&start);
	do {
		ssize_t res = tskt_socket_read(
			sock, buf + len, buf_cap - 1 - len, NULL);
		if (res > 0) {
			len += (size_t)res;
			buf[len] = '\0';
			if (strstr(buf, "\r\n\r\n") != NULL)
				return true;
		}
		pomp_loop_wait_and_process(loop, TLS_TEST_POLL_MS);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)timeout_ms * 1000)
			break;
	} while (len + 1 < buf_cap);

	return false;
}


/* Writes 'len' bytes to 'sock' in full, pumping 'loop' between attempts to
 * let a backpressured/still-handshaking TLS socket make progress. */
static bool wait_tls_write_all(struct pomp_loop *loop,
			       struct tskt_socket *sock,
			       const char *data,
			       size_t len,
			       int timeout_ms)
{
	struct timespec start;
	size_t sent = 0;

	time_get_monotonic(&start);
	while (sent < len) {
		ssize_t res = tskt_socket_write(sock, data + sent, len - sent);
		if (res > 0)
			sent += (size_t)res;
		pomp_loop_wait_and_process(loop, TLS_TEST_POLL_MS);
		struct timespec now;
		uint64_t elapsed_us;
		int sign;
		time_get_monotonic(&now);
		time_timespec_diff_us(&start, &now, &elapsed_us, &sign);
		if (elapsed_us >= (uint64_t)timeout_ms * 1000)
			break;
	}

	return sent == len;
}


/* Answers one RTSP OPTIONS request over an already TLS-wrapped socket,
 * echoing back the request's own CSeq. */
static void reply_to_options_over_tls(struct pomp_loop *loop,
				      struct tskt_socket *tls_sock)
{
	char req[2048];
	char resp[512];
	int cseq = 1;
	const char *p;

	CU_ASSERT_TRUE_FATAL(wait_tls_request(
		loop, tls_sock, req, sizeof(req), TLS_TEST_TIMEOUT_MS));

	p = strstr(req, "CSeq:");
	if (p != NULL)
		sscanf(p, "CSeq: %d", &cseq);

	snprintf(resp,
		 sizeof(resp),
		 "RTSP/1.0 200 OK\r\n"
		 "CSeq: %d\r\n"
		 "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, "
		 "GET_PARAMETER\r\n"
		 "\r\n",
		 cseq);

	CU_ASSERT_TRUE(wait_tls_write_all(
		loop, tls_sock, resp, strlen(resp), TLS_TEST_TIMEOUT_MS));
}


/*
 * Client-side callbacks / context
 */

struct tls_test_ctx {
	bool connected;
	bool disconnected;
	bool session_removed;
	bool options_done;
	enum rtsp_client_req_status options_status;
	int options_code;
};


static void tls_client_connection_state(struct rtsp_client *client,
					enum rtsp_client_conn_state state,
					void *userdata)
{
	struct tls_test_ctx *ctx = userdata;
	UNUSED(client);
	if (state == RTSP_CLIENT_CONN_STATE_CONNECTED)
		ctx->connected = true;
	else if (state == RTSP_CLIENT_CONN_STATE_DISCONNECTED)
		ctx->disconnected = true;
}


static void tls_client_session_removed(struct rtsp_client *client,
				       const char *session_id,
				       int status,
				       void *userdata)
{
	struct tls_test_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(session_id);
	UNUSED(status);
	ctx->session_removed = true;
}


static void tls_client_announce(struct rtsp_client *client,
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


static void tls_client_options_resp(struct rtsp_client *client,
				    enum rtsp_client_req_status req_status,
				    int status,
				    uint32_t methods,
				    const struct rtsp_header_ext *ext,
				    size_t ext_count,
				    void *userdata,
				    void *req_userdata)
{
	struct tls_test_ctx *ctx = userdata;
	UNUSED(client);
	UNUSED(methods);
	UNUSED(ext);
	UNUSED(ext_count);
	UNUSED(req_userdata);
	ctx->options_status = req_status;
	ctx->options_code = status;
	ctx->options_done = true;
}


/*
 * Tests
 */

/* Full, real TLS handshake + an RTSP OPTIONS request/response exchanged over
 * it, done twice on the same rtsp_client (connect / round trip / disconnect /
 * reconnect / round trip again) so the second rtsp_client_connect() call
 * exercises the "already initialized" skip branches for ttls_init()/
 * SSL_CTX_new() (client->ttls_init and client->ssl_ctx are both already set
 * from the first connect), not just the first-time init path. */
static void test_rtsp_tls_client_options_round_trip(void)
{
	struct tls_test_ctx ctx;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_client *client = NULL;
	struct pomp_loop *loop;
	struct tskt_socket *listener = NULL;
	SSL_CTX *server_ctx;
	char cert_path[64], key_path[64];
	char url[96];
	uint16_t port;
	int ret;
	int round;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	ret = tskt_socket_new_tcp(loop, &listener);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	ret = tskt_socket_listen(listener, "127.0.0.1", 0);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	port = tskt_socket_get_local_port(listener);
	CU_ASSERT_TRUE_FATAL(port != 0);

	CU_ASSERT_EQUAL_FATAL(write_temp_pem(RTSP_TEST_TLS_CERT_PEM,
					     cert_path,
					     sizeof(cert_path)),
			      0);
	CU_ASSERT_EQUAL_FATAL(write_temp_pem(RTSP_TEST_TLS_KEY_PEM,
					     key_path,
					     sizeof(key_path)),
			      0);

	server_ctx = SSL_CTX_new(TLS_server_method());
	CU_ASSERT_PTR_NOT_NULL_FATAL(server_ctx);
	CU_ASSERT_EQUAL_FATAL(
		ttls_ctx_use_certificate(server_ctx, cert_path, key_path), 0);

	snprintf(url, sizeof(url), "rtsps://127.0.0.1:%u/live", (unsigned)port);

	memset(&ctx, 0, sizeof(ctx));
	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &tls_client_connection_state;
	client_cbs.session_removed = &tls_client_session_removed;
	client_cbs.announce = &tls_client_announce;
	client_cbs.options_resp = &tls_client_options_resp;

	ret = rtsp_client_new(loop, NULL, &client_cbs, &ctx, &client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(client);

	for (round = 0; round < 2; round++) {
		struct tskt_socket *tcp_accepted = NULL;
		struct tskt_socket *tls_sock = NULL;

		memset(&ctx, 0, sizeof(ctx));

		ret = rtsp_client_connect(client, url);
		CU_ASSERT_EQUAL_FATAL(ret, 0);

		CU_ASSERT_TRUE_FATAL(wait_accept(
			loop, listener, &tcp_accepted, TLS_TEST_TIMEOUT_MS));
		CU_ASSERT_PTR_NOT_NULL_FATAL(tcp_accepted);

		ret = ttls_socket_new_with_ctx(
			server_ctx, tcp_accepted, &tls_sock);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(tls_sock);

		CU_ASSERT_TRUE_FATAL(
			wait_flag(loop, &ctx.connected, TLS_TEST_TIMEOUT_MS));

		ret = rtsp_client_options(
			client, NULL, 0, NULL, TLS_TEST_TIMEOUT_MS);
		CU_ASSERT_EQUAL_FATAL(ret, 0);

		/* drives both the server side of the TLS handshake and the
		 * request/response exchange, pumping 'loop' throughout so the
		 * client side (real TLS handshake, real RTSP write) makes
		 * progress concurrently */
		reply_to_options_over_tls(loop, tls_sock);

		CU_ASSERT_TRUE_FATAL(wait_flag(
			loop, &ctx.options_done, TLS_TEST_TIMEOUT_MS));
		CU_ASSERT_EQUAL(ctx.options_status, RTSP_CLIENT_REQ_STATUS_OK);
		CU_ASSERT_EQUAL(ctx.options_code, 0);

		tskt_socket_destroy(tls_sock);

		ret = rtsp_client_disconnect(client);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_TRUE(wait_flag(
			loop, &ctx.disconnected, TLS_TEST_TIMEOUT_MS));
	}

	ret = rtsp_client_destroy(client);
	CU_ASSERT_EQUAL(ret, 0);

	tskt_socket_destroy(listener);
	SSL_CTX_free(server_ctx);
	remove(cert_path);
	remove(key_path);
	pomp_loop_destroy(loop);
}


/* The peer accepts the TCP connection but is destroyed immediately, before
 * ever wrapping it in TLS or exchanging a single handshake byte. No
 * certificate is needed since no TLS is ever spoken on the peer side.
 *
 * This does NOT surface as an automatic CONNECTED/DISCONNECTED transition:
 * the TCP connect() itself succeeds, so tskt_client's socket_upgrade_cb
 * synchronously wraps the (not-yet-writable) raw socket in TLS, which
 * defers its first handshake attempt (-EAGAIN); only once the raw socket
 * actually becomes writable does the deferred SSL_do_handshake() really
 * attempt to send the ClientHello, discover the peer is gone, and fail --
 * asynchronously, via tskt_client's sock_event_cb's error path. That path
 * only fires TSKT_CLIENT_EVENT_DISCONNECTED if the client had previously
 * reached "connected" (see tskt_client.c's sock_event_cb: the error label's
 * `if (self->connected)` guard), which it never did here, so tskt_client
 * just silently arms an internal reconnect retry instead. This is by
 * design, not a gap: rtsp_client_disconnect() itself anticipates exactly
 * this ("already_disconnected" handling for RTSP_CLIENT_CONN_STATE_
 * CONNECTING) and forces a clean transition unconditionally rather than
 * waiting on any event from tskt_client -- which is what this test actually
 * exercises as the real, documented recovery path for a stuck connection
 * attempt. */
static void test_rtsp_tls_client_peer_closed_before_handshake(void)
{
	struct tls_test_ctx ctx;
	struct rtsp_client_cbs client_cbs;
	struct rtsp_client *client = NULL;
	struct pomp_loop *loop;
	struct tskt_socket *listener = NULL;
	struct tskt_socket *tcp_accepted = NULL;
	char url[96];
	uint16_t port;
	int ret;

	loop = pomp_loop_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(loop);

	ret = tskt_socket_new_tcp(loop, &listener);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	ret = tskt_socket_listen(listener, "127.0.0.1", 0);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	port = tskt_socket_get_local_port(listener);
	CU_ASSERT_TRUE_FATAL(port != 0);

	snprintf(url, sizeof(url), "rtsps://127.0.0.1:%u/live", (unsigned)port);

	memset(&ctx, 0, sizeof(ctx));
	memset(&client_cbs, 0, sizeof(client_cbs));
	client_cbs.connection_state = &tls_client_connection_state;
	client_cbs.session_removed = &tls_client_session_removed;
	client_cbs.announce = &tls_client_announce;
	client_cbs.options_resp = &tls_client_options_resp;

	ret = rtsp_client_new(loop, NULL, &client_cbs, &ctx, &client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(client);

	ret = rtsp_client_connect(client, url);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	CU_ASSERT_TRUE_FATAL(wait_accept(
		loop, listener, &tcp_accepted, TLS_TEST_TIMEOUT_MS));
	CU_ASSERT_PTR_NOT_NULL_FATAL(tcp_accepted);

	/* close the raw connection right away: no TLS ServerHello, no
	 * Finished -- once the deferred TLS handshake actually attempts its
	 * ClientHello, this makes it fail hard (Broken pipe) */
	tskt_socket_destroy(tcp_accepted);

	/* Give the deferred handshake failure a bounded window to (not)
	 * produce any visible transition -- see the comment above the test:
	 * neither is expected to fire here, by design. */
	(void)wait_either_flag(loop, &ctx.connected, &ctx.disconnected, 500);
	CU_ASSERT_FALSE(ctx.connected);
	CU_ASSERT_FALSE(ctx.disconnected);

	/* The real, documented recovery path: an explicit disconnect from
	 * RTSP_CLIENT_CONN_STATE_CONNECTING forces a clean transition
	 * unconditionally, regardless of whatever tskt_client is silently
	 * doing internally (see rtsp_client_disconnect()'s
	 * "already_disconnected" handling). */
	ret = rtsp_client_disconnect(client);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE_FATAL(
		wait_flag(loop, &ctx.disconnected, TLS_TEST_TIMEOUT_MS));

	ret = rtsp_client_destroy(client);
	CU_ASSERT_EQUAL(ret, 0);

	tskt_socket_destroy(listener);
	pomp_loop_destroy(loop);
}


CU_TestInfo g_rtsp_test_tls[] = {
	{FN("client-options-round-trip"),
	 &test_rtsp_tls_client_options_round_trip},
	{FN("client-peer-closed-before-handshake"),
	 &test_rtsp_tls_client_peer_closed_before_handshake},

	CU_TEST_INFO_NULL,
};
