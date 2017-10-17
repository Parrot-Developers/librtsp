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

#include "rtsp.h"


static int rtsp_transport_header_read(char *value,
	struct rtsp_transport_header *transport)
{
	char *_transport, *param, *temp = NULL;
	int server_stream_port = 0, server_control_port = 0;

	RTSP_RETURN_ERR_IF_FAILED(value != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(transport != NULL, -EINVAL);

	/*TODO: parse all params*/

	_transport = strtok_r(value, ";", &temp);
	if ((strncmp(_transport, RTSP_TRANSPORT_RTPAVP,
		strlen(RTSP_TRANSPORT_RTPAVP))) &&
		(strncmp(_transport,
			RTSP_TRANSPORT_RTPAVPUDP,
			strlen(RTSP_TRANSPORT_RTPAVPUDP)))) {
		RTSP_LOGE("unsupported transport protocol");
		return -1;
	}

	param = strtok_r(NULL, ";", &temp);
	while (param) {
		char *key, *val, *val2, *temp2;
		key = strtok_r(param, "=", &temp2);
		val = strtok_r(NULL, "", &temp2);
		if ((!strncmp(key, RTSP_TRANSPORT_SERVER_PORT,
			strlen(RTSP_TRANSPORT_SERVER_PORT))) &&
			(val)) {
			val2 = strchr(val, '-');
			server_stream_port = atoi(val);
			server_control_port = server_stream_port + 1;
			if (val2)
				server_control_port = atoi(val2 + 1);

		}
		param = strtok_r(NULL, ";", &temp);
	}

	transport->transport = _transport;
	transport->server_stream_port = server_stream_port;
	transport->server_control_port = server_control_port;

	return 0;
}


static int rtsp_session_header_read(char *value,
	char **session_id, unsigned int *timeout)
{
	RTSP_RETURN_ERR_IF_FAILED(value != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(session_id != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(timeout != NULL, -EINVAL);

	char *p3 = strchr(value, ';');
	char *timeout_str = NULL;

	if (p3) {
		timeout_str = p3 + 1;
		*p3 = '\0';
	}
	if ((timeout_str) && (!strncmp(timeout_str,
		RTSP_HEADER_SESSION_TIMEOUT,
		strlen(RTSP_HEADER_SESSION_TIMEOUT)))) {
		char *p4 = strchr(timeout_str, '=');
		if (p4)
			*timeout = atoi(p4 + 1);
	}

	*session_id = value;
	return 0;
}


static int rtsp_public_header_read(char *value, uint32_t *options)
{
	char *method, *temp = NULL;
	uint32_t _options = 0;

	RTSP_RETURN_ERR_IF_FAILED(value != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(options != NULL, -EINVAL);

	method = strtok_r(value, ",", &temp);
	while (method) {
		if (*method == ' ')
			method++;
		if (!strncmp(method, RTSP_METHOD_OPTIONS,
			strlen(RTSP_METHOD_OPTIONS)))
			_options |= RTSP_METHOD_FLAG_OPTIONS;
		else if (!strncmp(method, RTSP_METHOD_DESCRIBE,
			strlen(RTSP_METHOD_DESCRIBE)))
			_options |= RTSP_METHOD_FLAG_DESCRIBE;
		else if (!strncmp(method, RTSP_METHOD_ANNOUNCE,
			strlen(RTSP_METHOD_ANNOUNCE)))
			_options |= RTSP_METHOD_FLAG_ANNOUNCE;
		else if (!strncmp(method, RTSP_METHOD_SETUP,
			strlen(RTSP_METHOD_SETUP)))
			_options |= RTSP_METHOD_FLAG_SETUP;
		else if (!strncmp(method, RTSP_METHOD_PLAY,
			strlen(RTSP_METHOD_PLAY)))
			_options |= RTSP_METHOD_FLAG_PLAY;
		else if (!strncmp(method, RTSP_METHOD_PAUSE,
			strlen(RTSP_METHOD_PAUSE)))
			_options |= RTSP_METHOD_FLAG_PAUSE;
		else if (!strncmp(method, RTSP_METHOD_TEARDOWN,
			strlen(RTSP_METHOD_TEARDOWN)))
			_options |= RTSP_METHOD_FLAG_TEARDOWN;
		else if (!strncmp(method, RTSP_METHOD_GET_PARAMETER,
			strlen(RTSP_METHOD_GET_PARAMETER)))
			_options |= RTSP_METHOD_FLAG_GET_PARAMETER;
		else if (!strncmp(method, RTSP_METHOD_SET_PARAMETER,
			strlen(RTSP_METHOD_SET_PARAMETER)))
			_options |= RTSP_METHOD_FLAG_SET_PARAMETER;
		else if (!strncmp(method, RTSP_METHOD_REDIRECT,
			strlen(RTSP_METHOD_REDIRECT)))
			_options |= RTSP_METHOD_FLAG_REDIRECT;
		else if (!strncmp(method, RTSP_METHOD_RECORD,
			strlen(RTSP_METHOD_RECORD)))
			_options |= RTSP_METHOD_FLAG_RECORD;
		method = strtok_r(NULL, ",", &temp);
	}

	*options = _options;
	return 0;
}


int rtsp_response_header_copy(struct rtsp_response_header *src,
	struct rtsp_response_header *dst)
{
	RTSP_RETURN_ERR_IF_FAILED(src != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(dst != NULL, -EINVAL);

	dst->status_code = src->status_code;
	dst->status_string = xstrdup(src->status_string);
	dst->content_length = src->content_length;
	dst->content_type = xstrdup(src->content_type);
	dst->content_encoding = xstrdup(src->content_encoding);
	dst->content_language = xstrdup(src->content_language);
	dst->content_base = xstrdup(src->content_base);
	dst->content_location = xstrdup(src->content_location);
	dst->options = src->options;
	dst->cseq = src->cseq;
	dst->session_id = xstrdup(src->session_id);
	dst->timeout = src->timeout;
	dst->transport.transport = xstrdup(src->transport.transport);
	dst->transport.server_stream_port = src->transport.server_stream_port;
	dst->transport.server_control_port = src->transport.server_control_port;
	dst->body = xstrdup(src->body);

	return 0;
}


int rtsp_response_header_free(struct rtsp_response_header *header)
{
	RTSP_RETURN_ERR_IF_FAILED(header != NULL, -EINVAL);

	xfree((void **)&header->status_string);
	xfree((void **)&header->content_type);
	xfree((void **)&header->content_encoding);
	xfree((void **)&header->content_language);
	xfree((void **)&header->content_base);
	xfree((void **)&header->content_location);
	xfree((void **)&header->session_id);
	xfree((void **)&header->transport.transport);
	xfree((void **)&header->body);

	return 0;
}


int rtsp_response_header_read(char *response,
	struct rtsp_response_header *header)
{
	char *p, *temp, *temp2;
	char *version, *status_code_str;

	RTSP_RETURN_ERR_IF_FAILED(response != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(header != NULL, -EINVAL);

	p = strtok_r(response, "\n", &temp);
	if (!p) {
		RTSP_LOGE("invalid response data");
		return -1;
	}

	version = strtok_r(p, " ", &temp2);
	status_code_str = strtok_r(NULL, " ", &temp2);
	header->status_string = strtok_r(NULL, "\n", &temp2);

	if ((!version) || (strncmp(version, RTSP_VERSION,
		strlen(RTSP_VERSION)))) {
		RTSP_LOGE("invalid RTSP protocol version");
		return -1;
	}
	if ((!status_code_str) || (!header->status_string)) {
		RTSP_LOGE("malformed RTSP response");
		return -1;
	}
	header->status_code = atoi(status_code_str);
	if (RTSP_STATUS_CLASS(header->status_code) !=
		RTSP_STATUS_CLASS_SUCCESS) {
		RTSP_LOGE("RTSP status %d: %s",
			header->status_code, header->status_string);
		return -1;
	}

	p = strtok_r(NULL, "\n", &temp);
	while (p) {
		char *field, *value, *p2;

		/* remove the '\r' before '\n' if present */
		if (p[strlen(p) - 1] == '\r')
			p[strlen(p) - 1] = '\0';

		if (strlen(p) == 0) {
			header->body = strtok_r(NULL, "", &temp);
			break;
		}

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
				header->cseq = atoi(value);
			} else if (!strncasecmp(field,
				RTSP_HEADER_SESSION,
				strlen(RTSP_HEADER_SESSION))) {
				int ret = rtsp_session_header_read(value,
					&header->session_id, &header->timeout);
				if (ret != 0) {
					RTSP_LOGE("failed to parse "
						"'session' header");
					return -1;
				}
			} else if (!strncasecmp(field,
				RTSP_HEADER_CONTENT_LENGTH,
				strlen(RTSP_HEADER_CONTENT_LENGTH))) {
				header->content_length = atoi(value);
			} else if (!strncasecmp(field,
				RTSP_HEADER_CONTENT_TYPE,
				strlen(RTSP_HEADER_CONTENT_TYPE))) {
				header->content_type = value;
			} else if (!strncasecmp(field,
				RTSP_HEADER_CONTENT_ENCODING,
				strlen(RTSP_HEADER_CONTENT_ENCODING))) {
				header->content_encoding = value;
			} else if (!strncasecmp(field,
				RTSP_HEADER_CONTENT_LANGUAGE,
				strlen(RTSP_HEADER_CONTENT_LANGUAGE))) {
				header->content_language = value;
			} else if (!strncasecmp(field,
				RTSP_HEADER_CONTENT_BASE,
				strlen(RTSP_HEADER_CONTENT_BASE))) {
				header->content_base = value;
			} else if (!strncasecmp(field,
				RTSP_HEADER_CONTENT_LOCATION,
				strlen(RTSP_HEADER_CONTENT_LOCATION))) {
				header->content_location = value;
			} else if (!strncasecmp(field,
				RTSP_HEADER_TRANSPORT,
				strlen(RTSP_HEADER_TRANSPORT))) {
				int ret = rtsp_transport_header_read(value,
					&header->transport);
				if (ret != 0) {
					RTSP_LOGE("failed to parse "
						"'transport' header");
					return -1;
				}
			} else if (!strncasecmp(field,
				RTSP_HEADER_PUBLIC,
				strlen(RTSP_HEADER_PUBLIC))) {
				int ret = rtsp_public_header_read(value,
					&header->options);
				if (ret != 0) {
					RTSP_LOGE("failed to parse "
						"'public' header");
					return -1;
				}

			}
		}

		p = strtok_r(NULL, "\n", &temp);
	}

	return 0;
}
