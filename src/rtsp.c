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

#define ULOG_TAG rtsp
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp);


/* codecheck_ignore[COMPLEX_MACRO] */
#define RTSP_ENUM_CASE(_prefix, _name)                                         \
	case _prefix##_name:                                                   \
		return #_name

#define RTSP_REV_ENUM_CASE(_val, _str_prefix, _enum_prefix, _name)             \
	do {                                                                   \
		if (strcmp(_val, _str_prefix##_name) == 0)                     \
			return _enum_prefix##_name;                            \
	} while (0)

/* codecheck_ignore[MULTISTATEMENT_MACRO_USE_DO_WHILE] */
#define RTSP_STATUS_ENUM_CASE(_name, _status_code, _status_string)             \
	case RTSP_STATUS_CODE(_name):                                          \
		_status_code = RTSP_STATUS_CODE(_name);                        \
		_status_string = RTSP_STATUS_STRING(_name);                    \
		break


const char *rtsp_lower_transport_str(enum rtsp_lower_transport val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_LOWER_TRANSPORT_, UDP);
	RTSP_ENUM_CASE(RTSP_LOWER_TRANSPORT_, TCP);
	RTSP_ENUM_CASE(RTSP_LOWER_TRANSPORT_, MUX);
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


const char *rtsp_delivery_str(enum rtsp_delivery val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_DELIVERY_, MULTICAST);
	RTSP_ENUM_CASE(RTSP_DELIVERY_, UNICAST);
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


const char *rtsp_time_format_str(enum rtsp_time_format val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_TIME_FORMAT_, UNKNOWN);
	RTSP_ENUM_CASE(RTSP_TIME_FORMAT_, NPT);
	RTSP_ENUM_CASE(RTSP_TIME_FORMAT_, SMPTE);
	RTSP_ENUM_CASE(RTSP_TIME_FORMAT_, ABSOLUTE);
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


const char *rtsp_method_type_str(enum rtsp_method_type val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, OPTIONS);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, DESCRIBE);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, ANNOUNCE);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, SETUP);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, PLAY);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, PAUSE);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, TEARDOWN);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, GET_PARAMETER);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, SET_PARAMETER);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, REDIRECT);
	RTSP_ENUM_CASE(RTSP_METHOD_TYPE_, RECORD);

	case RTSP_METHOD_TYPE_UNKNOWN: /* NO BREAK */
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


static enum rtsp_method_type rtsp_method_type_enum(const char *val)
{
	if (val == NULL)
		return RTSP_METHOD_TYPE_UNKNOWN;

	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, OPTIONS);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, DESCRIBE);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, ANNOUNCE);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, SETUP);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, PLAY);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, PAUSE);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, TEARDOWN);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, GET_PARAMETER);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, SET_PARAMETER);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, REDIRECT);
	RTSP_REV_ENUM_CASE(val, RTSP_METHOD_, RTSP_METHOD_TYPE_, RECORD);

	return RTSP_METHOD_TYPE_UNKNOWN;
}


void rtsp_status_get(int status, int *code, const char **str)
{
	ULOG_ERRNO_RETURN_IF(code == NULL, EINVAL);
	ULOG_ERRNO_RETURN_IF(str == NULL, EINVAL);

	/* clang-format off */
	switch (status) {
	RTSP_STATUS_ENUM_CASE(CONTINUE, *code, *str);
	RTSP_STATUS_ENUM_CASE(OK, *code, *str);
	RTSP_STATUS_ENUM_CASE(CREATED, *code, *str);
	RTSP_STATUS_ENUM_CASE(LOW_ON_STORAGE, *code, *str);
	RTSP_STATUS_ENUM_CASE(MULTIPLE_CHOICES, *code, *str);
	RTSP_STATUS_ENUM_CASE(MOVED_PERMANENTLY, *code, *str);
	RTSP_STATUS_ENUM_CASE(MOVED_TEMPORARITY, *code, *str);
	RTSP_STATUS_ENUM_CASE(SEE_OTHER, *code, *str);
	RTSP_STATUS_ENUM_CASE(NOT_MODIFIED, *code, *str);
	RTSP_STATUS_ENUM_CASE(USE_PROXY, *code, *str);
	RTSP_STATUS_ENUM_CASE(BAD_REQUEST, *code, *str);
	RTSP_STATUS_ENUM_CASE(UNAUTHORIZED, *code, *str);
	RTSP_STATUS_ENUM_CASE(PAYMENT_REQUIRED, *code, *str);
	RTSP_STATUS_ENUM_CASE(FORBIDDEN, *code, *str);
	RTSP_STATUS_ENUM_CASE(NOT_FOUND, *code, *str);
	RTSP_STATUS_ENUM_CASE(METHOD_NOT_ALLOWED, *code, *str);
	RTSP_STATUS_ENUM_CASE(NOT_ACCEPTABLE, *code, *str);
	RTSP_STATUS_ENUM_CASE(PROXY_AUTHENTICATION_REQUIRED, *code, *str);
	RTSP_STATUS_ENUM_CASE(REQUEST_TIMEOUT, *code, *str);
	RTSP_STATUS_ENUM_CASE(GONE, *code, *str);
	RTSP_STATUS_ENUM_CASE(LENGTH_REQUIRED, *code, *str);
	RTSP_STATUS_ENUM_CASE(PRECONDITION_FAILED, *code, *str);
	RTSP_STATUS_ENUM_CASE(REQUEST_ENTITY_TOO_LARGE, *code, *str);
	RTSP_STATUS_ENUM_CASE(REQUEST_URI_TOO_LARGE, *code, *str);
	RTSP_STATUS_ENUM_CASE(UNSUPPORTED_MEDIA_TYPE, *code, *str);
	RTSP_STATUS_ENUM_CASE(PARMETER_NOT_UNDERSTOOD, *code, *str);
	RTSP_STATUS_ENUM_CASE(CONFERENCE_NOT_FOUND, *code, *str);
	RTSP_STATUS_ENUM_CASE(NOT_ENOUGH_BANDWIDTH, *code, *str);
	RTSP_STATUS_ENUM_CASE(SESSION_NOT_FOUND, *code, *str);
	RTSP_STATUS_ENUM_CASE(METHOD_NOT_VALID, *code, *str);
	RTSP_STATUS_ENUM_CASE(HEADER_FIELD_NOT_VALID, *code, *str);
	RTSP_STATUS_ENUM_CASE(INVALID_RANGE, *code, *str);
	RTSP_STATUS_ENUM_CASE(PARAMETER_READ_ONLY, *code, *str);
	RTSP_STATUS_ENUM_CASE(AGGREGATE_OPERATION_NOT_ALLOWED, *code, *str);
	RTSP_STATUS_ENUM_CASE(ONLY_AGGREGATE_OPERATION_ALLOWED, *code, *str);
	RTSP_STATUS_ENUM_CASE(UNSUPPORTED_TRANSPORT, *code, *str);
	RTSP_STATUS_ENUM_CASE(DESTINATION_UNREACHABLE, *code, *str);
	RTSP_STATUS_ENUM_CASE(INTERNAL_SERVER_ERROR, *code, *str);
	RTSP_STATUS_ENUM_CASE(NOT_IMPLEMENTED, *code, *str);
	RTSP_STATUS_ENUM_CASE(BAD_GATEWAY, *code, *str);
	RTSP_STATUS_ENUM_CASE(SERVICE_UNAVAILABLE, *code, *str);
	RTSP_STATUS_ENUM_CASE(GATEWAY_TIMEOUT, *code, *str);
	RTSP_STATUS_ENUM_CASE(RTSP_VERSION_NOT_SUPPORTED, *code, *str);
	RTSP_STATUS_ENUM_CASE(OPTION_NOT_SUPPORTED, *code, *str);
	case 0:
		*code = RTSP_STATUS_CODE(OK);
		*str = RTSP_STATUS_STRING(OK);
		break;
	case -EPROTO:
		*code = RTSP_STATUS_CODE(BAD_REQUEST);
		*str = RTSP_STATUS_STRING(BAD_REQUEST);
		break;
	case -EPERM:
		*code = RTSP_STATUS_CODE(UNAUTHORIZED);
		*str = RTSP_STATUS_STRING(UNAUTHORIZED);
		break;
	case -EACCES:
		*code = RTSP_STATUS_CODE(FORBIDDEN);
		*str = RTSP_STATUS_STRING(FORBIDDEN);
		break;
	case -ENOENT:
		*code = RTSP_STATUS_CODE(NOT_FOUND);
		*str = RTSP_STATUS_STRING(NOT_FOUND);
		break;
	case -ETIMEDOUT:
		*code = RTSP_STATUS_CODE(REQUEST_TIMEOUT);
		*str = RTSP_STATUS_STRING(REQUEST_TIMEOUT);
		break;
	case -ENOSYS:
		*code = RTSP_STATUS_CODE(NOT_IMPLEMENTED);
		*str = RTSP_STATUS_STRING(NOT_IMPLEMENTED);
		break;
	case -EBUSY:
	case -EAGAIN:
		*code = RTSP_STATUS_CODE(SERVICE_UNAVAILABLE);
		*str = RTSP_STATUS_STRING(SERVICE_UNAVAILABLE);
		break;
	default:
		*code = RTSP_STATUS_CODE(INTERNAL_SERVER_ERROR);
		*str = RTSP_STATUS_STRING(INTERNAL_SERVER_ERROR);
		break;
	}
	/* clang-format on */
}


int rtsp_status_to_errno(int status)
{
	/* Return negative errno directly */
	if (status < 0)
		return status;

	/* Convert RTSP status code to errno */
	switch (status) {
	case RTSP_STATUS_CODE_OK:
		return 0;
	case RTSP_STATUS_CODE_BAD_REQUEST:
		return -EPROTO;
	case RTSP_STATUS_CODE_UNAUTHORIZED:
		return -EPERM;
	case RTSP_STATUS_CODE_FORBIDDEN:
		return -EACCES;
	case RTSP_STATUS_CODE_NOT_FOUND:
		return -ENOENT;
	case RTSP_STATUS_CODE_REQUEST_TIMEOUT:
		return -ETIMEDOUT;
	case RTSP_STATUS_CODE_NOT_IMPLEMENTED:
		return -ENOSYS;
	case RTSP_STATUS_CODE_SERVICE_UNAVAILABLE:
		return -EBUSY;
	default:
		return -EPROTO;
	}
}


const char *rtsp_status_str(int status)
{
	int status_code = 0;
	const char *status_string = NULL;

	rtsp_status_get(status, &status_code, &status_string);

	return status_string;
}


const char *rtsp_client_conn_state_str(enum rtsp_client_conn_state val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_CLIENT_CONN_STATE_, DISCONNECTED);
	RTSP_ENUM_CASE(RTSP_CLIENT_CONN_STATE_, CONNECTING);
	RTSP_ENUM_CASE(RTSP_CLIENT_CONN_STATE_, CONNECTED);
	RTSP_ENUM_CASE(RTSP_CLIENT_CONN_STATE_, DISCONNECTING);
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


const char *rtsp_client_req_status_str(enum rtsp_client_req_status val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_CLIENT_REQ_STATUS_, OK);
	RTSP_ENUM_CASE(RTSP_CLIENT_REQ_STATUS_, CANCELED);
	RTSP_ENUM_CASE(RTSP_CLIENT_REQ_STATUS_, FAILED);
	RTSP_ENUM_CASE(RTSP_CLIENT_REQ_STATUS_, ABORTED);
	RTSP_ENUM_CASE(RTSP_CLIENT_REQ_STATUS_, TIMEOUT);
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


int rtsp_url_parse(char *url, char **host, uint16_t *port, char **path)
{
	uint16_t _port = RTSP_DEFAULT_PORT;
	char *_host = NULL, *_path = NULL, *temp = NULL, *p, *p2;

	ULOG_ERRNO_RETURN_ERR_IF(url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(url[0] == '\0', EINVAL);

	if (strncmp(url, RTSP_SCHEME_TCP, strlen(RTSP_SCHEME_TCP)) != 0) {
		ULOGE("invalid URL scheme: '%s'", url);
		return -EINVAL;
	}

	_host = url + strlen(RTSP_SCHEME_TCP);
	if (_host[0] == '\0') {
		ULOGE("invalid URL: '%s'", url);
		return -EINVAL;
	}

	p = strtok_r(_host, "/", &temp);
	if (p == NULL) {
		ULOGE("invalid URL: '%s'", url);
		return -EINVAL;
	}

	/* Host */
	p2 = strchr(p, ':');
	if (p2) {
		/* Port */
		_port = atoi(p2 + 1);
		*p2 = '\0';
	}

	/* Absolute path, can be NULL */
	_path = strtok_r(NULL, "", &temp);

	if (host)
		*host = _host;
	if (port)
		*port = _port;
	if (path)
		*path = _path;

	return 0;
}


static char *rtsp_strnstr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	c = *find;
	if (c != '\0') {
		find++;
		len = strlen(find);
		do {
			do {
				sc = *s++;
				if (slen-- < 1 || sc == '\0')
					return NULL;
			} while (sc != c);
			if (len > slen)
				return NULL;
		} while (strncmp(s, find, len) != 0);
		s--;
		c = *find;
	}
	return (char *)s;
}


static char *find_double_newline(const char *s, size_t slen, size_t *found_len)
{
	size_t len = 0;
	char *r;

	r = rtsp_strnstr(s, "\r\n\r\n", slen);
	if (r) {
		len = 4;
		goto exit;
	}
	r = rtsp_strnstr(s, "\n\r\n\r", slen);
	if (r) {
		len = 4;
		goto exit;
	}
	r = rtsp_strnstr(s, "\n\n", slen);
	if (r) {
		len = 2;
		goto exit;
	}
	r = rtsp_strnstr(s, "\r\r", slen);
	if (r) {
		len = 2;
		goto exit;
	}

exit:
	if (found_len)
		*found_len = len;
	return r;
}


static int rtsp_time_write(const struct rtsp_time *time,
			   struct rtsp_string *str)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(time == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	switch (time->format) {
	case RTSP_TIME_FORMAT_NPT:
		if (time->npt.now) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   RTSP_TIME_NPT_NOW);
		} else {
			ULOG_ERRNO_RETURN_ERR_IF(time->npt.infinity, EINVAL);
			unsigned int hrs, min;
			unsigned int sec =
				time->npt.sec + time->npt.usec / 1000000;
			hrs = sec / (60 * 60);
			min = sec / 60 - hrs * 60;
			sec = sec - min * 60 - hrs * 60 * 60;
			float usec = (float)time->npt.usec / 1000000. -
				     (time->npt.usec / 1000000);
			char fraction[6];
			snprintf(fraction, sizeof(fraction), "%.3f", usec);
			if ((min > 0) || (hrs > 0)) {
				CHECK_FUNC(rtsp_sprintf,
					   ret,
					   return ret,
					   str,
					   "%u:%02u:%02u%s",
					   hrs,
					   min,
					   sec,
					   (usec != 0.) ? fraction + 1 : "");
			} else {
				CHECK_FUNC(rtsp_sprintf,
					   ret,
					   return ret,
					   str,
					   "%u%s",
					   sec,
					   (usec != 0.) ? fraction + 1 : "");
			}
		}
		break;
	case RTSP_TIME_FORMAT_SMPTE:
		/* TODO */
		ULOGE("unsupported time format: %d", time->format);
		return -ENOSYS;
	case RTSP_TIME_FORMAT_ABSOLUTE:
		/* TODO */
		ULOGE("unsupported time format: %d", time->format);
		return -ENOSYS;
	default:
		ULOGE("unknown time format: %d", time->format);
		return -EINVAL;
	}

	return ret;
}


static int rtsp_time_read(char *str, struct rtsp_time *time)
{
	char *s;

	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(time == NULL, EINVAL);

	switch (time->format) {
	case RTSP_TIME_FORMAT_NPT:
		s = strchr(str, ':');
		if (s != NULL) {
			/* Hours, minutes, seconds */
			char *hrs_str = str;
			*s = '\0';
			char *min_str = s + 1;
			s = strchr(min_str, ':');
			ULOG_ERRNO_RETURN_ERR_IF(s == NULL, EINVAL);
			*s = '\0';
			char *sec_str = s + 1;
			unsigned int hrs = atoi(hrs_str);
			unsigned int min = atoi(min_str);
			float sec_f = atof(sec_str);
			time->npt.sec = (uint64_t)sec_f;
			time->npt.sec += min * 60 + hrs * 60 * 60;
			time->npt.usec = (uint32_t)(
				(sec_f - (float)((unsigned int)sec_f)) *
				1000000);
		} else {
			if (strcmp(str, RTSP_TIME_NPT_NOW) == 0) {
				/* 'now' */
				time->npt.now = 1;
			} else {
				/* Seconds only */
				float sec = atof(str);
				time->npt.sec = (uint64_t)sec;
				time->npt.usec = (uint32_t)(
					(sec - (float)time->npt.sec) * 1000000);
			}
		}
		break;
	case RTSP_TIME_FORMAT_SMPTE:
		/* TODO */
		ULOGE("unsupported time format: %d", time->format);
		return -ENOSYS;
	case RTSP_TIME_FORMAT_ABSOLUTE:
		/* TODO */
		ULOGE("unsupported time format: %d", time->format);
		return -ENOSYS;
	default:
		ULOGE("unknown time format: %d", time->format);
		return -EINVAL;
	}

	return 0;
}


static int rtsp_methods_write(uint32_t methods, struct rtsp_string *str)
{
	int ret = 0;
	int first = 1;

	ULOG_ERRNO_RETURN_ERR_IF(methods == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	if (methods & RTSP_METHOD_FLAG_OPTIONS) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_OPTIONS,
			   "");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_DESCRIBE) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_DESCRIBE,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_ANNOUNCE) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_ANNOUNCE,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_SETUP) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_SETUP,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_PLAY) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_PLAY,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_PAUSE) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_PAUSE,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_TEARDOWN) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_TEARDOWN,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_GET_PARAMETER) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_GET_PARAMETER,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_SET_PARAMETER) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_SET_PARAMETER,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_REDIRECT) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_REDIRECT,
			   (first) ? "" : ",");
		first = 0;
	}
	if (methods & RTSP_METHOD_FLAG_RECORD) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s" RTSP_METHOD_RECORD,
			   (first) ? "" : ",");
		first = 0;
	}

	return ret;
}


static int rtsp_methods_read(char *str, uint32_t *methods)
{
	char *method, *temp;
	uint32_t _methods = 0;

	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(methods == NULL, EINVAL);

	method = strtok_r(str, ",", &temp);
	while (method) {
		while (*method == ' ')
			method++;
		if (strcmp(method, RTSP_METHOD_OPTIONS) == 0)
			_methods |= RTSP_METHOD_FLAG_OPTIONS;
		else if (strcmp(method, RTSP_METHOD_DESCRIBE) == 0)
			_methods |= RTSP_METHOD_FLAG_DESCRIBE;
		else if (strcmp(method, RTSP_METHOD_ANNOUNCE) == 0)
			_methods |= RTSP_METHOD_FLAG_ANNOUNCE;
		else if (strcmp(method, RTSP_METHOD_SETUP) == 0)
			_methods |= RTSP_METHOD_FLAG_SETUP;
		else if (strcmp(method, RTSP_METHOD_PLAY) == 0)
			_methods |= RTSP_METHOD_FLAG_PLAY;
		else if (strcmp(method, RTSP_METHOD_PAUSE) == 0)
			_methods |= RTSP_METHOD_FLAG_PAUSE;
		else if (strcmp(method, RTSP_METHOD_TEARDOWN) == 0)
			_methods |= RTSP_METHOD_FLAG_TEARDOWN;
		else if (strcmp(method, RTSP_METHOD_GET_PARAMETER) == 0)
			_methods |= RTSP_METHOD_FLAG_GET_PARAMETER;
		else if (strcmp(method, RTSP_METHOD_SET_PARAMETER) == 0)
			_methods |= RTSP_METHOD_FLAG_SET_PARAMETER;
		else if (strcmp(method, RTSP_METHOD_REDIRECT) == 0)
			_methods |= RTSP_METHOD_FLAG_REDIRECT;
		else if (strcmp(method, RTSP_METHOD_RECORD) == 0)
			_methods |= RTSP_METHOD_FLAG_RECORD;
		method = strtok_r(NULL, ",", &temp);
	}

	*methods = _methods;
	return 0;
}


/**
 * RTSP Allow header
 * see RFC 2326 chapter 12.4
 */
int rtsp_allow_header_write(uint32_t methods, struct rtsp_string *str)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(methods == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_HEADER_ALLOW ": ");

	ret = rtsp_methods_write(methods, str);
	if (ret < 0)
		return ret;

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP Allow header
 * see RFC 2326 chapter 12.4
 */
int rtsp_allow_header_read(char *str, uint32_t *methods)
{
	return rtsp_methods_read(str, methods);
}


/**
 * RTSP Public header
 * see RFC 2326 chapter 12.28, redirecting to RFC 2068 chapter 14.35
 */
int rtsp_public_header_write(uint32_t methods, struct rtsp_string *str)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(methods == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_HEADER_PUBLIC ": ");

	ret = rtsp_methods_write(methods, str);
	if (ret < 0)
		return ret;

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP Public header
 * see RFC 2326 chapter 12.28, redirecting to RFC 2068 chapter 14.35
 */
int rtsp_public_header_read(char *str, uint32_t *methods)
{
	return rtsp_methods_read(str, methods);
}


/**
 * RTSP Range header
 * see RFC 2326 chapter 12.29
 */
int rtsp_range_header_write(const struct rtsp_range *range,
			    struct rtsp_string *str)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		(range->start.format != range->stop.format) &&
			(range->stop.format != RTSP_TIME_FORMAT_UNKNOWN),
		EINVAL);

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_HEADER_RANGE ": ");

	switch (range->start.format) {
	case RTSP_TIME_FORMAT_NPT:
		CHECK_FUNC(
			rtsp_sprintf, ret, return ret, str, RTSP_TIME_NPT "=");
		if (!range->start.npt.infinity) {
			ret = rtsp_time_write(&range->start, str);
			if (ret < 0)
				return ret;
		}
		if (range->stop.format == RTSP_TIME_FORMAT_NPT) {
			/* Start and stop cannot be both infinity */
			ULOG_ERRNO_RETURN_ERR_IF(
				range->start.npt.infinity &&
					(range->start.npt.infinity ==
					 range->stop.npt.infinity),
				EINVAL);
			/* Stop cannot be now */
			ULOG_ERRNO_RETURN_ERR_IF(range->stop.npt.now, EINVAL);
			CHECK_FUNC(rtsp_sprintf, ret, return ret, str, "-");
			if (!range->stop.npt.infinity) {
				ret = rtsp_time_write(&range->stop, str);
				if (ret < 0)
					return ret;
			}
		}
		break;
	case RTSP_TIME_FORMAT_SMPTE:
		/* TODO */
		ULOGE("unsupported time format: %d", range->start.format);
		return -ENOSYS;
	case RTSP_TIME_FORMAT_ABSOLUTE:
		/* TODO */
		ULOGE("unsupported time format: %d", range->start.format);
		return -ENOSYS;
	default:
		ULOGE("unknown time format: %d", range->start.format);
		return -EINVAL;
	}

	if (range->time > 0) {
		char time_str[22];
		ret = time_local_format(range->time,
					0,
					TIME_FMT_ISO8601_SHORT,
					time_str,
					sizeof(time_str));
		if (ret < 0) {
			ULOG_ERRNO("time_local_format", -ret);
			return ret;
		}
		/* Replace the +0000 time zone by 'Z' */
		time_str[15] = 'Z';
		time_str[16] = '\0';
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   ";" RTSP_RANGE_TIME "=%s",
			   time_str);
	}

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP Range header
 * see RFC 2326 chapter 12.29
 */
int rtsp_range_header_read(char *str, struct rtsp_range *range)
{
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);

	char *s;
	char *start_str = NULL, *stop_str = NULL, *time_str = NULL;
	int ret;

	memset(range, 0, sizeof(*range));
	range->time = 0;

	s = strchr(str, ';');
	if (s) {
		time_str = s + 1;
		*s = '\0';
	}

	s = strchr(str, '=');
	if (s == NULL) {
		ULOGE("%s:%d: malformed string", __func__, __LINE__);
		return -EINVAL;
	}

	*s = '\0';
	start_str = s + 1;

	s = strchr(start_str, '-');
	if (s != NULL) {
		*s = '\0';
		stop_str = s + 1;
	}

	if (strcmp(str, RTSP_TIME_NPT) == 0) {
		/* Normal Play Time (NPT) */
		range->start.format = RTSP_TIME_FORMAT_NPT;
		if (strlen(start_str)) {
			ret = rtsp_time_read(start_str, &range->start);
			if (ret < 0)
				return ret;
		} else {
			range->start.npt.infinity = 1;
		}
		if (stop_str != NULL) {
			range->stop.format = RTSP_TIME_FORMAT_NPT;
			if (strlen(stop_str)) {
				ret = rtsp_time_read(stop_str, &range->stop);
				if (ret < 0)
					return ret;
			} else {
				range->stop.npt.infinity = 1;
			}
		}

	} else if (strcmp(str, RTSP_TIME_SMPTE) == 0) {
		/* SMPTE Relative Timestamps */
		range->start.format = RTSP_TIME_FORMAT_SMPTE;
		range->stop.format = RTSP_TIME_FORMAT_SMPTE;
		/* TODO */
		ULOGE("unsupported time format: %s", str);
		return -ENOSYS;

	} else if (strcmp(str, RTSP_TIME_ABSOLUTE) == 0) {
		/* Absolute Time (UTC, ISO 8601) */
		range->start.format = RTSP_TIME_FORMAT_ABSOLUTE;
		range->stop.format = RTSP_TIME_FORMAT_ABSOLUTE;
		/* TODO */
		ULOGE("unsupported time format: %s", str);
		return -ENOSYS;

	} else {
		ULOGE("unknown time format: %s", str);
		return -EINVAL;
	}

	/* If stop time is present it must be of the same format as start */
	ULOG_ERRNO_RETURN_ERR_IF(
		(range->stop.format != RTSP_TIME_FORMAT_UNKNOWN) &&
			(range->stop.format != range->start.format),
		EINVAL);
	if (range->stop.format != RTSP_TIME_FORMAT_NPT) {
		/* Start and stop cannot be both infinity */
		ULOG_ERRNO_RETURN_ERR_IF(range->start.npt.infinity &&
						 (range->start.npt.infinity ==
						  range->stop.npt.infinity),
					 EINVAL);
		/* Stop cannot be now */
		ULOG_ERRNO_RETURN_ERR_IF(range->stop.npt.now, EINVAL);
	}

	/* 'time' */
	if ((time_str) &&
	    (strncmp(time_str, RTSP_RANGE_TIME, strlen(RTSP_RANGE_TIME)) ==
	     0)) {
		uint64_t epoch_sec = 0;
		int32_t utc_offset_sec = 0;
		s = strchr(time_str, '=');
		if (s == NULL) {
			ULOGE("%s:%d: malformed string", __func__, __LINE__);
			return -EINVAL;
		}
		time_str = s + 1;
		ret = time_local_parse(time_str, &epoch_sec, &utc_offset_sec);
		if (ret < 0) {
			ULOG_ERRNO("time_local_parse", -ret);
			return ret;
		}
		range->time = epoch_sec;
	}

	return 0;
}


/**
 * RTSP Session header
 * see RFC 2326 chapter 12.37
 */
int rtsp_session_header_write(char *session_id,
			      unsigned int session_timeout,
			      struct rtsp_string *str)
{
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id[0] == '\0', EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	CHECK_FUNC(rtsp_sprintf,
		   ret,
		   return ret,
		   str,
		   RTSP_HEADER_SESSION ": %s",
		   session_id);

	if (session_timeout > 0) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   ";" RTSP_SESSION_TIMEOUT "=%d",
			   session_timeout);
	}

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP Session header
 * see RFC 2326 chapter 12.37
 */
int rtsp_session_header_read(char *str,
			     char **session_id,
			     unsigned int *session_timeout)
{
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_timeout == NULL, EINVAL);

	char *p3 = strchr(str, ';');
	char *timeout_str = NULL;

	*session_timeout = 0;
	if (p3) {
		timeout_str = p3 + 1;
		*p3 = '\0';
	}
	if ((timeout_str) && (strncmp(timeout_str,
				      RTSP_SESSION_TIMEOUT,
				      strlen(RTSP_SESSION_TIMEOUT)) == 0)) {
		char *p4 = strchr(timeout_str, '=');
		if (p4)
			*session_timeout = atoi(p4 + 1);
	}

	*session_id = strdup(str);
	return 0;
}


/**
 * RTSP RTP-Info header
 * see RFC 2326 chapter 12.33
 */
struct rtsp_rtp_info_header *rtsp_rtp_info_header_new(void)
{
	struct rtsp_rtp_info_header *rtp_info = calloc(1, sizeof(*rtp_info));
	ULOG_ERRNO_RETURN_VAL_IF(rtp_info == NULL, ENOMEM, NULL);

	return rtp_info;
}


/**
 * RTSP RTP-Info header
 * see RFC 2326 chapter 12.33
 */
int rtsp_rtp_info_header_free(struct rtsp_rtp_info_header **rtp_info)
{
	ULOG_ERRNO_RETURN_ERR_IF(rtp_info == NULL, EINVAL);

	if (*rtp_info != NULL)
		xfree((void **)&(*rtp_info)->url);

	xfree((void **)rtp_info);

	return 0;
}


/**
 * RTSP RTP-Info header
 * see RFC 2326 chapter 12.33
 */
int rtsp_rtp_info_header_copy(const struct rtsp_rtp_info_header *src,
			      struct rtsp_rtp_info_header *dst)
{
	ULOG_ERRNO_RETURN_ERR_IF(src == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst == NULL, EINVAL);

	dst->url = xstrdup(src->url);
	dst->seq_valid = src->seq_valid;
	dst->seq = src->seq;
	dst->rtptime_valid = src->rtptime_valid;
	dst->rtptime = src->rtptime;

	return 0;
}


/**
 * RTSP RTP-Info header
 * see RFC 2326 chapter 12.33
 */
int rtsp_rtp_info_header_write(struct rtsp_rtp_info_header *const *rtp_info,
			       unsigned int count,
			       struct rtsp_string *str)
{
	int ret = 0;
	unsigned int i;
	const struct rtsp_rtp_info_header *rtpi;

	ULOG_ERRNO_RETURN_ERR_IF(rtp_info == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	CHECK_FUNC(
		rtsp_sprintf, ret, return ret, str, RTSP_HEADER_RTP_INFO ": ");

	/* Loop on RTP info */
	for (i = 0; i < count; i++) {
		rtpi = rtp_info[i];
		if (rtpi == NULL) {
			ULOGW("%s: invalid pointer", __func__);
			continue;
		}
		if ((rtpi->url == NULL) || (rtpi->url[0] == '\0')) {
			ULOGW("%s: rtp-info: invalid url", __func__);
			continue;
		}

		/* 'url' */
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_RTP_INFO_URL "=%s",
			   rtpi->url);

		/* 'seq' */
		if (rtpi->seq_valid) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_RTP_INFO_SEQ "=%d",
				   rtpi->seq);
		}

		/* 'rtptime' */
		if (rtpi->rtptime_valid) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_RTP_INFO_RTPTIME "=%" PRIu32,
				   rtpi->rtptime);
		}

		/* Separator for next RTP info */
		if (i != count - 1)
			CHECK_FUNC(rtsp_sprintf, ret, return ret, str, ",");
	}

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP RTP-Info header
 * see RFC 2326 chapter 12.33
 */
int rtsp_rtp_info_header_read(char *str,
			      struct rtsp_rtp_info_header **rtp_info,
			      unsigned int max_count,
			      unsigned int *count)
{
	int ret = 0;
	unsigned int _count = 0;
	struct rtsp_rtp_info_header *rtpi;
	char *rtpi_str, *param, *temp, *temp2, *temp3;
	char *key, *val;

	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(rtp_info == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(max_count == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == NULL, EINVAL);

	/* Loop on RTP info */
	rtpi_str = strtok_r(str, ",", &temp);
	while ((rtpi_str) && (_count < max_count)) {
		param = strtok_r(str, ";", &temp2);

		if (param == NULL) {
			rtpi_str = strtok_r(NULL, ",", &temp);
			continue;
		}

		rtpi = rtsp_rtp_info_header_new();
		if (rtpi == NULL) {
			ret = -ENOMEM;
			break;
		}

		/* 'url' */
		key = strtok_r(param, "=", &temp3);
		val = strtok_r(NULL, "", &temp3);
		if ((key == NULL) || (strcmp(key, RTSP_RTP_INFO_URL) != 0) ||
		    (val == NULL)) {
			ULOGE("%s: invalid url", __func__);
			rtsp_rtp_info_header_free(&rtpi);
			ret = -EPROTO;
			break;
		}
		rtpi->url = xstrdup(val);

		param = strtok_r(NULL, ";", &temp2);
		while (param) {
			key = strtok_r(param, "=", &temp3);
			val = strtok_r(NULL, "", &temp3);

			if (key == NULL) {
				ULOGE("invalid RTSP Header key");
				rtsp_rtp_info_header_free(&rtpi);
				ret = -EINVAL;
				goto exit;
			}
			/* 'seq' */
			if ((strcmp(key, RTSP_RTP_INFO_SEQ) == 0) && (val)) {
				rtpi->seq = atoi(val);
				rtpi->seq_valid = 1;
			}

			/* 'rtptime' */
			if ((strcmp(key, RTSP_RTP_INFO_RTPTIME) == 0) &&
			    (val)) {
				char *endptr = NULL;
				errno = 0;
				uint32_t parsedint = strtoul(val, &endptr, 10);
				if ((val[0] == '\0') || (endptr[0] != '\0') ||
				    (errno != 0)) {
					ULOGE("%s: invalid rtptime: '%s'",
					      __func__,
					      val);
					rtsp_rtp_info_header_free(&rtpi);
					ret = -errno;
					goto exit;
				} else {
					rtpi->rtptime = parsedint;
					rtpi->rtptime_valid = 1;
				}
			}

			param = strtok_r(NULL, ";", &temp2);
		}

		rtp_info[_count++] = rtpi;

		rtpi_str = strtok_r(NULL, ",", &temp);
	}

exit:
	*count = _count;

	return ret;
}


/**
 * RTSP Transport header
 * see RFC 2326 chapter 12.39
 */
struct rtsp_transport_header *rtsp_transport_header_new(void)
{
	struct rtsp_transport_header *transport = calloc(1, sizeof(*transport));
	ULOG_ERRNO_RETURN_VAL_IF(transport == NULL, ENOMEM, NULL);

	return transport;
}


/**
 * RTSP Transport header
 * see RFC 2326 chapter 12.39
 */
int rtsp_transport_header_free(struct rtsp_transport_header **transport)
{
	ULOG_ERRNO_RETURN_ERR_IF(transport == NULL, EINVAL);

	if (*transport != NULL) {
		xfree((void **)&(*transport)->transport_protocol);
		xfree((void **)&(*transport)->transport_profile);
		xfree((void **)&(*transport)->destination);
		xfree((void **)&(*transport)->source);
	}

	xfree((void **)transport);

	return 0;
}


/**
 * RTSP Transport header
 * see RFC 2326 chapter 12.39
 */
int rtsp_transport_header_copy(const struct rtsp_transport_header *src,
			       struct rtsp_transport_header *dst)
{
	ULOG_ERRNO_RETURN_ERR_IF(src == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst == NULL, EINVAL);

	dst->transport_protocol = xstrdup(src->transport_protocol);
	dst->transport_profile = xstrdup(src->transport_profile);
	dst->lower_transport = src->lower_transport;
	dst->delivery = src->delivery;
	dst->destination = xstrdup(src->destination);
	dst->source = xstrdup(src->source);
	dst->layers = src->layers;
	dst->method = src->method;
	dst->append = src->append;
	dst->ttl = src->ttl;
	dst->dst_stream_port = src->dst_stream_port;
	dst->dst_control_port = src->dst_control_port;
	dst->src_stream_port = src->src_stream_port;
	dst->src_control_port = src->src_control_port;
	dst->ssrc_valid = src->ssrc_valid;
	dst->ssrc = src->ssrc;

	return 0;
}


/**
 * RTSP Transport header
 * see RFC 2326 chapter 12.39
 */
int rtsp_transport_header_write(struct rtsp_transport_header *const *transport,
				unsigned int count,
				struct rtsp_string *str)
{
	int ret = 0;
	unsigned int i;
	const char *lower_transport;
	const struct rtsp_transport_header *trsp;

	ULOG_ERRNO_RETURN_ERR_IF(transport == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);

	CHECK_FUNC(
		rtsp_sprintf, ret, return ret, str, RTSP_HEADER_TRANSPORT ": ");

	/* Loop on transports */
	for (i = 0; i < count; i++) {
		trsp = transport[i];
		if (trsp == NULL) {
			ULOGW("%s: invalid pointer", __func__);
			continue;
		}
		if ((trsp->transport_protocol == NULL) ||
		    (trsp->transport_protocol[0] == '\0')) {
			ULOGW("%s: invalid transport protocol", __func__);
			continue;
		}
		if ((trsp->transport_profile == NULL) ||
		    (trsp->transport_profile[0] == '\0')) {
			ULOGW("%s: invalid transport profile", __func__);
			continue;
		}

		lower_transport =
			rtsp_lower_transport_str(trsp->lower_transport);
		if (lower_transport == NULL)
			lower_transport = RTSP_TRANSPORT_LOWER_UDP;

		/* transport-protocol/profile[/lower-transport] */
		/* ;(unicast|multicast) */
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s/%s/%s;%s",
			   trsp->transport_protocol,
			   trsp->transport_profile,
			   lower_transport,
			   (trsp->delivery == RTSP_DELIVERY_UNICAST)
				   ? RTSP_TRANSPORT_UNICAST
				   : RTSP_TRANSPORT_MULTICAST);

		/* 'destination' */
		if (trsp->destination != NULL) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_TRANSPORT_DESTINATION "=%s",
				   trsp->destination);
		}

		/* 'source' */
		if (trsp->source != NULL) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_TRANSPORT_SOURCE "=%s",
				   trsp->source);
		}

		/* 'append' */
		if (trsp->append) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_TRANSPORT_APPEND);
		}

		/* 'ttl' */
		if (trsp->ttl) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_TRANSPORT_TTL "=%d",
				   trsp->ttl);
		}

		/* 'layers' */
		if (trsp->layers) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_TRANSPORT_LAYERS "=%d",
				   trsp->layers);
		}

		if (trsp->delivery == RTSP_DELIVERY_UNICAST) {
			/* 'client_port' */
			if (trsp->dst_stream_port) {
				uint16_t control_port = trsp->dst_control_port;
				if (control_port == 0) {
					control_port =
						trsp->dst_stream_port + 1;
				}
				CHECK_FUNC(rtsp_sprintf,
					   ret,
					   return ret,
					   str,
					   ";" RTSP_TRANSPORT_CLIENT_PORT
					   "=%" PRIu16 "-%" PRIu16,
					   trsp->dst_stream_port,
					   control_port);
			}

			/* 'server_port' */
			if (trsp->src_stream_port) {
				uint16_t control_port = trsp->src_control_port;
				if (control_port == 0) {
					control_port =
						trsp->src_stream_port + 1;
				}
				CHECK_FUNC(rtsp_sprintf,
					   ret,
					   return ret,
					   str,
					   ";" RTSP_TRANSPORT_SERVER_PORT
					   "=%" PRIu16 "-%" PRIu16,
					   trsp->src_stream_port,
					   control_port);
			}
		} else {
			/* 'port' */
			if (trsp->dst_stream_port) {
				uint16_t control_port = trsp->dst_control_port;
				if (control_port == 0)
					control_port =
						trsp->dst_stream_port + 1;
				CHECK_FUNC(rtsp_sprintf,
					   ret,
					   return ret,
					   str,
					   ";" RTSP_TRANSPORT_PORT "=%" PRIu16
					   "-%" PRIu16,
					   trsp->dst_stream_port,
					   control_port);
			}
		}

		/* 'ssrc' */
		if (trsp->ssrc_valid) {
			CHECK_FUNC(rtsp_sprintf,
				   ret,
				   return ret,
				   str,
				   ";" RTSP_TRANSPORT_SSRC "=%08X",
				   trsp->ssrc);
		}

		/* 'mode' */
		if (trsp->method != RTSP_TRANSPORT_METHOD_UNKNOWN) {
			CHECK_FUNC(
				rtsp_sprintf,
				ret,
				return ret,
				str,
				";" RTSP_TRANSPORT_MODE "=%s",
				(trsp->method == RTSP_TRANSPORT_METHOD_RECORD)
					? RTSP_TRANSPORT_MODE_RECORD
					: RTSP_TRANSPORT_MODE_PLAY);
		}

		/* Separator for next transport */
		if (i != count - 1)
			CHECK_FUNC(rtsp_sprintf, ret, return ret, str, ",");
	}

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * Process each key/value from header
 */
static void process_key_val(struct rtsp_transport_header *trsp,
			    const char *key,
			    const char *val)
{
	char *val2;

	/* 'unicast' */
	if (strcmp(key, RTSP_TRANSPORT_UNICAST) == 0) {
		trsp->delivery = RTSP_DELIVERY_UNICAST;
		goto out;
	}

	/* 'multicast' */
	if (strcmp(key, RTSP_TRANSPORT_MULTICAST) == 0) {
		trsp->delivery = RTSP_DELIVERY_MULTICAST;
		goto out;
	}

	/* 'destination' */
	if (strcmp(key, RTSP_TRANSPORT_DESTINATION) == 0) {
		if (val)
			trsp->destination = strdup(val);
		goto out;
	}

	/* 'source' */
	if (strcmp(key, RTSP_TRANSPORT_SOURCE) == 0) {
		if (val)
			trsp->source = strdup(val);
		goto out;
	}

	/* 'append' */
	if (strcmp(key, RTSP_TRANSPORT_APPEND) == 0) {
		trsp->append = 1;
		goto out;
	}

	/* 'ttl' */
	if (strcmp(key, RTSP_TRANSPORT_TTL) == 0) {
		if (val)
			trsp->ttl = atoi(val);
		goto out;
	}

	/* 'layers' */
	if (strcmp(key, RTSP_TRANSPORT_LAYERS) == 0) {
		if (val)
			trsp->layers = atoi(val);
		goto out;
	}

	/* 'port' */
	if (strcmp(key, RTSP_TRANSPORT_PORT) == 0) {
		if (val) {
			val2 = strchr(val, '-');
			trsp->dst_stream_port = atoi(val);
			trsp->dst_control_port = trsp->dst_stream_port + 1;
			if (val2)
				trsp->dst_control_port = atoi(val2 + 1);
		}
		goto out;
	}

	/* 'client_port' */
	if (strcmp(key, RTSP_TRANSPORT_CLIENT_PORT) == 0) {
		if (val) {
			val2 = strchr(val, '-');
			trsp->dst_stream_port = atoi(val);
			trsp->dst_control_port = trsp->dst_stream_port + 1;
			if (val2)
				trsp->dst_control_port = atoi(val2 + 1);
		}
		goto out;
	}

	/* 'server_port' */
	if (strcmp(key, RTSP_TRANSPORT_SERVER_PORT) == 0) {
		if (val) {
			val2 = strchr(val, '-');
			trsp->src_stream_port = atoi(val);
			trsp->src_control_port = trsp->src_stream_port + 1;
			if (val2)
				trsp->src_control_port = atoi(val2 + 1);
		}
		goto out;
	}

	/* 'ssrc' */
	if (strcmp(key, RTSP_TRANSPORT_SSRC) == 0) {
		if (val)
			if (sscanf(val, "%08X", &trsp->ssrc) == 1)
				trsp->ssrc_valid = 1;
		goto out;
	}

	/* 'mode' */
	if (strcmp(key, RTSP_TRANSPORT_MODE) == 0) {
		if (val) {
			/* clang-format off */
			if (!strncasecmp(
					val,
					RTSP_TRANSPORT_MODE_PLAY,
					strlen(RTSP_TRANSPORT_MODE_PLAY))) {
				trsp->method = RTSP_TRANSPORT_METHOD_PLAY;
			} else if (!strncasecmp(
					val,
					RTSP_TRANSPORT_MODE_RECORD,
					strlen(RTSP_TRANSPORT_MODE_RECORD))) {
				trsp->method = RTSP_TRANSPORT_METHOD_RECORD;
			}
			/* clang-format on */
		}
		goto out;
	}
out:
	return;
}


/**
 * RTSP Transport header
 * see RFC 2326 chapter 12.39
 */
int rtsp_transport_header_read(char *str,
			       struct rtsp_transport_header **transport,
			       unsigned int max_count,
			       unsigned int *count)
{
	int ret = 0;
	unsigned int _count = 0;
	struct rtsp_transport_header *trsp;
	char *trsp_str, *param, *temp, *temp2, *temp3;
	char *key, *val;

	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(transport == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(max_count == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == NULL, EINVAL);

	/* Loop on transports */
	trsp_str = strtok_r(str, ",", &temp);
	while ((trsp_str) && (_count < max_count)) {
		param = strtok_r(str, ";", &temp2);

		if (param == NULL) {
			trsp_str = strtok_r(NULL, ",", &temp);
			continue;
		}

		trsp = rtsp_transport_header_new();
		if (trsp == NULL) {
			ret = -ENOMEM;
			break;
		}

		/* 'transport-protocol' */
		val = strtok_r(param, "/", &temp3);
		if (val == NULL) {
			ULOGE("%s: invalid transport protocol", __func__);
			rtsp_transport_header_free(&trsp);
			ret = -EPROTO;
			break;
		} else if (strcmp(val, RTSP_TRANSPORT_PROTOCOL_RTP) != 0) {
			ULOGE("%s: unsupported transport protocol", __func__);
			rtsp_transport_header_free(&trsp);
			ret = -EPROTO;
			break;
		}
		trsp->transport_protocol = xstrdup(val);

		/* 'transport-profile' */
		val = strtok_r(NULL, "/", &temp3);
		if (val == NULL) {
			ULOGE("%s: invalid transport profile", __func__);
			rtsp_transport_header_free(&trsp);
			ret = -EPROTO;
			break;
		} else if (strcmp(val, RTSP_TRANSPORT_PROFILE_AVP) != 0) {
			ULOGE("%s: unsupported transport profile", __func__);
			rtsp_transport_header_free(&trsp);
			ret = -EPROTO;
			break;
		}
		trsp->transport_profile = xstrdup(val);

		/* 'lower-transport' (optional, default is "UDP") */
		val = strtok_r(NULL, "/", &temp3);
		if (val == NULL) {
			trsp->lower_transport = RTSP_LOWER_TRANSPORT_UDP;
		} else if (strcmp(val, RTSP_TRANSPORT_LOWER_UDP) == 0) {
			trsp->lower_transport = RTSP_LOWER_TRANSPORT_UDP;
		} else if (strcmp(val, RTSP_TRANSPORT_LOWER_MUX) == 0) {
			trsp->lower_transport = RTSP_LOWER_TRANSPORT_MUX;
		} else {
			ULOGE("%s: unsupported lower transport", __func__);
			rtsp_transport_header_free(&trsp);
			ret = -EPROTO;
			break;
		}

		param = strtok_r(NULL, ";", &temp2);
		while (param) {
			key = strtok_r(param, "=", &temp3);
			val = strtok_r(NULL, "", &temp3);

			if (key == NULL)
				ULOGW("no key");
			else
				process_key_val(trsp, key, val);

			param = strtok_r(NULL, ";", &temp2);
		}

		transport[_count++] = trsp;

		trsp_str = strtok_r(NULL, ",", &temp);
	}

	*count = _count;

	return ret;
}


/**
 * RTSP Request
 * see RFC 2326 chapter 6
 */
int rtsp_request_header_clear(struct rtsp_request_header *header)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	xfree((void **)&header->uri);
	xfree((void **)&header->session_id);
	for (i = 0; i < header->transport_count; i++)
		rtsp_transport_header_free(&header->transport[i]);
	xfree((void **)&header->content_type);
	xfree((void **)&header->user_agent);
	xfree((void **)&header->server);
	xfree((void **)&header->accept);

	for (i = 0; i < header->ext_count; i++) {
		xfree((void **)&header->ext[i].key);
		xfree((void **)&header->ext[i].value);
	}
	free(header->ext);

	memset(header, 0, sizeof(*header));

	return 0;
}


/**
 * RTSP Request
 * see RFC 2326 chapter 6
 */
int rtsp_request_header_copy(const struct rtsp_request_header *src,
			     struct rtsp_request_header *dst)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(src == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst == NULL, EINVAL);

	dst->method = src->method;
	dst->uri = xstrdup(src->uri);
	dst->cseq = src->cseq;
	dst->date = src->date;
	dst->session_id = xstrdup(src->session_id);
	dst->session_timeout = src->session_timeout;
	for (i = 0; i < src->transport_count; i++) {
		struct rtsp_transport_header *t = rtsp_transport_header_new();
		if (t == NULL)
			return -ENOMEM;
		rtsp_transport_header_copy(src->transport[i], t);
		dst->transport[i] = t;
	}
	dst->transport_count = src->transport_count;
	dst->content_type = xstrdup(src->content_type);
	dst->scale = src->scale;
	dst->user_agent = xstrdup(src->user_agent);
	dst->server = xstrdup(src->server);
	dst->accept = xstrdup(src->accept);
	dst->range = src->range;
	dst->content_length = src->content_length;
	int ret = rtsp_request_header_copy_ext(dst, src->ext, src->ext_count);
	if (ret < 0)
		return ret;

	return 0;
}


/**
 * RTSP Request
 * see RFC 2326 chapter 6
 */
int rtsp_request_header_copy_ext(struct rtsp_request_header *header,
				 const struct rtsp_header_ext *ext,
				 size_t ext_count)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	if (ext_count == 0)
		return 0;

	ULOG_ERRNO_RETURN_ERR_IF(ext == NULL, EINVAL);

	header->ext = calloc(ext_count, sizeof(struct rtsp_header_ext));
	if (header->ext == NULL)
		return -ENOMEM;
	header->ext_count = ext_count;
	for (i = 0; i < ext_count; i++) {
		header->ext[i].key = xstrdup(ext[i].key);
		header->ext[i].value = xstrdup(ext[i].value);
	}

	return 0;
}


/**
 * RTSP Request
 * see RFC 2326 chapter 6
 */
int rtsp_request_header_write(const struct rtsp_request_header *header,
			      struct rtsp_string *str)
{
	int ret;
	const char *method;

	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header->uri == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header->uri[0] == '\0', EINVAL);

	method = rtsp_method_type_str(header->method);
	ULOG_ERRNO_RETURN_ERR_IF(method == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strcmp(method, "UNKNOWN") == 0, EINVAL);

	/* Request line */
	CHECK_FUNC(rtsp_sprintf,
		   ret,
		   return ret,
		   str,
		   "%s %s " RTSP_VERSION RTSP_CRLF,
		   method,
		   header->uri);

	/* 'CSeq' */
	if (header->cseq >= 0) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CSEQ ": %d" RTSP_CRLF,
			   header->cseq);
	}

	/* 'Date' */
	if (header->date > 0) {
		char time_str[32];
		ret = time_local_format(header->date,
					0,
					TIME_FMT_RFC1123,
					time_str,
					sizeof(time_str));
		if (ret < 0) {
			ULOG_ERRNO("time_local_format", -ret);
			return ret;
		}
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_DATE ": %s" RTSP_CRLF,
			   time_str);
	}

	/* 'Session' */
	if ((header->session_id != NULL) && (header->session_id[0] != '\0')) {
		ret = rtsp_session_header_write(
			header->session_id, header->session_timeout, str);
		if (ret < 0)
			return ret;
	}

	/* 'Transport' */
	if (header->transport_count > 0) {
		ret = rtsp_transport_header_write(
			header->transport, header->transport_count, str);
		if (ret < 0)
			return ret;
	}

	/* 'Content-Type' */
	if ((header->content_type != NULL) &&
	    (header->content_type[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_TYPE ": %s" RTSP_CRLF,
			   header->content_type);
	}

	/* 'Scale' */
	if (header->scale != 0.) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_SCALE ": %.2f" RTSP_CRLF,
			   header->scale);
	}

	/* 'User-Agent' */
	if ((header->user_agent != NULL) && (header->user_agent[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_USER_AGENT ": %s" RTSP_CRLF,
			   header->user_agent);
	}

	/* 'Server' */
	if ((header->server != NULL) && (header->server[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_SERVER ": %s" RTSP_CRLF,
			   header->server);
	}

	/* 'Accept' */
	if ((header->accept != NULL) && (header->accept[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_ACCEPT ": %s" RTSP_CRLF,
			   header->accept);
	}

	/* 'Range' */
	if (header->range.start.format != RTSP_TIME_FORMAT_UNKNOWN) {
		ret = rtsp_range_header_write(&header->range, str);
		if (ret < 0)
			return ret;
	}

	/* 'Content-Length' */
	if (header->content_length > 0) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_LENGTH ": %d" RTSP_CRLF,
			   header->content_length);
	}

	/* Header extensions */
	for (size_t i = 0; i < header->ext_count; i++) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s: %s" RTSP_CRLF,
			   header->ext[i].key,
			   header->ext[i].value);
	}

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP Request
 * see RFC 2326 chapter 6
 */
int rtsp_request_header_read(char *str,
			     size_t len,
			     struct rtsp_request_header *header,
			     char **body)
{
	int ret;
	char *p, *temp, *temp2;
	char *method, *uri, *version;
	size_t nl_len, i;

	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	rtsp_request_header_clear(header);

	/* Find the body (double line terminator) */
	if (body)
		*body = NULL;
	p = find_double_newline(str, len, &nl_len);
	if (p) {
		if (body)
			*body = p + nl_len;
		for (i = 0; i < nl_len; i++)
			p[i] = '\0';
	} else {
		ULOGE("%s: end of header not found", __func__);
		return -EPROTO;
	}

	p = strtok_r(str, RTSP_CRLF, &temp);
	if (!p) {
		ULOGE("%s: invalid request data", __func__);
		return -EPROTO;
	}

	/* Request line */
	method = strtok_r(p, " ", &temp2);
	uri = strtok_r(NULL, " ", &temp2);
	version = strtok_r(NULL, RTSP_CRLF, &temp2);

	header->method = rtsp_method_type_enum(method);
	if (header->method == RTSP_METHOD_TYPE_UNKNOWN) {
		ULOGE("%s: unknown or invalid method", __func__);
		return -EPROTO;
	}

	if ((uri == NULL) || (uri[0] == '\0')) {
		ULOGE("%s: invalid URI", __func__);
		return -EPROTO;
	}
	header->uri = strdup(uri);

	if ((version == NULL) || (strcmp(version, RTSP_VERSION) != 0)) {
		ULOGE("%s: invalid RTSP protocol version", __func__);
		return -EPROTO;
	}

	p = strtok_r(NULL, RTSP_CRLF, &temp);
	while (p) {
		char *field, *value, *p2;

		p2 = strchr(p, ':');
		if (p2) {
			*p2 = '\0';
			field = p;
			value = p2 + 1;
			if (*value == ' ')
				value++;

			if (!strncasecmp(field,
					 RTSP_HEADER_CSEQ,
					 strlen(RTSP_HEADER_CSEQ))) {
				/* 'CSeq' */
				header->cseq = atoi(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_DATE,
						strlen(RTSP_HEADER_DATE))) {
				/* 'Date' */
				uint64_t epoch_sec = 0;
				int32_t utc_offset_sec = 0;
				ret = time_local_parse(
					value, &epoch_sec, &utc_offset_sec);
				if (ret < 0) {
					ULOG_ERRNO("time_local_parse", -ret);
					return ret;
				}
				header->date = epoch_sec;

			} else if (!strncasecmp(field,
						RTSP_HEADER_SESSION,
						strlen(RTSP_HEADER_SESSION))) {
				/* 'Session' */
				ret = rtsp_session_header_read(
					value,
					&header->session_id,
					&header->session_timeout);
				if (ret < 0)
					return ret;

			} else if (!strncasecmp(
					   field,
					   RTSP_HEADER_TRANSPORT,
					   strlen(RTSP_HEADER_TRANSPORT))) {
				/* 'Transport' */
				ret = rtsp_transport_header_read(
					value,
					header->transport,
					RTSP_TRANSPORT_MAX_COUNT,
					&header->transport_count);
				if ((ret < 0) || (header->transport_count == 0))
					return ret;

			} else if (!strncasecmp(
					   field,
					   RTSP_HEADER_CONTENT_TYPE,
					   strlen(RTSP_HEADER_CONTENT_TYPE))) {
				/* 'Content-Type' */
				header->content_type = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_SCALE,
						strlen(RTSP_HEADER_SCALE))) {
				/* 'Scale' */
				header->scale = atof(value);

			} else if (!strncasecmp(
					   field,
					   RTSP_HEADER_USER_AGENT,
					   strlen(RTSP_HEADER_USER_AGENT))) {
				/* 'User-Agent' */
				free(header->user_agent);
				header->user_agent = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_SERVER,
						strlen(RTSP_HEADER_SERVER))) {
				/* 'Server' */
				header->server = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_ACCEPT,
						strlen(RTSP_HEADER_ACCEPT))) {
				/* 'Accept' */
				header->accept = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_RANGE,
						strlen(RTSP_HEADER_RANGE))) {
				/* 'Range' */
				ret = rtsp_range_header_read(value,
							     &header->range);
				if (ret < 0)
					return ret;

			} else if (
				!strncasecmp(
					field,
					RTSP_HEADER_CONTENT_LENGTH,
					strlen(RTSP_HEADER_CONTENT_LENGTH))) {
				/* 'Content-Length' */
				header->content_length = atoi(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_EXT,
						strlen(RTSP_HEADER_EXT))) {
				/* 'X-*' header extension */
				struct rtsp_header_ext *tmp = realloc(
					header->ext,
					(header->ext_count + 1) *
						sizeof(struct rtsp_header_ext));
				if (tmp == NULL)
					return -ENOMEM;
				header->ext = tmp;
				header->ext[header->ext_count].key =
					strdup(field);
				header->ext[header->ext_count].value =
					strdup(value);
				header->ext_count += 1;
			}
		}

		p = strtok_r(NULL, RTSP_CRLF, &temp);
	}

	return 0;
}


/**
 * RTSP Response
 * see RFC 2326 chapter 7
 */
int rtsp_response_header_clear(struct rtsp_response_header *header)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	xfree((void **)&header->status_string);
	xfree((void **)&header->session_id);
	rtsp_transport_header_free(&header->transport);
	xfree((void **)&header->content_type);
	for (i = 0; i < header->rtp_info_count; i++)
		rtsp_rtp_info_header_free(&header->rtp_info[i]);
	xfree((void **)&header->server);
	xfree((void **)&header->content_encoding);
	xfree((void **)&header->content_language);
	xfree((void **)&header->content_base);
	xfree((void **)&header->content_location);

	for (i = 0; i < header->ext_count; i++) {
		xfree((void **)&header->ext[i].key);
		xfree((void **)&header->ext[i].value);
	}
	free(header->ext);

	memset(header, 0, sizeof(*header));

	return 0;
}


/**
 * RTSP Response
 * see RFC 2326 chapter 7
 */
int rtsp_response_header_copy(const struct rtsp_response_header *src,
			      struct rtsp_response_header *dst)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(src == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst == NULL, EINVAL);

	dst->status_code = src->status_code;
	dst->status_string = xstrdup(src->status_string);
	dst->cseq = src->cseq;
	dst->date = src->date;
	dst->session_id = xstrdup(src->session_id);
	dst->session_timeout = src->session_timeout;
	if (src->transport) {
		struct rtsp_transport_header *t = rtsp_transport_header_new();
		if (t == NULL)
			return -ENOMEM;
		rtsp_transport_header_copy(src->transport, t);
		dst->transport = t;
	} else {
		dst->transport = NULL;
	}
	dst->content_type = xstrdup(src->content_type);
	dst->scale = src->scale;
	dst->public_methods = src->public_methods;
	dst->allowed_methods = src->allowed_methods;
	for (i = 0; i < src->rtp_info_count; i++) {
		struct rtsp_rtp_info_header *r = rtsp_rtp_info_header_new();
		if (r == NULL)
			return -ENOMEM;
		rtsp_rtp_info_header_copy(src->rtp_info[i], r);
		dst->rtp_info[i] = r;
	}
	dst->rtp_info_count = src->rtp_info_count;
	dst->server = xstrdup(src->server);
	dst->range = src->range;
	dst->content_length = src->content_length;
	dst->content_encoding = xstrdup(src->content_encoding);
	dst->content_language = xstrdup(src->content_language);
	dst->content_base = xstrdup(src->content_base);
	dst->content_location = xstrdup(src->content_location);
	int ret = rtsp_response_header_copy_ext(dst, src->ext, src->ext_count);
	if (ret < 0)
		return ret;

	return 0;
}


/**
 * RTSP Response
 * see RFC 2326 chapter 7
 */
int rtsp_response_header_copy_ext(struct rtsp_response_header *header,
				  const struct rtsp_header_ext *ext,
				  size_t ext_count)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	if (ext_count == 0)
		return 0;

	ULOG_ERRNO_RETURN_ERR_IF(ext == NULL, EINVAL);

	header->ext = calloc(ext_count, sizeof(struct rtsp_header_ext));
	if (header->ext == NULL)
		return -ENOMEM;
	header->ext_count = ext_count;
	for (i = 0; i < ext_count; i++) {
		header->ext[i].key = xstrdup(ext[i].key);
		header->ext[i].value = xstrdup(ext[i].value);
	}

	return 0;
}


/**
 * RTSP Response
 * see RFC 2326 chapter 7
 */
int rtsp_response_header_write(const struct rtsp_response_header *header,
			       struct rtsp_string *str)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header->status_code == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header->status_string == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header->status_string[0] == '\0', EINVAL);

	/* Status line */
	CHECK_FUNC(rtsp_sprintf,
		   ret,
		   return ret,
		   str,
		   RTSP_VERSION " %d %s" RTSP_CRLF,
		   header->status_code,
		   header->status_string);

	/* 'CSeq' */
	if (header->cseq >= 0) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CSEQ ": %d" RTSP_CRLF,
			   header->cseq);
	}

	/* 'Date' */
	if (header->date > 0) {
		char time_str[32];
		ret = time_local_format(header->date,
					0,
					TIME_FMT_RFC1123,
					time_str,
					sizeof(time_str));
		if (ret < 0) {
			ULOG_ERRNO("time_local_format", -ret);
			return ret;
		}
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_DATE ": %s" RTSP_CRLF,
			   time_str);
	}

	/* 'Session' */
	if ((header->session_id != NULL) && (header->session_id[0] != '\0')) {
		ret = rtsp_session_header_write(
			header->session_id, header->session_timeout, str);
		if (ret < 0)
			return ret;
	}

	/* 'Transport' */
	if (header->transport != NULL) {
		ret = rtsp_transport_header_write(&header->transport, 1, str);
		if (ret < 0)
			return ret;
	}

	/* 'Content-Type' */
	if ((header->content_type != NULL) &&
	    (header->content_type[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_TYPE ": %s" RTSP_CRLF,
			   header->content_type);
	}

	/* 'Scale' */
	if (header->scale != 0.) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_SCALE ": %.2f" RTSP_CRLF,
			   header->scale);
	}

	/* 'Public' */
	if (header->public_methods > 0) {
		ret = rtsp_public_header_write(header->public_methods, str);
		if (ret < 0)
			return ret;
	}

	/* 'Allow' */
	if (header->allowed_methods > 0) {
		ret = rtsp_allow_header_write(header->allowed_methods, str);
		if (ret < 0)
			return ret;
	}

	/* 'RTP-Info' */
	if (header->rtp_info_count > 0) {
		ret = rtsp_rtp_info_header_write(
			header->rtp_info, header->rtp_info_count, str);
		if (ret < 0)
			return ret;
	}

	/* 'Server' */
	if ((header->server != NULL) && (header->server[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_SERVER ": %s" RTSP_CRLF,
			   header->server);
	}

	/* 'Range' */
	if (header->range.start.format != RTSP_TIME_FORMAT_UNKNOWN) {
		ret = rtsp_range_header_write(&header->range, str);
		if (ret < 0)
			return ret;
	}

	/* 'Content-Length' */
	CHECK_FUNC(rtsp_sprintf,
		   ret,
		   return ret,
		   str,
		   RTSP_HEADER_CONTENT_LENGTH ": %d" RTSP_CRLF,
		   header->content_length);

	/* 'Content-Encoding' */
	if ((header->content_encoding != NULL) &&
	    (header->content_encoding[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_ENCODING ": %s" RTSP_CRLF,
			   header->content_encoding);
	}

	/* 'Content-Language' */
	if ((header->content_language != NULL) &&
	    (header->content_language[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_LANGUAGE ": %s" RTSP_CRLF,
			   header->content_language);
	}

	/* 'Content-Base' */
	if ((header->content_base != NULL) &&
	    (header->content_base[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_BASE ": %s" RTSP_CRLF,
			   header->content_base);
	}

	/* 'Content-Location' */
	if ((header->content_location != NULL) &&
	    (header->content_location[0] != '\0')) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   RTSP_HEADER_CONTENT_LOCATION ": %s" RTSP_CRLF,
			   header->content_location);
	}

	/* Header extensions */
	for (size_t i = 0; i < header->ext_count; i++) {
		CHECK_FUNC(rtsp_sprintf,
			   ret,
			   return ret,
			   str,
			   "%s: %s" RTSP_CRLF,
			   header->ext[i].key,
			   header->ext[i].value);
	}

	CHECK_FUNC(rtsp_sprintf, ret, return ret, str, RTSP_CRLF);

	return ret;
}


/**
 * RTSP Response
 * see RFC 2326 chapter 7
 */
int rtsp_response_header_read(char *msg,
			      size_t len,
			      struct rtsp_response_header *header,
			      char **body)
{
	int ret;
	char *p, *temp, *temp2;
	char *version, *status_code_str, *status_string;
	size_t nl_len, i;

	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(header == NULL, EINVAL);

	rtsp_response_header_clear(header);

	/* Find the body (double line terminator) */
	if (body)
		*body = NULL;
	p = find_double_newline(msg, len, &nl_len);
	if (p) {
		if (body)
			*body = p + nl_len;
		for (i = 0; i < nl_len; i++)
			p[i] = '\0';
	} else {
		ULOGW("%s: end of header not found", __func__);
		return -EPROTO;
	}

	p = strtok_r(msg, RTSP_CRLF, &temp);
	if (!p) {
		ULOGE("%s: invalid response data", __func__);
		return -EPROTO;
	}

	/* Status line */
	version = strtok_r(p, " ", &temp2);
	status_code_str = strtok_r(NULL, " ", &temp2);
	status_string = strtok_r(NULL, RTSP_CRLF, &temp2);

	if ((!version) || (strcmp(version, RTSP_VERSION) != 0)) {
		ULOGE("%s: invalid RTSP protocol version", __func__);
		return -EPROTO;
	}
	if ((!status_code_str) || (!status_string)) {
		ULOGE("%s: malformed RTSP response", __func__);
		return -EPROTO;
	}
	header->status_code = atoi(status_code_str);
	header->status_string = strdup(status_string);

	p = strtok_r(NULL, RTSP_CRLF, &temp);
	while (p) {
		char *field, *value, *p2;

		p2 = strchr(p, ':');
		if (p2) {
			*p2 = '\0';
			field = p;
			value = p2 + 1;
			if (*value == ' ')
				value++;

			if (!strncasecmp(field,
					 RTSP_HEADER_CSEQ,
					 strlen(RTSP_HEADER_CSEQ))) {
				/* 'CSeq' */
				header->cseq = atoi(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_DATE,
						strlen(RTSP_HEADER_DATE))) {
				/* 'Date' */
				uint64_t epoch_sec = 0;
				int32_t utc_offset_sec = 0;
				ret = time_local_parse(
					value, &epoch_sec, &utc_offset_sec);
				if (ret < 0) {
					ULOG_ERRNO("time_local_parse", -ret);
					return ret;
				}
				header->date = epoch_sec;

			} else if (!strncasecmp(field,
						RTSP_HEADER_SESSION,
						strlen(RTSP_HEADER_SESSION))) {
				/* 'Session' */
				ret = rtsp_session_header_read(
					value,
					&header->session_id,
					&header->session_timeout);
				if (ret < 0)
					return ret;

			} else if (!strncasecmp(
					   field,
					   RTSP_HEADER_TRANSPORT,
					   strlen(RTSP_HEADER_TRANSPORT))) {
				/* 'Transport' */
				unsigned int transport_count = 0;
				ret = rtsp_transport_header_read(
					value,
					&header->transport,
					1,
					&transport_count);
				if ((ret < 0) || (transport_count == 0))
					return ret;

			} else if (!strncasecmp(
					   field,
					   RTSP_HEADER_CONTENT_TYPE,
					   strlen(RTSP_HEADER_CONTENT_TYPE))) {
				/* 'Content-Type' */
				header->content_type = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_SCALE,
						strlen(RTSP_HEADER_SCALE))) {
				/* 'Scale' */
				header->scale = atof(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_PUBLIC,
						strlen(RTSP_HEADER_PUBLIC))) {
				/* 'Public' */
				ret = rtsp_public_header_read(
					value, &header->public_methods);
				if (ret < 0)
					return ret;

			} else if (!strncasecmp(field,
						RTSP_HEADER_ALLOW,
						strlen(RTSP_HEADER_ALLOW))) {
				/* 'Allow' */
				ret = rtsp_allow_header_read(
					value, &header->allowed_methods);
				if (ret < 0)
					return ret;

			} else if (!strncasecmp(field,
						RTSP_HEADER_RTP_INFO,
						strlen(RTSP_HEADER_RTP_INFO))) {
				/* 'RTP-Info' */
				ret = rtsp_rtp_info_header_read(
					value,
					header->rtp_info,
					1,
					&header->rtp_info_count);
				if ((ret < 0) || (header->rtp_info_count == 0))
					return ret;

			} else if (!strncasecmp(field,
						RTSP_HEADER_SERVER,
						strlen(RTSP_HEADER_SERVER))) {
				/* 'Server' */
				header->server = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_RANGE,
						strlen(RTSP_HEADER_RANGE))) {
				/* 'Range' */
				ret = rtsp_range_header_read(value,
							     &header->range);
				if (ret < 0)
					return ret;

			} else if (
				!strncasecmp(
					field,
					RTSP_HEADER_CONTENT_LENGTH,
					strlen(RTSP_HEADER_CONTENT_LENGTH))) {
				/* 'Content-Length' */
				header->content_length = atoi(value);

			} else if (
				!strncasecmp(
					field,
					RTSP_HEADER_CONTENT_ENCODING,
					strlen(RTSP_HEADER_CONTENT_ENCODING))) {
				/* 'Content-Encoding' */
				header->content_encoding = strdup(value);

			} else if (
				!strncasecmp(
					field,
					RTSP_HEADER_CONTENT_LANGUAGE,
					strlen(RTSP_HEADER_CONTENT_LANGUAGE))) {
				/* 'Content-Language' */
				header->content_language = strdup(value);

			} else if (!strncasecmp(
					   field,
					   RTSP_HEADER_CONTENT_BASE,
					   strlen(RTSP_HEADER_CONTENT_BASE))) {
				/* 'Content-Base' */
				header->content_base = strdup(value);

			} else if (
				!strncasecmp(
					field,
					RTSP_HEADER_CONTENT_LOCATION,
					strlen(RTSP_HEADER_CONTENT_LOCATION))) {
				/* 'Content-Location' */
				header->content_location = strdup(value);

			} else if (!strncasecmp(field,
						RTSP_HEADER_EXT,
						strlen(RTSP_HEADER_EXT))) {
				/* 'X-*' header extension */
				struct rtsp_header_ext *tmp = realloc(
					header->ext,
					(header->ext_count + 1) *
						sizeof(struct rtsp_header_ext));
				if (tmp == NULL)
					return -ENOMEM;
				header->ext = tmp;
				header->ext[header->ext_count].key =
					strdup(field);
				header->ext[header->ext_count].value =
					strdup(value);
				header->ext_count += 1;
			}
		}

		p = strtok_r(NULL, RTSP_CRLF, &temp);
	}

	return 0;
}


void rtsp_buffer_remove_first_bytes(struct pomp_buffer *buffer, size_t count)
{
	int ret;
	uint8_t *data;
	size_t len;
	ssize_t rem;
	ULOG_ERRNO_RETURN_IF(buffer == NULL, EINVAL);

	if (count == 0)
		return;

	ret = pomp_buffer_get_data(buffer, (void **)&data, &len, NULL);
	if (ret < 0) {
		ULOG_ERRNO("pomp_buffer_get_data", -ret);
		return;
	}

	rem = len - count;
	if (rem < 0) {
		ULOGE("%s: trying to remove %zu bytes from a "
		      "buffer containing only %zu bytes",
		      __func__,
		      count,
		      len);
		rem = 0;
	}

	if (rem > 0)
		memmove(data, &data[count], rem);

	ret = pomp_buffer_set_len(buffer, rem);
	if (ret < 0)
		ULOG_ERRNO("pomp_buffer_set_len", -ret);
}


void rtsp_message_clear(struct rtsp_message *msg)
{
	if (!msg)
		return;

	switch (msg->type) {
	case RTSP_MESSAGE_TYPE_REQUEST:
		rtsp_request_header_clear(&msg->header.req);
		break;
	case RTSP_MESSAGE_TYPE_RESPONSE:
		rtsp_response_header_clear(&msg->header.resp);
		break;
	default:
		break;
	}

	memset(msg, 0, sizeof(*msg));
}


/**
 * Reads the next header (+optional body) from data
 * If no header is found, or if the body is not complete, returns -EAGAIN.
 * If msg->total_len is non-zero, this number of bytes should be removed
 * from the _front_ of the data buffer before next call, regardless of the
 * return code. This can be used to skip a bad header.
 * If the return code is zero, then msg contains information about a
 * complete request/response, depending on its "type" field.
 */
int rtsp_get_next_message(struct pomp_buffer *data,
			  struct rtsp_message *msg,
			  struct rtsp_message_parser_ctx *ctx)
{
	int ret;
	void *raw_data;
	char *header_end;
	size_t len, nl_len;

	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);
	rtsp_message_clear(msg);

	ULOG_ERRNO_RETURN_ERR_IF(data == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ctx == NULL, EINVAL);

	/* Get pomp_buffer info */
	ret = pomp_buffer_get_data(data, &raw_data, &len, NULL);
	if (ret < 0) {
		ULOG_ERRNO("pomp_buffer_get_data", -ret);
		return ret;
	}

	if (ctx->msg.type == RTSP_MESSAGE_TYPE_UNKNOWN) {
		/* Search for first double newline: end of header */
		header_end = find_double_newline(raw_data, len, &nl_len);
		if (!header_end)
			return -EAGAIN;

		ctx->header_len = header_end - (char *)raw_data + nl_len;

		/* Check if it is a request or a response */
		if (ctx->header_len >= strlen(RTSP_VERSION) &&
		    strncmp(raw_data, RTSP_VERSION, strlen(RTSP_VERSION)) == 0)
			ctx->msg.type = RTSP_MESSAGE_TYPE_RESPONSE;
		else
			ctx->msg.type = RTSP_MESSAGE_TYPE_REQUEST;

		/* Try to parse the request or response */
		if (ctx->msg.type == RTSP_MESSAGE_TYPE_REQUEST) {
			ret = rtsp_request_header_read(raw_data,
						       ctx->header_len,
						       &ctx->msg.header.req,
						       NULL);
			if (ret < 0)
				ULOG_ERRNO("rtsp_request_header_read", -ret);
			ctx->msg.body_len = ctx->msg.header.req.content_length;
		} else {
			ret = rtsp_response_header_read(raw_data,
							ctx->header_len,
							&ctx->msg.header.resp,
							NULL);
			if (ret < 0)
				ULOG_ERRNO("rtsp_response_header_read", -ret);
			ctx->msg.body_len = ctx->msg.header.resp.content_length;
		}
		if (ret < 0) {
			rtsp_message_clear(&ctx->msg);
			msg->total_len = ctx->header_len;
			return ret;
		}
	}

	ctx->msg.total_len = ctx->msg.body_len + ctx->header_len;
	if (len < ctx->msg.total_len)
		return -EAGAIN;

	if (ctx->msg.type == RTSP_MESSAGE_TYPE_REQUEST)
		rtsp_request_header_copy(&ctx->msg.header.req,
					 &msg->header.req);
	else
		rtsp_response_header_copy(&ctx->msg.header.resp,
					  &msg->header.resp);
	msg->type = ctx->msg.type;
	/* Pointer to the pomp_buffer must be set only when the message is
	 * complete to avoid pointer invalidation due to pomp_buffer realloc */
	msg->body = (char *)raw_data + ctx->header_len;
	msg->body_len = ctx->msg.body_len;
	msg->total_len = ctx->msg.total_len;

	rtsp_message_clear(&ctx->msg);

	return 0;
}


int rtsp_range_get_duration_us(const struct rtsp_range *range,
			       int64_t *duration)
{
	int64_t diff;

	ULOG_ERRNO_RETURN_ERR_IF(!range || !duration, EINVAL);

	/* Only handle ranges in NPT format */
	ULOG_ERRNO_RETURN_ERR_IF(
		(range->start.format != RTSP_TIME_FORMAT_NPT) ||
			(range->stop.format != RTSP_TIME_FORMAT_NPT),
		EINVAL);

	/* If the end is infinity, report the max possible duration */
	if (range->stop.npt.infinity) {
		*duration = INT64_MAX;
		return 0;
	}

	/* If the beginning is infinity, report the max possible
	 * negative duration */
	if (range->start.npt.infinity) {
		*duration = INT64_MIN;
		return 0;
	}

	/* If any of the values are 'now', we can't compute anything */
	ULOG_ERRNO_RETURN_ERR_IF(range->start.npt.now || range->stop.npt.now,
				 EINVAL);

	/* Compute actual diff */
	diff = range->stop.npt.sec * INT64_C(1000000);
	diff += range->stop.npt.usec;
	diff -= range->start.npt.sec * INT64_C(1000000);
	diff -= range->start.npt.usec;

	*duration = diff;
	return 0;
}
