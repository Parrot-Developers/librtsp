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

#ifndef _RTSP_PRIV_H_
#define _RTSP_PRIV_H_

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <futils/futils.h>
#include <futils/random.h>
#include <libpomp.h>
#include <rtsp/rtsp.h>


#define RTSP_DEFAULT_PORT 554

#define RTSP_SCHEME_TCP "rtsp://"
#define RTSP_SCHEME_UDP "rtspu://"

#define RTSP_VERSION "RTSP/1.0"

#define RTSP_CRLF "\r\n"

/* clang-format off */
#define RTSP_HEADER_ACCEPT		"Accept"		/* opt. */
#define RTSP_HEADER_ALLOW		"Allow"			/* opt. */
#define RTSP_HEADER_CONNECTION		"Connection"		/* TODO */
#define RTSP_HEADER_CONTENT_BASE	"Content-Base"		/* opt. */
#define RTSP_HEADER_CONTENT_ENCODING	"Content-Encoding"
#define RTSP_HEADER_CONTENT_LANGUAGE	"Content-Language"
#define RTSP_HEADER_CONTENT_LENGTH	"Content-Length"
#define RTSP_HEADER_CONTENT_LOCATION	"Content-Location"	/* opt. */
#define RTSP_HEADER_CONTENT_TYPE	"Content-Type"
#define RTSP_HEADER_CSEQ		"Cseq"
#define RTSP_HEADER_DATE		"Date"			/* opt. */
#define RTSP_HEADER_PROXY_REQUIRE	"Proxy-Require"		/* TODO */
#define RTSP_HEADER_PUBLIC		"Public"		/* opt. */
#define RTSP_HEADER_RANGE		"Range"			/* opt. */
#define RTSP_HEADER_REQUIRE		"Require"		/* TODO */
#define RTSP_HEADER_RTP_INFO		"RTP-Info"
#define RTSP_HEADER_SESSION		"Session"
#define RTSP_HEADER_SCALE		"Scale"			/* opt. */
#define RTSP_HEADER_SERVER		"Server"		/* opt. */
#define RTSP_HEADER_TRANSPORT		"Transport"
#define RTSP_HEADER_UNSUPPORTED		"Unsupported"		/* TODO */
#define RTSP_HEADER_USER_AGENT		"User-Agent"		/* opt. */
/* clang-format on */

#define RTSP_SESSION_TIMEOUT "timeout"

#define RTSP_CONTENT_TYPE_SDP "application/sdp"

#define RTSP_DATE_FORMAT_RFC1123 "%a, %d %b %Y %T %Z"


/**
 * RTSP Range header
 * see RFC 2326 chapter 12.29
 */

#define RTSP_RANGE_TIME "time"

#define RTSP_TIME_NPT "npt"
#define RTSP_TIME_NPT_NOW "now"
#define RTSP_TIME_SMPTE "smpte"
#define RTSP_TIME_SMPTE_30_DROP "smpte-30-drop"
#define RTSP_TIME_SMPTE_25 "smpte-25"
#define RTSP_TIME_ABSOLUTE "clock"
#define RTSP_TIME_FORMAT_ISO8601 "%Y%m%dT%H%M%SZ"


/**
 * RTSP RTP-Info header
 * see RFC 2326 chapter 12.33
 */

#define RTSP_RTP_INFO_MAX_COUNT 10

#define RTSP_RTP_INFO_URL "url"
#define RTSP_RTP_INFO_SEQ "seq"
#define RTSP_RTP_INFO_RTPTIME "rtptime"

struct rtsp_rtp_info_header {
	char *url;
	int seq_valid;
	uint16_t seq;
	int rtptime_valid;
	uint32_t rtptime;
};


/**
 * RTSP Transport header
 * see RFC 2326 chapter 12.39
 */

#define RTSP_TRANSPORT_MAX_COUNT 5

/* clang-format off */
#define RTSP_TRANSPORT_PROTOCOL_RTP	"RTP"
#define RTSP_TRANSPORT_PROFILE_AVP	"AVP"
#define RTSP_TRANSPORT_LOWER_UDP	"UDP"
#define RTSP_TRANSPORT_LOWER_TCP	"TCP"		/* not supported */
#define RTSP_TRANSPORT_LOWER_MUX	"MUX"		/* Parrot-specific */
#define RTSP_TRANSPORT_UNICAST		"unicast"
#define RTSP_TRANSPORT_MULTICAST	"multicast"
#define RTSP_TRANSPORT_DESTINATION	"destination"
#define RTSP_TRANSPORT_SOURCE		"source"
#define RTSP_TRANSPORT_LAYERS		"layers"
#define RTSP_TRANSPORT_MODE		"mode"
#define RTSP_TRANSPORT_MODE_PLAY	"PLAY"
#define RTSP_TRANSPORT_MODE_RECORD	"RECORD"
#define RTSP_TRANSPORT_APPEND		"append"
#define RTSP_TRANSPORT_INTERLEAVED	"interleaved"	/* not supported */
#define RTSP_TRANSPORT_TTL		"ttl"
#define RTSP_TRANSPORT_PORT		"port"
#define RTSP_TRANSPORT_CLIENT_PORT	"client_port"
#define RTSP_TRANSPORT_SERVER_PORT	"server_port"
#define RTSP_TRANSPORT_SSRC		"ssrc"
/* clang-format on */

enum rtsp_transport_method {
	RTSP_TRANSPORT_METHOD_UNKNOWN = 0,
	RTSP_TRANSPORT_METHOD_PLAY,
	RTSP_TRANSPORT_METHOD_RECORD,
};

struct rtsp_transport_header {
	char *transport_protocol;
	char *transport_profile;
	enum rtsp_lower_transport lower_transport;
	enum rtsp_delivery delivery;
	char *destination;
	char *source;
	unsigned int layers;
	enum rtsp_transport_method method;
	int append;
	unsigned int ttl;
	uint16_t src_stream_port;
	uint16_t src_control_port;
	uint16_t dst_stream_port;
	uint16_t dst_control_port;
	int ssrc_valid;
	uint32_t ssrc;
};


/**
 * RTSP Request
 * see RFC 2326 chapter 6
 */

#define RTSP_METHOD_OPTIONS "OPTIONS"
#define RTSP_METHOD_DESCRIBE "DESCRIBE"
#define RTSP_METHOD_ANNOUNCE "ANNOUNCE"
#define RTSP_METHOD_SETUP "SETUP"
#define RTSP_METHOD_PLAY "PLAY"
#define RTSP_METHOD_PAUSE "PAUSE"
#define RTSP_METHOD_TEARDOWN "TEARDOWN"
#define RTSP_METHOD_GET_PARAMETER "GET_PARAMETER"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"
#define RTSP_METHOD_REDIRECT "REDIRECT"
#define RTSP_METHOD_RECORD "RECORD"

struct rtsp_request_header {
	/* Request line */
	enum rtsp_method_type method;
	char *uri;

	/* General header */
	int cseq;
	time_t date;
	char *session_id;
	unsigned int session_timeout;
	struct rtsp_transport_header *transport[RTSP_TRANSPORT_MAX_COUNT];
	unsigned int transport_count;
	char *content_type;
	float scale;

	/* Request header */
	char *user_agent;
	char *server;
	char *accept;
	struct rtsp_range range;

	/* Entity header */
	int content_length;
};


/**
 * RTSP Response
 * see RFC 2326 chapter 7
 */

#define RTSP_STATUS_CLASS_INFORMATIONAL 1
#define RTSP_STATUS_CLASS_SUCCESS 2
#define RTSP_STATUS_CLASS_REDIRECTION 3
#define RTSP_STATUS_CLASS_CLIENT_ERROR 4
#define RTSP_STATUS_CLASS_SERVER_ERROR 5

#define RTSP_STATUS_CLASS(_status) ((_status) / 100)
#define RTSP_STATUS_CODE(_status) (RTSP_STATUS_CODE_##_status)
#define RTSP_STATUS_STRING(_status) (RTSP_STATUS_STRING_##_status)

#define RTSP_STATUS_CODE_CONTINUE 100
#define RTSP_STATUS_STRING_CONTINUE "Continue"
#define RTSP_STATUS_CODE_OK 200
#define RTSP_STATUS_STRING_OK "OK"
#define RTSP_STATUS_CODE_CREATED 201
#define RTSP_STATUS_STRING_CREATED "Created"
#define RTSP_STATUS_CODE_LOW_ON_STORAGE 250
#define RTSP_STATUS_STRING_LOW_ON_STORAGE "Low On Storage Space"
#define RTSP_STATUS_CODE_MULTIPLE_CHOICES 300
#define RTSP_STATUS_STRING_MULTIPLE_CHOICES "Multiple Choices"
#define RTSP_STATUS_CODE_MOVED_PERMANENTLY 301
#define RTSP_STATUS_STRING_MOVED_PERMANENTLY "Moved Permanently"
#define RTSP_STATUS_CODE_MOVED_TEMPORARITY 302
#define RTSP_STATUS_STRING_MOVED_TEMPORARITY "Moved Temporarily"
#define RTSP_STATUS_CODE_SEE_OTHER 303
#define RTSP_STATUS_STRING_SEE_OTHER "See Other"
#define RTSP_STATUS_CODE_NOT_MODIFIED 304
#define RTSP_STATUS_STRING_NOT_MODIFIED "Not Modified"
#define RTSP_STATUS_CODE_USE_PROXY 305
#define RTSP_STATUS_STRING_USE_PROXY "Use Proxy"
#define RTSP_STATUS_CODE_BAD_REQUEST 400
#define RTSP_STATUS_STRING_BAD_REQUEST "Bad Request"
#define RTSP_STATUS_CODE_UNAUTHORIZED 401
#define RTSP_STATUS_STRING_UNAUTHORIZED "Unauthorized"
#define RTSP_STATUS_CODE_PAYMENT_REQUIRED 402
#define RTSP_STATUS_STRING_PAYMENT_REQUIRED "Payment Required"
#define RTSP_STATUS_CODE_FORBIDDEN 403
#define RTSP_STATUS_STRING_FORBIDDEN "Forbidden"
#define RTSP_STATUS_CODE_NOT_FOUND 404
#define RTSP_STATUS_STRING_NOT_FOUND "Not Found"
#define RTSP_STATUS_CODE_METHOD_NOT_ALLOWED 405
#define RTSP_STATUS_STRING_METHOD_NOT_ALLOWED "Method Not Allowed"
#define RTSP_STATUS_CODE_NOT_ACCEPTABLE 406
#define RTSP_STATUS_STRING_NOT_ACCEPTABLE "Not Acceptable"
#define RTSP_STATUS_CODE_PROXY_AUTHENTICATION_REQUIRED 407
#define RTSP_STATUS_STRING_PROXY_AUTHENTICATION_REQUIRED                       \
	"Proxy Authentication Required"
#define RTSP_STATUS_CODE_REQUEST_TIMEOUT 408
#define RTSP_STATUS_STRING_REQUEST_TIMEOUT "Request Time-out"
#define RTSP_STATUS_CODE_GONE 410
#define RTSP_STATUS_STRING_GONE "Gone"
#define RTSP_STATUS_CODE_LENGTH_REQUIRED 411
#define RTSP_STATUS_STRING_LENGTH_REQUIRED "Length Required"
#define RTSP_STATUS_CODE_PRECONDITION_FAILED 412
#define RTSP_STATUS_STRING_PRECONDITION_FAILED "Precondition Failed"
#define RTSP_STATUS_CODE_REQUEST_ENTITY_TOO_LARGE 413
#define RTSP_STATUS_STRING_REQUEST_ENTITY_TOO_LARGE "Request Entity Too Large"
#define RTSP_STATUS_CODE_REQUEST_URI_TOO_LARGE 414
#define RTSP_STATUS_STRING_REQUEST_URI_TOO_LARGE "Request-URI Too Large"
#define RTSP_STATUS_CODE_UNSUPPORTED_MEDIA_TYPE 415
#define RTSP_STATUS_STRING_UNSUPPORTED_MEDIA_TYPE "Unsupported Media Type"
#define RTSP_STATUS_CODE_PARMETER_NOT_UNDERSTOOD 451
#define RTSP_STATUS_STRING_PARMETER_NOT_UNDERSTOOD "Parameter Not Understood"
#define RTSP_STATUS_CODE_CONFERENCE_NOT_FOUND 452
#define RTSP_STATUS_STRING_CONFERENCE_NOT_FOUND "Conference Not Found"
#define RTSP_STATUS_CODE_NOT_ENOUGH_BANDWIDTH 453
#define RTSP_STATUS_STRING_NOT_ENOUGH_BANDWIDTH "Not Enough Bandwidth"
#define RTSP_STATUS_CODE_SESSION_NOT_FOUND 454
#define RTSP_STATUS_STRING_SESSION_NOT_FOUND "Session Not Found"
#define RTSP_STATUS_CODE_METHOD_NOT_VALID 455
#define RTSP_STATUS_STRING_METHOD_NOT_VALID "Method Not Valid In This State"
#define RTSP_STATUS_CODE_HEADER_FIELD_NOT_VALID 456
#define RTSP_STATUS_STRING_HEADER_FIELD_NOT_VALID                              \
	"Header Field Not Valid For Resource"
#define RTSP_STATUS_CODE_INVALID_RANGE 457
#define RTSP_STATUS_STRING_INVALID_RANGE "Invalid Range"
#define RTSP_STATUS_CODE_PARAMETER_READ_ONLY 458
#define RTSP_STATUS_STRING_PARAMETER_READ_ONLY "Parameter Is Read-Only"
#define RTSP_STATUS_CODE_AGGREGATE_OPERATION_NOT_ALLOWED 459
#define RTSP_STATUS_STRING_AGGREGATE_OPERATION_NOT_ALLOWED                     \
	"Aggregate Operation Not Allowed"
#define RTSP_STATUS_CODE_ONLY_AGGREGATE_OPERATION_ALLOWED 460
#define RTSP_STATUS_STRING_ONLY_AGGREGATE_OPERATION_ALLOWED                    \
	"Only Aggregate Operation Allowed"
#define RTSP_STATUS_CODE_UNSUPPORTED_TRANSPORT 461
#define RTSP_STATUS_STRING_UNSUPPORTED_TRANSPORT "Unsupported Transport"
#define RTSP_STATUS_CODE_DESTINATION_UNREACHABLE 462
#define RTSP_STATUS_STRING_DESTINATION_UNREACHABLE "Destination Unreachable"
#define RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR 500
#define RTSP_STATUS_STRING_INTERNAL_SERVER_ERROR "Internal Server Error"
#define RTSP_STATUS_CODE_NOT_IMPLEMENTED 501
#define RTSP_STATUS_STRING_NOT_IMPLEMENTED "Not Implemented"
#define RTSP_STATUS_CODE_BAD_GATEWAY 502
#define RTSP_STATUS_STRING_BAD_GATEWAY "Bad Gateway"
#define RTSP_STATUS_CODE_SERVICE_UNAVAILABLE 503
#define RTSP_STATUS_STRING_SERVICE_UNAVAILABLE "Service Unavailable"
#define RTSP_STATUS_CODE_GATEWAY_TIMEOUT 504
#define RTSP_STATUS_STRING_GATEWAY_TIMEOUT "Gateway Time-out"
#define RTSP_STATUS_CODE_RTSP_VERSION_NOT_SUPPORTED 505
#define RTSP_STATUS_STRING_RTSP_VERSION_NOT_SUPPORTED                          \
	"RTSP Version Not Supported"
#define RTSP_STATUS_CODE_OPTION_NOT_SUPPORTED 551
#define RTSP_STATUS_STRING_OPTION_NOT_SUPPORTED "Option Not Supported"

struct rtsp_response_header {
	/* Status line */
	int status_code;
	char *status_string;

	/* General header */
	int cseq;
	time_t date;
	char *session_id;
	unsigned int session_timeout;
	struct rtsp_transport_header *transport;
	char *content_type;
	float scale;

	/* Response header */
	uint32_t public_methods;
	uint32_t allowed_methods;
	struct rtsp_rtp_info_header *rtp_info[RTSP_RTP_INFO_MAX_COUNT];
	unsigned int rtp_info_count;
	char *server;
	struct rtsp_range range;

	/* Entity header */
	int content_length;
	char *content_encoding;
	char *content_language;
	char *content_base;
	char *content_location;
};


struct rtsp_string {
	char *str;
	size_t len;
	size_t max_len;
};


/**
 * RTSP message
 */

enum rtsp_message_type {
	RTSP_MESSAGE_TYPE_UNKNOWN = 0,
	RTSP_MESSAGE_TYPE_REQUEST,
	RTSP_MESSAGE_TYPE_RESPONSE,
};

struct rtsp_message {
	union {
		struct rtsp_request_header req;
		struct rtsp_response_header resp;
	} header;
	enum rtsp_message_type type;

	char *body;
	size_t body_len;
	size_t total_len;
};

struct rtsp_message_parser_ctx {
	struct rtsp_message msg;
	size_t header_len;
};


/**
 * Function prototypes
 */

void rtsp_status_get(int status, int *code, const char **str);


int rtsp_status_to_errno(int status);


const char *rtsp_status_str(int status);


int rtsp_url_parse(char *url, char **host, uint16_t *port, char **path);


int rtsp_allow_header_write(uint32_t methods, struct rtsp_string *str);


int rtsp_allow_header_read(char *str, uint32_t *methods);


int rtsp_public_header_write(uint32_t methods, struct rtsp_string *str);


int rtsp_public_header_read(char *str, uint32_t *methods);


int rtsp_range_header_write(const struct rtsp_range *range,
			    struct rtsp_string *str);


int rtsp_range_header_read(char *str, struct rtsp_range *range);


int rtsp_session_header_write(char *session_id,
			      unsigned int session_timeout,
			      struct rtsp_string *str);


int rtsp_session_header_read(char *str,
			     char **session_id,
			     unsigned int *session_timeout);


struct rtsp_rtp_info_header *rtsp_rtp_info_header_new(void);


int rtsp_rtp_info_header_free(struct rtsp_rtp_info_header **rtp_info);


int rtsp_rtp_info_header_copy(const struct rtsp_rtp_info_header *src,
			      struct rtsp_rtp_info_header *dst);


int rtsp_rtp_info_header_write(struct rtsp_rtp_info_header *const *rtp_info,
			       unsigned int count,
			       struct rtsp_string *str);


int rtsp_rtp_info_header_read(char *str,
			      struct rtsp_rtp_info_header **rtp_info,
			      unsigned int max_count,
			      unsigned int *count);


struct rtsp_transport_header *rtsp_transport_header_new(void);


int rtsp_transport_header_free(struct rtsp_transport_header **transport);


int rtsp_transport_header_copy(const struct rtsp_transport_header *src,
			       struct rtsp_transport_header *dst);


int rtsp_transport_header_write(struct rtsp_transport_header *const *transport,
				unsigned int count,
				struct rtsp_string *str);


int rtsp_transport_header_read(char *str,
			       struct rtsp_transport_header **transport,
			       unsigned int max_count,
			       unsigned int *count);


int rtsp_request_header_clear(struct rtsp_request_header *header);


int rtsp_request_header_copy(const struct rtsp_request_header *src,
			     struct rtsp_request_header *dst);


int rtsp_request_header_write(const struct rtsp_request_header *header,
			      struct rtsp_string *str);


int rtsp_request_header_read(char *str,
			     size_t len,
			     struct rtsp_request_header *header,
			     char **body);


int rtsp_response_header_clear(struct rtsp_response_header *header);


int rtsp_response_header_copy(const struct rtsp_response_header *src,
			      struct rtsp_response_header *dst);


int rtsp_response_header_write(const struct rtsp_response_header *header,
			       struct rtsp_string *str);


int rtsp_response_header_read(char *msg,
			      size_t len,
			      struct rtsp_response_header *header,
			      char **body);


void rtsp_buffer_remove_first_bytes(struct pomp_buffer *buffer, size_t count);


void rtsp_message_clear(struct rtsp_message *msg);


int rtsp_get_next_message(struct pomp_buffer *data,
			  struct rtsp_message *msg,
			  struct rtsp_message_parser_ctx *ctx);


#define CHECK_FUNC(_func, _ret, _on_err, ...)                                  \
	do {                                                                   \
		_ret = _func(__VA_ARGS__);                                     \
		if (_ret < 0) {                                                \
			ULOG_ERRNO(#_func, -_ret);                             \
			_on_err;                                               \
		}                                                              \
	} while (0)


/* clang-format off */
__attribute__((__format__(__printf__, 2, 3)))
static inline int rtsp_sprintf(struct rtsp_string *str, const char *fmt, ...)
/* clang-format on */
{
	int len;
	if (str->len >= str->max_len)
		return -ENOBUFS;
	va_list args;
	va_start(args, fmt);
	len = vsnprintf(
		str->str + str->len, str->max_len - str->len, fmt, args);
	va_end(args);
	if (len < 0)
		return len;
	if (len >= (signed)(str->max_len - str->len))
		return -ENOBUFS;
	str->len += len;
	return 0;
}


static inline void xfree(void **ptr)
{
	if (ptr) {
		free(*ptr);
		*ptr = NULL;
	}
}


static inline char *xstrdup(const char *s)
{
	return s == NULL ? NULL : strdup(s);
}


static inline void get_time_with_ms_delay(struct timespec *ts,
					  unsigned int delay)
{
	struct timeval tp;
	gettimeofday(&tp, NULL);

	struct timespec ts2;
	time_timeval_to_timespec(&tp, &ts2);
	time_timespec_add_us(&ts2, (int64_t)delay * 1000, ts);
}


static inline char get_last_char(const char *str)
{
	return str[strlen(str) - 1];
}


#endif /* !_RTSP_PRIV_H_ */
