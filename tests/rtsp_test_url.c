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


#define SAMPLE_RTSP_URL "rtsp://192.168.43.1/live"


#define ASSERT_STR_EQ(got, expected)                                           \
	do {                                                                   \
		int _ok = strcmp(got, expected) == 0;                          \
		if (!_ok) {                                                    \
			printf("ASSERT_STR_EQ FAILED:\n");                     \
			if (!_ok)                                              \
				printf("  got: '%s' != expected: '%s'\n",      \
				       got,                                    \
				       expected);                              \
		}                                                              \
		CU_ASSERT_STRING_EQUAL(got, expected);                         \
	} while (0)


#define MAKE_URL(_url,                                                         \
		 _valid,                                                       \
		 _scheme,                                                      \
		 _host,                                                        \
		 _resolved_host,                                               \
		 _port,                                                        \
		 _port_explicit,                                               \
		 _path,                                                        \
		 _user,                                                        \
		 _pass)                                                        \
	{                                                                      \
		_url, _valid, _scheme, _host, _resolved_host, _port,           \
			_port_explicit, _path, _user, _pass                    \
	}

#define MAKE_OK_URL_WITH_USER_PASS(_url,                                       \
				   _scheme,                                    \
				   _host,                                      \
				   _resolved_host,                             \
				   _port,                                      \
				   _port_explicit,                             \
				   _path,                                      \
				   _user,                                      \
				   _pass)                                      \
	MAKE_URL(_url,                                                         \
		 true,                                                         \
		 _scheme,                                                      \
		 _host,                                                        \
		 _resolved_host,                                               \
		 _port,                                                        \
		 _port_explicit,                                               \
		 _path,                                                        \
		 _user,                                                        \
		 _pass)

#define MAKE_OK_URL(                                                           \
	_url, _scheme, _host, _resolved_host, _port, _port_explicit, _path)    \
	MAKE_URL(_url,                                                         \
		 true,                                                         \
		 _scheme,                                                      \
		 _host,                                                        \
		 _resolved_host,                                               \
		 _port,                                                        \
		 _port_explicit,                                               \
		 _path,                                                        \
		 NULL,                                                         \
		 NULL)

#define MAKE_KO_URL(_url)                                                      \
	MAKE_URL(_url,                                                         \
		 RTSP_URL_SCHEME_UNKNOWN,                                      \
		 false,                                                        \
		 NULL,                                                         \
		 NULL,                                                         \
		 0,                                                            \
		 false,                                                        \
		 NULL,                                                         \
		 NULL,                                                         \
		 NULL)


struct rtsp_url_params {
	const char *url;
	bool valid;
	enum rtsp_url_scheme scheme;
	const char *host;
	const char *resolved_host;
	uint16_t port;
	bool port_explicit;
	const char *path;
	const char *user;
	const char *pass;
};


static const struct rtsp_url_params test_cases_parse_ko_map[] = {
	/* Invalid scheme */
	MAKE_KO_URL("ftp://host/stream"),
	MAKE_KO_URL("http://host/stream"),

	/* Missing host */
	MAKE_KO_URL("rtsp:///stream"),

	/* Port without host */
	MAKE_KO_URL("rtsp://:554/stream"),
	MAKE_KO_URL("rtsp://:8554/"),
	MAKE_KO_URL("rtsps://:443/stream"),

	/* Invalid port */
	MAKE_KO_URL("rtsp://host:-1/stream"),
	MAKE_KO_URL("rtsp://host:70000/stream"),
	MAKE_KO_URL("rtsp://host:abc/stream"),
	MAKE_KO_URL("rtsps://host:-1/stream"),
	MAKE_KO_URL("rtsps://host:70000/stream"),
	MAKE_KO_URL("rtsps://host:abc/stream"),

	/* Invalid characters in path */
	MAKE_KO_URL("rtsp://host/stream^01"),
	MAKE_KO_URL("rtsp://host/stream space"),
	MAKE_KO_URL("rtsps://host/stream^01"),
	MAKE_KO_URL("rtsps://host/stream space"),
};


/* OK URLs */
static const struct rtsp_url_params test_cases_parse_ok_map[] = {
	/**
	 * RTSP
	 */
	MAKE_OK_URL("rtsp://192.168.53.1:5554/live",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.53.1",
		    "192.168.53.1",
		    5554,
		    true,
		    "live"),
	MAKE_OK_URL("rtsp://192.168.53.1:5555/live-front-airsdk",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.53.1",
		    "192.168.53.1",
		    5555,
		    true,
		    "live-front-airsdk"),
	MAKE_OK_URL("rtsp://192.168.1.100:8554/mystream/live",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.1.100",
		    "192.168.1.100",
		    8554,
		    true,
		    "mystream/live"),
	MAKE_OK_URL("rtsp://192.168.1.100:8554/mystream/live/",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.1.100",
		    "192.168.1.100",
		    8554,
		    true,
		    "mystream/live"),

	/* IPv6 host */
	MAKE_OK_URL("rtsp://[2001:db8::1]:554/mystream",
		    RTSP_URL_SCHEME_TCP,
		    "2001:db8::1",
		    "2001:db8::1",
		    554,
		    true,
		    "mystream"),
	MAKE_OK_URL("rtsp://[2001:db8::1]:5554/mystream",
		    RTSP_URL_SCHEME_TCP,
		    "2001:db8::1",
		    "2001:db8::1",
		    5554,
		    true,
		    "mystream"),

	/* Hostname simple path */
	MAKE_OK_URL("rtsp://example.com/live",
		    RTSP_URL_SCHEME_TCP,
		    "example.com",
		    NULL,
		    554,
		    false,
		    "live"),
	MAKE_OK_URL("rtsp://192.168.1.100:554/",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.1.100",
		    "192.168.1.100",
		    554,
		    true,
		    NULL),
	MAKE_OK_URL("rtsp://example.com",
		    RTSP_URL_SCHEME_TCP,
		    "example.com",
		    NULL,
		    554,
		    false,
		    NULL),

	/* Multi-segment path */
	MAKE_OK_URL("rtsp://server.local:1935/app1/streamA/track1",
		    RTSP_URL_SCHEME_TCP,
		    "server.local",
		    NULL,
		    1935,
		    true,
		    "app1/streamA/track1"),

	/* Path with valid special characters */
	MAKE_OK_URL("rtsp://server/app-01/stream_01!$&'()*+,;=:@",
		    RTSP_URL_SCHEME_TCP,
		    "server",
		    NULL,
		    554,
		    false,
		    "app-01/stream_01!$&'()*+,;=:@"),

	/* YouTube / Wowza / alias cases */
	MAKE_OK_URL("rtsp://192.168.43.1/live-front",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.43.1",
		    "192.168.43.1",
		    554,
		    false,
		    "live-front"),
	MAKE_OK_URL("rtsp://192.168.43.1/live-front/",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.43.1",
		    "192.168.43.1",
		    554,
		    false,
		    "live-front"),
	MAKE_OK_URL("rtsp://192.168.43.1:5554/live",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.43.1",
		    "192.168.43.1",
		    5554,
		    true,
		    "live"),
	MAKE_OK_URL("rtsp://192.168.43.1:5554/live-front",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.43.1",
		    "192.168.43.1",
		    5554,
		    true,
		    "live-front"),
	MAKE_OK_URL("rtsp://a.rtsp.youtube.com/"
		    "live2-AaBb-CcDd-EeFf-GgHh-IiJj",
		    RTSP_URL_SCHEME_TCP,
		    "a.rtsp.youtube.com",
		    NULL,
		    554,
		    false,
		    "live2-AaBb-CcDd-EeFf-GgHh-IiJj"),
	MAKE_OK_URL("rtsp://a.rtsp.youtube.com:1935/"
		    "live2-AaBb-CcDd-EeFf-GgHh-IiJj",
		    RTSP_URL_SCHEME_TCP,
		    "a.rtsp.youtube.com",
		    NULL,
		    1935,
		    true,
		    "live2-AaBb-CcDd-EeFf-GgHh-IiJj"),
	MAKE_OK_URL("rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com/"
		    "app-AaBbCcDd-EeFfGgHh",
		    RTSP_URL_SCHEME_TCP,
		    "AaBbCcDdEeFf.entrypoint.cloud.wowza.com",
		    NULL,
		    554,
		    false,
		    "app-AaBbCcDd-EeFfGgHh"),
	MAKE_OK_URL("rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com:1935/"
		    "app-AaBbCcDd-EeFfGgHh",
		    RTSP_URL_SCHEME_TCP,
		    "AaBbCcDdEeFf.entrypoint.cloud.wowza.com",
		    NULL,
		    1935,
		    true,
		    "app-AaBbCcDd-EeFfGgHh"),
	MAKE_OK_URL("rtsp://AaBbCcDdEeFf.wowza.com/app-AaBbCcDd-EeFfGgHh",
		    RTSP_URL_SCHEME_TCP,
		    "AaBbCcDdEeFf.wowza.com",
		    NULL,
		    554,
		    false,
		    "app-AaBbCcDd-EeFfGgHh"),
	MAKE_OK_URL("rtsp://192.168.1.10:554/"
		    "live2-AaBb-CcDd-EeFf-GgHh-IiJj",
		    RTSP_URL_SCHEME_TCP,
		    "192.168.1.10",
		    "192.168.1.10",
		    554,
		    true,
		    "live2-AaBb-CcDd-EeFf-GgHh-IiJj"),
	MAKE_OK_URL("rtsp://camera.local:8554/stream-AaBbCcDdEeFf",
		    RTSP_URL_SCHEME_TCP,
		    "camera.local",
		    NULL,
		    8554,
		    true,
		    "stream-AaBbCcDdEeFf"),
	MAKE_OK_URL("rtsp://media.server/live2-ABC123",
		    RTSP_URL_SCHEME_TCP,
		    "media.server",
		    NULL,
		    554,
		    false,
		    "live2-ABC123"),

	/**
	 * RTSP with user/pass
	 */
	MAKE_OK_URL_WITH_USER_PASS("rtsp://user@192.168.1.100:554/live",
				   RTSP_URL_SCHEME_TCP,
				   "192.168.1.100",
				   "192.168.1.100",
				   554,
				   true,
				   "live",
				   "user",
				   NULL),
	MAKE_OK_URL_WITH_USER_PASS(
		"rtsp://user:pass@192.168.1.101:8554/mystream",
		RTSP_URL_SCHEME_TCP,
		"192.168.1.101",
		"192.168.1.101",
		8554,
		true,
		"mystream",
		"user",
		"pass"),
	MAKE_OK_URL_WITH_USER_PASS("rtsp://alice:secret@camera.local/stream01",
				   RTSP_URL_SCHEME_TCP,
				   "camera.local",
				   NULL,
				   554,
				   false,
				   "stream01",
				   "alice",
				   "secret"),
	MAKE_OK_URL_WITH_USER_PASS(
		"rtsp://bob@host.example.com:1935/app1/trackA",
		RTSP_URL_SCHEME_TCP,
		"host.example.com",
		NULL,
		1935,
		true,
		"app1/trackA",
		"bob",
		NULL),

	/**
	 * RTSPS
	 */
	MAKE_OK_URL("rtsps://secure.server.com",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "secure.server.com",
		    NULL,
		    322,
		    false,
		    NULL),
	MAKE_OK_URL("rtsps://secure.server.com/",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "secure.server.com",
		    NULL,
		    322,
		    false,
		    NULL),
	MAKE_OK_URL("rtsps://secure.server.com:443/live",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "secure.server.com",
		    NULL,
		    443,
		    true,
		    "live"),
	MAKE_OK_URL("rtsps://secure.server.com:443/live-front",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "secure.server.com",
		    NULL,
		    443,
		    true,
		    "live-front"),
	MAKE_OK_URL("rtsps://192.168.1.50:322/mystream",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "192.168.1.50",
		    "192.168.1.50",
		    322,
		    true,
		    "mystream"),
	MAKE_OK_URL("rtsps://[2001:db8::2]/live",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "2001:db8::2",
		    "2001:db8::2",
		    322,
		    false,
		    "live"),
	MAKE_OK_URL("rtsps://secure.server.com/app-01/"
		    "stream_01!$&'()*+,;=:@",
		    RTSP_URL_SCHEME_TCP_TLS,
		    "secure.server.com",
		    NULL,
		    322,
		    false,
		    "app-01/stream_01!$&'()*+,;=:@"),

	/**
	 * RTSPS with user/pass
	 */
	MAKE_OK_URL_WITH_USER_PASS(
		"rtsps://secureuser:securepass@secure.server.com/live",
		RTSP_URL_SCHEME_TCP_TLS,
		"secure.server.com",
		NULL,
		322,
		false,
		"live",
		"secureuser",
		"securepass"),
	MAKE_OK_URL_WITH_USER_PASS("rtsps://user@192.168.1.50:322/mystream",
				   RTSP_URL_SCHEME_TCP_TLS,
				   "192.168.1.50",
				   "192.168.1.50",
				   322,
				   true,
				   "mystream",
				   "user",
				   NULL),
	MAKE_OK_URL_WITH_USER_PASS(
		"rtsps://admin:admin123@[2001:db8::2]:322/live",
		RTSP_URL_SCHEME_TCP_TLS,
		"2001:db8::2",
		"2001:db8::2",
		322,
		true,
		"live",
		"admin",
		"admin123"),
};


struct rtsp_url_cmp_params {
	const char *url1;
	const char *url2;
	bool identical;
};


static const struct rtsp_url_cmp_params test_cases_cmp_map[] = {
	/**
	 * Identical
	 */
	{"rtsp://192.168.43.1", "rtsp://192.168.43.1", true},
	{"rtsps://192.168.43.1", "rtsps://192.168.43.1", true},
	{"rtsp://192.168.43.1:554", "rtsp://192.168.43.1:554", true},
	{"rtsp://192.168.43.1:554/live", "rtsp://192.168.43.1:554/live", true},
	{"rtsp://192.168.43.1:554/live", "rtsp://192.168.43.1:554/live/", true},
	{"rtsp://192.168.43.1:554/live",
	 "rtsp://192.168.43.1:554/live/////",
	 true},
	{"rtsp://192.168.43.1:554/live-front",
	 "rtsp://192.168.43.1:554/live-front",
	 true},
	{"rtsp://192.168.43.1:554/live-front",
	 "rtsp://192.168.43.1:554/live-front",
	 true},
	{"rtsps://192.168.43.1:554/live-front",
	 "rtsps://192.168.43.1:554/live-front",
	 true},
	{"rtsp://localhost/live-front", "rtsp://localhost/live-front", true},
	{"rtsp://user:pass@localhost/live-front",
	 "rtsp://user:pass@localhost/live-front",
	 true},
	{"rtsps://user:pass@localhost/live-front",
	 "rtsps://user:pass@localhost/live-front",
	 true},

	/*
	 * Not identical
	 */
	/* Scheme */
	{"rtsp://192.168.43.1", "rtsps://192.168.43.2", false},
	/* Host */
	{"rtsp://192.168.43.1", "rtsp://192.168.43.2", false},
	/* Port */
	{"rtsp://192.168.43.1:554", "rtsp://192.168.43.1:555", false},
	{"rtsp://192.168.43.1:5554", "rtsp://192.168.43.1:5555", false},
	/* Port explicit */
	{"rtsp://192.168.43.1:554", "rtsp://192.168.43.1", false},
	/* Path */
	{"rtsp://192.168.43.1:554/live",
	 "rtsp://192.168.43.1:554/live2",
	 false},
	{"rtsp://192.168.43.1:554/live/front",
	 "rtsp://192.168.43.1:554/live-front",
	 false},
	/* Credentials */
	{"rtsp://user:pass@localhost/live-front",
	 "rtsp://user:pass2@localhost/live-front",
	 false},
	{"rtsp://user2:pass@localhost/live-front",
	 "rtsp://user:pass@localhost/live-front",
	 false},
	{"rtsp://user:pass@localhost/live-front",
	 "rtsp://user:@localhost/live-front",
	 false},
	{"rtsp://user:pass@localhost/live-front",
	 "rtsp://localhost/live-front",
	 false},
};


struct rtsp_url_str_map {
	const char *url;
	const char *stripped;
	const char *anon;
};


static const struct rtsp_url_str_map test_cases_str_ko_map[] = {
	/* Missing RTSP scheme */
	{"a.rtsp.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj", NULL, NULL},

	/* Invalid scheme: HTTP */
	{"http://a.rtsp.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 NULL,
	 NULL},

	/* Invalid scheme: FTP */
	{"ftp://a.rtsp.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj", NULL, NULL},

	/* Incomplete address */
	{"rtsp://", NULL, NULL},

	/* Typo in scheme */
	{"rstp://a.rtsp.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 NULL,
	 NULL},

	/* User/pass but host missing */
	{"rtsp://user:pass@/stream", NULL, NULL},
	{"rtsp://admin:@/live", NULL, NULL},

	/* Invalid port with user/pass */
	{"rtsp://user:pass@host:-1/stream", NULL, NULL},
	{"rtsp://admin:pass@host:70000/live", NULL, NULL},
	{"rtsp://admin:pass@host:abc/stream", NULL, NULL},

	/* Invalid characters in host with user/pass */
	{"rtsp://user:pass@ho^st/live", NULL, NULL},
	{"rtsp://admin:pass@ser ver/live", NULL, NULL},

	/* Malformed IPv6 with user/pass */
	{"rtsp://user:pass@[2001:db8::1/stream", NULL, NULL},
	{"rtsps://admin:pass@[2001:db8::g]/live", NULL, NULL},
};


static const struct rtsp_url_str_map test_cases_str_ok_map[] = {
	/* IP without path */
	{"rtsp://192.168.43.1", "rtsp://192.168.43.1", "rtsp://192.168.43.1"},

	/* IP without path with '/' (will be stripped) */
	{"rtsp://192.168.43.1/", "rtsp://192.168.43.1", "rtsp://192.168.43.1"},

	/* IP without path + port */
	{"rtsp://192.168.43.1:5554",
	 "rtsp://192.168.43.1:5554",
	 "rtsp://192.168.43.1:5554"},

	/* IP without path + port with '/' (will be stripped) */
	{"rtsp://192.168.43.1:5554/",
	 "rtsp://192.168.43.1:5554",
	 "rtsp://192.168.43.1:5554"},

	/* IP */
	{"rtsp://192.168.43.1/live",
	 "rtsp://192.168.43.1/live",
	 "rtsp://192.168.43.1/live"},

	/* Alias */
	{"rtsp://192.168.43.1/live-front",
	 "rtsp://192.168.43.1/live-front",
	 "rtsp://192.168.43.1/li******nt"},

	/* Alias with '/' (will be stripped) */
	{"rtsp://192.168.43.1/live-front/",
	 "rtsp://192.168.43.1/live-front",
	 "rtsp://192.168.43.1/li******nt"},

	/* IP + port */
	{"rtsp://192.168.43.1:5554/live",
	 "rtsp://192.168.43.1:5554/live",
	 "rtsp://192.168.43.1:5554/live"},

	/* Alias + port */
	{"rtsp://192.168.43.1:5554/live-front",
	 "rtsp://192.168.43.1:5554/live-front",
	 "rtsp://192.168.43.1:5554/li******nt"},

	/* YouTube Live */
	{"rtsp://a.rtsp.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsp://a.rtsp.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsp://a.rtsp.youtube.com/li**************************Jj"},

	/* YouTube Live (+ port) */
	{"rtsp://a.rtsp.youtube.com:1935/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsp://a.rtsp.youtube.com:1935/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsp://a.rtsp.youtube.com:1935/li**************************Jj"},

	/* YouTube Live (RTSPS, port 443) */
	{"rtsps://a.rtsps.youtube.com:443/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsps://a.rtsps.youtube.com:443/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsps://a.rtsps.youtube.com:443/li**************************Jj"},

	/* YouTube Live (RTSPS, no port) */
	{"rtsps://a.rtsps.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsps://a.rtsps.youtube.com/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsps://a.rtsps.youtube.com/li**************************Jj"},

	/* Wowza */
	{"rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com/"
	 "app-AaBbCcDd-EeFfGgHh",
	 "rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com/"
	 "app-AaBbCcDd-EeFfGgHh",
	 "rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com/"
	 "ap*****************Hh"},

	/* Wowza (+ port) */
	{"rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com:1935/"
	 "app-AaBbCcDd-EeFfGgHh",
	 "rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com:1935/"
	 "app-AaBbCcDd-EeFfGgHh",
	 "rtsp://AaBbCcDdEeFf.entrypoint.cloud.wowza.com:1935/"
	 "ap*****************Hh"},

	/* Wowza (simplified domain) */
	{"rtsp://AaBbCcDdEeFf.wowza.com/app-AaBbCcDd-EeFfGgHh",
	 "rtsp://AaBbCcDdEeFf.wowza.com/app-AaBbCcDd-EeFfGgHh",
	 "rtsp://AaBbCcDdEeFf.wowza.com/ap*****************Hh"},

	/* IPv4 address */
	{"rtsp://192.168.1.10:554/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsp://192.168.1.10:554/live2-AaBb-CcDd-EeFf-GgHh-IiJj",
	 "rtsp://192.168.1.10:554/li**************************Jj"},

	/* IP camera typical stream */
	{"rtsp://camera.local:8554/stream-AaBbCcDdEeFf",
	 "rtsp://camera.local:8554/stream-AaBbCcDdEeFf",
	 "rtsp://camera.local:8554/st***************Ff"},

	/* Short stream ID */
	{"rtsp://media.server/live2-ABC123",
	 "rtsp://media.server/live2-ABC123",
	 "rtsp://media.server/li********23"},

	/* RTSPS with numeric path */
	{"rtsps://secure.server.com/live2-1234567890abcdef",
	 "rtsps://secure.server.com/live2-1234567890abcdef",
	 "rtsps://secure.server.com/li******************ef"},

	/* IP with path and user/pass */
	{"rtsp://camera:secret@192.168.43.1/live-front",
	 "rtsp://192.168.43.1/live-front",
	 "rtsp://******:******@192.168.43.1/li******nt"},

	/* IP with port and user/pass */
	{"rtsp://admin:123@192.168.43.1:5554/live",
	 "rtsp://192.168.43.1:5554/live",
	 "rtsp://*****:***@192.168.43.1:5554/live"},

	/* IPv6 with user/pass */
	{"rtsp://user:pass@[2001:db8::1]/stream",
	 "rtsp://[2001:db8::1]/stream",
	 "rtsp://****:****@[2001:db8::1]/st**am"},

	/* User only, no password */
	{"rtsp://streamer@192.168.1.1/live",
	 "rtsp://192.168.1.1/live",
	 "rtsp://********@192.168.1.1/live"},

	/* RTSPS User only, no password */
	{"rtsps://streamer@192.168.1.1/live",
	 "rtsps://192.168.1.1/live",
	 "rtsps://********@192.168.1.1/live"},

	/* RTSPS User with special chars in username (% must be unencoded) */
	{"rtsps://user-1.0%%@media.server/stream",
	 "rtsps://media.server/stream",
	 "rtsps://**********@media.server/st**am"},

	/* RTSPS User/Pass with special chars (ampersand, colon, dollar) */
	{"rtsps://admin&user:p@ss:w$rd@domain.com/feed",
	 "rtsps://domain.com/feed",
	 "rtsps://**********:*********@domain.com/feed"},

	/* RTSPS User/Pass with long domain and port */
	{"rtsps://testuser:testpass@long.sub.domain.net:554/"
	 "path/to/stream",
	 "rtsps://long.sub.domain.net:554/path/to/stream",
	 "rtsps://********:********@long.sub.domain.net:554/"
	 "pa**********am"},

	/* RTSPS User/Pass with path containing numbers and hyphens */
	{"rtsps://u123:p456@mycam.local/channel-2/stream.ts",
	 "rtsps://mycam.local/channel-2/stream.ts",
	 "rtsps://****:****@mycam.local/ch***************ts"},

	/* RTSPS IPv6 with special char in password */
	{"rtsps://user:pass!@!@!@[2001:db8::2]/video",
	 "rtsps://[2001:db8::2]/video",
	 "rtsps://****:*********@[2001:db8::2]/vi*eo"},

	/* RTSPS User/Pass with user containing '@' (encoded as %40) */
	{"rtsps://user%%40name:password@server.ip/stream",
	 "rtsps://server.ip/stream",
	 "rtsps://************:********@server.ip/st**am"},
};


static void test_rtsp_url_parse_case(const struct rtsp_url_params *item)
{
	struct rtsp_url *parsed = NULL;
	int err = rtsp_url_parse(item->url, &parsed);

	if (!item->valid) {
		CU_ASSERT_EQUAL(err, -EINVAL);
		CU_ASSERT_PTR_NULL(parsed);
	} else {
		const char *user;
		const char *pass;
		const char *host;
		bool has_resolved_host;
		const char *resolved_host;
		const char *path;
		enum rtsp_url_scheme scheme;
		uint16_t port;
		bool port_explicit;

		CU_ASSERT_EQUAL(err, 0);

		scheme = rtsp_url_get_scheme(parsed);
		user = rtsp_url_get_user(parsed);
		pass = rtsp_url_get_pass(parsed);
		host = rtsp_url_get_host(parsed);
		has_resolved_host = rtsp_url_has_resolved_host(parsed);
		resolved_host = rtsp_url_get_resolved_host(parsed);
		port = rtsp_url_get_port(parsed);
		port_explicit = rtsp_url_is_port_explicit(parsed);
		path = rtsp_url_get_path(parsed);

		CU_ASSERT_EQUAL(scheme, item->scheme);

		if (item->user != NULL) {
			CU_ASSERT_PTR_NOT_NULL_FATAL(user);
			ASSERT_STR_EQ(user, item->user);
		} else {
			CU_ASSERT_PTR_NULL(user);
		}

		if (item->pass != NULL) {
			CU_ASSERT_PTR_NOT_NULL_FATAL(pass);
			ASSERT_STR_EQ(pass, item->pass);
		} else {
			CU_ASSERT_PTR_NULL(pass);
		}

		CU_ASSERT_PTR_NOT_NULL_FATAL(host);
		ASSERT_STR_EQ(host, item->host);

		if (item->resolved_host != NULL) {
			CU_ASSERT_TRUE(has_resolved_host);
			ASSERT_STR_EQ(resolved_host, item->resolved_host);
		} else {
			CU_ASSERT_FALSE(has_resolved_host);
			CU_ASSERT_PTR_NULL(resolved_host);
		}

		CU_ASSERT_EQUAL(port, item->port);
		CU_ASSERT_EQUAL(port_explicit, item->port_explicit);

		if (item->path != NULL) {
			CU_ASSERT_PTR_NOT_NULL_FATAL(path);
			ASSERT_STR_EQ(path, item->path);
		} else {
			CU_ASSERT_PTR_NULL(path);
		}
	}

	rtsp_url_free(parsed);
}


static void test_rtsp_url_parse(void)
{
	/* KO URLs */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_parse_ko_map); i++)
		test_rtsp_url_parse_case(&test_cases_parse_ko_map[i]);

	/* OK URLs */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_parse_ok_map); i++)
		test_rtsp_url_parse_case(&test_cases_parse_ok_map[i]);
}


static void test_rtsp_url_copy(void)
{
	int err;

	/* Error cases */
	err = rtsp_url_copy(NULL, NULL);
	CU_ASSERT_EQUAL(err, -EINVAL);

	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_parse_ok_map); i++) {
		bool ret;
		const struct rtsp_url_params *item =
			&test_cases_parse_ok_map[i];
		struct rtsp_url *src = NULL;
		struct rtsp_url *dst = NULL;

		err = rtsp_url_parse(item->url, &src);
		CU_ASSERT_EQUAL(err, 0);

		/* Error cases */
		err = rtsp_url_copy(src, NULL);
		CU_ASSERT_EQUAL(err, -EINVAL);

		err = rtsp_url_copy(NULL, &dst);
		CU_ASSERT_EQUAL(err, -EINVAL);

		/* OK */
		err = rtsp_url_copy(src, &dst);
		CU_ASSERT_EQUAL(err, 0);

		ret = rtsp_url_cmp(src, dst);
		CU_ASSERT_TRUE(ret);

		rtsp_url_free(src);
		rtsp_url_free(dst);
	}
}


static void test_rtsp_url_cmp(void)
{
	bool ret;

	ret = rtsp_url_cmp(NULL, NULL);
	CU_ASSERT_TRUE(ret);

	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_cmp_map); i++) {
		int err;
		const struct rtsp_url_cmp_params *item = &test_cases_cmp_map[i];
		struct rtsp_url *url1 = NULL;
		struct rtsp_url *url2 = NULL;

		err = rtsp_url_parse(item->url1, &url1);
		CU_ASSERT_EQUAL(err, 0);

		err = rtsp_url_parse(item->url2, &url2);
		CU_ASSERT_EQUAL(err, 0);

		/* Error cases */
		ret = rtsp_url_cmp(url1, NULL);
		CU_ASSERT_FALSE(ret);

		ret = rtsp_url_cmp(NULL, url2);
		CU_ASSERT_FALSE(ret);

		ret = rtsp_url_cmp(url1, url2);
		CU_ASSERT_EQUAL(ret, item->identical);

		rtsp_url_free(url1);
		rtsp_url_free(url2);
	}
}


static void test_rtsp_url_strip_credentials(void)
{
	int ret = 0;
	char *stripped = NULL;

	ret = rtsp_url_strip_credentials(NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(stripped);

	/* KO */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_str_ko_map); i++) {
		stripped = NULL;
		ret = rtsp_url_strip_credentials(test_cases_str_ko_map[i].url,
						 &stripped);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		CU_ASSERT_PTR_NULL(stripped);
	}

	/* OK */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_str_ok_map); i++) {
		stripped = NULL;
		ret = rtsp_url_strip_credentials(test_cases_str_ok_map[i].url,
						 &stripped);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(stripped);
		ASSERT_STR_EQ(stripped, test_cases_str_ok_map[i].stripped);
		free(stripped);
	}
}


static void test_rtsp_url_anonymize(void)
{
	int ret = 0;
	char *anonymized = NULL;

	ret = rtsp_url_anonymize(NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	CU_ASSERT_PTR_NULL(anonymized);

	/* KO */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_str_ko_map); i++) {
		anonymized = NULL;
		ret = rtsp_url_anonymize(test_cases_str_ko_map[i].url,
					 &anonymized);
		CU_ASSERT_EQUAL(ret, -EINVAL);
		CU_ASSERT_PTR_NULL(anonymized);
	}

	/* OK */
	for (size_t i = 0; i < SIZEOF_ARRAY(test_cases_str_ok_map); i++) {
		anonymized = NULL;
		ret = rtsp_url_anonymize(test_cases_str_ok_map[i].url,
					 &anonymized);
		CU_ASSERT_EQUAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL(anonymized);
		ASSERT_STR_EQ(anonymized, test_cases_str_ok_map[i].anon);
		free(anonymized);
	}
}


CU_TestInfo g_rtsp_test_url_c[] = {
	{FN("rtsp-url-parse"), &test_rtsp_url_parse},
	{FN("rtsp-url-copy"), &test_rtsp_url_copy},
	{FN("rtsp-url-cmp"), &test_rtsp_url_cmp},
	{FN("rtsp-url-strip-credentials"), &test_rtsp_url_strip_credentials},
	{FN("rtsp-url-anonymize"), &test_rtsp_url_anonymize},

	CU_TEST_INFO_NULL,
};
