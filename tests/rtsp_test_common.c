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


/* X-macro over every RTSP status defined in rtsp_internal.h, so the table
 * below stays in sync with the header instead of duplicating literal
 * codes/strings. */
/* clang-format off */
#define RTSP_TEST_STATUS_CASES(_case)                                          \
	_case(CONTINUE)                                                        \
	_case(OK)                                                              \
	_case(CREATED)                                                         \
	_case(LOW_ON_STORAGE)                                                  \
	_case(MULTIPLE_CHOICES)                                                \
	_case(MOVED_PERMANENTLY)                                               \
	_case(MOVED_TEMPORARITY)                                               \
	_case(SEE_OTHER)                                                       \
	_case(NOT_MODIFIED)                                                    \
	_case(USE_PROXY)                                                       \
	_case(BAD_REQUEST)                                                     \
	_case(UNAUTHORIZED)                                                    \
	_case(PAYMENT_REQUIRED)                                                \
	_case(FORBIDDEN)                                                       \
	_case(NOT_FOUND)                                                       \
	_case(METHOD_NOT_ALLOWED)                                              \
	_case(NOT_ACCEPTABLE)                                                  \
	_case(PROXY_AUTHENTICATION_REQUIRED)                                   \
	_case(REQUEST_TIMEOUT)                                                 \
	_case(GONE)                                                            \
	_case(LENGTH_REQUIRED)                                                 \
	_case(PRECONDITION_FAILED)                                             \
	_case(REQUEST_ENTITY_TOO_LARGE)                                        \
	_case(REQUEST_URI_TOO_LARGE)                                           \
	_case(UNSUPPORTED_MEDIA_TYPE)                                          \
	_case(PARMETER_NOT_UNDERSTOOD)                                         \
	_case(CONFERENCE_NOT_FOUND)                                            \
	_case(NOT_ENOUGH_BANDWIDTH)                                            \
	_case(SESSION_NOT_FOUND)                                               \
	_case(METHOD_NOT_VALID)                                                \
	_case(HEADER_FIELD_NOT_VALID)                                         \
	_case(INVALID_RANGE)                                                   \
	_case(PARAMETER_READ_ONLY)                                            \
	_case(AGGREGATE_OPERATION_NOT_ALLOWED)                                 \
	_case(ONLY_AGGREGATE_OPERATION_ALLOWED)                                \
	_case(UNSUPPORTED_TRANSPORT)                                           \
	_case(DESTINATION_UNREACHABLE)                                         \
	_case(INTERNAL_SERVER_ERROR)                                          \
	_case(NOT_IMPLEMENTED)                                                 \
	_case(BAD_GATEWAY)                                                     \
	_case(SERVICE_UNAVAILABLE)                                            \
	_case(GATEWAY_TIMEOUT)                                                 \
	_case(RTSP_VERSION_NOT_SUPPORTED)                                      \
	_case(OPTION_NOT_SUPPORTED)
/* clang-format on */


static void test_rtsp_common_method_type_str(void)
{
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_OPTIONS),
			       "OPTIONS");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_DESCRIBE),
			       "DESCRIBE");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_ANNOUNCE),
			       "ANNOUNCE");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_SETUP),
			       "SETUP");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_PLAY),
			       "PLAY");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_PAUSE),
			       "PAUSE");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_TEARDOWN),
			       "TEARDOWN");
	CU_ASSERT_STRING_EQUAL(
		rtsp_method_type_str(RTSP_METHOD_TYPE_GET_PARAMETER),
		"GET_PARAMETER");
	CU_ASSERT_STRING_EQUAL(
		rtsp_method_type_str(RTSP_METHOD_TYPE_SET_PARAMETER),
		"SET_PARAMETER");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_REDIRECT),
			       "REDIRECT");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_RECORD),
			       "RECORD");
	CU_ASSERT_STRING_EQUAL(rtsp_method_type_str(RTSP_METHOD_TYPE_UNKNOWN),
			       "UNKNOWN");
	CU_ASSERT_STRING_EQUAL(
		rtsp_method_type_str((enum rtsp_method_type)9999), "UNKNOWN");
}


static void test_rtsp_common_delivery_str(void)
{
	CU_ASSERT_STRING_EQUAL(rtsp_delivery_str(RTSP_DELIVERY_MULTICAST),
			       "MULTICAST");
	CU_ASSERT_STRING_EQUAL(rtsp_delivery_str(RTSP_DELIVERY_UNICAST),
			       "UNICAST");
	CU_ASSERT_STRING_EQUAL(rtsp_delivery_str((enum rtsp_delivery)9999),
			       "UNKNOWN");
}


static void test_rtsp_common_lower_transport_str(void)
{
	CU_ASSERT_STRING_EQUAL(
		rtsp_lower_transport_str(RTSP_LOWER_TRANSPORT_UDP), "UDP");
	CU_ASSERT_STRING_EQUAL(
		rtsp_lower_transport_str(RTSP_LOWER_TRANSPORT_TCP), "TCP");
	CU_ASSERT_STRING_EQUAL(
		rtsp_lower_transport_str(RTSP_LOWER_TRANSPORT_MUX), "MUX");
	CU_ASSERT_STRING_EQUAL(
		rtsp_lower_transport_str((enum rtsp_lower_transport)9999),
		"UNKNOWN");
}


static void test_rtsp_common_time_format_str(void)
{
	CU_ASSERT_STRING_EQUAL(rtsp_time_format_str(RTSP_TIME_FORMAT_UNKNOWN),
			       "UNKNOWN");
	CU_ASSERT_STRING_EQUAL(rtsp_time_format_str(RTSP_TIME_FORMAT_NPT),
			       "NPT");
	CU_ASSERT_STRING_EQUAL(rtsp_time_format_str(RTSP_TIME_FORMAT_SMPTE),
			       "SMPTE");
	CU_ASSERT_STRING_EQUAL(rtsp_time_format_str(RTSP_TIME_FORMAT_ABSOLUTE),
			       "ABSOLUTE");
	CU_ASSERT_STRING_EQUAL(
		rtsp_time_format_str((enum rtsp_time_format)9999), "UNKNOWN");
}


static void test_rtsp_common_client_conn_state_str(void)
{
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_conn_state_str(RTSP_CLIENT_CONN_STATE_DISCONNECTED),
		"DISCONNECTED");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_conn_state_str(RTSP_CLIENT_CONN_STATE_CONNECTING),
		"CONNECTING");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_conn_state_str(RTSP_CLIENT_CONN_STATE_CONNECTED),
		"CONNECTED");
	CU_ASSERT_STRING_EQUAL(rtsp_client_conn_state_str(
				       RTSP_CLIENT_CONN_STATE_DISCONNECTING),
			       "DISCONNECTING");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_conn_state_str((enum rtsp_client_conn_state)9999),
		"UNKNOWN");
}


static void test_rtsp_common_client_req_status_str(void)
{
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_req_status_str(RTSP_CLIENT_REQ_STATUS_OK), "OK");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_req_status_str(RTSP_CLIENT_REQ_STATUS_CANCELED),
		"CANCELED");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_req_status_str(RTSP_CLIENT_REQ_STATUS_FAILED),
		"FAILED");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_req_status_str(RTSP_CLIENT_REQ_STATUS_ABORTED),
		"ABORTED");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_req_status_str(RTSP_CLIENT_REQ_STATUS_TIMEOUT),
		"TIMEOUT");
	CU_ASSERT_STRING_EQUAL(
		rtsp_client_req_status_str((enum rtsp_client_req_status)9999),
		"UNKNOWN");
}


static void test_rtsp_common_server_teardown_reason_str(void)
{
	CU_ASSERT_STRING_EQUAL(
		rtsp_server_teardown_reason_str(
			RTSP_SERVER_TEARDOWN_REASON_CLIENT_REQUEST),
		"CLIENT_REQUEST");
	CU_ASSERT_STRING_EQUAL(
		rtsp_server_teardown_reason_str(
			RTSP_SERVER_TEARDOWN_REASON_SESSION_TIMEOUT),
		"SESSION_TIMEOUT");
	CU_ASSERT_STRING_EQUAL(
		rtsp_server_teardown_reason_str(
			RTSP_SERVER_TEARDOWN_REASON_FORCED_TEARDOWN),
		"FORCED_TEARDOWN");
	CU_ASSERT_STRING_EQUAL(rtsp_server_teardown_reason_str(
				       (enum rtsp_server_teardown_reason)9999),
			       "UNKNOWN");
}


static void test_rtsp_common_status_get(void)
{
	int code;
	const char *str;

	struct status_case {
		int status_code;
		const char *status_string;
	};

	/* clang-format off */
	struct status_case cases[] = {
#define STATUS_CASE(_name) {RTSP_STATUS_CODE(_name), RTSP_STATUS_STRING(_name)},
		RTSP_TEST_STATUS_CASES(STATUS_CASE)
#undef STATUS_CASE
	};
	/* clang-format on */

	/* KO cases: NULL out-params must leave nothing touched */
	code = 999;
	str = "unchanged";
	rtsp_status_get(RTSP_STATUS_CODE(OK), NULL, &str);
	CU_ASSERT_STRING_EQUAL(str, "unchanged");
	rtsp_status_get(RTSP_STATUS_CODE(OK), &code, NULL);
	CU_ASSERT_EQUAL(code, 999);

	/* OK cases: every documented status code round-trips to itself */
	for (size_t i = 0; i < SIZEOF_ARRAY(cases); i++) {
		code = 0;
		str = NULL;
		rtsp_status_get(cases[i].status_code, &code, &str);
		CU_ASSERT_EQUAL(code, cases[i].status_code);
		CU_ASSERT_PTR_NOT_NULL(str);
		if (str)
			CU_ASSERT_STRING_EQUAL(str, cases[i].status_string);
	}

	/* OK cases: negative errno values map to a status code/string */
	struct errno_case {
		int err;
		int expected_code;
	};

	/* clang-format off */
	struct errno_case errno_cases[] = {
		{0, RTSP_STATUS_CODE(OK)},
		{-EPROTO, RTSP_STATUS_CODE(BAD_REQUEST)},
		{-EPERM, RTSP_STATUS_CODE(UNAUTHORIZED)},
		{-EACCES, RTSP_STATUS_CODE(FORBIDDEN)},
		{-ENOENT, RTSP_STATUS_CODE(NOT_FOUND)},
		{-ETIMEDOUT, RTSP_STATUS_CODE(REQUEST_TIMEOUT)},
		{-ENOSYS, RTSP_STATUS_CODE(NOT_IMPLEMENTED)},
		{-EBUSY, RTSP_STATUS_CODE(SERVICE_UNAVAILABLE)},
		{-EAGAIN, RTSP_STATUS_CODE(SERVICE_UNAVAILABLE)},
		{-EPROTONOSUPPORT, RTSP_STATUS_CODE(UNSUPPORTED_TRANSPORT)},
		{-EINVAL, RTSP_STATUS_CODE(INTERNAL_SERVER_ERROR)},
	};
	/* clang-format on */

	for (size_t i = 0; i < SIZEOF_ARRAY(errno_cases); i++) {
		code = 0;
		str = NULL;
		rtsp_status_get(errno_cases[i].err, &code, &str);
		CU_ASSERT_EQUAL(code, errno_cases[i].expected_code);
		CU_ASSERT_PTR_NOT_NULL(str);
	}
}


static void test_rtsp_common_status_to_errno(void)
{
	struct to_errno_case {
		int status_code;
		int expected_err;
	};

	/* clang-format off */
	struct to_errno_case cases[] = {
		{RTSP_STATUS_CODE(OK), 0},
		{RTSP_STATUS_CODE(BAD_REQUEST), -EPROTO},
		{RTSP_STATUS_CODE(UNAUTHORIZED), -EPERM},
		{RTSP_STATUS_CODE(FORBIDDEN), -EACCES},
		{RTSP_STATUS_CODE(NOT_FOUND), -ENOENT},
		{RTSP_STATUS_CODE(REQUEST_TIMEOUT), -ETIMEDOUT},
		{RTSP_STATUS_CODE(NOT_IMPLEMENTED), -ENOSYS},
		{RTSP_STATUS_CODE(SERVICE_UNAVAILABLE), -EBUSY},
		{RTSP_STATUS_CODE(UNSUPPORTED_TRANSPORT), -EPROTONOSUPPORT},
		/* Not part of the explicit mapping -> default */
		{RTSP_STATUS_CODE(CREATED), -EPROTO},
		{RTSP_STATUS_CODE(GATEWAY_TIMEOUT), -EPROTO},
	};
	/* clang-format on */

	for (size_t i = 0; i < SIZEOF_ARRAY(cases); i++) {
		CU_ASSERT_EQUAL(rtsp_status_to_errno(cases[i].status_code),
				cases[i].expected_err);
	}

	/* Negative values already are errno values: passed through as-is */
	CU_ASSERT_EQUAL(rtsp_status_to_errno(-EINVAL), -EINVAL);
	CU_ASSERT_EQUAL(rtsp_status_to_errno(-1234), -1234);
}


static void test_rtsp_common_status_str(void)
{
	CU_ASSERT_STRING_EQUAL(rtsp_status_str(RTSP_STATUS_CODE(OK)), "OK");
	CU_ASSERT_STRING_EQUAL(rtsp_status_str(RTSP_STATUS_CODE(NOT_FOUND)),
			       "Not Found");
	CU_ASSERT_STRING_EQUAL(rtsp_status_str(-ENOENT), "Not Found");
	CU_ASSERT_STRING_EQUAL(rtsp_status_str(-EPERM), "Unauthorized");
}


static void test_rtsp_common_range_get_duration_us(void)
{
	int ret;
	int64_t duration;
	struct rtsp_range range;

	memset(&range, 0, sizeof(range));
	range.start.format = RTSP_TIME_FORMAT_NPT;
	range.stop.format = RTSP_TIME_FORMAT_NPT;

	/* KO cases */
	ret = rtsp_range_get_duration_us(NULL, &duration);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_range_get_duration_us(&range, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Non-NPT format is not supported */
	range.start.format = RTSP_TIME_FORMAT_SMPTE;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	range.start.format = RTSP_TIME_FORMAT_NPT;

	range.stop.format = RTSP_TIME_FORMAT_ABSOLUTE;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	range.stop.format = RTSP_TIME_FORMAT_NPT;

	/* 'now' can't be used to compute a duration */
	range.start.npt.now = 1;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	range.start.npt.now = 0;

	range.stop.npt.now = 1;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	range.stop.npt.now = 0;

	/* OK cases */
	range.stop.npt.infinity = 1;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(duration, INT64_MAX);
	range.stop.npt.infinity = 0;

	range.start.npt.infinity = 1;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(duration, INT64_MIN);
	range.start.npt.infinity = 0;

	range.start.npt.sec = 1;
	range.start.npt.usec = 500000;
	range.stop.npt.sec = 3;
	range.stop.npt.usec = 250000;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(duration, INT64_C(1750000));

	/* A range where stop precedes start yields a negative duration */
	range.start.npt.sec = 5;
	range.start.npt.usec = 0;
	range.stop.npt.sec = 2;
	range.stop.npt.usec = 0;
	ret = rtsp_range_get_duration_us(&range, &duration);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(duration, INT64_C(-3000000));
}


static void test_rtsp_common_range_cmp(void)
{
	struct rtsp_range range1, range2;

	memset(&range1, 0, sizeof(range1));
	memset(&range2, 0, sizeof(range2));

	/* NULL handling */
	CU_ASSERT_TRUE(rtsp_range_cmp(NULL, NULL));
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, NULL));
	CU_ASSERT_FALSE(rtsp_range_cmp(NULL, &range2));

	/* Two zeroed ranges are equal */
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));

	/* Different formats */
	range1.start.format = RTSP_TIME_FORMAT_NPT;
	range2.start.format = RTSP_TIME_FORMAT_SMPTE;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
	range2.start.format = RTSP_TIME_FORMAT_NPT;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));

	/* NPT fields */
	range1.start.npt.sec = 1;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
	range2.start.npt.sec = 1;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));

	range1.start.npt.usec = 42;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
	range2.start.npt.usec = 42;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));

	range1.start.npt.now = 1;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
	range2.start.npt.now = 1;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));

	range1.start.npt.infinity = 1;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
	range2.start.npt.infinity = 1;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));

	/* SMPTE fields */
	memset(&range1, 0, sizeof(range1));
	memset(&range2, 0, sizeof(range2));
	range1.stop.format = RTSP_TIME_FORMAT_SMPTE;
	range2.stop.format = RTSP_TIME_FORMAT_SMPTE;
	range1.stop.smpte.sec = 10;
	range1.stop.smpte.frames = 5;
	range2.stop.smpte.sec = 10;
	range2.stop.smpte.frames = 5;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));
	range2.stop.smpte.frames = 6;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
	range2.stop.smpte.frames = 5;
	range2.stop.smpte.infinity = 1;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));

	/* Absolute fields */
	memset(&range1, 0, sizeof(range1));
	memset(&range2, 0, sizeof(range2));
	range1.stop.format = RTSP_TIME_FORMAT_ABSOLUTE;
	range2.stop.format = RTSP_TIME_FORMAT_ABSOLUTE;
	range1.stop.absolute.sec = 100;
	range1.stop.absolute.usec = 1;
	range2.stop.absolute.sec = 100;
	range2.stop.absolute.usec = 1;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));
	range2.stop.absolute.usec = 2;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));

	/* The 'time' field (NTP wall-clock reference) is compared too */
	memset(&range1, 0, sizeof(range1));
	memset(&range2, 0, sizeof(range2));
	range1.time = 111;
	range2.time = 111;
	CU_ASSERT_TRUE(rtsp_range_cmp(&range1, &range2));
	range2.time = 222;
	CU_ASSERT_FALSE(rtsp_range_cmp(&range1, &range2));
}


static void test_rtsp_common_time_us_to_npt(void)
{
	int ret;
	struct rtsp_time_npt npt;

	ret = rtsp_time_us_to_npt(1000, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Stale flags must be cleared unconditionally */
	npt.now = 1;
	npt.infinity = 1;

	ret = rtsp_time_us_to_npt(0, &npt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(npt.now, 0);
	CU_ASSERT_EQUAL(npt.infinity, 0);
	CU_ASSERT_EQUAL(npt.sec, (uint64_t)0);
	CU_ASSERT_EQUAL(npt.usec, (uint32_t)0);

	ret = rtsp_time_us_to_npt(1234567, &npt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(npt.sec, (uint64_t)1);
	CU_ASSERT_EQUAL(npt.usec, (uint32_t)234567);

	ret = rtsp_time_us_to_npt(999999, &npt);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(npt.sec, (uint64_t)0);
	CU_ASSERT_EQUAL(npt.usec, (uint32_t)999999);
}


static void test_rtsp_common_time_npt_to_us(void)
{
	int ret;
	uint64_t us;
	struct rtsp_time_npt npt = {
		.now = 0, .infinity = 0, .sec = 1, .usec = 234567};

	ret = rtsp_time_npt_to_us(NULL, &us);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = rtsp_time_npt_to_us(&npt, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	npt.now = 1;
	ret = rtsp_time_npt_to_us(&npt, &us);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	npt.now = 0;

	npt.infinity = 1;
	ret = rtsp_time_npt_to_us(&npt, &us);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	npt.infinity = 0;

	ret = rtsp_time_npt_to_us(&npt, &us);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(us, (uint64_t)1234567);
}


static void test_rtsp_common_time_npt_us_round_trip(void)
{
	static const uint64_t values_us[] = {
		0, 1, 999999, 1000000, 1000001, INT64_C(123456789)};

	for (size_t i = 0; i < SIZEOF_ARRAY(values_us); i++) {
		int ret;
		struct rtsp_time_npt npt;
		uint64_t us_out;

		ret = rtsp_time_us_to_npt(values_us[i], &npt);
		CU_ASSERT_EQUAL(ret, 0);
		ret = rtsp_time_npt_to_us(&npt, &us_out);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(us_out, values_us[i]);
	}
}


static void test_rtsp_common_sprintf(void)
{
	char buf[16];
	struct rtsp_string str;
	int ret;

	/* KO: buffer already full */
	str.str = buf;
	str.len = sizeof(buf);
	str.max_len = sizeof(buf);
	ret = rtsp_sprintf(&str, "%s", "x");
	CU_ASSERT_EQUAL(ret, -ENOBUFS);
	CU_ASSERT_EQUAL(str.len, sizeof(buf));

	/* KO: formatted output does not fit in the remaining space */
	str.str = buf;
	str.len = 0;
	str.max_len = 4;
	ret = rtsp_sprintf(&str, "%s", "abcde");
	CU_ASSERT_EQUAL(ret, -ENOBUFS);
	CU_ASSERT_EQUAL(str.len, (size_t)0);

	/* OK: single write */
	str.str = buf;
	str.len = 0;
	str.max_len = sizeof(buf);
	ret = rtsp_sprintf(&str, "%s", "abc");
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(str.len, (size_t)3);
	CU_ASSERT_STRING_EQUAL(buf, "abc");

	/* OK: a second write appends after the first */
	ret = rtsp_sprintf(&str, "%d", 123);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_EQUAL(str.len, (size_t)6);
	CU_ASSERT_STRING_EQUAL(buf, "abc123");
}


static void test_rtsp_common_xfree(void)
{
	void *ptr = malloc(4);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);

	xfree(&ptr);
	CU_ASSERT_PTR_NULL(ptr);

	/* Freeing an already-NULL slot, and a NULL slot pointer, must not
	 * crash */
	xfree(&ptr);
	xfree(NULL);
}


static void test_rtsp_common_xstrdup(void)
{
	char *s;

	CU_ASSERT_PTR_NULL(xstrdup(NULL));

	s = xstrdup("hello");
	CU_ASSERT_PTR_NOT_NULL_FATAL(s);
	CU_ASSERT_STRING_EQUAL(s, "hello");
	free(s);
}


static void test_rtsp_common_dup_field(void)
{
	char *dst = xstrdup("old");
	CU_ASSERT_PTR_NOT_NULL_FATAL(dst);

	dup_field(&dst, "new");
	CU_ASSERT_PTR_NOT_NULL_FATAL(dst);
	CU_ASSERT_STRING_EQUAL(dst, "new");

	dup_field(&dst, NULL);
	CU_ASSERT_PTR_NULL(dst);
}


static void test_rtsp_common_get_last_char(void)
{
	CU_ASSERT_EQUAL(get_last_char(NULL, 10), '\0');
	CU_ASSERT_EQUAL(get_last_char("", 10), '\0');
	CU_ASSERT_EQUAL(get_last_char("abc", 10), 'c');
	CU_ASSERT_EQUAL(get_last_char("abc", 4), 'c');

	/* When the string is exactly as long as max_len, strnlen() reports
	 * len == max_len and the function treats that as "unterminated
	 * within bounds" -> '\0', not the last character. */
	CU_ASSERT_EQUAL(get_last_char("abc", 3), '\0');
	CU_ASSERT_EQUAL(get_last_char("abc", 2), '\0');
	CU_ASSERT_EQUAL(get_last_char("abc", 1), '\0');
}


/* clang-format off */
CU_TestInfo g_rtsp_test_common[] = {
	{FN("rtsp-common-method-type-str"),
	 &test_rtsp_common_method_type_str},
	{FN("rtsp-common-delivery-str"), &test_rtsp_common_delivery_str},
	{FN("rtsp-common-lower-transport-str"),
	 &test_rtsp_common_lower_transport_str},
	{FN("rtsp-common-time-format-str"),
	 &test_rtsp_common_time_format_str},
	{FN("rtsp-common-client-conn-state-str"),
	 &test_rtsp_common_client_conn_state_str},
	{FN("rtsp-common-client-req-status-str"),
	 &test_rtsp_common_client_req_status_str},
	{FN("rtsp-common-server-teardown-reason-str"),
	 &test_rtsp_common_server_teardown_reason_str},
	{FN("rtsp-common-status-get"), &test_rtsp_common_status_get},
	{FN("rtsp-common-status-to-errno"),
	 &test_rtsp_common_status_to_errno},
	{FN("rtsp-common-status-str"), &test_rtsp_common_status_str},
	{FN("rtsp-common-range-get-duration-us"),
	 &test_rtsp_common_range_get_duration_us},
	{FN("rtsp-common-range-cmp"), &test_rtsp_common_range_cmp},
	{FN("rtsp-common-time-us-to-npt"),
	 &test_rtsp_common_time_us_to_npt},
	{FN("rtsp-common-time-npt-to-us"),
	 &test_rtsp_common_time_npt_to_us},
	{FN("rtsp-common-time-npt-us-round-trip"),
	 &test_rtsp_common_time_npt_us_round_trip},
	{FN("rtsp-common-sprintf"), &test_rtsp_common_sprintf},
	{FN("rtsp-common-xfree"), &test_rtsp_common_xfree},
	{FN("rtsp-common-xstrdup"), &test_rtsp_common_xstrdup},
	{FN("rtsp-common-dup-field"), &test_rtsp_common_dup_field},
	{FN("rtsp-common-get-last-char"), &test_rtsp_common_get_last_char},

	CU_TEST_INFO_NULL,
};
/* clang-format on */
