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

#include <ctype.h>

#define ULOG_TAG rtsp_url
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_url);


struct rtsp_url {
	enum rtsp_url_scheme scheme;
	char *user;
	char *pass;
	char *host;
	char *resolved_host;
	uint16_t port;
	bool port_explicit;
	char *path;
};


struct {
	enum rtsp_url_scheme scheme;
	uint16_t default_port;
	const char *str;
} rtsp_url_scheme_map[] = {
	{RTSP_URL_SCHEME_TCP, RTSP_DEFAULT_PORT, RTSP_SCHEME_TCP},
	{RTSP_URL_SCHEME_UDP, RTSP_DEFAULT_PORT, RTSP_SCHEME_UDP},
	{RTSP_URL_SCHEME_TCP_TLS, RTSPS_DEFAULT_PORT, RTSP_SCHEME_TCP_TLS},
};


const char *rtsp_url_scheme_str(enum rtsp_url_scheme val)
{
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_url_scheme_map); i++) {
		if (val == rtsp_url_scheme_map[i].scheme)
			return rtsp_url_scheme_map[i].str;
	}
	return NULL;
}


enum rtsp_url_scheme rtsp_url_scheme_from_str(const char *str)
{
	if (!str)
		return RTSP_URL_SCHEME_UNKNOWN;
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_url_scheme_map); i++) {
		if (!strcasecmp(str, rtsp_url_scheme_map[i].str))
			return rtsp_url_scheme_map[i].scheme;
	}
	return RTSP_URL_SCHEME_UNKNOWN;
}


uint16_t rtsp_url_scheme_default_port(enum rtsp_url_scheme val)
{
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_url_scheme_map); i++) {
		if (val == rtsp_url_scheme_map[i].scheme)
			return rtsp_url_scheme_map[i].default_port;
	}
	return 0;
}


static inline int is_valid_rtsp_path_char(char c)
{
	const char *valid = "-._~!$&'()*+,;=:@/";
	return isalnum((unsigned char)c) || strchr(valid, c) != NULL;
}


static int
parse_host_and_port(const char *url, char *host, char **_host, uint16_t *_port)
{
	char *host_start = host;
	const char *port_str = NULL;

	/* IPv6 literal: [2001:db8::1]:8554 */
	if (host_start[0] == '[') {
		char *closing = strchr(host_start, ']');
		if (!closing) {
			ULOGE("unterminated IPv6 address in URL: '%s'", url);
			return -EINVAL;
		}

		*closing = '\0'; /* terminate IPv6 address */
		*_host = host_start + 1; /* skip '[' */

		if ((*_host)[0] == '\0') {
			ULOGE("missing IPv6 host in URL: '%s'", url);
			return -EINVAL;
		}

		if (*(closing + 1) == ':') {
			port_str = closing + 2;
		} else if (*(closing + 1) != '\0') {
			ULOGE("invalid character after IPv6 address "
			      "in URL: '%s'",
			      url);
			return -EINVAL;
		}

	} else {
		/* IPv4 or hostname: example.com:554 */
		char *colon = strchr(host_start, ':');
		if (colon) {
			*colon = '\0';
			port_str = colon + 1;
		}
		*_host = host_start;

		if ((*_host)[0] == '\0') {
			ULOGE("missing host in URL: '%s'", url);
			return -EINVAL;
		}
	}

	/* Parse port if present */
	if (port_str && *port_str != '\0') {
		char *endptr = NULL;
		long val = strtol(port_str, &endptr, 10);
		if (*endptr != '\0' || val < 0 || val > UINT16_MAX) {
			ULOGE("invalid port in URL: '%s'", url);
			return -EINVAL;
		}
		*_port = (uint16_t)val;
	}

	/* Validate host characters */
	const char *c = *_host;
	if (host_start[0] == '[') {
		/* IPv6 literal validation */
		for (; *c; ++c) {
			if (!isxdigit((unsigned char)*c) && *c != ':' &&
			    *c != '.') {
				ULOGE("invalid IPv6 character '%c' "
				      "in URL: '%s'",
				      *c,
				      url);
				return -EINVAL;
			}
		}
	} else {
		/* IPv4 or DNS name validation */
		for (; *c; ++c) {
			if (!isalnum((unsigned char)*c) && *c != '.' &&
			    *c != '-') {
				ULOGE("invalid host character '%c'"
				      "in URL: '%s'",
				      *c,
				      url);
				return -EINVAL;
			}
		}
	}

	return 0;
}


static int parse_authority(const char *url,
			   char *authority,
			   char **_username,
			   char **_password,
			   char **_host,
			   uint16_t *_port)
{
	char *userpass_end = NULL;
	char *hostpart = authority;

	*_username = NULL;
	*_password = NULL;
	*_host = NULL;

	/* Look for '@' for user/pass */
	userpass_end = strrchr(authority, '@');
	if (userpass_end) {
		*userpass_end = '\0';
		hostpart = userpass_end + 1;

		char *colon = strchr(authority, ':');
		if (colon) {
			*colon = '\0';
			*_username = authority;
			*_password = colon + 1;
		} else {
			*_username = authority;
		}

		if ((*_username)[0] == '\0') {
			ULOGE("username is empty in URL: '%s'", url);
			return -EINVAL;
		}
	}

	/* Host + port */
	return parse_host_and_port(url, hostpart, _host, _port);
}


static int parse_path(char **_path)
{
	if (!_path || !*_path)
		return -EINVAL;

	char *path = *_path;
	size_t len = strnlen(path, PATH_MAX);

	if (len >= PATH_MAX)
		return -ENAMETOOLONG;

	while (len > 0 && path[len - 1] == '/')
		path[--len] = '\0';

	if (len == 0) {
		*_path = NULL;
	} else {
		for (size_t i = 0; i < len; ++i) {
			if (!is_valid_rtsp_path_char(path[i])) {
				ULOGE("invalid path character: '%c'\n",
				      path[i]);
				return -EINVAL;
			}
		}
	}
	return 0;
}


static int rtsp_url_parse_internal(char *url,
				   enum rtsp_url_scheme *scheme,
				   char **username,
				   char **password,
				   char **host,
				   uint16_t *port,
				   bool *port_explicit,
				   char **path)
{
	int ret;
	uint16_t default_port = 0;
	uint16_t _port = 0;
	char *_host = NULL;
	char *_username = NULL;
	char *_password = NULL;
	char *_path = NULL;
	char *temp = NULL;
	char *p;
	bool found = false;
	enum rtsp_url_scheme _scheme = RTSP_URL_SCHEME_UNKNOWN;
	size_t offset;

	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(url[0] == '\0', EINVAL);

	size_t url_len = strnlen(url, PATH_MAX);
	if ((url_len == 0) || (url_len >= PATH_MAX))
		return -EINVAL;

	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_url_scheme_map); i++) {
		const char *scheme_str = rtsp_url_scheme_map[i].str;
		const size_t len = strlen(scheme_str);
		if (url_len < len)
			continue;
		if (strncmp(url, scheme_str, len) == 0) {
			_scheme = rtsp_url_scheme_map[i].scheme;
			default_port = rtsp_url_scheme_map[i].default_port;
			offset = len;
			found = true;
			break;
		}
	}
	if (!found) {
		ULOGE("invalid URL scheme: '%s'", url);
		return -EINVAL;
	}

	_host = url + offset;
	if (_host[0] == '\0' || _host[0] == '/') {
		ULOGE("missing host in URL: '%s'\n", url);
		return -EINVAL;
	}

	p = strtok_r(_host, "/", &temp);
	if (!p || *p == '\0') {
		ULOGE("missing host in URL: '%s'\n", url);
		return -EINVAL;
	}

	ret = parse_authority(url, p, &_username, &_password, &_host, &_port);
	if (ret < 0) {
		/* Error already logged, propagate */
		return ret;
	}

	bool _port_explicit = (_port != 0);

	if (_port == 0)
		_port = default_port;

	if (port_explicit)
		*port_explicit = _port_explicit;

	_path = strtok_r(NULL, "", &temp);
	if (_path) {
		/* Absolute path, can be NULL */
		ret = parse_path(&_path);
		if (ret < 0) {
			/* Error already logged, propagate */
			return ret;
		}
	}

	if (scheme)
		*scheme = _scheme;
	if (username)
		*username = _username;
	if (password)
		*password = _password;
	if (host)
		*host = _host;
	if (port)
		*port = _port;
	if (path)
		*path = _path;

	return 0;
}


int rtsp_url_parse_host_and_path(char *url, char **host, char **path)
{
	return rtsp_url_parse_internal(
		url, NULL, NULL, NULL, host, NULL, NULL, path);
}


int rtsp_url_parse_path(char *url, char **path)
{
	return rtsp_url_parse_internal(
		url, NULL, NULL, NULL, NULL, NULL, NULL, path);
}


static bool is_valid_ipv4_or_ipv6(const char *str)
{
	struct in_addr sa;
	struct in6_addr sa6;

	if (str == NULL)
		return false;

	if (inet_pton(AF_INET, str, &sa) == 1)
		return true;

	if (inet_pton(AF_INET6, str, &sa6) == 1)
		return true;

	return false;
}


int rtsp_url_parse(const char *url, struct rtsp_url **ret_obj)
{
	int ret;
	char *url_copy = NULL;
	struct rtsp_url *_url = NULL;
	char *user = NULL;
	char *pass = NULL;
	char *host = NULL;
	char *path = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);

	url_copy = strdup(url);
	if (url_copy == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("strdup", -ret);
		goto error;
	}

	_url = calloc(1, sizeof(*_url));
	if (_url == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}

	ret = rtsp_url_parse_internal(url_copy,
				      &_url->scheme,
				      &user,
				      &pass,
				      &host,
				      &_url->port,
				      &_url->port_explicit,
				      &path);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_url_parse_internal", -ret);
		goto error;
	}

	_url->user = xstrdup(user);
	_url->pass = xstrdup(pass);
	_url->host = xstrdup(host);
	if (is_valid_ipv4_or_ipv6(_url->host))
		_url->resolved_host = xstrdup(_url->host);
	_url->path = xstrdup(path);

	*ret_obj = _url;
	free(url_copy);
	return 0;

error:
	rtsp_url_free(_url);
	free(url_copy);
	return ret;
}


static int rtsp_url_to_str_internal(const struct rtsp_url *url,
				    char **str,
				    bool resolved,
				    bool include_path)
{
	int ret;
	bool is_ipv6 = false;
	char port_str[6] = ""; /* max 65535 + '\0' */
	const char *scheme_str = NULL;
	char *_ret_str = NULL;
	const char *host_to_use = NULL;
	const char *path_to_use = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	if (resolved) {
		host_to_use =
			url->resolved_host ? url->resolved_host : url->host;
	} else {
		host_to_use = url->host;
	}

	if (include_path)
		path_to_use = url->path;

	scheme_str = rtsp_url_scheme_str(url->scheme);
	ULOG_ERRNO_RETURN_ERR_IF(scheme_str == NULL, EINVAL);

	if (url->port != 0 && url->port_explicit)
		snprintf(port_str, sizeof(port_str), ":%u", url->port);

	is_ipv6 = host_to_use && strchr(host_to_use, ':') != NULL;

	/* rtsp[s]://[user]:[pass]@[host]:[port]/[path] */
	ret = asprintf(&_ret_str,
		       "%s" /* scheme */
		       "%s%s%s%s" /* [user]:[pass]@ */
		       "%s%s%s" /* [host] */
		       "%s" /* :port */
		       "%s%s", /* /[path] */
		       scheme_str,
		       url->user ? url->user : "",
		       url->user && url->pass ? ":" : "",
		       url->pass ? url->pass : "",
		       url->user ? "@" : "",
		       is_ipv6 ? "[" : "",
		       host_to_use,
		       is_ipv6 ? "]" : "",
		       port_str,
		       path_to_use ? "/" : "",
		       path_to_use ? path_to_use : "");
	if (ret < 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		goto out;
	}

	*str = _ret_str;
	return 0;

out:
	free(_ret_str);
	return ret;
}


int rtsp_url_to_str(const struct rtsp_url *url, char **str)
{
	return rtsp_url_to_str_internal(url, str, false, true);
}


int rtsp_url_to_str_resolved(const struct rtsp_url *url, char **str)
{
	return rtsp_url_to_str_internal(url, str, true, true);
}


int rtsp_url_to_str_no_path(const struct rtsp_url *url, char **str)
{
	return rtsp_url_to_str_internal(url, str, false, false);
}


int rtsp_url_to_str_no_path_resolved(const struct rtsp_url *url, char **str)
{
	return rtsp_url_to_str_internal(url, str, true, false);
}


int rtsp_url_copy(const struct rtsp_url *src, struct rtsp_url **dst)
{
	int ret;
	struct rtsp_url *_dst = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(src == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst == NULL, EINVAL);

	_dst = calloc(1, sizeof(*_dst));
	if (_dst == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}

	*_dst = *src;

	_dst->user = xstrdup(src->user);
	_dst->pass = xstrdup(src->pass);
	_dst->host = xstrdup(src->host);
	_dst->resolved_host = xstrdup(src->resolved_host);
	_dst->path = xstrdup(src->path);

	*dst = _dst;
	return 0;

error:
	rtsp_url_free(_dst);
	return ret;
}


static int nullsafe_strcmp(const char *s1, const char *s2)
{
	if (s1 == s2)
		return 0;
	if (s1 == NULL || s2 == NULL)
		return -1;
	return strcmp(s1, s2);
}


#define CMP_FIELD_STR_SAFE(_url1, _url2, _field)                               \
	do {                                                                   \
		if (nullsafe_strcmp((_url1)->_field, (_url2)->_field) != 0)    \
			return false;                                          \
	} while (0)


#define CMP_FIELD_VAL(_url1, _url2, _field)                                    \
	do {                                                                   \
		if (_url1->_field != _url2->_field)                            \
			return false;                                          \
	} while (0)


bool rtsp_url_cmp(const struct rtsp_url *url1, const struct rtsp_url *url2)
{
	if (url1 == url2)
		return true;
	if (!url1 || !url2)
		return false;

	CMP_FIELD_VAL(url1, url2, scheme);
	CMP_FIELD_STR_SAFE(url1, url2, user);
	CMP_FIELD_STR_SAFE(url1, url2, pass);
	CMP_FIELD_STR_SAFE(url1, url2, host);
	CMP_FIELD_STR_SAFE(url1, url2, resolved_host);
	CMP_FIELD_VAL(url1, url2, port);
	CMP_FIELD_VAL(url1, url2, port_explicit);
	CMP_FIELD_STR_SAFE(url1, url2, path);

	return true;
}


int rtsp_url_free(struct rtsp_url *url)
{
	if (url == NULL)
		return 0;

	free(url->user);
	free(url->pass);
	free(url->host);
	free(url->resolved_host);
	free(url->path);

	free(url);
	return 0;
}


/* Getters */
enum rtsp_url_scheme rtsp_url_get_scheme(const struct rtsp_url *url)
{
	return url ? url->scheme : RTSP_URL_SCHEME_UNKNOWN;
}


const char *rtsp_url_get_user(const struct rtsp_url *url)
{
	return url ? url->user : NULL;
}


const char *rtsp_url_get_pass(const struct rtsp_url *url)
{
	return url ? url->pass : NULL;
}


const char *rtsp_url_get_host(const struct rtsp_url *url)
{
	return url ? url->host : NULL;
}


int rtsp_url_set_resolved_host(struct rtsp_url *url, const char *resolved_host)
{
	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(resolved_host == NULL, EINVAL);

	xfree((void **)&url->resolved_host);
	url->resolved_host = xstrdup(resolved_host);
	return 0;
}


const char *rtsp_url_get_resolved_host(const struct rtsp_url *url)
{
	return url ? url->resolved_host : NULL;
}


bool rtsp_url_has_resolved_host(const struct rtsp_url *url)
{
	return (rtsp_url_get_resolved_host(url) != NULL);
}


uint16_t rtsp_url_get_port(const struct rtsp_url *url)
{
	return url ? url->port : 0;
}


bool rtsp_url_is_port_explicit(const struct rtsp_url *url)
{
	return url ? url->port_explicit : false;
}


const char *rtsp_url_get_path(const struct rtsp_url *url)
{
	return url ? url->path : NULL;
}


int rtsp_url_strip_credentials(const char *url, char **ret_url)
{
	int ret;
	char *result = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_url == NULL, EINVAL);

	struct rtsp_url *parsed = NULL;
	ret = rtsp_url_parse(url, &parsed);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_url_parse", -ret);
		return ret;
	}

	xfree((void **)&parsed->user);
	xfree((void **)&parsed->pass);

	ret = rtsp_url_to_str(parsed, &result);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_url_to_str", -ret);
		goto out;
	}

	*ret_url = result;
	ret = 0;

out:
	rtsp_url_free(parsed);
	return ret;
}


static char *anonymize_str(const char *str, bool keep_prefix_suffix)
{
	size_t len;
	char *output = NULL;

	if (str == NULL)
		return NULL;

	len = strnlen(str, PATH_MAX);
	if (len >= PATH_MAX)
		return NULL;

	output = strdup(str);
	if (!output)
		return NULL;

	size_t prefix_suffix_len = keep_prefix_suffix ? 2 : 0;

	if (len > 2 * prefix_suffix_len) {
		for (size_t i = prefix_suffix_len;
		     i < (len - prefix_suffix_len);
		     i++)
			output[i] = '*';
	}

	return output;
}


static inline int anonymize_field(char **field, bool is_path)
{
	if (*field) {
		char *tmp = anonymize_str(*field, is_path);
		if (!tmp)
			return -ENOMEM;
		xfree((void **)field);
		*field = tmp;
	}
	return 0;
}


int rtsp_url_anonymize(const char *url, char **ret_url)
{
	int ret;
	char *result = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_url == NULL, EINVAL);

	struct rtsp_url *parsed = NULL;
	ret = rtsp_url_parse(url, &parsed);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_url_parse", -ret);
		return ret;
	}

	ret = anonymize_field(&parsed->user, false);
	if (ret < 0)
		goto out;
	ret = anonymize_field(&parsed->pass, false);
	if (ret < 0)
		goto out;
	ret = anonymize_field(&parsed->path, true);
	if (ret < 0)
		goto out;

	ret = rtsp_url_to_str(parsed, &result);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_url_to_str", -ret);
		goto out;
	}

	*ret_url = result;
	ret = 0;

out:
	rtsp_url_free(parsed);
	return ret;
}
