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

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#else /* !_WIN32 */
#	include <arpa/inet.h>
#	include <netinet/in.h>
#	include <netdb.h>
#endif /* !_WIN32 */

#include <futils/futils.h>
#include <futils/random.h>
#include <libpomp.h>
#include <rtsp/rtsp.h>

#include "internal/rtsp/rtsp_internal.h"

#ifdef _WIN32
#	define PIPE_BUF 4096
#endif /* _WIN32 */

#define RTSP_DEFAULT_PORT 554

#define RTSP_SCHEME_TCP "rtsp://"
#define RTSP_SCHEME_UDP "rtspu://"


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


int rtsp_request_header_copy_ext(struct rtsp_request_header *header,
				 const struct rtsp_header_ext *ext,
				 size_t ext_count);


int rtsp_request_header_read(char *str,
			     size_t len,
			     struct rtsp_request_header *header,
			     char **body);


int rtsp_response_header_clear(struct rtsp_response_header *header);


int rtsp_response_header_copy(const struct rtsp_response_header *src,
			      struct rtsp_response_header *dst);


int rtsp_response_header_copy_ext(struct rtsp_response_header *header,
				  const struct rtsp_header_ext *ext,
				  size_t ext_count);


int rtsp_response_header_read(char *msg,
			      size_t len,
			      struct rtsp_response_header *header,
			      char **body);


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
