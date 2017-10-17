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
*   * Neither the name of the Parrot Drones SAS Company nor the
*     names of its contributors may be used to endorse or promote products
*     derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT DRONES SAS COMPANY BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _RTSP_H_
#define _RTSP_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

#include <librtsp.h>
#include <libpomp.h>
#include <futils/futils.h>

#include "rtsp_log.h"


#define RTSP_DEFAULT_PORT 554

#define RTSP_SCHEME_TCP                 "rtsp://"
#define RTSP_SCHEME_UDP                 "rtspu://"

#define RTSP_CLIENT_DEFAULT_USER_AGENT  "librtsp_client"
#define RTSP_SERVER_DEFAULT_USER_AGENT  "librtsp_server"

#define RTSP_VERSION                    "RTSP/1.0"

#define RTSP_METHOD_OPTIONS             "OPTIONS"
#define RTSP_METHOD_DESCRIBE            "DESCRIBE"
#define RTSP_METHOD_ANNOUNCE            "ANNOUNCE"
#define RTSP_METHOD_SETUP               "SETUP"
#define RTSP_METHOD_PLAY                "PLAY"
#define RTSP_METHOD_PAUSE               "PAUSE"
#define RTSP_METHOD_TEARDOWN            "TEARDOWN"
#define RTSP_METHOD_GET_PARAMETER       "GET_PARAMETER"
#define RTSP_METHOD_SET_PARAMETER       "SET_PARAMETER"
#define RTSP_METHOD_REDIRECT            "REDIRECT"
#define RTSP_METHOD_RECORD              "RECORD"

#define RTSP_METHOD_FLAG_OPTIONS        0x00000001UL
#define RTSP_METHOD_FLAG_DESCRIBE       0x00000002UL
#define RTSP_METHOD_FLAG_ANNOUNCE       0x00000004UL
#define RTSP_METHOD_FLAG_SETUP          0x00000008UL
#define RTSP_METHOD_FLAG_PLAY           0x00000010UL
#define RTSP_METHOD_FLAG_PAUSE          0x00000020UL
#define RTSP_METHOD_FLAG_TEARDOWN       0x00000040UL
#define RTSP_METHOD_FLAG_GET_PARAMETER  0x00000080UL
#define RTSP_METHOD_FLAG_SET_PARAMETER  0x00000100UL
#define RTSP_METHOD_FLAG_REDIRECT       0x00000200UL
#define RTSP_METHOD_FLAG_RECORD         0x00000400UL

#define RTSP_HEADER_CSEQ                "Cseq"
#define RTSP_HEADER_CONNECTION          "Connection"
#define RTSP_HEADER_SESSION             "Session"
#define RTSP_HEADER_SESSION_TIMEOUT     "timeout"
#define RTSP_HEADER_TRANSPORT           "Transport"
#define RTSP_HEADER_PUBLIC              "Public"
#define RTSP_HEADER_CONTENT_LANGUAGE    "Content-Language"
#define RTSP_HEADER_CONTENT_ENCODING    "Content-Encoding"
#define RTSP_HEADER_CONTENT_LENGTH      "Content-Length"
#define RTSP_HEADER_CONTENT_TYPE        "Content-Type"
#define RTSP_HEADER_CONTENT_BASE        "Content-Base"
#define RTSP_HEADER_CONTENT_LOCATION    "Content-Location"
#define RTSP_HEADER_USER_AGENT          "User-Agent"
#define RTSP_HEADER_ACCEPT              "Accept"
#define RTSP_HEADER_RANGE               "Range"

#define RTSP_TRANSPORT_RTPAVP           "RTP/AVP"
#define RTSP_TRANSPORT_RTPAVPUDP        "RTP/AVP/UDP"
#define RTSP_TRANSPORT_UNICAST          "unicast"
#define RTSP_TRANSPORT_MULTICAST        "multicast"
#define RTSP_TRANSPORT_CLIENT_PORT      "client_port"
#define RTSP_TRANSPORT_SERVER_PORT      "server_port"

#define RTSP_CONTENT_TYPE_SDP           "application/sdp"

#define RTSP_STATUS_CLASS_INFORMATIONAL 1
#define RTSP_STATUS_CLASS_SUCCESS       2
#define RTSP_STATUS_CLASS_REDIRECTION   3
#define RTSP_STATUS_CLASS_CLIENT_ERROR  4
#define RTSP_STATUS_CLASS_SERVER_ERROR  5

#define RTSP_STATUS_CLASS(_status) ((_status) / 100)

#define RTSP_STATUS_CODE_CONTINUE 100
#define RTSP_STATUS_STRING_CONTINUE \
	"Continue"
#define RTSP_STATUS_CODE_OK 200
#define RTSP_STATUS_STRING_OK \
	"OK"
#define RTSP_STATUS_CODE_CREATED 201
#define RTSP_STATUS_STRING_CREATED \
	"Created"
#define RTSP_STATUS_CODE_LOW_ON_STORAGE 250
#define RTSP_STATUS_STRING_LOW_ON_STORAGE \
	"Low On Storage Space"
#define RTSP_STATUS_CODE_MULTIPLE_CHOICES 300
#define RTSP_STATUS_STRING_MULTIPLE_CHOICES \
	"Multiple Choices"
#define RTSP_STATUS_CODE_MOVED_PERMANENTLY 301
#define RTSP_STATUS_STRING_MOVED_PERMANENTLY \
	"Moved Permanently"
#define RTSP_STATUS_CODE_MOVED_TEMPORARITY 302
#define RTSP_STATUS_STRING_MOVED_TEMPORARITY \
	"Moved Temporarily"
#define RTSP_STATUS_CODE_SEE_OTHER 303
#define RTSP_STATUS_STRING_SEE_OTHER \
	"See Other"
#define RTSP_STATUS_CODE_NOT_MODIFIED 304
#define RTSP_STATUS_STRING_NOT_MODIFIED \
	"Not Modified"
#define RTSP_STATUS_CODE_USE_PROXY 305
#define RTSP_STATUS_STRING_USE_PROXY \
	"Use Proxy"
#define RTSP_STATUS_CODE_BAD_REQUEST 400
#define RTSP_STATUS_STRING_BAD_REQUEST \
	"Bad Request"
#define RTSP_STATUS_CODE_UNAUTHORIZED 401
#define RTSP_STATUS_STRING_UNAUTHORIZED \
	"Unauthorized"
#define RTSP_STATUS_CODE_PAYMENT_REQUIRED 402
#define RTSP_STATUS_STRING_PAYMENT_REQUIRED \
	"Payment Required"
#define RTSP_STATUS_CODE_FORBIDDEN 403
#define RTSP_STATUS_STRING_FORBIDDEN \
	"Forbidden"
#define RTSP_STATUS_CODE_NOT_FOUND 404
#define RTSP_STATUS_STRING_NOT_FOUND \
	"Not Found"
#define RTSP_STATUS_CODE_METHOD_NOT_ALLOWED 405
#define RTSP_STATUS_STRING_METHOD_NOT_ALLOWED \
	"Method Not Allowed"
#define RTSP_STATUS_CODE_NOT_ACCEPTABLE 406
#define RTSP_STATUS_STRING_NOT_ACCEPTABLE \
	"Not Acceptable"
#define RTSP_STATUS_CODE_PROXY_AUTHENTICATION_REQUIRED 407
#define RTSP_STATUS_STRING_PROXY_AUTHENTICATION_REQUIRED \
	"Proxy Authentication Required"
#define RTSP_STATUS_CODE_REQUEST_TIMEOUT 408
#define RTSP_STATUS_STRING_REQUEST_TIMEOUT \
	"Request Time-out"
#define RTSP_STATUS_CODE_GONE 410
#define RTSP_STATUS_STRING_GONE \
	"Gone"
#define RTSP_STATUS_CODE_LENGTH_REQUIRED 411
#define RTSP_STATUS_STRING_LENGTH_REQUIRED \
	"Length Required"
#define RTSP_STATUS_CODE_PRECONDITION_FAILED 412
#define RTSP_STATUS_STRING_PRECONDITION_FAILED \
	"Precondition Failed"
#define RTSP_STATUS_CODE_REQUEST_ENTITY_TOO_LARGE 413
#define RTSP_STATUS_STRING_REQUEST_ENTITY_TOO_LARGE \
	"Request Entity Too Large"
#define RTSP_STATUS_CODE_REQUEST_URI_TOO_LARGE 414
#define RTSP_STATUS_STRING_REQUEST_URI_TOO_LARGE \
	"Request-URI Too Large"
#define RTSP_STATUS_CODE_UNSUPPORTED_MEDIA_TYPE 415
#define RTSP_STATUS_STRING_UNSUPPORTED_MEDIA_TYPE \
	"Unsupported Media Type"
#define RTSP_STATUS_CODE_PARMETER_NOT_UNDERSTOOD 451
#define RTSP_STATUS_STRING_PARMETER_NOT_UNDERSTOOD \
	"Parameter Not Understood"
#define RTSP_STATUS_CODE_CONFERENCE_NOT_FOUND 452
#define RTSP_STATUS_STRING_CONFERENCE_NOT_FOUND \
	"Conference Not Found"
#define RTSP_STATUS_CODE_NOT_ENOUGH_BANDWIDTH 453
#define RTSP_STATUS_STRING_NOT_ENOUGH_BANDWIDTH \
	"Not Enough Bandwidth"
#define RTSP_STATUS_CODE_SESSION_NOT_FOUND 454
#define RTSP_STATUS_STRING_SESSION_NOT_FOUND \
	"Session Not Found"
#define RTSP_STATUS_CODE_METHOD_NOT_VALID 455
#define RTSP_STATUS_STRING_METHOD_NOT_VALID \
	"Method Not Valid In This State"
#define RTSP_STATUS_CODE_HEADER_FIELD_NOT_VALID 456
#define RTSP_STATUS_STRING_HEADER_FIELD_NOT_VALID \
	"Header Field Not Valid For Resource"
#define RTSP_STATUS_CODE_INVALID_RANGE 457
#define RTSP_STATUS_STRING_INVALID_RANGE \
	"Invalid Range"
#define RTSP_STATUS_CODE_PARAMETER_READ_ONLY 458
#define RTSP_STATUS_STRING_PARAMETER_READ_ONLY \
	"Parameter Is Read-Only"
#define RTSP_STATUS_CODE_AGGREGATE_OPERATION_NOT_ALLOWED 459
#define RTSP_STATUS_STRING_AGGREGATE_OPERATION_NOT_ALLOWED \
	"Aggregate Operation Not Allowed"
#define RTSP_STATUS_CODE_ONLY_AGGREGATE_OPERATION_ALLOWED 460
#define RTSP_STATUS_STRING_ONLY_AGGREGATE_OPERATION_ALLOWED \
	"Only Aggregate Operation Allowed"
#define RTSP_STATUS_CODE_UNSUPPORTED_TRANSPORT 461
#define RTSP_STATUS_STRING_UNSUPPORTED_TRANSPORT \
	"Unsupported Transport"
#define RTSP_STATUS_CODE_DESTINATION_UNREACHABLE 462
#define RTSP_STATUS_STRING_DESTINATION_UNREACHABLE \
	"Destination Unreachable"
#define RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR 500
#define RTSP_STATUS_STRING_INTERNAL_SERVER_ERROR \
	"Internal Server Error"
#define RTSP_STATUS_CODE_NOT_IMPLEMENTED 501
#define RTSP_STATUS_STRING_NOT_IMPLEMENTED \
	"Not Implemented"
#define RTSP_STATUS_CODE_BAD_GATEWAY 502
#define RTSP_STATUS_STRING_BAD_GATEWAY \
	"Bad Gateway"
#define RTSP_STATUS_CODE_SERVICE_UNAVAILABLE 503
#define RTSP_STATUS_STRING_SERVICE_UNAVAILABLE \
	"Service Unavailable"
#define RTSP_STATUS_CODE_GATEWAY_TIMEOUT 504
#define RTSP_STATUS_STRING_GATEWAY_TIMEOUT \
	"Gateway Time-out"
#define RTSP_STATUS_CODE_RTSP_VERSION_NOT_SUPPORTED 505
#define RTSP_STATUS_STRING_RTSP_VERSION_NOT_SUPPORTED \
	"RTSP Version Not Supported"
#define RTSP_STATUS_CODE_OPTION_NOT_SUPPORTED 551
#define RTSP_STATUS_STRING_OPTION_NOT_SUPPORTED \
	"Option Not Supported"


enum rtsp_tcp_state {
	RTSP_TCP_STATE_IDLE = 0,
	RTSP_TCP_STATE_CONNECTED,
};


enum rtsp_client_state {
	RTSP_CLIENT_STATE_IDLE = 0,
	RTSP_CLIENT_STATE_OPTIONS_WAITING_REPLY,
	RTSP_CLIENT_STATE_OPTIONS_OK,
	RTSP_CLIENT_STATE_DESCRIBE_WAITING_REPLY,
	RTSP_CLIENT_STATE_DESCRIBE_OK,
	RTSP_CLIENT_STATE_SETUP_WAITING_REPLY,
	RTSP_CLIENT_STATE_SETUP_OK,
	RTSP_CLIENT_STATE_PLAY_WAITING_REPLY,
	RTSP_CLIENT_STATE_PLAY_OK,
	RTSP_CLIENT_STATE_TEARDOWN_WAITING_REPLY,
	RTSP_CLIENT_STATE_TEARDOWN_OK,
	RTSP_CLIENT_STATE_KEEPALIVE_WAITING_REPLY,
};


struct rtsp_transport_header {
	char *transport;
	int server_stream_port;
	int server_control_port;
};


struct rtsp_response_header {
	int status_code;
	char *status_string;
	int content_length;
	char *content_type;
	char *content_encoding;
	char *content_language;
	char *content_base;
	char *content_location;
	int cseq;
	uint32_t options;
	char *session_id;
	unsigned int timeout;
	struct rtsp_transport_header transport;
	char *body;
};


struct rtsp_server {
	struct sockaddr_in listen_addr_in;
	struct pomp_ctx *pomp;
};


struct rtsp_client {
	struct sockaddr_in remote_addr_in;
	struct pomp_loop *loop;
	struct pomp_ctx *pomp;
	struct pomp_timer *timer;
	int connect_pipe[2];
	struct mbox *mbox;
	unsigned int max_msg_size;

	/* states */
	enum rtsp_tcp_state tcp_state;
	enum rtsp_client_state client_state;
	int waiting_reply;
	int playing;
	unsigned int cseq;
	char *session_id;
	unsigned int timeout;
	uint32_t options;

	char *user_agent;
	char *content_encoding;
	char *content_language;
	char *content_base;
	char *content_location;
	char *url;
	char *server_host;
	char *abs_path;
	char *sdp;
	int server_port;
	int server_stream_port;
	int server_control_port;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int pending_content_length;
	int pending_content_offset;
	char *pending_content;
	struct rtsp_response_header current_header;
};


int rtsp_response_header_copy(struct rtsp_response_header *src,
	struct rtsp_response_header *dst);


int rtsp_response_header_free(struct rtsp_response_header *header);


int rtsp_response_header_read(char *response,
	struct rtsp_response_header *header);


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


static inline void get_time_with_ms_delay(
	struct timespec *ts, unsigned int delay)
{
	struct timeval tp;
	gettimeofday(&tp, NULL);

	struct timespec ts2;
	time_timeval_to_timespec(&tp, &ts2);
	time_timespec_add_us(&ts2, (int64_t)delay * 1000, ts);
}


#endif /* !_RTSP_H_ */
