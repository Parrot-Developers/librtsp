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


#define TEST_MSG_BUF_LEN 1024


static struct pomp_buffer *buf_from_str(const char *s)
{
	return pomp_buffer_new_with_data(s, strlen(s));
}


/*
 * rtsp_message_clear()
 */

static void test_rtsp_message_clear(void)
{
	struct rtsp_message msg;

	/* NULL is a safe no-op */
	rtsp_message_clear(NULL);

	/* UNKNOWN type: no sub-header to clear, still zeroes the struct */
	memset(&msg, 0xff, sizeof(msg));
	msg.type = RTSP_MESSAGE_TYPE_UNKNOWN;
	rtsp_message_clear(&msg);
	CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_UNKNOWN);
	CU_ASSERT_EQUAL(msg.total_len, (size_t)0);

	/* REQUEST type: the request header's own fields are freed first */
	memset(&msg, 0, sizeof(msg));
	msg.type = RTSP_MESSAGE_TYPE_REQUEST;
	msg.header.req.uri = xstrdup("rtsp://host/stream");
	rtsp_message_clear(&msg);
	CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_UNKNOWN);
	CU_ASSERT_PTR_NULL(msg.header.req.uri);

	/* RESPONSE type: same, via the response header's own fields */
	memset(&msg, 0, sizeof(msg));
	msg.type = RTSP_MESSAGE_TYPE_RESPONSE;
	msg.header.resp.status_string = xstrdup("OK");
	rtsp_message_clear(&msg);
	CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_UNKNOWN);
	CU_ASSERT_PTR_NULL(msg.header.resp.status_string);
}


/*
 * rtsp_buffer_remove_first_bytes()
 */

static void test_rtsp_message_buffer_remove_first_bytes(void)
{
	struct pomp_buffer *buf;
	const void *cdata;
	size_t len;
	int ret;

	/* NULL buffer is a safe no-op */
	rtsp_buffer_remove_first_bytes(NULL, 4);

	/* count == 0 is a no-op, content untouched */
	buf = buf_from_str("hello");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	rtsp_buffer_remove_first_bytes(buf, 0);
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(len, (size_t)5);
	CU_ASSERT_EQUAL(memcmp(cdata, "hello", 5), 0);
	pomp_buffer_unref(buf);

	/* Removing from the front shifts the remaining bytes down */
	buf = buf_from_str("helloworld");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	rtsp_buffer_remove_first_bytes(buf, 5);
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(len, (size_t)5);
	CU_ASSERT_EQUAL(memcmp(cdata, "world", 5), 0);
	pomp_buffer_unref(buf);

	/* Removing exactly the whole buffer empties it */
	buf = buf_from_str("hello");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	rtsp_buffer_remove_first_bytes(buf, 5);
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(len, (size_t)0);
	pomp_buffer_unref(buf);

	/* Removing more than the buffer holds is clamped, not an error */
	buf = buf_from_str("hi");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	rtsp_buffer_remove_first_bytes(buf, 100);
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(len, (size_t)0);
	pomp_buffer_unref(buf);
}


/*
 * rtsp_build_interleaved()
 */

static void test_rtsp_message_build_interleaved(void)
{
	struct rtsp_interleaved_info info;
	struct pomp_buffer *buf = NULL;
	const void *cdata;
	size_t len;
	int ret;
	static const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};

	memset(&info, 0, sizeof(info));

	/* KO cases */
	ret = rtsp_build_interleaved(NULL, &buf);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	info.channel = 0;
	info.data = payload;
	info.len = sizeof(payload);
	ret = rtsp_build_interleaved(&info, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	info.data = NULL;
	ret = rtsp_build_interleaved(&info, &buf);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	info.data = payload;

	info.len = 0;
	ret = rtsp_build_interleaved(&info, &buf);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	info.len = sizeof(payload);

	info.len = UINT16_MAX - 3; /* one more than the max allowed */
	ret = rtsp_build_interleaved(&info, &buf);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	info.len = sizeof(payload);

	/* OK: exact wire format is a fixed 4-byte header (0x24, channel,
	 * big-endian 16-bit length) followed by the payload */
	info.channel = 3;
	ret = rtsp_build_interleaved(&info, &buf);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(len, (size_t)(4 + sizeof(payload)));
	{
		static const uint8_t expected[] = {
			0x24, 0x03, 0x00, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05};
		CU_ASSERT_EQUAL(memcmp(cdata, expected, sizeof(expected)), 0);
	}
	pomp_buffer_unref(buf);
}


/*
 * rtsp_get_next_message() argument validation
 */

static void test_rtsp_message_get_next_message_args(void)
{
	struct rtsp_message msg;
	struct rtsp_message_parser_ctx ctx;
	struct pomp_buffer *buf;
	int ret;

	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	buf = buf_from_str("OPTIONS rtsp://host/x RTSP/1.0\r\n\r\n");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

	ret = rtsp_get_next_message(buf, NULL, &ctx);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_get_next_message(NULL, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_get_next_message(buf, &msg, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	pomp_buffer_unref(buf);
}


/*
 * rtsp_get_next_message() - interleaved packets
 */

static void test_rtsp_message_get_next_message_interleaved(void)
{
	struct rtsp_message msg;
	struct rtsp_message_parser_ctx ctx;
	struct rtsp_interleaved_info info;
	struct pomp_buffer *full = NULL, *truncated;
	int ret;
	static const uint8_t payload[] = {0xaa, 0xbb, 0xcc};

	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	memset(&info, 0, sizeof(info));
	info.channel = 1;
	info.data = payload;
	info.len = sizeof(payload);
	ret = rtsp_build_interleaved(&info, &full);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(full);

	/* Incomplete: header present but payload not fully arrived yet */
	{
		const void *cdata;
		size_t len;
		pomp_buffer_get_cdata(full, &cdata, &len, NULL);
		truncated = pomp_buffer_new_with_data(cdata, len - 1);
	}
	CU_ASSERT_PTR_NOT_NULL_FATAL(truncated);
	ret = rtsp_get_next_message(truncated, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	pomp_buffer_unref(truncated);

	/* Complete */
	ret = rtsp_get_next_message(full, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_INTERLEAVED);
	CU_ASSERT_EQUAL(msg.interleaved.channel, (uint8_t)1);
	CU_ASSERT_EQUAL(msg.interleaved.len, (uint16_t)sizeof(payload));
	CU_ASSERT_EQUAL(memcmp(msg.interleaved.data, payload, sizeof(payload)),
			0);
	CU_ASSERT_EQUAL(msg.total_len, (size_t)(4 + sizeof(payload)));

	pomp_buffer_unref(full);
}


/*
 * rtsp_get_next_message() - text requests
 */

static void test_rtsp_message_get_next_message_request(void)
{
	struct rtsp_message msg;
	struct rtsp_message_parser_ctx ctx;
	struct pomp_buffer *buf;
	int ret;

	/* Incomplete: no end-of-header marker yet */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	buf = buf_from_str("OPTIONS rtsp://host/x RTSP/1.0\r\nCSeq: 1\r\n");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	ret = rtsp_get_next_message(buf, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, -EAGAIN);
	pomp_buffer_unref(buf);

	/* Malformed: unknown method. The bad header's length is still
	 * reported via msg->total_len, so the caller can skip past it. */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	{
		static const char req[] =
			"BOGUS rtsp://host/x RTSP/1.0\r\n\r\n";
		buf = buf_from_str(req);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		ret = rtsp_get_next_message(buf, &msg, &ctx);
		CU_ASSERT_EQUAL(ret, -EPROTO);
		CU_ASSERT_EQUAL(msg.total_len, strlen(req));
		pomp_buffer_unref(buf);
	}

	/* Complete, no body */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	{
		static const char req[] =
			"OPTIONS rtsp://host/x RTSP/1.0\r\n"
			"CSeq: 1\r\n"
			"\r\n";
		buf = buf_from_str(req);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		ret = rtsp_get_next_message(buf, &msg, &ctx);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_REQUEST);
		CU_ASSERT_EQUAL(msg.header.req.method,
				RTSP_METHOD_TYPE_OPTIONS);
		CU_ASSERT_STRING_EQUAL(msg.header.req.uri, "rtsp://host/x");
		CU_ASSERT_EQUAL(msg.header.req.cseq, 1);
		CU_ASSERT_EQUAL(msg.body_len, (size_t)0);
		CU_ASSERT_EQUAL(msg.total_len, strlen(req));
		rtsp_message_clear(&msg);
		pomp_buffer_unref(buf);
	}

	/* Complete header, body not fully arrived yet, then complete: two
	 * separate calls on the same ctx, mirroring how a real caller
	 * accumulates bytes off the wire (rtsp_client.c's read callback). */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	{
		static const char header[] =
			"ANNOUNCE rtsp://host/x RTSP/1.0\r\n"
			"CSeq: 2\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: 11\r\n"
			"\r\n";
		static const char body[] = "hello world";
		char partial[TEST_MSG_BUF_LEN];
		char full[TEST_MSG_BUF_LEN];
		size_t header_len = strlen(header);

		snprintf(partial,
			 sizeof(partial),
			 "%s%.5s" /* only the first 5 body bytes */,
			 header,
			 body);
		buf = pomp_buffer_new_with_data(partial, header_len + 5);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		ret = rtsp_get_next_message(buf, &msg, &ctx);
		CU_ASSERT_EQUAL(ret, -EAGAIN);
		pomp_buffer_unref(buf);

		snprintf(full, sizeof(full), "%s%s", header, body);
		buf = buf_from_str(full);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		ret = rtsp_get_next_message(buf, &msg, &ctx);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_REQUEST);
		CU_ASSERT_EQUAL(msg.header.req.method,
				RTSP_METHOD_TYPE_ANNOUNCE);
		CU_ASSERT_EQUAL(msg.header.req.content_length, 11);
		CU_ASSERT_EQUAL(msg.body_len, (size_t)11);
		CU_ASSERT_EQUAL(memcmp(msg.body, body, 11), 0);
		CU_ASSERT_EQUAL(msg.total_len, header_len + 11);
		rtsp_message_clear(&msg);
		pomp_buffer_unref(buf);
	}

	/* Two complete requests back-to-back in one buffer, drained in a
	 * loop exactly like rtsp_client.c's tskt_client_data_cb() does */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	{
		static const char two[] =
			"OPTIONS rtsp://host/x RTSP/1.0\r\nCSeq: 1\r\n\r\n"
			"OPTIONS rtsp://host/x RTSP/1.0\r\nCSeq: 2\r\n\r\n";
		int seen_cseqs[2];
		int count = 0;

		buf = buf_from_str(two);
		CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
		while ((ret = rtsp_get_next_message(buf, &msg, &ctx)) == 0) {
			CU_ASSERT_TRUE_FATAL(count < 2);
			seen_cseqs[count++] = msg.header.req.cseq;
			rtsp_buffer_remove_first_bytes(buf, msg.total_len);
		}
		CU_ASSERT_EQUAL(ret, -EAGAIN);
		CU_ASSERT_EQUAL(count, 2);
		CU_ASSERT_EQUAL(seen_cseqs[0], 1);
		CU_ASSERT_EQUAL(seen_cseqs[1], 2);
		/* The last successfully-returned msg (cseq 2) is not
		 * consumed by another rtsp_get_next_message() call (which
		 * would otherwise clear it internally on entry) - clear it
		 * explicitly. */
		rtsp_message_clear(&msg);
		pomp_buffer_unref(buf);
	}
}


/*
 * Regression test for a bug found while writing this suite: in
 * rtsp_request_header_read() (rtsp.c), the 'Authorization' header branch
 * called rtsp_authorization_header_read() with 'str' - the whole function's
 * input pointer, already truncated by an earlier strtok_r() call down to
 * just the request line (e.g. "SETUP rtsp://host/x RTSP/1.0") - instead of
 * 'value', the actual Authorization header's value on the current line.
 * Every other header branch (Session, Transport, Content-Type, ...)
 * correctly used 'value'; the response-side equivalent
 * (rtsp_response_header_read()'s 'WWW-Authenticate' branch) already did too.
 * As a result, any RTSP request carrying an Authorization header could
 * never be parsed: rtsp_authorization_header_read() would try to read the
 * request line as an auth scheme token ("SETUP" does not match "Basic" or
 * "Digest"), fail with -EPROTO, and that failure was propagated straight
 * out of rtsp_request_header_read() - and therefore out of
 * rtsp_get_next_message() - discarding the whole request. Fixed by passing
 * 'value' instead of 'str'.
 */
static void test_rtsp_message_request_authorization_regression(void)
{
	struct rtsp_message msg;
	struct rtsp_message_parser_ctx ctx;
	struct pomp_buffer *buf;
	int ret;
	static const char req[] =
		"SETUP rtsp://host/x/trackID=1 RTSP/1.0\r\n"
		"CSeq: 3\r\n"
		"Authorization: Digest username=\"bob\", realm=\"example.com\", "
		"nonce=\"abc123\", uri=\"rtsp://host/x/trackID=1\", "
		"response=\"deadbeef\"\r\n"
		"Transport: RTP/AVP;unicast;client_port=5000-5001\r\n"
		"\r\n";

	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	buf = buf_from_str(req);
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

	ret = rtsp_get_next_message(buf, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_REQUEST);
	CU_ASSERT_EQUAL(msg.header.req.method, RTSP_METHOD_TYPE_SETUP);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.header.req.authorization);
	CU_ASSERT_EQUAL(msg.header.req.authorization->type,
			RTSP_AUTH_TYPE_DIGEST);
	CU_ASSERT_STRING_EQUAL(msg.header.req.authorization->username, "bob");
	CU_ASSERT_STRING_EQUAL(msg.header.req.authorization->realm,
			       "example.com");
	CU_ASSERT_STRING_EQUAL(msg.header.req.authorization->response,
			       "deadbeef");
	CU_ASSERT_EQUAL(msg.header.req.transport_count, (unsigned int)1);

	rtsp_message_clear(&msg);
	pomp_buffer_unref(buf);
}


/*
 * rtsp_get_next_message() - text responses
 */

static void test_rtsp_message_get_next_message_response(void)
{
	struct rtsp_message msg;
	struct rtsp_message_parser_ctx ctx;
	struct pomp_buffer *buf;
	int ret;

	/* Complete, no body */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	buf = buf_from_str(
		"RTSP/1.0 200 OK\r\n"
		"CSeq: 1\r\n"
		"Public: OPTIONS,DESCRIBE,SETUP,PLAY,TEARDOWN\r\n"
		"Content-Length: 0\r\n"
		"\r\n");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	ret = rtsp_get_next_message(buf, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(msg.type, RTSP_MESSAGE_TYPE_RESPONSE);
	CU_ASSERT_EQUAL(msg.header.resp.status_code, 200);
	CU_ASSERT_STRING_EQUAL(msg.header.resp.status_string, "OK");
	CU_ASSERT_EQUAL(msg.header.resp.public_methods,
			RTSP_METHOD_FLAG_OPTIONS | RTSP_METHOD_FLAG_DESCRIBE |
				RTSP_METHOD_FLAG_SETUP | RTSP_METHOD_FLAG_PLAY |
				RTSP_METHOD_FLAG_TEARDOWN);
	CU_ASSERT_EQUAL(msg.body_len, (size_t)0);
	rtsp_message_clear(&msg);
	pomp_buffer_unref(buf);

	/* WWW-Authenticate: also exercises the (fixed in this same session)
	 * opaque/qop constant collision from the response side */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	buf = buf_from_str(
		"RTSP/1.0 401 Unauthorized\r\n"
		"CSeq: 1\r\n"
		"WWW-Authenticate: Digest realm=\"example.com\", "
		"nonce=\"abc123\", qop=\"auth\"\r\n"
		"Content-Length: 0\r\n"
		"\r\n");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	ret = rtsp_get_next_message(buf, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(msg.header.resp.status_code, 401);
	CU_ASSERT_PTR_NOT_NULL_FATAL(msg.header.resp.authenticate);
	CU_ASSERT_STRING_EQUAL(msg.header.resp.authenticate->realm,
			       "example.com");
	CU_ASSERT_EQUAL(msg.header.resp.authenticate->qop, RTSP_AUTH_QOP_AUTH);
	rtsp_message_clear(&msg);
	pomp_buffer_unref(buf);

	/* Malformed: bad protocol version */
	memset(&msg, 0, sizeof(msg));
	memset(&ctx, 0, sizeof(ctx));
	buf = buf_from_str("RTSP/2.0 200 OK\r\n\r\n");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
	ret = rtsp_get_next_message(buf, &msg, &ctx);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	pomp_buffer_unref(buf);
}


/*
 * rtsp_request_header_write() / rtsp_request_header_read() - direct calls
 */

static void test_rtsp_message_request_header_write(void)
{
	char buf[TEST_MSG_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_request_header header;
	int ret;

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_request_header_write(NULL, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	memset(&header, 0, sizeof(header));
	header.method = RTSP_METHOD_TYPE_OPTIONS;
	header.uri = "rtsp://host/x";
	ret = rtsp_request_header_write(&header, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* KO: no uri */
	memset(&header, 0, sizeof(header));
	header.method = RTSP_METHOD_TYPE_OPTIONS;
	str.len = 0;
	ret = rtsp_request_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	header.uri = "";
	str.len = 0;
	ret = rtsp_request_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* KO: unknown method */
	header.uri = "rtsp://host/x";
	header.method = RTSP_METHOD_TYPE_UNKNOWN;
	str.len = 0;
	ret = rtsp_request_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK: minimal (cseq < 0 is skipped) */
	memset(&header, 0, sizeof(header));
	header.method = RTSP_METHOD_TYPE_OPTIONS;
	header.uri = "rtsp://host/x";
	header.cseq = -1;
	str.len = 0;
	ret = rtsp_request_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "OPTIONS rtsp://host/x RTSP/1.0\r\n\r\n");

	/* OK: with CSeq, User-Agent, Content-Length */
	memset(&header, 0, sizeof(header));
	header.method = RTSP_METHOD_TYPE_SETUP;
	header.uri = "rtsp://host/x/trackID=1";
	header.cseq = 5;
	header.user_agent = "test-agent";
	header.content_length = 0;
	str.len = 0;
	ret = rtsp_request_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf,
			       "SETUP rtsp://host/x/trackID=1 RTSP/1.0\r\n"
			       "CSeq: 5\r\n"
			       "User-Agent: test-agent\r\n"
			       "\r\n");
}


static void test_rtsp_message_request_header_read(void)
{
	struct rtsp_request_header header;
	int ret;

	/* rtsp_request_header_read() calls rtsp_request_header_clear() on
	 * 'header' internally (to release any content from a previous
	 * call), which frees its pointer fields - so 'header' must start
	 * zeroed, not uninitialized stack garbage. */
	memset(&header, 0, sizeof(header));

	ret = rtsp_request_header_read(NULL, 0, &header, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	{
		char v[] = "OPTIONS rtsp://host/x RTSP/1.0\r\n\r\n";
		ret = rtsp_request_header_read(v, strlen(v), NULL, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* No end-of-header marker */
	{
		char v[] = "OPTIONS rtsp://host/x RTSP/1.0\r\n";
		ret = rtsp_request_header_read(v, strlen(v), &header, NULL);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* Wrong protocol version: the method and uri are already parsed (and
	 * uri already allocated) by the time the version check fails */
	{
		char v[] = "OPTIONS rtsp://host/x RTSP/2.0\r\n\r\n";
		ret = rtsp_request_header_read(v, strlen(v), &header, NULL);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	rtsp_request_header_clear(&header);
}


/*
 * rtsp_response_header_write() / rtsp_response_header_read() - direct
 */

static void test_rtsp_message_response_header_write(void)
{
	char buf[TEST_MSG_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_response_header header;
	int ret;

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_response_header_write(NULL, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	memset(&header, 0, sizeof(header));
	header.status_code = 200;
	header.status_string = "OK";
	ret = rtsp_response_header_write(&header, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* KO: no status code / string */
	memset(&header, 0, sizeof(header));
	str.len = 0;
	ret = rtsp_response_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	header.status_code = 200;
	str.len = 0;
	ret = rtsp_response_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	header.status_string = "";
	str.len = 0;
	ret = rtsp_response_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK: minimal. Content-Length is always written, even at 0, unlike
	 * the request side where it is only written if > 0. */
	memset(&header, 0, sizeof(header));
	header.status_code = 200;
	header.status_string = "OK";
	header.cseq = -1;
	str.len = 0;
	ret = rtsp_response_header_write(&header, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf,
			       "RTSP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n");
}


static void test_rtsp_message_response_header_read(void)
{
	struct rtsp_response_header header;
	int ret;

	/* rtsp_response_header_read() calls rtsp_response_header_clear() on
	 * 'header' internally, which frees its pointer fields - so 'header'
	 * must start zeroed, not uninitialized stack garbage. */
	memset(&header, 0, sizeof(header));

	ret = rtsp_response_header_read(NULL, 0, &header, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	{
		char v[] = "RTSP/1.0 200 OK\r\n\r\n";
		ret = rtsp_response_header_read(v, strlen(v), NULL, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* No end-of-header marker */
	{
		char v[] = "RTSP/1.0 200 OK\r\n";
		ret = rtsp_response_header_read(v, strlen(v), &header, NULL);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* Wrong protocol version */
	{
		char v[] = "RTSP/2.0 200 OK\r\n\r\n";
		ret = rtsp_response_header_read(v, strlen(v), &header, NULL);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* Missing status code/string */
	{
		char v[] = "RTSP/1.0\r\n\r\n";
		ret = rtsp_response_header_read(v, strlen(v), &header, NULL);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	rtsp_response_header_clear(&header);
}


/* clang-format off */
CU_TestInfo g_rtsp_test_message[] = {
	{FN("rtsp-message-clear"), &test_rtsp_message_clear},
	{FN("rtsp-message-buffer-remove-first-bytes"),
	 &test_rtsp_message_buffer_remove_first_bytes},
	{FN("rtsp-message-build-interleaved"),
	 &test_rtsp_message_build_interleaved},
	{FN("rtsp-message-get-next-message-args"),
	 &test_rtsp_message_get_next_message_args},
	{FN("rtsp-message-get-next-message-interleaved"),
	 &test_rtsp_message_get_next_message_interleaved},
	{FN("rtsp-message-get-next-message-request"),
	 &test_rtsp_message_get_next_message_request},
	{FN("rtsp-message-request-authorization-regression"),
	 &test_rtsp_message_request_authorization_regression},
	{FN("rtsp-message-get-next-message-response"),
	 &test_rtsp_message_get_next_message_response},
	{FN("rtsp-message-request-header-write"),
	 &test_rtsp_message_request_header_write},
	{FN("rtsp-message-request-header-read"),
	 &test_rtsp_message_request_header_read},
	{FN("rtsp-message-response-header-write"),
	 &test_rtsp_message_response_header_write},
	{FN("rtsp-message-response-header-read"),
	 &test_rtsp_message_response_header_read},

	CU_TEST_INFO_NULL,
};
/* clang-format on */
