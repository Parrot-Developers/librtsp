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


static void test_rtsp_base64_encode(void)
{
	int ret;
	char *out = NULL;
	uint8_t data[] = {0x41, 0x42, 0x43}; /* ABC */

	/* KO cases */
	/* Ensure that out is not modified in case of error */
	out = (char *)0xdeadbeef;
	ret = rtsp_base64_encode(NULL, 0, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	out = NULL;
	ret = rtsp_base64_encode(NULL, sizeof(data), &out);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(out);

	ret = rtsp_base64_encode(data, sizeof(data), NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_base64_encode(data, 0, &out);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_base64_encode(NULL, 10, &out);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_base64_encode(data, (size_t)-1, &out);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	struct base64_test_case {
		const uint8_t *in_data;
		size_t in_size;
		const char *expected_out;
	};

	/* OK cases */
	struct base64_test_case ok_cases[] = {
		{(uint8_t[]){0x41, 0x42, 0x43},
		 3,
		 "QUJD"}, /* ABC (no padding) */
		{(uint8_t[]){0x41, 0x42}, 2, "QUI="}, /* AB (1 = padding) */
		{(uint8_t[]){0x41}, 1, "QQ=="}, /* A (2 = padding) */
		{(uint8_t[]){0x00, 0x00, 0x00}, 3, "AAAA"}, /* Zeroes */
		{(uint8_t[]){0xff, 0xff, 0xff}, 3, "////"}, /* Max bits */
		{(uint8_t[]){0x00, 0x01, 0x02}, 3, "AAEC"},
		{(uint8_t[]){0x3f, 0x7f, 0xff}, 3, "P3//"},
		{(uint8_t[]){0x01, 0x02, 0x03, 0x04}, 4, "AQIDBA=="},
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(ok_cases); i++) {
		out = NULL;
		ret = rtsp_base64_encode(
			ok_cases[i].in_data, ok_cases[i].in_size, &out);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(out);
		if (out) {
			CU_ASSERT_STRING_EQUAL(out, ok_cases[i].expected_out);
			free(out);
		}
	}

	/* Test of max size (3072B) */
	size_t max_in = (MAX_RTSP_BASE64_LEN / 4) * 3;
	uint8_t *large_data = calloc(max_in, 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(large_data);
	memset(large_data, 0xAA, max_in);

	/* Should pass (exact limit) */
	out = NULL;
	ret = rtsp_base64_encode(large_data, max_in, &out);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL(out);
	if (out) {
		CU_ASSERT_EQUAL(strlen(out), MAX_RTSP_BASE64_LEN);
		free(out);
	}

	/* Should fail (limit + 1) */
	out = NULL;
	ret = rtsp_base64_encode(large_data, max_in + 1, &out);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(out);

	free(large_data);
}


static void test_rtsp_base64_decode(void)
{
	int ret = 0;
	void *out = NULL;
	size_t out_size = 0;
	const char *valid_str = "QUJD"; /* "ABC" */

	/* KO cases */
	/* Ensure that out is not modified in case of error */
	out = (void *)0xdeadbeef;
	ret = rtsp_base64_decode(NULL, &out, &out_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	out = NULL;
	ret = rtsp_base64_decode(valid_str, NULL, &out_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_base64_decode(valid_str, &out, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(out);

	ret = rtsp_base64_decode("", &out, &out_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(out);

	ret = rtsp_base64_decode("ABC", &out, &out_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Too long (> MAX_RTSP_BASE64_LEN) */
	char long_str[MAX_RTSP_BASE64_LEN + 5];
	memset(long_str, 'A', sizeof(long_str) - 1);
	long_str[sizeof(long_str) - 1] = '\0';
	ret = rtsp_base64_decode(long_str, &out, &out_size);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	struct base64_decode_ko_case {
		const char *in_str;
		int expected_ret;
	} ko_cases[] = {
		{"QU#D", -EINVAL}, /* Non-base64 character */
		{"A===", -EINVAL}, /* Too much padding (max 2) */
		{"QU=D", -EINVAL}, /* Padding in the middle */
		{"=QJD", -EINVAL}, /* Padding at the beginning */
		{"QUJD====", -EINVAL}, /* Multiple of 4 but invalid padding */
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(ko_cases); i++) {
		out = NULL;
		ret = rtsp_base64_decode(ko_cases[i].in_str, &out, &out_size);
		CU_ASSERT_EQUAL(ret, ko_cases[i].expected_ret);
		CU_ASSERT_PTR_NULL(out);
	}

	/* OK cases */
	struct base64_decode_ok_case {
		const char *in_str;
		const uint8_t *expected_data;
		size_t expected_len;
	} ok_cases[] = {
		{"QUJD", (uint8_t[]){0x41, 0x42, 0x43}, 3}, /* ABC */
		{"QUI=", (uint8_t[]){0x41, 0x42}, 2}, /* AB */
		{"QQ==", (uint8_t[]){0x41}, 1}, /* A */
		{"AAAA", (uint8_t[]){0x00, 0x00, 0x00}, 3}, /* Zeroes */
		{"////", (uint8_t[]){0xff, 0xff, 0xff}, 3}, /* Max bits */
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(ok_cases); i++) {
		out = NULL;
		out_size = 0;
		ret = rtsp_base64_decode(ok_cases[i].in_str, &out, &out_size);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_EQUAL(out_size, ok_cases[i].expected_len);
		CU_ASSERT_PTR_NOT_NULL(out);
		if (out) {
			CU_ASSERT_EQUAL(memcmp(out,
					       ok_cases[i].expected_data,
					       out_size),
					0);
			free(out);
		}
	}
}


CU_TestInfo g_rtsp_test_base64[] = {
	{FN("rtsp-base64-encode"), &test_rtsp_base64_encode},
	{FN("rtsp-base64-decode"), &test_rtsp_base64_decode},

	CU_TEST_INFO_NULL,
};
