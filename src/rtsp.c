/**
 * @file rtsp.c
 * @brief Real Time Streaming Protocol library
 * @date 26/06/2017
 * @author aurelien.barre@akaaba.net
 *
 * Copyright (c) 2017 Aurelien Barre <aurelien.barre@akaaba.net>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of the copyright holder nor the names of the
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "rtsp.h"


int rtsp_parse_transport_header(char *value, char **transport,
	int *server_stream_port, int *server_control_port)
{
	char *_transport, *param, *temp = NULL;
	int _server_stream_port = 0, _server_control_port = 0;

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
			_server_stream_port = atoi(val);
			_server_control_port = _server_stream_port + 1;
			if (val2)
				_server_control_port = atoi(val2 + 1);

		}
		param = strtok_r(NULL, ";", &temp);
	}

	if (transport)
		*transport = _transport;
	if (server_stream_port)
		*server_stream_port = _server_stream_port;
	if (server_control_port)
		*server_control_port = _server_control_port;
	return 0;
}
