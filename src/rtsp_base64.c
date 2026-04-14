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

#define ULOG_TAG rtsp_base64
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_base64);


static inline char encode_char(unsigned char val)
{
	if (val <= 25)
		return 'A' + val;
	else if (val <= 51)
		return 'a' + val - 26;
	else if (val <= 61)
		return '0' + val - 52;
	else if (val == 62)
		return '+';
	else
		return '/';
}


int rtsp_base64_encode(const void *data, size_t size, char **out)
{
	ULOG_ERRNO_RETURN_ERR_IF(data == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(size == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(size > (MAX_RTSP_BASE64_LEN / 4) * 3, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(out == NULL, EINVAL);

	const uint8_t *_data = (const uint8_t *)data;
	size_t out_size = (size / 3) * 4 + ((size % 3) ? 4 : 0);
	char *_out = calloc(out_size + 1, sizeof(char));
	ULOG_ERRNO_RETURN_ERR_IF(_out == NULL, ENOMEM);

	size_t i;
	char *t = _out;
	for (i = 0; i < (size / 3) * 3; i += 3) {
		uint8_t x1 = _data[i];
		uint8_t x2 = _data[i + 1];
		uint8_t x3 = _data[i + 2];
		*t++ = encode_char(x1 >> 2);
		*t++ = encode_char((x1 << 4 | x2 >> 4) & 0x3f);
		*t++ = encode_char((x2 << 2 | x3 >> 6) & 0x3f);
		*t++ = encode_char(x3 & 0x3f);
	}

	switch (size % 3) {
	default:
		break;
	case 1: {
		uint8_t x1 = _data[i];
		*t++ = encode_char(x1 >> 2);
		*t++ = encode_char((x1 << 4) & 0x3f);
		*t++ = '=';
		*t++ = '=';
		break;
	}
	case 2: {
		uint8_t x1 = _data[i];
		uint8_t x2 = _data[i + 1];
		*t++ = encode_char(x1 >> 2);
		*t++ = encode_char((x1 << 4 | x2 >> 4) & 0x3f);
		*t++ = encode_char((x2 << 2) & 0x3f);
		*t++ = '=';
		break;
	}
	}

	/* String is already null-terminated (calloc) */
	*out = _out;
	return 0;
}


int rtsp_base64_decode(const char *str, void **out, size_t *out_size)
{
	ULOG_ERRNO_RETURN_ERR_IF(out == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(out_size == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(str[0] == '\0', EINVAL);
	size_t n = strnlen(str, MAX_RTSP_BASE64_LEN);
	ULOG_ERRNO_RETURN_ERR_IF(n == MAX_RTSP_BASE64_LEN, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF((n % 4) != 0, EINVAL);

	size_t padding = 0;
	size_t i;
	for (i = n; i && str[i - 1] == '='; i--)
		padding++;

	if (padding > 2) {
		ULOGE("%s: invalid padding in input base64 string", __func__);
		return -EINVAL;
	}

	size_t out_len = (n / 4) * 3 - padding;
	uint8_t *_out = calloc(out_len, 1);
	ULOG_ERRNO_RETURN_ERR_IF(_out == NULL, ENOMEM);

	size_t j = 0;
	uint32_t acc = 0;
	for (i = 0; i < n; i++) {
		char c = str[i];
		uint32_t value = 0;
		if ((c >= 'A') && (c <= 'Z'))
			value = c - 'A';
		else if ((c >= 'a') && (c <= 'z'))
			value = 26 + c - 'a';
		else if ((c >= '0') && (c <= '9'))
			value = 52 + c - '0';
		else if (c == '+')
			value = 62;
		else if (c == '/')
			value = 63;
		else if (c != '=')
			goto error;
		else if (i < n - padding)
			goto error;
		acc = (acc << 6) | value;
		if (((i + 1) % 4) == 0) {
			_out[j++] = (uint8_t)(acc >> 16);
			if (j < out_len)
				_out[j++] = (acc >> 8) & 0xff;
			if (j < out_len)
				_out[j++] = acc & 0xff;
			acc = 0;
		}
	}

	*out = (void *)_out;
	*out_size = out_len;
	return 0;

error:
	free(_out);
	ULOGE("%s: invalid input base64 string", __func__);
	return -EINVAL;
}
