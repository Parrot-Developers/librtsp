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


static void test_rtsp_auth_type_from_to_str(void)
{
	const char *res_str;

	/* Invalid cases (to_str) */
	res_str = rtsp_auth_type_str(RTSP_AUTH_TYPE_UNKNOWN);
	CU_ASSERT_PTR_NULL(res_str);
	res_str = rtsp_auth_type_str((enum rtsp_auth_type)999);
	CU_ASSERT_PTR_NULL(res_str);

	/* Invalid cases (from_str) */
	const char *bad_strings[] = {"Unknown", "", "  ", "Basic ", NULL};
	for (size_t i = 0; i < SIZEOF_ARRAY(bad_strings); i++) {
		enum rtsp_auth_type res_type;

		res_type = rtsp_auth_type_from_str(bad_strings[i]);
		CU_ASSERT_EQUAL(res_type, RTSP_AUTH_TYPE_UNKNOWN);
	}

	/* OK cases map */
	struct {
		enum rtsp_auth_type type;
		const char *expected_str;
		const char *input_alt;
	} ok_cases[] = {
		{RTSP_AUTH_TYPE_BASIC, "Basic", "basic"},
		{RTSP_AUTH_TYPE_DIGEST, "Digest", "DIGEST"},
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(ok_cases); i++) {

		enum rtsp_auth_type res_type;

		res_type = rtsp_auth_type_from_str(ok_cases[i].expected_str);
		CU_ASSERT_EQUAL(res_type, ok_cases[i].type);

		res_type = rtsp_auth_type_from_str(ok_cases[i].input_alt);
		CU_ASSERT_EQUAL(res_type, ok_cases[i].type);

		res_str = rtsp_auth_type_str(ok_cases[i].type);
		CU_ASSERT_PTR_NOT_NULL_FATAL(res_str);
		CU_ASSERT_STRING_EQUAL(res_str, ok_cases[i].expected_str);
	}
}


static void test_rtsp_auth_algorithm_from_to_str(void)
{
	const char *res_str;

	/* Invalid cases (to_str) */
	res_str = rtsp_auth_algorithm_str(RTSP_AUTH_ALGORITHM_UNSPECIFIED);
	CU_ASSERT_PTR_NULL(res_str);

	res_str = rtsp_auth_algorithm_str((enum rtsp_auth_algorithm)999);
	CU_ASSERT_PTR_NULL(res_str);

	/* Invalid cases (from_str, shall return UNSPECIFIED) */
	const char *unspecified_strings[] = {NULL, ""};
	for (size_t i = 0; i < SIZEOF_ARRAY(unspecified_strings); i++) {
		enum rtsp_auth_algorithm res_algo;
		res_algo = rtsp_auth_algorithm_from_str(unspecified_strings[i]);
		CU_ASSERT_EQUAL(res_algo, RTSP_AUTH_ALGORITHM_UNSPECIFIED);
	}

	/* Unknown cases, shall return UNKNOWN */
	const char *unknown_strings[] = {"SHA-256", "MD4", "  ", "MD5 "};
	for (size_t i = 0; i < SIZEOF_ARRAY(unknown_strings); i++) {
		enum rtsp_auth_algorithm res_algo;
		res_algo = rtsp_auth_algorithm_from_str(unknown_strings[i]);
		CU_ASSERT_EQUAL(res_algo, RTSP_AUTH_ALGORITHM_UNKNOWN);
	}

	/* OK cases map */
	struct {
		enum rtsp_auth_algorithm algo;
		const char *expected_str;
		const char *input_alt;
	} ok_cases[] = {
		{RTSP_AUTH_ALGORITHM_MD5, "MD5", "md5"},
		{RTSP_AUTH_ALGORITHM_MD5_SESS, "MD5-sess", "MD5-SESS"},
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(ok_cases); i++) {
		enum rtsp_auth_algorithm res_algo;

		res_algo =
			rtsp_auth_algorithm_from_str(ok_cases[i].expected_str);
		CU_ASSERT_EQUAL(res_algo, ok_cases[i].algo);

		res_algo = rtsp_auth_algorithm_from_str(ok_cases[i].input_alt);
		CU_ASSERT_EQUAL(res_algo, ok_cases[i].algo);

		res_str = rtsp_auth_algorithm_str(ok_cases[i].algo);
		CU_ASSERT_PTR_NOT_NULL_FATAL(res_str);
		CU_ASSERT_STRING_EQUAL(res_str, ok_cases[i].expected_str);
	}
}


static void test_rtsp_auth_qop_from_to_str(void)
{
	const char *res_str;
	enum rtsp_auth_qop res_qop;

	/* Invalid cases (to_str) */
	res_str = rtsp_auth_qop_str(RTSP_AUTH_QOP_UNSPECIFIED);
	CU_ASSERT_PTR_NULL(res_str);

	res_str = rtsp_auth_qop_str((enum rtsp_auth_qop)999);
	CU_ASSERT_PTR_NULL(res_str);

	/* Invalid cases (from_str, shall return UNSPECIFIED) */
	const char *unspecified_strings[] = {NULL, ""};
	for (size_t i = 0; i < SIZEOF_ARRAY(unspecified_strings); i++) {
		res_qop = rtsp_auth_qop_from_str(unspecified_strings[i]);
		CU_ASSERT_EQUAL(res_qop, RTSP_AUTH_QOP_UNSPECIFIED);
	}

	/* Unknown cases, shall return UNKNOWN */
	const char *unknown_strings[] = {"auth-conf", "none", "unknown"};
	for (size_t i = 0; i < SIZEOF_ARRAY(unknown_strings); i++) {
		res_qop = rtsp_auth_qop_from_str(unknown_strings[i]);
		CU_ASSERT_EQUAL(res_qop, RTSP_AUTH_QOP_UNKNOWN);
	}

	/* OK cases map */
	struct {
		enum rtsp_auth_qop qop;
		const char *expected_str;
		const char *input_alt;
	} ok_cases[] = {
		{RTSP_AUTH_QOP_AUTH, "auth", "AUTH"},
		{RTSP_AUTH_QOP_AUTH_INT, "auth-int", "Auth-Int"},
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(ok_cases); i++) {
		res_qop = rtsp_auth_qop_from_str(ok_cases[i].expected_str);
		CU_ASSERT_EQUAL(res_qop, ok_cases[i].qop);

		res_qop = rtsp_auth_qop_from_str(ok_cases[i].input_alt);
		CU_ASSERT_EQUAL(res_qop, ok_cases[i].qop);

		res_str = rtsp_auth_qop_str(ok_cases[i].qop);
		CU_ASSERT_PTR_NOT_NULL_FATAL(res_str);
		CU_ASSERT_STRING_EQUAL(res_str, ok_cases[i].expected_str);
	}
}


static void test_rtsp_auth_nc_str(void)
{
	int ret;
	char buffer[16];

	/* Invalid args */
	ret = rtsp_auth_nc_str(NULL, 0, 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	ret = rtsp_auth_nc_str(NULL, sizeof(buffer), 0);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	struct nc_test_case {
		unsigned int nc;
		const char *expected;
		size_t buf_len;
		int expected_ret;
	};

	struct nc_test_case cases[] = {
		/* KO cases */
		{1, "", 0, -EINVAL},
		{1, "", 8, -EINVAL}, /* Too small for \0 */

		/* OK cases */
		{0, "00000000", 9, 0},
		{1, "00000001", 16, 0},
		{15, "0000000f", 16, 0},
		{0xABCDEF, "00abcdef", 16, 0}, /* Partial padding */
		{0xFFFFFFFF, "ffffffff", 9, 0}, /*  Max unsigned int */
	};

	for (size_t i = 0; i < SIZEOF_ARRAY(cases); i++) {
		memset(buffer, 'X', sizeof(buffer));

		ret = rtsp_auth_nc_str(buffer, cases[i].buf_len, cases[i].nc);
		CU_ASSERT_EQUAL(ret, cases[i].expected_ret);

		if (cases[i].expected_ret == 0) {
			CU_ASSERT_STRING_EQUAL(buffer, cases[i].expected);
			CU_ASSERT_EQUAL(buffer[8], '\0');
		}
	}
}


static void test_rtsp_auth_generate_basic_response(void)
{
	int ret;
	struct rtsp_authorization_header auth;

	const struct basic_auth_test_case {
		const char *user;
		const char *pass;
		const char *expected_creds;
	} cases[] = {
		{"admin", "admin", "YWRtaW46YWRtaW4="},
		{"guest", "12345", "Z3Vlc3Q6MTIzNDU="},
		{"", "onlypass", "Om9ubHlwYXNz"},
		{"onlyuser", "", "b25seXVzZXI6"},
		{"foo:bar",
		 "baz",
		 "Zm9vOmJhcjpiYXo="}, /* ':' separator in user */
	};

	/* KO cases */
	ret = rtsp_auth_generate_basic_response(NULL, "password");
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* User NULL */
	memset(&auth, 0, sizeof(auth));
	auth.type = RTSP_AUTH_TYPE_BASIC;
	ret = rtsp_auth_generate_basic_response(&auth, "password");
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK cases */
	for (size_t i = 0; i < SIZEOF_ARRAY(cases); i++) {
		memset(&auth, 0, sizeof(auth));
		auth.type = RTSP_AUTH_TYPE_BASIC;
		auth.username = (char *)cases[i].user;
		auth.credentials = NULL;

		ret = rtsp_auth_generate_basic_response(&auth, cases[i].pass);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(auth.credentials);

		if (auth.credentials) {
			CU_ASSERT_STRING_EQUAL(auth.credentials,
					       cases[i].expected_creds);
			free(auth.credentials);
		}
	}
}


static void test_rtsp_auth_generate_digest_response(void)
{
	int ret;
	struct rtsp_authorization_header auth;

	struct digest_ko_test_case {
		struct rtsp_authorization_header auth;
		const char *pass;
		enum rtsp_method_type method;
		int expected_ret;
	} ko_cases[] = {
		{
			/* Too long nonce (>256) */
			.auth =
				{
					.type = RTSP_AUTH_TYPE_DIGEST,
					.username = "a",
					.realm = "a",
					.uri = "a",
					.nonce = "some_very_very_very_long_"
						 "_nonce_longer_than_256_chars",
				},
			.pass = "admin",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.expected_ret = -EINVAL,
		},
		{
			/* Too short cnonce (<4) */
			.auth =
				{
					.type = RTSP_AUTH_TYPE_DIGEST,
					.username = "a",
					.realm = "a",
					.uri = "a",
					.nonce = "nonce_ok",
					.cnonce = "123",
				},
			.pass = "admin",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.expected_ret = -EINVAL,
		},
		{
			/* Unknown algorithm */
			.auth =
				{
					.type = RTSP_AUTH_TYPE_DIGEST,
					.username = "a",
					.realm = "a",
					.uri = "a",
					.nonce = "nonce_ok",
					.algorithm =
						RTSP_AUTH_ALGORITHM_UNKNOWN,
				},
			.pass = "admin",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.expected_ret = -EINVAL,
		},
		{
			/* Unsupported QOP (AUTH_INT) */
			.auth =
				{
					.type = RTSP_AUTH_TYPE_DIGEST,
					.username = "a",
					.realm = "a",
					.uri = "a",
					.nonce = "nonce_ok",
					.qop = RTSP_AUTH_QOP_AUTH_INT,
					.algorithm = RTSP_AUTH_ALGORITHM_MD5,
				},
			.pass = "admin",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.expected_ret = -ENOSYS,
		},
		{
			/* Missing URI */
			.auth =
				{
					.type = RTSP_AUTH_TYPE_DIGEST,
					.username = "admin",
					.realm = "RTSP",
					.nonce = "ok",
					.uri = NULL,
				},
			.pass = "admin",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.expected_ret = -EINVAL,
		},
	};

	/* KO cases */
	for (size_t i = 0; i < SIZEOF_ARRAY(ko_cases); i++) {
		ret = rtsp_auth_generate_digest_response(&ko_cases[i].auth,
							 ko_cases[i].pass,
							 ko_cases[i].method);
		CU_ASSERT_EQUAL(ret, ko_cases[i].expected_ret);
		CU_ASSERT_PTR_NULL(ko_cases[i].auth.response);
	}

	const struct digest_test_case {
		const char *user;
		const char *pass;
		const char *realm;
		const char *nonce;
		const char *uri;
		enum rtsp_method_type method;
		enum rtsp_auth_algorithm algo;
		enum rtsp_auth_qop qop;
		const char *cnonce;
		unsigned int nc_in;
		const char *expected;
	} cases[] = {
		{
			/* With qop="auth" */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "368bc09da99e8a4c217e6b2a59f80213",
		},
		{
			/* Without QOP (RFC 2069 style) */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_UNSPECIFIED,
			.cnonce = "AAAAAAAA",
			.nc_in = 0,
			.expected = "1e1aa851256fbc54f57063683c74cf2a",
		},
		{
			/* Without QOP (RFC 2069 style) with other cnone */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_UNSPECIFIED,
			.cnonce = "BBBBBBBB",
			.nc_in = 0,
			.expected = "1e1aa851256fbc54f57063683c74cf2a",
		},
		{
			/* MD5-sess algorithm (with qop="auth") */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5_SESS,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "b6054310704dd8117eec2373931b9efa",
		},
		{
			/* Pass containing special chars */
			.user = "admin",
			.pass = "p@ss:w0rd",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "5c23ef6a",
			.nc_in = 0,
			.expected = "a557f25a053a962515531cba8a280def",
		},
		{
			/* Very long URL (query params) */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test?stream=1&codec=h264&res=1080p&auth=true",
			.method = RTSP_METHOD_TYPE_SETUP,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 5,
			.expected = "2c5a1891e279de1165000b69d552bd70",
		},
		{
			/* MD5-sess WITHOUT QOP (Rare case)
			 * HA1 uses cnonce, but response does NOT use cnonce or
			 * nc. Changing cnonce SHOULD change the result here.
			 */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5_SESS,
			.qop = RTSP_AUTH_QOP_UNSPECIFIED,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "045e41ee24bbdb3cd539e69bd1de8de0",
		},
		{
			/* Empty password case
			 * Should not crash and produce a valid hash. */
			.user = "admin",
			.pass = "",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "ef7e8d8ac45b474fde027adb474eb5d5",
		},
		{
			/* High NC count (Testing hex format %08x)
			 * nc_in = 254 -> nc becomes 255 (000000ff) */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 254,
			.expected = "a19b41bf62f641020aae8a3fd8fb2df8",
		},
		{
			/* Nonce has changed */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "7df922e86448376f9d2737609201a45",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "870a85065a7fa9ac995acab6efe57b96",
		},
		{
			/* Short nonce (on some servers) */
			.user = "admin",
			.pass = "admin",
			.realm = "RTSP",
			.nonce = "short_nonce",
			.uri = "rtsp://127.0.0.1/test",
			.method = RTSP_METHOD_TYPE_DESCRIBE,
			.algo = RTSP_AUTH_ALGORITHM_MD5,
			.qop = RTSP_AUTH_QOP_AUTH,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "c39516ee34c357d42b2f57aebe94cdc6",
		},
		{
			/* Wowza-like (SETUP) */
			.user = "client123456",
			.pass = "0a4f113b",
			.realm = "Streaming Server",
			.nonce = "4f8a2b19e7c30d56f9a81b724d6e5302",
			.uri = "rtsp://a12b3c4d5e6f.entrypoint.cloud.wowza.com:"
			       "8080/app-z9y87Xw1/f1234a56",
			.method = RTSP_METHOD_TYPE_SETUP,
			.algo = RTSP_AUTH_ALGORITHM_UNSPECIFIED,
			.qop = RTSP_AUTH_QOP_UNSPECIFIED,
			.cnonce = "0a4f113b",
			.nc_in = 0,
			.expected = "866e4a4d2a5f2f47b2a21bfb853f670e",
		},
		{
			/* Wowza-like (RECORD) */
			.user = "client123456",
			.pass = "0a4f113b",
			.realm = "Streaming Server",
			.nonce = "4f8a2b19e7c30d56f9a81b724d6e5302",
			.uri = "rtsp://a12b3c4d5e6f.entrypoint.cloud.wowza.com:"
			       "8080/app-z9y87Xw1/f1234a56",
			.method = RTSP_METHOD_TYPE_RECORD,
			.algo = RTSP_AUTH_ALGORITHM_UNSPECIFIED,
			.qop = RTSP_AUTH_QOP_UNSPECIFIED,
			.cnonce = "0a4f113b",
			.nc_in = 1,
			.expected = "b2513b49fccc6b817b1486475af5e9a3",
		},
	};

	/* KO cases */
	ret = rtsp_auth_generate_digest_response(
		NULL, "pass", RTSP_METHOD_TYPE_DESCRIBE);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* OK cases */
	for (size_t i = 0; i < SIZEOF_ARRAY(cases); i++) {
		memset(&auth, 0, sizeof(auth));
		auth.type = RTSP_AUTH_TYPE_DIGEST;
		auth.algorithm = cases[i].algo;
		auth.qop = cases[i].qop;
		auth.username = strdup(cases[i].user);
		auth.realm = strdup(cases[i].realm);
		auth.nonce = strdup(cases[i].nonce);
		auth.uri = strdup(cases[i].uri);

		if (cases[i].cnonce)
			auth.cnonce = strdup(cases[i].cnonce);
		auth.nc = cases[i].nc_in;

		ret = rtsp_auth_generate_digest_response(
			&auth, cases[i].pass, cases[i].method);

		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(auth.response);
		if (auth.response) {
			printf("got: %s, expected: %s\n",
			       auth.response,
			       cases[i].expected);

			CU_ASSERT_STRING_EQUAL(auth.response,
					       cases[i].expected);
		}

		free(auth.username);
		free(auth.realm);
		free(auth.nonce);
		free(auth.uri);
		free(auth.cnonce);
		free(auth.response);
	}
}


CU_TestInfo g_rtsp_test_auth[] = {
	{FN("rtsp-auth-type-from-to-str"), &test_rtsp_auth_type_from_to_str},
	{FN("rtsp-auth-algorithm-from-to-str"),
	 &test_rtsp_auth_algorithm_from_to_str},
	{FN("rtsp-auth-qop-from-to-str"), &test_rtsp_auth_qop_from_to_str},
	{FN("rtsp-auth-nc-str"), &test_rtsp_auth_nc_str},
	{FN("rtsp-auth-generate-basic-response"),
	 &test_rtsp_auth_generate_basic_response},
	{FN("rtsp-auth-generate-digest-response"),
	 &test_rtsp_auth_generate_digest_response},

	CU_TEST_INFO_NULL,
};
