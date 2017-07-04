/**
 * @file rtsp_client.c
 * @brief Real Time Streaming Protocol library - client implementation
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


static void rtsp_client_pomp_cb(
	struct pomp_ctx *ctx,
	struct pomp_conn *conn,
	struct pomp_buffer *buf,
	void *userdata);


struct rtsp_client *rtsp_client_new(
	const char *url,
	struct pomp_loop *loop)
{
	int ret;

	RTSP_RETURN_VAL_IF_FAILED(url != NULL, -EINVAL, NULL);
	RTSP_RETURN_VAL_IF_FAILED(strlen(url), -EINVAL, NULL);
	RTSP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);

	struct rtsp_client *client = calloc(1, sizeof(*client));
	RTSP_RETURN_VAL_IF_FAILED(client != NULL, -ENOMEM, NULL);

	/*TODO: parse the URL*/

	client->pomp = pomp_ctx_new_with_loop(
		NULL, (void *)client, loop);
	if (!client->pomp) {
		RTSP_LOGE("pomp creation failed, aborting");
		goto error;
	}

	ret = pomp_ctx_set_raw(client->pomp, rtsp_client_pomp_cb);
	if (ret < 0) {
		RTSP_LOG_ERRNO("cannot switch pomp context to raw mode", -ret);
		goto error;
	}

	/*TODO*/
	client->remote_addr_in.sin_family = AF_INET;
	client->remote_addr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	client->remote_addr_in.sin_port = htons(8080);

	ret = pomp_ctx_connect(client->pomp,
		(const struct sockaddr *)&client->remote_addr_in,
		sizeof(client->remote_addr_in));
	if (ret < 0) {
		RTSP_LOG_ERRNO("failed to connect, aborting", -ret);
		goto error;
	}

	return client;

error:
	if (client->pomp)
		pomp_ctx_destroy(client->pomp);
	free(client);
	return NULL;
}


int rtsp_client_destroy(
	struct rtsp_client *client)
{
	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	pomp_ctx_stop(client->pomp);
	pomp_ctx_destroy(client->pomp);
	free(client);

	return 0;
}


int rtsp_client_send_request(
	struct rtsp_client *client)
{
	int ret = 0, err;
	char *request;
	struct pomp_buffer *req_buf = NULL;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	/* create request */
	err = asprintf(&request, "POUET"); /*TODO*/
	if (err == -1) {
		RTSP_LOGE("failed to allocate request");
		ret = -1;
		goto out;
	}

	RTSP_LOGI("request: %s", request);

	/* send request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		ret = -1;
		goto out;
	}

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(request);
	return ret;
}


static void rtsp_client_pomp_cb(
	struct pomp_ctx *ctx,
	struct pomp_conn *conn,
	struct pomp_buffer *buf,
	void *userdata)
{
	struct rtsp_client *client = (struct rtsp_client *)userdata;
	int ret;
	size_t len;
	const void *cdata = NULL;

	RTSP_RETURN_IF_FAILED(client != NULL, -EINVAL);

	/* get response data */
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	if ((ret < 0) || (!cdata)) {
		RTSP_LOGE("failed to get response data");
		goto error;
	}

	RTSP_LOGI("response: %s", (char *)cdata);

	return;

error:
	pomp_conn_disconnect(conn);
}
