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

#ifndef _RTSP_COMMON_H_
#define _RTSP_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* To be used for all public API */
#ifdef RTSP_API_EXPORTS
#	ifdef _WIN32
#		define RTSP_API __declspec(dllexport)
#	else /* !_WIN32 */
#		define RTSP_API __attribute__((visibility("default")))
#	endif /* !_WIN32 */
#else /* !RTSP_API_EXPORTS */
#	define RTSP_API
#endif /* !RTSP_API_EXPORTS */

#include <errno.h>
#include <inttypes.h>

#include <libpomp.h>


/* RTSP methods */
enum rtsp_method_type {
	RTSP_METHOD_TYPE_UNKNOWN = 0,
	RTSP_METHOD_TYPE_OPTIONS,
	RTSP_METHOD_TYPE_DESCRIBE,
	RTSP_METHOD_TYPE_ANNOUNCE,
	RTSP_METHOD_TYPE_SETUP,
	RTSP_METHOD_TYPE_PLAY,
	RTSP_METHOD_TYPE_PAUSE,
	RTSP_METHOD_TYPE_TEARDOWN,
	RTSP_METHOD_TYPE_GET_PARAMETER,
	RTSP_METHOD_TYPE_SET_PARAMETER,
	RTSP_METHOD_TYPE_REDIRECT,
	RTSP_METHOD_TYPE_RECORD,
};


#define RTSP_METHOD_FLAG_OPTIONS 0x00000001UL
#define RTSP_METHOD_FLAG_DESCRIBE 0x00000002UL
#define RTSP_METHOD_FLAG_ANNOUNCE 0x00000004UL
#define RTSP_METHOD_FLAG_SETUP 0x00000008UL
#define RTSP_METHOD_FLAG_PLAY 0x00000010UL
#define RTSP_METHOD_FLAG_PAUSE 0x00000020UL
#define RTSP_METHOD_FLAG_TEARDOWN 0x00000040UL
#define RTSP_METHOD_FLAG_GET_PARAMETER 0x00000080UL
#define RTSP_METHOD_FLAG_SET_PARAMETER 0x00000100UL
#define RTSP_METHOD_FLAG_REDIRECT 0x00000200UL
#define RTSP_METHOD_FLAG_RECORD 0x00000400UL


/**
 * Transport definitions
 */

enum rtsp_delivery {
	RTSP_DELIVERY_MULTICAST = 0,
	RTSP_DELIVERY_UNICAST,
};

enum rtsp_lower_transport {
	RTSP_LOWER_TRANSPORT_UDP = 0,
	RTSP_LOWER_TRANSPORT_TCP,
	RTSP_LOWER_TRANSPORT_MUX,
};


/**
 * RTSP Range header definitions
 * see RFC 2326 chapter 12.29
 */

enum rtsp_time_format {
	RTSP_TIME_FORMAT_UNKNOWN = 0,
	RTSP_TIME_FORMAT_NPT,
	RTSP_TIME_FORMAT_SMPTE,
	RTSP_TIME_FORMAT_ABSOLUTE,
};

/* RTSP Normal Play Time (NPT), see RFC 2326 chapter 3.6 */
struct rtsp_time_npt {
	int now;
	int infinity;
	time_t sec;
	uint32_t usec;
};

/* RTSP SMPTE Relative Timestamps, see RFC 2326 chapter 3.5 */
struct rtsp_time_smpte {
	int infinity;
	time_t sec;
	unsigned int frames;
};

/* RTSP Absolute Time (UTC, ISO 8601), see RFC 2326 chapter 3.7 */
struct rtsp_time_absolute {
	int infinity;
	time_t sec;
	uint32_t usec;
};

struct rtsp_time {
	enum rtsp_time_format format;
	union {
		struct rtsp_time_npt npt;
		struct rtsp_time_smpte smpte;
		struct rtsp_time_absolute absolute;
	};
};

struct rtsp_range {
	struct rtsp_time start;
	struct rtsp_time stop;
	time_t time;
};


RTSP_API const char *rtsp_method_type_str(enum rtsp_method_type val);


RTSP_API const char *rtsp_delivery_str(enum rtsp_delivery val);


RTSP_API const char *rtsp_lower_transport_str(enum rtsp_lower_transport val);


RTSP_API const char *rtsp_time_format_str(enum rtsp_time_format val);


RTSP_API int rtsp_range_get_duration_us(const struct rtsp_range *range,
					int64_t *duration);


static inline int rtsp_time_us_to_npt(uint64_t time_us,
				      struct rtsp_time_npt *time_npt)
{
	if (time_npt == NULL)
		return -EINVAL;

	time_npt->now = 0;
	time_npt->infinity = 0;
	time_npt->sec = time_us / 1000000;
	time_npt->usec = time_us - time_npt->sec * 1000000;

	return 0;
}


static inline int rtsp_time_npt_to_us(const struct rtsp_time_npt *time_npt,
				      uint64_t *time_us)
{
	if ((time_npt == NULL) || (time_us == NULL))
		return -EINVAL;
	if ((time_npt->now) || (time_npt->infinity))
		return -EINVAL;

	*time_us = time_npt->sec * 1000000 + time_npt->usec;

	return 0;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTSP_COMMON_H_ */
