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


#define TEST_HDR_BUF_LEN 512


/* Given a full serialized "<Name>: <value>\r\n" header line, return a
 * pointer to <value>, truncated in place before the trailing CRLF, so it
 * can be fed back into the matching *_read() function: request/response
 * header parsing always strips the header name before dispatching to a
 * per-header reader, so the readers themselves only ever see the value. */
static char *header_value(char *line, const char *name)
{
	char *value = line + strlen(name) + 2 /* ": " */;
	char *crlf = strstr(value, RTSP_CRLF);
	if (crlf)
		*crlf = '\0';
	return value;
}


/*
 * Allow / Public headers (share the same rtsp_methods_write/_read backend)
 */

static void test_rtsp_header_allow_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	int ret;

	/* KO cases */
	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_allow_header_write(0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_allow_header_write(RTSP_METHOD_FLAG_OPTIONS, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK: single method */
	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_allow_header_write(RTSP_METHOD_FLAG_OPTIONS, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Allow: OPTIONS\r\n");

	/* OK: several methods, always serialized in a fixed enum order
	 * regardless of the order the flags were combined in */
	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_allow_header_write(RTSP_METHOD_FLAG_TEARDOWN |
					      RTSP_METHOD_FLAG_OPTIONS |
					      RTSP_METHOD_FLAG_PLAY,
				      &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Allow: OPTIONS,PLAY,TEARDOWN\r\n");

	/* OK: every method */
	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_allow_header_write(
		RTSP_METHOD_FLAG_OPTIONS | RTSP_METHOD_FLAG_DESCRIBE |
			RTSP_METHOD_FLAG_ANNOUNCE | RTSP_METHOD_FLAG_SETUP |
			RTSP_METHOD_FLAG_PLAY | RTSP_METHOD_FLAG_PAUSE |
			RTSP_METHOD_FLAG_TEARDOWN |
			RTSP_METHOD_FLAG_GET_PARAMETER |
			RTSP_METHOD_FLAG_SET_PARAMETER |
			RTSP_METHOD_FLAG_REDIRECT | RTSP_METHOD_FLAG_RECORD,
		&str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Allow: OPTIONS,DESCRIBE,ANNOUNCE,SETUP,PLAY,PAUSE,"
		"TEARDOWN,GET_PARAMETER,SET_PARAMETER,REDIRECT,RECORD\r\n");

	/* KO: buffer too small */
	str.str = buf;
	str.len = 0;
	str.max_len = 4;
	ret = rtsp_allow_header_write(RTSP_METHOD_FLAG_OPTIONS, &str);
	CU_ASSERT_EQUAL(ret, -ENOBUFS);
}


static void test_rtsp_header_allow_read(void)
{
	uint32_t methods;
	int ret;

	ret = rtsp_allow_header_read(NULL, &methods);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	{
		char v[] = "OPTIONS";
		ret = rtsp_allow_header_read(v, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* Empty value: not an error, simply no methods */
	{
		char v[] = "";
		methods = 0xffffffff;
		ret = rtsp_allow_header_read(v, &methods);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(methods, (uint32_t)0);
	}

	/* Single method */
	{
		char v[] = "OPTIONS";
		methods = 0;
		ret = rtsp_allow_header_read(v, &methods);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(methods, RTSP_METHOD_FLAG_OPTIONS);
	}

	/* Several methods, with an extra space after the comma (RFC allows
	 * optional LWS around list separators) */
	{
		char v[] = "OPTIONS, PLAY,TEARDOWN";
		methods = 0;
		ret = rtsp_allow_header_read(v, &methods);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(methods,
				RTSP_METHOD_FLAG_OPTIONS |
					RTSP_METHOD_FLAG_PLAY |
					RTSP_METHOD_FLAG_TEARDOWN);
	}

	/* Unknown method names are silently ignored */
	{
		char v[] = "OPTIONS,BOGUS,PLAY";
		methods = 0;
		ret = rtsp_allow_header_read(v, &methods);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(methods,
				RTSP_METHOD_FLAG_OPTIONS |
					RTSP_METHOD_FLAG_PLAY);
	}
}


static void test_rtsp_header_allow_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str = {.str = buf, .len = 0, .max_len = sizeof(buf)};
	uint32_t methods_in = RTSP_METHOD_FLAG_OPTIONS |
			      RTSP_METHOD_FLAG_DESCRIBE |
			      RTSP_METHOD_FLAG_SETUP | RTSP_METHOD_FLAG_PLAY |
			      RTSP_METHOD_FLAG_TEARDOWN;
	uint32_t methods_out = 0;
	int ret;

	ret = rtsp_allow_header_write(methods_in, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_allow_header_read(header_value(buf, RTSP_HEADER_ALLOW),
				     &methods_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(methods_out, methods_in);
}


static void test_rtsp_header_public_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str = {.str = buf, .len = 0, .max_len = sizeof(buf)};
	int ret;

	ret = rtsp_public_header_write(0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_public_header_write(RTSP_METHOD_FLAG_OPTIONS, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	str.len = 0;
	ret = rtsp_public_header_write(
		RTSP_METHOD_FLAG_OPTIONS | RTSP_METHOD_FLAG_DESCRIBE, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Public: OPTIONS,DESCRIBE\r\n");
}


static void test_rtsp_header_public_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str = {.str = buf, .len = 0, .max_len = sizeof(buf)};
	uint32_t methods_in =
		RTSP_METHOD_FLAG_OPTIONS | RTSP_METHOD_FLAG_DESCRIBE |
		RTSP_METHOD_FLAG_ANNOUNCE | RTSP_METHOD_FLAG_SETUP |
		RTSP_METHOD_FLAG_PLAY | RTSP_METHOD_FLAG_TEARDOWN |
		RTSP_METHOD_FLAG_RECORD;
	uint32_t methods_out = 0;
	int ret;

	ret = rtsp_public_header_write(methods_in, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_public_header_read(header_value(buf, RTSP_HEADER_PUBLIC),
				      &methods_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(methods_out, methods_in);
}


/*
 * Range header
 */

static void test_rtsp_header_range_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_range range;
	int ret;

	/* KO cases */
	memset(&range, 0, sizeof(range));
	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_range_header_write(NULL, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_range_header_write(&range, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* start/stop format mismatch (stop is neither UNKNOWN nor equal to
	 * start) */
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.stop.format = RTSP_TIME_FORMAT_SMPTE;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* SMPTE and absolute formats are not implemented */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_SMPTE;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_ABSOLUTE;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, -ENOSYS);

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_UNKNOWN;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Start and stop cannot both be infinity */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.infinity = 1;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	str.len = 0;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Stop cannot be 'now' */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.now = 1;
	str.len = 0;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK: open range, no explicit stop */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	str.len = 0;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Range: npt=0\r\n");

	/* OK: closed range with sub-second precision on both ends */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.sec = 5;
	range.start.npt.usec = 500000;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.sec = 10;
	range.stop.npt.usec = 250000;
	str.len = 0;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Range: npt=5.500-10.250\r\n");

	/* OK: infinite stop */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.stop.format = RTSP_TIME_FORMAT_NPT;
	range.stop.npt.infinity = 1;
	str.len = 0;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Range: npt=0-\r\n");

	/* OK: hours:minutes:seconds formatting */
	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.start.npt.sec = 3661; /* 1h 1m 1s */
	str.len = 0;
	ret = rtsp_range_header_write(&range, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Range: npt=1:01:01\r\n");
}


static void test_rtsp_header_range_read(void)
{
	struct rtsp_range range;
	int ret;

	{
		char v[] = "npt=0-";
		ret = rtsp_range_header_read(NULL, &range);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_range_header_read(v, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* Missing '=' */
	{
		char v[] = "invalidnoequals";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* Unknown time format prefix */
	{
		char v[] = "bogus=5";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* SMPTE / absolute are recognized but not implemented */
	{
		char v[] = "smpte=00:00:10";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}
	{
		char v[] = "clock=20230101T000000Z-";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, -ENOSYS);
	}

	/* Too long a string is rejected outright */
	{
		char *huge = malloc(PATH_MAX + 16);
		CU_ASSERT_PTR_NOT_NULL_FATAL(huge);
		memset(huge, 'a', PATH_MAX + 15);
		huge[PATH_MAX + 15] = '\0';
		ret = rtsp_range_header_read(huge, &range);
		CU_ASSERT_EQUAL(ret, -ENAMETOOLONG);
		free(huge);
	}

	/* OK: open range */
	{
		char v[] = "npt=0";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(range.start.format, RTSP_TIME_FORMAT_NPT);
		CU_ASSERT_EQUAL(range.start.npt.sec, (uint64_t)0);
		CU_ASSERT_EQUAL(range.stop.format, RTSP_TIME_FORMAT_UNKNOWN);
	}

	/* OK: infinite stop */
	{
		char v[] = "npt=0-";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(range.stop.format, RTSP_TIME_FORMAT_NPT);
		CU_ASSERT_EQUAL(range.stop.npt.infinity, 1);
	}

	/* OK: closed range with sub-second precision */
	{
		char v[] = "npt=5.500-10.250";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(range.start.npt.sec, (uint64_t)5);
		CU_ASSERT_EQUAL(range.start.npt.usec, (uint32_t)500000);
		CU_ASSERT_EQUAL(range.stop.npt.sec, (uint64_t)10);
		CU_ASSERT_EQUAL(range.stop.npt.usec, (uint32_t)250000);
	}

	/* OK: hours:minutes:seconds formatting */
	{
		char v[] = "npt=1:01:01";
		ret = rtsp_range_header_read(v, &range);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(range.start.npt.sec, (uint64_t)3661);
	}
}


static void test_rtsp_header_range_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_range range_in, range_out;
	int ret;

	struct rtsp_range_test_case {
		struct rtsp_range range;
	};

	/* clang-format off */
	struct rtsp_range_test_case cases[] = {
		/* Open range */
		{{.start = {.format = RTSP_TIME_FORMAT_NPT}}},
		/* Infinite stop */
		{{.start = {.format = RTSP_TIME_FORMAT_NPT},
		  .stop = {.format = RTSP_TIME_FORMAT_NPT,
			   .npt = {.infinity = 1}}}},
		/* Closed range with sub-second precision */
		{{.start = {.format = RTSP_TIME_FORMAT_NPT,
			    .npt = {.sec = 5, .usec = 500000}},
		  .stop = {.format = RTSP_TIME_FORMAT_NPT,
			   .npt = {.sec = 10, .usec = 250000}}}},
		/* Hours:minutes:seconds */
		{{.start = {.format = RTSP_TIME_FORMAT_NPT,
			    .npt = {.sec = 3661, .usec = 250000}}}},
	};
	/* clang-format on */

	for (size_t i = 0; i < SIZEOF_ARRAY(cases); i++) {
		range_in = cases[i].range;
		memset(&range_out, 0xff, sizeof(range_out));

		str.str = buf;
		str.len = 0;
		str.max_len = sizeof(buf);
		ret = rtsp_range_header_write(&range_in, &str);
		CU_ASSERT_EQUAL(ret, 0);

		ret = rtsp_range_header_read(
			header_value(buf, RTSP_HEADER_RANGE), &range_out);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_TRUE(rtsp_range_cmp(&range_in, &range_out));
	}
}


/*
 * Session header
 */

static void test_rtsp_header_session_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	char session_id[] = "abc123";
	char empty_id[] = "";
	int ret;

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_session_header_write(NULL, 0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_session_header_write(empty_id, 0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_session_header_write(session_id, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK: no timeout */
	str.len = 0;
	ret = rtsp_session_header_write(session_id, 0, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Session: abc123\r\n");

	/* OK: with timeout */
	str.len = 0;
	ret = rtsp_session_header_write(session_id, 60, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Session: abc123;timeout=60\r\n");
}


static void test_rtsp_header_session_read(void)
{
	char *session_id;
	unsigned int timeout;
	int ret;

	{
		char v[] = "abc123";
		ret = rtsp_session_header_read(NULL, &session_id, &timeout);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_session_header_read(v, NULL, &timeout);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_session_header_read(v, &session_id, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* OK: no timeout */
	{
		char v[] = "abc123";
		session_id = NULL;
		timeout = 42;
		ret = rtsp_session_header_read(v, &session_id, &timeout);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(session_id);
		CU_ASSERT_STRING_EQUAL(session_id, "abc123");
		CU_ASSERT_EQUAL(timeout, (unsigned int)0);
		free(session_id);
	}

	/* OK: with timeout */
	{
		char v[] = "abc123;timeout=60";
		session_id = NULL;
		timeout = 0;
		ret = rtsp_session_header_read(v, &session_id, &timeout);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_STRING_EQUAL(session_id, "abc123");
		CU_ASSERT_EQUAL(timeout, (unsigned int)60);
		free(session_id);
	}

	/* OK: unrecognized parameter after the session id is ignored */
	{
		char v[] = "abc123;bogus=1";
		session_id = NULL;
		timeout = 42;
		ret = rtsp_session_header_read(v, &session_id, &timeout);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_STRING_EQUAL(session_id, "abc123");
		CU_ASSERT_EQUAL(timeout, (unsigned int)0);
		free(session_id);
	}
}


static void test_rtsp_header_session_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str = {.str = buf, .len = 0, .max_len = sizeof(buf)};
	char session_id_in[] = "session-42";
	char *session_id_out = NULL;
	unsigned int timeout_out = 0;
	int ret;

	ret = rtsp_session_header_write(session_id_in, 120, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_session_header_read(header_value(buf, RTSP_HEADER_SESSION),
				       &session_id_out,
				       &timeout_out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(session_id_out);
	CU_ASSERT_STRING_EQUAL(session_id_out, session_id_in);
	CU_ASSERT_EQUAL(timeout_out, (unsigned int)120);
	free(session_id_out);
}


/*
 * RTP-Info header
 */

static void test_rtsp_header_rtp_info_new_free(void)
{
	struct rtsp_rtp_info_header *rtpi;
	struct rtsp_rtp_info_header *null_rtpi = NULL;
	int ret;

	rtpi = rtsp_rtp_info_header_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(rtpi);
	CU_ASSERT_PTR_NULL(rtpi->url);
	CU_ASSERT_EQUAL(rtpi->seq_valid, 0);
	CU_ASSERT_EQUAL(rtpi->rtptime_valid, 0);

	ret = rtsp_rtp_info_header_free(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	rtpi->url = xstrdup("trackID=1");
	ret = rtsp_rtp_info_header_free(&rtpi);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NULL(rtpi);

	/* Freeing an already-NULL header pointer is safe */
	ret = rtsp_rtp_info_header_free(&null_rtpi);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_rtsp_header_rtp_info_copy(void)
{
	struct rtsp_rtp_info_header src, dst;
	int ret;

	memset(&src, 0, sizeof(src));
	memset(&dst, 0xff, sizeof(dst));

	ret = rtsp_rtp_info_header_copy(NULL, &dst);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_rtp_info_header_copy(&src, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* NULL url copies through as NULL */
	src.url = NULL;
	ret = rtsp_rtp_info_header_copy(&src, &dst);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NULL(dst.url);

	src.url = xstrdup("trackID=1");
	CU_ASSERT_PTR_NOT_NULL_FATAL(src.url);
	src.seq_valid = 1;
	src.seq = 100;
	src.rtptime_valid = 1;
	src.rtptime = 12345;
	ret = rtsp_rtp_info_header_copy(&src, &dst);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(dst.url);
	CU_ASSERT_PTR_NOT_EQUAL(dst.url, src.url);
	CU_ASSERT_STRING_EQUAL(dst.url, src.url);
	CU_ASSERT_EQUAL(dst.seq_valid, 1);
	CU_ASSERT_EQUAL(dst.seq, (uint16_t)100);
	CU_ASSERT_EQUAL(dst.rtptime_valid, 1);
	CU_ASSERT_EQUAL(dst.rtptime, (uint32_t)12345);

	free(src.url);
	free(dst.url);
}


static void test_rtsp_header_rtp_info_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_rtp_info_header a, b;
	struct rtsp_rtp_info_header *one[] = {&a};
	struct rtsp_rtp_info_header *two[] = {&a, &b};
	struct rtsp_rtp_info_header *empty_slot[] = {NULL, &b};
	int ret;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_rtp_info_header_write(NULL, 1, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_rtp_info_header_write(one, 0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_rtp_info_header_write(one, 1, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK: single entry, url only */
	a.url = "trackID=1";
	str.len = 0;
	ret = rtsp_rtp_info_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "RTP-Info: url=trackID=1\r\n");

	/* OK: single entry, url + seq + rtptime */
	a.seq_valid = 1;
	a.seq = 100;
	a.rtptime_valid = 1;
	a.rtptime = 1000;
	str.len = 0;
	ret = rtsp_rtp_info_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf, "RTP-Info: url=trackID=1;seq=100;rtptime=1000\r\n");

	/* OK: two entries, comma-separated */
	b.url = "trackID=2";
	b.seq_valid = 1;
	b.seq = 200;
	b.rtptime_valid = 1;
	b.rtptime = 2000;
	str.len = 0;
	ret = rtsp_rtp_info_header_write(two, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf,
			       "RTP-Info: url=trackID=1;seq=100;rtptime=1000,"
			       "url=trackID=2;seq=200;rtptime=2000\r\n");

	/* OK: a NULL slot in the array is silently skipped, with no stray
	 * separator left behind */
	str.len = 0;
	ret = rtsp_rtp_info_header_write(empty_slot, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf, "RTP-Info: url=trackID=2;seq=200;rtptime=2000\r\n");
}


static void test_rtsp_header_rtp_info_read(void)
{
	struct rtsp_rtp_info_header *out[RTSP_RTP_INFO_MAX_COUNT];
	unsigned int count;
	int ret;

	{
		char v[] = "url=trackID=1";
		ret = rtsp_rtp_info_header_read(NULL, out, 1, &count);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_rtp_info_header_read(v, NULL, 1, &count);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_rtp_info_header_read(v, out, 0, &count);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_rtp_info_header_read(v, out, 1, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* OK: single entry, url only */
	{
		char v[] = "url=trackID=1";
		count = 0xff;
		ret = rtsp_rtp_info_header_read(
			v, out, RTSP_RTP_INFO_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(count, (unsigned int)1);
		CU_ASSERT_STRING_EQUAL(out[0]->url, "trackID=1");
		CU_ASSERT_EQUAL(out[0]->seq_valid, 0);
		CU_ASSERT_EQUAL(out[0]->rtptime_valid, 0);
		rtsp_rtp_info_header_free(&out[0]);
	}

	/* OK: single entry, url + seq + rtptime */
	{
		char v[] = "url=trackID=1;seq=100;rtptime=1000";
		ret = rtsp_rtp_info_header_read(
			v, out, RTSP_RTP_INFO_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(count, (unsigned int)1);
		CU_ASSERT_STRING_EQUAL(out[0]->url, "trackID=1");
		CU_ASSERT_EQUAL(out[0]->seq_valid, 1);
		CU_ASSERT_EQUAL(out[0]->seq, (uint16_t)100);
		CU_ASSERT_EQUAL(out[0]->rtptime_valid, 1);
		CU_ASSERT_EQUAL(out[0]->rtptime, (uint32_t)1000);
		rtsp_rtp_info_header_free(&out[0]);
	}

	/* KO: first entry has no 'url' key */
	{
		char v[] = "seq=100";
		ret = rtsp_rtp_info_header_read(
			v, out, RTSP_RTP_INFO_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -EPROTO);
		CU_ASSERT_EQUAL(count, (unsigned int)0);
	}

	/* KO: rtptime overflows a 32-bit unsigned integer */
	{
		char v[] = "url=trackID=1;rtptime=99999999999999999999";
		ret = rtsp_rtp_info_header_read(
			v, out, RTSP_RTP_INFO_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -ERANGE);
	}

	/* OK: an unrecognized parameter is silently ignored */
	{
		char v[] = "url=trackID=1;bogus=1";
		ret = rtsp_rtp_info_header_read(
			v, out, RTSP_RTP_INFO_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(count, (unsigned int)1);
		CU_ASSERT_EQUAL(out[0]->seq_valid, 0);
		rtsp_rtp_info_header_free(&out[0]);
	}

	/* max_count caps the number of entries actually parsed */
	{
		char v[] = "url=trackID=1,url=trackID=2,url=trackID=3";
		ret = rtsp_rtp_info_header_read(v, out, 2, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(count, (unsigned int)2);
		CU_ASSERT_STRING_EQUAL(out[0]->url, "trackID=1");
		CU_ASSERT_STRING_EQUAL(out[1]->url, "trackID=2");
		rtsp_rtp_info_header_free(&out[0]);
		rtsp_rtp_info_header_free(&out[1]);
	}
}


/*
 * Regression test for a bug found while writing this suite: parsing a
 * multi-entry RTP-Info header (rtsp_rtp_info_header_read(), rtsp.c) used to
 * tokenize the *original* string on every loop iteration instead of the
 * current comma-separated entry, so every entry after the first was
 * silently parsed as a duplicate of the first entry. Fixed by tokenizing
 * the current entry (rtpi_str) instead of the original string (str).
 */
static void test_rtsp_header_rtp_info_multi_entry_regression(void)
{
	struct rtsp_rtp_info_header *out[RTSP_RTP_INFO_MAX_COUNT];
	unsigned int count;
	int ret;
	char v[] =
		"url=trackID=1;seq=100;rtptime=1000,"
		"url=trackID=2;seq=200;rtptime=2000,"
		"url=trackID=3;seq=300;rtptime=3000";

	ret = rtsp_rtp_info_header_read(
		v, out, RTSP_RTP_INFO_MAX_COUNT, &count);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(count, (unsigned int)3);

	CU_ASSERT_STRING_EQUAL(out[0]->url, "trackID=1");
	CU_ASSERT_EQUAL(out[0]->seq, (uint16_t)100);
	CU_ASSERT_EQUAL(out[0]->rtptime, (uint32_t)1000);

	CU_ASSERT_STRING_EQUAL(out[1]->url, "trackID=2");
	CU_ASSERT_EQUAL(out[1]->seq, (uint16_t)200);
	CU_ASSERT_EQUAL(out[1]->rtptime, (uint32_t)2000);

	CU_ASSERT_STRING_EQUAL(out[2]->url, "trackID=3");
	CU_ASSERT_EQUAL(out[2]->seq, (uint16_t)300);
	CU_ASSERT_EQUAL(out[2]->rtptime, (uint32_t)3000);

	rtsp_rtp_info_header_free(&out[0]);
	rtsp_rtp_info_header_free(&out[1]);
	rtsp_rtp_info_header_free(&out[2]);
}


static void test_rtsp_header_rtp_info_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str = {.str = buf, .len = 0, .max_len = sizeof(buf)};
	struct rtsp_rtp_info_header a, b;
	struct rtsp_rtp_info_header *in[] = {&a, &b};
	struct rtsp_rtp_info_header *out[RTSP_RTP_INFO_MAX_COUNT];
	unsigned int count;
	int ret;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.url = "trackID=1";
	a.seq_valid = 1;
	a.seq = 1;
	a.rtptime_valid = 1;
	a.rtptime = 111;
	b.url = "trackID=2";
	b.seq_valid = 1;
	b.seq = 2;
	b.rtptime_valid = 1;
	b.rtptime = 222;

	ret = rtsp_rtp_info_header_write(in, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_rtp_info_header_read(header_value(buf, RTSP_HEADER_RTP_INFO),
					out,
					RTSP_RTP_INFO_MAX_COUNT,
					&count);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(count, (unsigned int)2);
	CU_ASSERT_STRING_EQUAL(out[0]->url, a.url);
	CU_ASSERT_EQUAL(out[0]->seq, a.seq);
	CU_ASSERT_EQUAL(out[0]->rtptime, a.rtptime);
	CU_ASSERT_STRING_EQUAL(out[1]->url, b.url);
	CU_ASSERT_EQUAL(out[1]->seq, b.seq);
	CU_ASSERT_EQUAL(out[1]->rtptime, b.rtptime);

	rtsp_rtp_info_header_free(&out[0]);
	rtsp_rtp_info_header_free(&out[1]);
}


/*
 * Transport header
 */

static int strcmp_nullsafe(const char *a, const char *b)
{
	if ((a == NULL) && (b == NULL))
		return 0;
	if ((a == NULL) || (b == NULL))
		return 1;
	return strcmp(a, b);
}


static bool transport_eq(const struct rtsp_transport_header *a,
			 const struct rtsp_transport_header *b)
{
	if (strcmp_nullsafe(a->transport_protocol, b->transport_protocol))
		return false;
	if (strcmp_nullsafe(a->transport_profile, b->transport_profile))
		return false;
	if (a->lower_transport != b->lower_transport)
		return false;
	if (a->delivery != b->delivery)
		return false;
	if (a->interleaved_count != b->interleaved_count)
		return false;
	if (memcmp(a->interleaved,
		   b->interleaved,
		   a->interleaved_count * sizeof(a->interleaved[0])) != 0)
		return false;
	if (strcmp_nullsafe(a->destination, b->destination))
		return false;
	if (strcmp_nullsafe(a->source, b->source))
		return false;
	if (a->layers != b->layers)
		return false;
	if (a->method != b->method)
		return false;
	if (a->append != b->append)
		return false;
	if (a->ttl != b->ttl)
		return false;
	if (a->dst_stream_port != b->dst_stream_port)
		return false;
	if (a->dst_control_port != b->dst_control_port)
		return false;
	if (a->src_stream_port != b->src_stream_port)
		return false;
	if (a->src_control_port != b->src_control_port)
		return false;
	if (a->ssrc_valid != b->ssrc_valid)
		return false;
	if (a->ssrc_valid && (a->ssrc != b->ssrc))
		return false;
	return true;
}


static void test_rtsp_header_transport_new_free_copy(void)
{
	struct rtsp_transport_header *t, *null_t = NULL;
	struct rtsp_transport_header src, dst;
	int ret;

	t = rtsp_transport_header_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(t);
	CU_ASSERT_PTR_NULL(t->transport_protocol);
	CU_ASSERT_EQUAL(t->lower_transport, RTSP_LOWER_TRANSPORT_UDP);

	ret = rtsp_transport_header_free(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	t->transport_protocol = xstrdup("RTP");
	t->transport_profile = xstrdup("AVP");
	t->destination = xstrdup("239.0.0.1");
	t->source = xstrdup("10.0.0.1");
	ret = rtsp_transport_header_free(&t);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NULL(t);

	/* Freeing an already-NULL header pointer is safe */
	ret = rtsp_transport_header_free(&null_t);
	CU_ASSERT_EQUAL(ret, 0);

	memset(&src, 0, sizeof(src));
	memset(&dst, 0xff, sizeof(dst));
	ret = rtsp_transport_header_copy(NULL, &dst);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_transport_header_copy(&src, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	src.transport_protocol = xstrdup("RTP");
	src.transport_profile = xstrdup("AVP");
	src.lower_transport = RTSP_LOWER_TRANSPORT_TCP;
	src.interleaved_count = 2;
	src.interleaved[0].rtp = 0;
	src.interleaved[0].rtcp = 1;
	src.interleaved[1].rtp = 2;
	src.interleaved[1].rtcp = 3;
	src.delivery = RTSP_DELIVERY_UNICAST;
	src.destination = xstrdup("10.0.0.1");
	src.source = xstrdup("10.0.0.2");
	src.layers = 3;
	src.method = RTSP_TRANSPORT_METHOD_PLAY;
	src.append = 1;
	src.ttl = 15;
	src.dst_stream_port = 5000;
	src.dst_control_port = 5001;
	src.ssrc_valid = 1;
	src.ssrc = 0x12345678;

	ret = rtsp_transport_header_copy(&src, &dst);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_EQUAL(dst.transport_protocol, src.transport_protocol);
	CU_ASSERT_TRUE(transport_eq(&src, &dst));

	free(src.transport_protocol);
	free(src.transport_profile);
	free(src.destination);
	free(src.source);
	free(dst.transport_protocol);
	free(dst.transport_profile);
	free(dst.destination);
	free(dst.source);
}


static void test_rtsp_header_transport_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_transport_header t, t2;
	struct rtsp_transport_header *one[] = {&t};
	struct rtsp_transport_header *two[] = {&t, &t2};
	struct rtsp_transport_header *empty_slot[] = {NULL, &t2};
	int ret;

	memset(&t, 0, sizeof(t));
	memset(&t2, 0, sizeof(t2));

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_transport_header_write(NULL, 1, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_transport_header_write(one, 0, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_transport_header_write(one, 1, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* A NULL slot in the array is silently skipped, with no stray
	 * separator left behind */
	t2.transport_protocol = "RTP";
	t2.transport_profile = "AVP";
	t2.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t2.delivery = RTSP_DELIVERY_UNICAST;
	t2.dst_stream_port = 7000;
	str.len = 0;
	ret = rtsp_transport_header_write(empty_slot, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Transport: RTP/AVP/UDP;unicast;client_port=7000-7001\r\n");

	/* An entry with no protocol, or no profile, is silently skipped */
	memset(&t, 0, sizeof(t));
	t.transport_profile = "AVP"; /* protocol missing */
	str.len = 0;
	ret = rtsp_transport_header_write(two, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Transport: RTP/AVP/UDP;unicast;client_port=7000-7001\r\n");

	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP"; /* profile missing */
	str.len = 0;
	ret = rtsp_transport_header_write(two, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Transport: RTP/AVP/UDP;unicast;client_port=7000-7001\r\n");

	/* OK: unicast, client_port defaults its control port to stream+1 */
	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP";
	t.transport_profile = "AVP";
	t.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t.delivery = RTSP_DELIVERY_UNICAST;
	t.dst_stream_port = 5000;
	str.len = 0;
	ret = rtsp_transport_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Transport: RTP/AVP/UDP;unicast;client_port=5000-5001\r\n");

	/* OK: multicast uses 'port' instead of client_port/server_port */
	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP";
	t.transport_profile = "AVP";
	t.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t.delivery = RTSP_DELIVERY_MULTICAST;
	t.dst_stream_port = 6000;
	str.len = 0;
	ret = rtsp_transport_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf,
			       "Transport: RTP/AVP/UDP;multicast;"
			       "port=6000-6001\r\n");

	/* OK: destination/source/append/ttl/layers */
	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP";
	t.transport_profile = "AVP";
	t.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t.delivery = RTSP_DELIVERY_MULTICAST;
	t.destination = "239.0.0.1";
	t.source = "10.0.0.1";
	t.append = 1;
	t.ttl = 15;
	t.layers = 2;
	str.len = 0;
	ret = rtsp_transport_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf,
			       "Transport: RTP/AVP/UDP;multicast;"
			       "destination=239.0.0.1;source=10.0.0.1;"
			       "append;ttl=15;layers=2\r\n");

	/* OK: TCP interleaved channel pairs */
	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP";
	t.transport_profile = "AVP";
	t.lower_transport = RTSP_LOWER_TRANSPORT_TCP;
	t.delivery = RTSP_DELIVERY_UNICAST;
	t.interleaved_count = 2;
	t.interleaved[0].rtp = 0;
	t.interleaved[0].rtcp = 1;
	t.interleaved[1].rtp = 2;
	t.interleaved[1].rtcp = 3;
	str.len = 0;
	ret = rtsp_transport_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf, "Transport: RTP/AVP/TCP;unicast;interleaved=0-1,2-3\r\n");

	/* OK: ssrc and mode */
	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP";
	t.transport_profile = "AVP";
	t.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t.delivery = RTSP_DELIVERY_UNICAST;
	t.ssrc_valid = 1;
	t.ssrc = 0xdeadbeef;
	t.method = RTSP_TRANSPORT_METHOD_RECORD;
	str.len = 0;
	ret = rtsp_transport_header_write(one, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf,
			       "Transport: RTP/AVP/UDP;unicast;ssrc=DEADBEEF;"
			       "mode=RECORD\r\n");

	/* OK: two entries, comma-separated */
	memset(&t, 0, sizeof(t));
	t.transport_protocol = "RTP";
	t.transport_profile = "AVP";
	t.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t.delivery = RTSP_DELIVERY_UNICAST;
	t.dst_stream_port = 5000;
	memset(&t2, 0, sizeof(t2));
	t2.transport_protocol = "RTP";
	t2.transport_profile = "AVP";
	t2.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	t2.delivery = RTSP_DELIVERY_UNICAST;
	t2.dst_stream_port = 7000;
	str.len = 0;
	ret = rtsp_transport_header_write(two, 2, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Transport: RTP/AVP/UDP;unicast;client_port=5000-5001,"
		"RTP/AVP/UDP;unicast;client_port=7000-7001\r\n");
}


static void test_rtsp_header_transport_read(void)
{
	struct rtsp_transport_header *out[RTSP_TRANSPORT_MAX_COUNT];
	unsigned int count;
	int ret;

	{
		char v[] = "RTP/AVP";
		ret = rtsp_transport_header_read(NULL, out, 1, &count);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_transport_header_read(v, NULL, 1, &count);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_transport_header_read(v, out, 0, &count);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_transport_header_read(v, out, 1, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* KO: entry is just a bare '/', no protocol token at all */
	{
		char v[] = "/";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* KO: unsupported protocol/profile/lower-transport */
	{
		char v[] = "SCTP/AVP";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}
	{
		char v[] = "RTP/BADPROFILE";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}
	{
		char v[] = "RTP/AVP/SCTP";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* OK: lower-transport omitted defaults to UDP */
	{
		char v[] = "RTP/AVP;unicast;client_port=5000-5001";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(count, (unsigned int)1);
		CU_ASSERT_EQUAL(out[0]->lower_transport,
				RTSP_LOWER_TRANSPORT_UDP);
		CU_ASSERT_EQUAL(out[0]->delivery, RTSP_DELIVERY_UNICAST);
		CU_ASSERT_EQUAL(out[0]->dst_stream_port, (uint16_t)5000);
		CU_ASSERT_EQUAL(out[0]->dst_control_port, (uint16_t)5001);
		rtsp_transport_header_free(&out[0]);
	}

	/* OK: 'unicast'/'multicast' omitted defaults to MULTICAST (the
	 * struct's zero value) - a real caller relying on this default
	 * without an explicit tag would silently get multicast delivery.
	 * A single interleaved pair is used here (see the dedicated
	 * known-limitation test below for why more than one pair breaks). */
	{
		char v[] = "RTP/AVP/TCP;interleaved=0-1";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(out[0]->lower_transport,
				RTSP_LOWER_TRANSPORT_TCP);
		CU_ASSERT_EQUAL(out[0]->delivery, RTSP_DELIVERY_MULTICAST);
		CU_ASSERT_EQUAL(out[0]->interleaved_count, (unsigned int)1);
		CU_ASSERT_EQUAL(out[0]->interleaved[0].rtp, (uint8_t)0);
		CU_ASSERT_EQUAL(out[0]->interleaved[0].rtcp, (uint8_t)1);
		rtsp_transport_header_free(&out[0]);
	}

	/* OK: ssrc scanning is case-insensitive on input, unlike the
	 * always-uppercase output produced by the writer */
	{
		char v[] = "RTP/AVP;unicast;ssrc=deadbeef";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(out[0]->ssrc_valid, 1);
		CU_ASSERT_EQUAL(out[0]->ssrc, (uint32_t)0xdeadbeef);
		rtsp_transport_header_free(&out[0]);
	}

	/* OK (known imprecision, not fixed): 'mode' is matched by prefix,
	 * so a value merely starting with "play"/"record" also matches */
	{
		char v[] = "RTP/AVP;unicast;mode=PLAYlist";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(out[0]->method, RTSP_TRANSPORT_METHOD_PLAY);
		rtsp_transport_header_free(&out[0]);
	}

	/* OK: control port omitted defaults to stream port + 1 */
	{
		char v[] = "RTP/AVP;unicast;client_port=6000";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(out[0]->dst_stream_port, (uint16_t)6000);
		CU_ASSERT_EQUAL(out[0]->dst_control_port, (uint16_t)6001);
		rtsp_transport_header_free(&out[0]);
	}

	/* OK: destination/source strings, bare 'append' flag */
	{
		char v[] =
			"RTP/AVP;multicast;destination=239.0.0.1;"
			"source=10.0.0.5;append";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_STRING_EQUAL(out[0]->destination, "239.0.0.1");
		CU_ASSERT_STRING_EQUAL(out[0]->source, "10.0.0.5");
		CU_ASSERT_EQUAL(out[0]->append, 1);
		rtsp_transport_header_free(&out[0]);
	}

	/*
	 * Known limitation, not fixed: rtsp_transport_header_read() splits
	 * the whole header value into comma-separated entries *before*
	 * parsing any single entry's parameters. This is ambiguous with the
	 * 'interleaved' parameter's own comma-separated pair list: a
	 * genuine, single Transport entry carrying more than one
	 * interleaved pair (e.g. audio+video multiplexed on one TCP
	 * connection, "interleaved=0-1,2-3", which
	 * rtsp_transport_header_write() itself is happy to produce - see
	 * test_rtsp_header_transport_write() above) gets torn in two at the
	 * embedded comma: the first chunk ("RTP/AVP/TCP;interleaved=0-1")
	 * parses as a complete, valid entry with only the first pair, and
	 * the second chunk ("2-3") is then misread as a bogus *second*
	 * transport-spec entry, failing transport-protocol validation. The
	 * overall call fails with -EPROTO even though the input was
	 * well-formed and self-consistent. This is a real, reproducible
	 * write/read asymmetry, but fixing it means teaching the top-level
	 * splitter to recognize 'interleaved=' value boundaries (or
	 * switching interleaved to a non-comma separator), a parser
	 * restructuring beyond the scope of this test-writing pass - so
	 * this test pins down the current (broken) behavior rather than an
	 * intended one. A caller can still recover the first pair from the
	 * partially-populated entry left in the output array despite the
	 * error return.
	 */
	{
		char v[] = "RTP/AVP/TCP;interleaved=0-1,2-3";
		ret = rtsp_transport_header_read(
			v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
		CU_ASSERT_EQUAL(ret, -EPROTO);
		CU_ASSERT_EQUAL(count, (unsigned int)1);
		CU_ASSERT_PTR_NOT_NULL_FATAL(out[0]);
		CU_ASSERT_EQUAL(out[0]->interleaved_count, (unsigned int)1);
		CU_ASSERT_EQUAL(out[0]->interleaved[0].rtp, (uint8_t)0);
		CU_ASSERT_EQUAL(out[0]->interleaved[0].rtcp, (uint8_t)1);
		rtsp_transport_header_free(&out[0]);
	}
}


/*
 * Regression test for a bug found while writing this suite, identical in
 * shape to the one fixed in rtsp_rtp_info_header_read(): parsing a
 * multi-entry Transport header used to tokenize the original string on
 * every loop iteration instead of the current comma-separated entry, so
 * every entry after the first was silently parsed as a duplicate of the
 * first entry. Fixed by tokenizing the current entry (trsp_str) instead of
 * the original string (str).
 */
static void test_rtsp_header_transport_multi_entry_regression(void)
{
	struct rtsp_transport_header *out[RTSP_TRANSPORT_MAX_COUNT];
	unsigned int count;
	int ret;
	char v[] =
		"RTP/AVP;unicast;client_port=5000-5001,"
		"RTP/AVP;unicast;client_port=6000-6001,"
		"RTP/AVP;unicast;client_port=7000-7001";

	ret = rtsp_transport_header_read(
		v, out, RTSP_TRANSPORT_MAX_COUNT, &count);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(count, (unsigned int)3);

	CU_ASSERT_EQUAL(out[0]->dst_stream_port, (uint16_t)5000);
	CU_ASSERT_EQUAL(out[1]->dst_stream_port, (uint16_t)6000);
	CU_ASSERT_EQUAL(out[2]->dst_stream_port, (uint16_t)7000);

	rtsp_transport_header_free(&out[0]);
	rtsp_transport_header_free(&out[1]);
	rtsp_transport_header_free(&out[2]);

	/* max_count caps the number of entries actually parsed */
	{
		char v2[] =
			"RTP/AVP;unicast;client_port=5000-5001,"
			"RTP/AVP;unicast;client_port=6000-6001,"
			"RTP/AVP;unicast;client_port=7000-7001";
		ret = rtsp_transport_header_read(v2, out, 2, &count);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(count, (unsigned int)2);
		CU_ASSERT_EQUAL(out[0]->dst_stream_port, (uint16_t)5000);
		CU_ASSERT_EQUAL(out[1]->dst_stream_port, (uint16_t)6000);
		rtsp_transport_header_free(&out[0]);
		rtsp_transport_header_free(&out[1]);
	}
}


static void test_rtsp_header_transport_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_transport_header in, *out;
	unsigned int count;
	struct rtsp_transport_header *arr[] = {&in};
	int ret;

	/* Unicast, UDP, every non-interleaved field set */
	memset(&in, 0, sizeof(in));
	in.transport_protocol = "RTP";
	in.transport_profile = "AVP";
	in.lower_transport = RTSP_LOWER_TRANSPORT_UDP;
	in.delivery = RTSP_DELIVERY_UNICAST;
	in.destination = "10.0.0.9";
	in.source = "10.0.0.10";
	in.append = 1;
	in.ttl = 5;
	in.layers = 3;
	in.dst_stream_port = 5000;
	in.dst_control_port = 5010;
	in.src_stream_port = 6000;
	in.src_control_port = 6010;
	in.ssrc_valid = 1;
	in.ssrc = 0x12345678;
	in.method = RTSP_TRANSPORT_METHOD_PLAY;

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_transport_header_write(arr, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_transport_header_read(
		header_value(buf, RTSP_HEADER_TRANSPORT), &out, 1, &count);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(count, (unsigned int)1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(out);
	CU_ASSERT_TRUE(transport_eq(&in, out));
	rtsp_transport_header_free(&out);

	/* TCP, a single interleaved channel pair. A single pair is the
	 * realistic/dominant case (one RTP/RTCP pair per media) and is the
	 * only count that round-trips correctly: more than one pair hits
	 * the known top-level comma-splitting limitation documented and
	 * pinned in test_rtsp_header_transport_read() above. */
	memset(&in, 0, sizeof(in));
	in.transport_protocol = "RTP";
	in.transport_profile = "AVP";
	in.lower_transport = RTSP_LOWER_TRANSPORT_TCP;
	in.delivery = RTSP_DELIVERY_UNICAST;
	in.interleaved_count = 1;
	in.interleaved[0].rtp = 0;
	in.interleaved[0].rtcp = 1;

	str.len = 0;
	ret = rtsp_transport_header_write(arr, 1, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_transport_header_read(
		header_value(buf, RTSP_HEADER_TRANSPORT), &out, 1, &count);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(count, (unsigned int)1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(out);
	CU_ASSERT_TRUE(transport_eq(&in, out));
	rtsp_transport_header_free(&out);
}


/*
 * Authorization header
 */

static bool auth_eq(const struct rtsp_authorization_header *a,
		    const struct rtsp_authorization_header *b)
{
	if (a->type != b->type)
		return false;
	if (a->algorithm != b->algorithm)
		return false;
	if (a->qop != b->qop)
		return false;
	if (a->nc != b->nc)
		return false;
	if (strcmp_nullsafe(a->username, b->username))
		return false;
	if (strcmp_nullsafe(a->uri, b->uri))
		return false;
	if (strcmp_nullsafe(a->response, b->response))
		return false;
	if (strcmp_nullsafe(a->cnonce, b->cnonce))
		return false;
	if (strcmp_nullsafe(a->credentials, b->credentials))
		return false;
	if (strcmp_nullsafe(a->realm, b->realm))
		return false;
	if (strcmp_nullsafe(a->nonce, b->nonce))
		return false;
	if (strcmp_nullsafe(a->opaque, b->opaque))
		return false;
	return true;
}


static void test_rtsp_header_authorization_new_free_copy(void)
{
	struct rtsp_authorization_header *a, *null_a = NULL;
	struct rtsp_authorization_header src, dst;
	int ret;

	a = rtsp_authorization_header_new();
	CU_ASSERT_PTR_NOT_NULL_FATAL(a);
	CU_ASSERT_EQUAL(a->type, RTSP_AUTH_TYPE_UNKNOWN);
	CU_ASSERT_EQUAL(a->algorithm, RTSP_AUTH_ALGORITHM_UNSPECIFIED);
	CU_ASSERT_EQUAL(a->qop, RTSP_AUTH_QOP_UNSPECIFIED);
	CU_ASSERT_PTR_NULL(a->username);

	ret = rtsp_authorization_header_free(NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	a->username = xstrdup("bob");
	a->realm = xstrdup("example.com");
	a->nonce = xstrdup("abc123");
	a->uri = xstrdup("rtsp://host/stream");
	a->response = xstrdup("deadbeef");
	a->opaque = xstrdup("op4234");
	a->cnonce = xstrdup("cn5678");
	a->credentials = xstrdup("dXNlcjpwYXNz");
	ret = rtsp_authorization_header_free(&a);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NULL(a);

	/* Freeing an already-NULL header pointer is safe */
	ret = rtsp_authorization_header_free(&null_a);
	CU_ASSERT_EQUAL(ret, 0);

	/* copy(): unlike rtsp_rtp_info_header_copy()/
	 * rtsp_transport_header_copy() (plain pointer overwrite),
	 * rtsp_authorization_header_copy() goes through dup_field(), which
	 * frees the destination's existing pointer before duplicating the
	 * source - so dst must start zeroed (or otherwise hold only valid,
	 * freeable pointers), not poisoned. */
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
	ret = rtsp_authorization_header_copy(NULL, &dst);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_authorization_header_copy(&src, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	src.type = RTSP_AUTH_TYPE_DIGEST;
	src.algorithm = RTSP_AUTH_ALGORITHM_MD5;
	src.username = xstrdup("bob");
	src.uri = xstrdup("rtsp://host/stream");
	src.response = xstrdup("deadbeef");
	src.cnonce = xstrdup("cn5678");
	src.nc = 1;
	src.credentials = NULL;
	src.realm = xstrdup("example.com");
	src.nonce = xstrdup("abc123");
	src.opaque = xstrdup("op4234");
	src.qop = RTSP_AUTH_QOP_AUTH;

	ret = rtsp_authorization_header_copy(&src, &dst);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_EQUAL(dst.username, src.username);
	CU_ASSERT_TRUE(auth_eq(&src, &dst));

	/* copy_server_fields(): only type/realm/nonce/opaque/algorithm/qop */
	{
		struct rtsp_authorization_header dst2;
		memset(&dst2, 0, sizeof(dst2));
		dst2.username = xstrdup("untouched");

		ret = rtsp_authorization_header_copy_server_fields(NULL, &dst2);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_authorization_header_copy_server_fields(&src, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		ret = rtsp_authorization_header_copy_server_fields(&src, &dst2);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(dst2.type, src.type);
		CU_ASSERT_STRING_EQUAL(dst2.realm, src.realm);
		CU_ASSERT_STRING_EQUAL(dst2.nonce, src.nonce);
		CU_ASSERT_STRING_EQUAL(dst2.opaque, src.opaque);
		CU_ASSERT_EQUAL(dst2.algorithm, src.algorithm);
		CU_ASSERT_EQUAL(dst2.qop, src.qop);
		/* Client-provided fields are left untouched */
		CU_ASSERT_STRING_EQUAL(dst2.username, "untouched");
		CU_ASSERT_PTR_NULL(dst2.uri);

		free(dst2.username);
		free(dst2.realm);
		free(dst2.nonce);
		free(dst2.opaque);
	}

	/* copy_client_fields(): only username/credentials/uri/response/
	 * cnonce/nc */
	{
		struct rtsp_authorization_header dst3;
		memset(&dst3, 0, sizeof(dst3));
		dst3.realm = xstrdup("untouched");

		ret = rtsp_authorization_header_copy_client_fields(NULL, &dst3);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_authorization_header_copy_client_fields(&src, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);

		ret = rtsp_authorization_header_copy_client_fields(&src, &dst3);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_STRING_EQUAL(dst3.username, src.username);
		CU_ASSERT_STRING_EQUAL(dst3.uri, src.uri);
		CU_ASSERT_STRING_EQUAL(dst3.response, src.response);
		CU_ASSERT_STRING_EQUAL(dst3.cnonce, src.cnonce);
		CU_ASSERT_EQUAL(dst3.nc, src.nc);
		/* Server-provided fields are left untouched */
		CU_ASSERT_STRING_EQUAL(dst3.realm, "untouched");
		CU_ASSERT_PTR_NULL(dst3.nonce);

		free(dst3.username);
		free(dst3.uri);
		free(dst3.response);
		free(dst3.cnonce);
		free(dst3.realm);
	}

	free(src.username);
	free(src.uri);
	free(src.response);
	free(src.cnonce);
	free(src.realm);
	free(src.nonce);
	free(src.opaque);
	free(dst.username);
	free(dst.uri);
	free(dst.response);
	free(dst.cnonce);
	free(dst.realm);
	free(dst.nonce);
	free(dst.opaque);
}


static void test_rtsp_header_authorization_write(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_authorization_header auth;
	int ret;

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_authorization_header_write(NULL, &str);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_BASIC;
	auth.credentials = "dXNlcjpwYXNz";
	ret = rtsp_authorization_header_write(&auth, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* KO: unknown auth type */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_UNKNOWN;
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* KO: Basic with no credentials */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_BASIC;
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, -EPROTO);
	auth.credentials = "";
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, -EPROTO);

	/* OK: Basic */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_BASIC;
	auth.credentials = "dXNlcjpwYXNz";
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Authorization: Basic dXNlcjpwYXNz\r\n");

	/*
	 * Known behavior, not fixed: Digest with no response is explicitly
	 * marked "FIXME return -EPROTO" in the source and instead returns 0
	 * with only "Authorization: " already written to str (no CRLF, no
	 * auth-type token) - a malformed half-written header on a
	 * "successful" return. Pinned here rather than silently left
	 * unexercised.
	 */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_DIGEST;
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(buf, "Authorization: ");

	/* OK: Digest, minimal (only response set) */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_DIGEST;
	auth.response = "deadbeef";
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf, "Authorization: Digest response=\"deadbeef\"\r\n");

	/* OK: Digest, every field set */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_DIGEST;
	auth.username = "bob";
	auth.realm = "example.com";
	auth.nonce = "abc123";
	auth.uri = "rtsp://host/stream";
	auth.response = "deadbeef";
	auth.algorithm = RTSP_AUTH_ALGORITHM_MD5;
	auth.opaque = "op4234";
	auth.qop = RTSP_AUTH_QOP_AUTH;
	auth.cnonce = "cn5678";
	auth.nc = 1;
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf,
		"Authorization: Digest username=\"bob\", realm=\"example.com\", "
		"nonce=\"abc123\", uri=\"rtsp://host/stream\", "
		"response=\"deadbeef\", algorithm=\"MD5\", opaque=\"op4234\", "
		"qop=\"auth\", cnonce=\"cn5678\", nc=\"00000001\"\r\n");

	/* OK: cnonce/nc are only emitted when qop is actually set, per
	 * RFC 2617 - even if the caller populated them anyway */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_DIGEST;
	auth.response = "deadbeef";
	auth.cnonce = "cn5678";
	auth.nc = 1;
	auth.qop = RTSP_AUTH_QOP_UNSPECIFIED;
	str.len = 0;
	ret = rtsp_authorization_header_write(&auth, &str);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_STRING_EQUAL(
		buf, "Authorization: Digest response=\"deadbeef\"\r\n");
}


static void test_rtsp_header_authorization_read(void)
{
	struct rtsp_authorization_header *auth;
	int ret;

	{
		char v[] = "Basic dXNlcjpwYXNz";
		ret = rtsp_authorization_header_read(NULL, &auth);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		ret = rtsp_authorization_header_read(v, NULL);
		CU_ASSERT_EQUAL(ret, -EINVAL);
	}

	/* KO: empty string, no token at all */
	{
		char v[] = "";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* KO: unknown auth scheme */
	{
		char v[] = "Bearer abc123";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* KO: Basic with no credentials token */
	{
		char v[] = "Basic";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, -EPROTO);
	}

	/* OK: Basic, case-insensitive scheme name */
	{
		char v[] = "basic dXNlcjpwYXNz";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
		CU_ASSERT_EQUAL(auth->type, RTSP_AUTH_TYPE_BASIC);
		CU_ASSERT_STRING_EQUAL(auth->credentials, "dXNlcjpwYXNz");
		rtsp_authorization_header_free(&auth);
	}

	/* OK: Digest, every field set */
	{
		char v[] =
			"Digest username=\"bob\", realm=\"example.com\", "
			"nonce=\"abc123\", uri=\"rtsp://host/stream\", "
			"response=\"deadbeef\", algorithm=\"MD5\", "
			"opaque=\"op4234\", qop=\"auth\", "
			"cnonce=\"cn5678\", nc=\"00000001\"";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
		CU_ASSERT_EQUAL(auth->type, RTSP_AUTH_TYPE_DIGEST);
		CU_ASSERT_STRING_EQUAL(auth->username, "bob");
		CU_ASSERT_STRING_EQUAL(auth->realm, "example.com");
		CU_ASSERT_STRING_EQUAL(auth->nonce, "abc123");
		CU_ASSERT_STRING_EQUAL(auth->uri, "rtsp://host/stream");
		CU_ASSERT_STRING_EQUAL(auth->response, "deadbeef");
		CU_ASSERT_EQUAL(auth->algorithm, RTSP_AUTH_ALGORITHM_MD5);
		CU_ASSERT_STRING_EQUAL(auth->opaque, "op4234");
		CU_ASSERT_EQUAL(auth->qop, RTSP_AUTH_QOP_AUTH);
		CU_ASSERT_STRING_EQUAL(auth->cnonce, "cn5678");
		CU_ASSERT_EQUAL(auth->nc, (unsigned int)1);
		rtsp_authorization_header_free(&auth);
	}
}


/*
 * Regression test for a bug found while writing this suite:
 * RTSP_KEY_AUTH_OPAQUE (rtsp_internal.h) was defined as "qop", identical to
 * RTSP_KEY_AUTH_QOP, instead of "opaque". Since process_authorization_key_val()
 * checks the OPAQUE key before the QOP key, every incoming "qop=..."
 * parameter was silently misrouted into auth->opaque, and the real qop
 * branch was unreachable dead code - a client could never learn the
 * server's negotiated qop from a WWW-Authenticate header. Fixed by
 * correcting the constant to "opaque".
 */
static void test_rtsp_header_authorization_qop_opaque_regression(void)
{
	struct rtsp_authorization_header *auth;
	char v[] = "Digest opaque=\"op-value\", qop=\"auth\"";
	int ret;

	ret = rtsp_authorization_header_read(v, &auth);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
	CU_ASSERT_STRING_EQUAL(auth->opaque, "op-value");
	CU_ASSERT_EQUAL(auth->qop, RTSP_AUTH_QOP_AUTH);
	rtsp_authorization_header_free(&auth);
}


/*
 * Regression test for a bug found while writing a Basic-auth loopback test:
 * rtsp_authorization_header_read()'s Basic branch unconditionally treated
 * the token right after "Basic " as base64 credentials - correct for a
 * request-side "Authorization: Basic <base64>" header, but wrong for a
 * response-side "WWW-Authenticate: Basic realm=\"...\"" challenge, whose
 * only parameter (RFC 2617 S11.1) is realm=, not raw credentials. A real
 * challenge therefore got its realm parameter silently misparsed into
 * auth->credentials as garbage, leaving auth->realm NULL and making
 * generate_authorization_header()'s `if (!auth->credentials)` guard skip
 * rtsp_auth_generate_basic_response() entirely on the next request - Basic
 * auth against a real server could never actually work. Fixed by
 * recognizing the "realm=" prefix (which base64 credentials can never
 * start with) and routing it through the same key/value parsing Digest
 * already used for its own realm parameter.
 */
static void
test_rtsp_header_authorization_basic_realm_challenge_regression(void)
{
	struct rtsp_authorization_header *auth;
	char v[] = "Basic realm=\"test-realm\"";
	int ret;

	ret = rtsp_authorization_header_read(v, &auth);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
	CU_ASSERT_EQUAL(auth->type, RTSP_AUTH_TYPE_BASIC);
	CU_ASSERT_STRING_EQUAL(auth->realm, "test-realm");
	CU_ASSERT_PTR_NULL(auth->credentials);
	rtsp_authorization_header_free(&auth);
}


/*
 * Regression test for three related NULL-pointer-dereference crash bugs
 * found while writing this suite, all in the Digest parameter parsing of
 * rtsp_authorization_header_read()/process_authorization_key_val() - i.e.
 * all reachable straight from parsing an incoming, attacker-controlled
 * Authorization/WWW-Authenticate header:
 *  1. rtsp_authorization_header_read() trimmed leading spaces off 'val'
 *     unconditionally ("while (*val == ' ') val++;") before ever checking
 *     it for NULL, even though 'val' is legitimately NULL for any
 *     bare/flag-style parameter with no '=' at all.
 *  2. The same function also dereferenced 'key' the same way before its
 *     own NULL check, reachable with a parameter that is only an '=' sign
 *     (a single ambiguous delimiter with no key or value on either side).
 *  3. process_authorization_key_val()'s 'nc' branch passed the
 *     (potentially NULL, for the same bare-parameter reason) stripped
 *     value straight into strtoul() with no NULL check.
 * Fixed by checking for NULL before dereferencing in all three spots; a
 * bare/malformed parameter is now silently ignored instead of crashing.
 */
static void
test_rtsp_header_authorization_malformed_param_no_crash_regression(void)
{
	struct rtsp_authorization_header *auth;
	int ret;

	/* Bare flag-style parameter (no '=' at all) */
	{
		char v[] = "Digest username=\"bob\", bogus, realm=\"x\"";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
		CU_ASSERT_STRING_EQUAL(auth->username, "bob");
		CU_ASSERT_STRING_EQUAL(auth->realm, "x");
		rtsp_authorization_header_free(&auth);
	}

	/* A parameter that is only an ambiguous '=' delimiter */
	{
		char v[] = "Digest username=\"bob\", =, realm=\"x\"";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
		CU_ASSERT_STRING_EQUAL(auth->username, "bob");
		CU_ASSERT_STRING_EQUAL(auth->realm, "x");
		rtsp_authorization_header_free(&auth);
	}

	/* Bare 'nc' flag with no value */
	{
		char v[] = "Digest username=\"bob\", nc";
		ret = rtsp_authorization_header_read(v, &auth);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(auth);
		CU_ASSERT_STRING_EQUAL(auth->username, "bob");
		CU_ASSERT_EQUAL(auth->nc, (unsigned int)0);
		rtsp_authorization_header_free(&auth);
	}
}


static void test_rtsp_header_authorization_round_trip(void)
{
	char buf[TEST_HDR_BUF_LEN];
	struct rtsp_string str;
	struct rtsp_authorization_header in;
	struct rtsp_authorization_header *out;
	int ret;

	/* Basic. algorithm/qop are set to UNSPECIFIED, not left at the
	 * zero-initialized UNKNOWN, to match what rtsp_authorization_header_
	 * new() (used internally by the reader) actually defaults them to -
	 * neither field is ever touched for a Basic-type header. */
	memset(&in, 0, sizeof(in));
	in.type = RTSP_AUTH_TYPE_BASIC;
	in.credentials = "dXNlcjpwYXNz";
	in.algorithm = RTSP_AUTH_ALGORITHM_UNSPECIFIED;
	in.qop = RTSP_AUTH_QOP_UNSPECIFIED;

	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_authorization_header_write(&in, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_authorization_header_read(
		header_value(buf, RTSP_HEADER_AUTHORIZATION), &out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(out);
	CU_ASSERT_TRUE(auth_eq(&in, out));
	rtsp_authorization_header_free(&out);

	/* Digest, every field set */
	memset(&in, 0, sizeof(in));
	in.type = RTSP_AUTH_TYPE_DIGEST;
	in.username = "bob";
	in.realm = "example.com";
	in.nonce = "abc123";
	in.uri = "rtsp://host/stream";
	in.response = "deadbeef";
	in.algorithm = RTSP_AUTH_ALGORITHM_MD5;
	in.opaque = "op4234";
	in.qop = RTSP_AUTH_QOP_AUTH;
	in.cnonce = "cn5678";
	in.nc = 1;

	str.len = 0;
	ret = rtsp_authorization_header_write(&in, &str);
	CU_ASSERT_EQUAL(ret, 0);

	ret = rtsp_authorization_header_read(
		header_value(buf, RTSP_HEADER_AUTHORIZATION), &out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(out);
	CU_ASSERT_TRUE(auth_eq(&in, out));
	rtsp_authorization_header_free(&out);
}


/* clang-format off */
CU_TestInfo g_rtsp_test_header[] = {
	{FN("rtsp-header-allow-write"), &test_rtsp_header_allow_write},
	{FN("rtsp-header-allow-read"), &test_rtsp_header_allow_read},
	{FN("rtsp-header-allow-round-trip"),
	 &test_rtsp_header_allow_round_trip},
	{FN("rtsp-header-public-write"), &test_rtsp_header_public_write},
	{FN("rtsp-header-public-round-trip"),
	 &test_rtsp_header_public_round_trip},
	{FN("rtsp-header-range-write"), &test_rtsp_header_range_write},
	{FN("rtsp-header-range-read"), &test_rtsp_header_range_read},
	{FN("rtsp-header-range-round-trip"),
	 &test_rtsp_header_range_round_trip},
	{FN("rtsp-header-session-write"), &test_rtsp_header_session_write},
	{FN("rtsp-header-session-read"), &test_rtsp_header_session_read},
	{FN("rtsp-header-session-round-trip"),
	 &test_rtsp_header_session_round_trip},
	{FN("rtsp-header-rtp-info-new-free"),
	 &test_rtsp_header_rtp_info_new_free},
	{FN("rtsp-header-rtp-info-copy"), &test_rtsp_header_rtp_info_copy},
	{FN("rtsp-header-rtp-info-write"), &test_rtsp_header_rtp_info_write},
	{FN("rtsp-header-rtp-info-read"), &test_rtsp_header_rtp_info_read},
	{FN("rtsp-header-rtp-info-multi-entry-regression"),
	 &test_rtsp_header_rtp_info_multi_entry_regression},
	{FN("rtsp-header-rtp-info-round-trip"),
	 &test_rtsp_header_rtp_info_round_trip},
	{FN("rtsp-header-transport-new-free-copy"),
	 &test_rtsp_header_transport_new_free_copy},
	{FN("rtsp-header-transport-write"),
	 &test_rtsp_header_transport_write},
	{FN("rtsp-header-transport-read"), &test_rtsp_header_transport_read},
	{FN("rtsp-header-transport-multi-entry-regression"),
	 &test_rtsp_header_transport_multi_entry_regression},
	{FN("rtsp-header-transport-round-trip"),
	 &test_rtsp_header_transport_round_trip},
	{FN("rtsp-header-authorization-new-free-copy"),
	 &test_rtsp_header_authorization_new_free_copy},
	{FN("rtsp-header-authorization-write"),
	 &test_rtsp_header_authorization_write},
	{FN("rtsp-header-authorization-read"),
	 &test_rtsp_header_authorization_read},
	{FN("rtsp-header-authorization-qop-opaque-regression"),
	 &test_rtsp_header_authorization_qop_opaque_regression},
	{FN("rtsp-header-authorization-basic-realm-challenge-regression"),
	 &test_rtsp_header_authorization_basic_realm_challenge_regression},
	{FN("rtsp-header-authorization-malformed-param-no-crash-regression"),
	 &test_rtsp_header_authorization_malformed_param_no_crash_regression},
	{FN("rtsp-header-authorization-round-trip"),
	 &test_rtsp_header_authorization_round_trip},

	CU_TEST_INFO_NULL,
};
/* clang-format on */
