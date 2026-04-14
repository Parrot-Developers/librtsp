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

#include "rtsp_test.h"

#include <rtsp/rtsp_url.hpp>


struct RtspUrlTestCase {
	std::string description;
	std::string srcUrl;
	bool expectedValid;
	bool hasAuth;
	enum rtsp_url_scheme expectedScheme;
	std::string expectedUrl;
	std::string expectedUrlWithAuth;
	std::string expectedHost;
	uint16_t expectedPort;
	std::string expectedStreamName;
	std::string expectedAddress;
	std::string expectedBaseUrl;
	std::string username;
	std::string password;

	RtspUrlTestCase(const std::string &descr,
			const std::string &src,
			bool valid,
			bool auth = false,
			enum rtsp_url_scheme scheme = RTSP_URL_SCHEME_UNKNOWN,
			const std::string &u = "",
			const std::string &uWithAuth = "",
			const std::string &host = "",
			uint16_t port = 0,
			const std::string &stream = "",
			const std::string &addr = "",
			const std::string &base = "",
			const std::string &user = "",
			const std::string &pass = "") :
			description(descr),
			srcUrl(src), expectedValid(valid), hasAuth(auth),
			expectedScheme(scheme), expectedUrl(u),
			expectedUrlWithAuth(uWithAuth), expectedHost(host),
			expectedPort(port), expectedStreamName(stream),
			expectedAddress(addr), expectedBaseUrl(base),
			username(user), password(pass)
	{
	}
};


constexpr size_t RTSP_TEST_CASES_COUNT = 15;

static const std::array<RtspUrlTestCase, RTSP_TEST_CASES_COUNT>
	gRtspUrlTestCases{
		{RtspUrlTestCase("EmptyUrl", "", false),
		 RtspUrlTestCase("InvalidProtocolUrl",
				 "http://wrong.com/stream",
				 false),
		 RtspUrlTestCase("ValidUrl",
				 "rtsp://192.168.53.1:5554/live-front",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://192.168.53.1:5554/live-front",
				 "rtsp://192.168.53.1:5554/live-front",
				 "192.168.53.1",
				 5554,
				 "live-front",
				 "192.168.53.1:5554",
				 "rtsp://192.168.53.1:5554"),
		 RtspUrlTestCase("ValidUrlWithPortAndSlash",
				 "rtsp://192.168.1.100:8554/mystream/live",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://192.168.1.100:8554/mystream/live",
				 "rtsp://192.168.1.100:8554/mystream/live",
				 "192.168.1.100",
				 8554,
				 "mystream/live",
				 "192.168.1.100:8554",
				 "rtsp://192.168.1.100:8554"),
		 RtspUrlTestCase("ValidUrlWithoutPort",
				 "rtsp://camera.example.com/ch1",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://camera.example.com/ch1",
				 "rtsp://camera.example.com/ch1",
				 "camera.example.com",
				 554,
				 "ch1",
				 "camera.example.com:554",
				 "rtsp://camera.example.com"),
		 RtspUrlTestCase("ValidUrlWithStandardPort",
				 "rtsp://host.net:554/path",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://host.net:554/path",
				 "rtsp://host.net:554/path",
				 "host.net",
				 554,
				 "path",
				 "host.net:554",
				 "rtsp://host.net:554"),
		 /* RTSP user/pass cases */
		 RtspUrlTestCase("ValidUrlWithoutPortAndUserPass",
				 "rtsp://user:pass@host.net/path",
				 true,
				 true,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://host.net/path",
				 "rtsp://user:pass@host.net/path",
				 "host.net",
				 554,
				 "path",
				 "host.net:554",
				 "rtsp://host.net",
				 "user",
				 "pass"),
		 RtspUrlTestCase("ValidUrlWithStandardPortAndUserPass",
				 "rtsp://user:pass@host.net:554/path",
				 true,
				 true,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://host.net:554/path",
				 "rtsp://user:pass@host.net:554/path",
				 "host.net",
				 554,
				 "path",
				 "host.net:554",
				 "rtsp://host.net:554",
				 "user",
				 "pass"),
		 RtspUrlTestCase("ValidUrlWithCustomPortAndUserPass",
				 "rtsp://user:pass@host.net:5555/path",
				 true,
				 true,
				 RTSP_URL_SCHEME_TCP,
				 "rtsp://host.net:5555/path",
				 "rtsp://user:pass@host.net:5555/path",
				 "host.net",
				 5555,
				 "path",
				 "host.net:5555",
				 "rtsp://host.net:5555",
				 "user",
				 "pass"),
		 /* RTSPS cases (with user/pass) */
		 RtspUrlTestCase("ValidUrlWithoutPort",
				 "rtsps://camera.example.com/ch1",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP_TLS,
				 "rtsps://camera.example.com/ch1",
				 "rtsps://camera.example.com/ch1",
				 "camera.example.com",
				 322,
				 "ch1",
				 "camera.example.com:322",
				 "rtsps://camera.example.com"),
		 RtspUrlTestCase("ValidUrlWithStandardPort",
				 "rtsps://host.net:322/path",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP_TLS,
				 "rtsps://host.net:322/path",
				 "rtsps://host.net:322/path",
				 "host.net",
				 322,
				 "path",
				 "host.net:322",
				 "rtsps://host.net:322"),
		 RtspUrlTestCase("ValidUrlWithPortAndSlash",
				 "rtsps://192.168.1.100:8554/mystream/live",
				 true,
				 false,
				 RTSP_URL_SCHEME_TCP_TLS,
				 "rtsps://192.168.1.100:8554/mystream/live",
				 "rtsps://192.168.1.100:8554/mystream/live",
				 "192.168.1.100",
				 8554,
				 "mystream/live",
				 "192.168.1.100:8554",
				 "rtsps://192.168.1.100:8554"),
		 RtspUrlTestCase("ValidUrlWithoutPortAndUserPass",
				 "rtsps://user:pass@host.net/path",
				 true,
				 true,
				 RTSP_URL_SCHEME_TCP_TLS,
				 "rtsps://host.net/path",
				 "rtsps://user:pass@host.net/path",
				 "host.net",
				 322,
				 "path",
				 "host.net:322",
				 "rtsps://host.net",
				 "user",
				 "pass"),
		 RtspUrlTestCase("ValidUrlWithStandardPortAndUserPass",
				 "rtsps://user:pass@host.net:322/path",
				 true,
				 true,
				 RTSP_URL_SCHEME_TCP_TLS,
				 "rtsps://host.net:322/path",
				 "rtsps://user:pass@host.net:322/path",
				 "host.net",
				 322,
				 "path",
				 "host.net:322",
				 "rtsps://host.net:322",
				 "user",
				 "pass"),
		 RtspUrlTestCase("ValidUrlWithCustomPortAndUserPass",
				 "rtsps://user:pass@host.net:5555/path",
				 true,
				 true,
				 RTSP_URL_SCHEME_TCP_TLS,
				 "rtsps://host.net:5555/path",
				 "rtsps://user:pass@host.net:5555/path",
				 "host.net",
				 5555,
				 "path",
				 "host.net:5555",
				 "rtsps://host.net:5555",
				 "user",
				 "pass")}};


#define ASSERT_STR_EQ(getter, expected)                                        \
	do {                                                                   \
		int _ok = strcmp((getter).c_str(), (expected).c_str()) == 0;   \
		if (!_ok) {                                                    \
			printf("ASSERT_STR_EQ FAILED:\n");                     \
			if (!_ok)                                              \
				printf("  got: '%s' != expected: '%s'\n",      \
				       (getter).c_str(),                       \
				       (expected).c_str());                    \
		}                                                              \
		CU_ASSERT_STRING_EQUAL((getter).c_str(), (expected).c_str());  \
	} while (0)


static void testRtspUrl(void)
{
	for (const auto &testCase : gRtspUrlTestCases) {
		auto url = RtspUrl::create(testCase.srcUrl);
		if (testCase.expectedValid) {
			CU_ASSERT_PTR_NOT_NULL_FATAL(url.get());
		} else {
			CU_ASSERT_PTR_NULL(url.get());
			continue;
		}

		const std::string &expectedAuth =
			testCase.hasAuth
				? (testCase.expectedUrlWithAuth.empty()
					   ? testCase.expectedUrl
					   : testCase.expectedUrlWithAuth)
				: testCase.expectedUrl;
		ASSERT_STR_EQ(url->getUrl(), testCase.expectedUrl);
		ASSERT_STR_EQ(url->getUrlWithAuth(), expectedAuth);
		ASSERT_STR_EQ(url->getHost(), testCase.expectedHost);
		ASSERT_STR_EQ(url->getStreamName(),
			      testCase.expectedStreamName);
		ASSERT_STR_EQ(url->getAddr(), testCase.expectedAddress);
		ASSERT_STR_EQ(url->getBaseUrl(), testCase.expectedBaseUrl);
		ASSERT_STR_EQ(url->getUser(), testCase.username);
		ASSERT_STR_EQ(url->getPass(), testCase.password);

		CU_ASSERT_EQUAL(url->getScheme(), testCase.expectedScheme);
		CU_ASSERT_EQUAL(url->getPort(), testCase.expectedPort);
	}
}


CU_TestInfo g_rtsp_test_url_cpp[] = {
	{FN("rtsp-url-cpp"), &testRtspUrl},

	CU_TEST_INFO_NULL,
};
