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


static void rtsp_server_pomp_cb(
	struct pomp_ctx *ctx,
	struct pomp_conn *conn,
	struct pomp_buffer *buf,
	void *userdata);


struct rtsp_server *rtsp_server_new(
	int port,
	struct pomp_loop *loop)
{
	int ret;

	RTSP_RETURN_VAL_IF_FAILED(port != 0, -EINVAL, NULL);
	RTSP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);

	struct rtsp_server *server = calloc(1, sizeof(*server));
	RTSP_RETURN_VAL_IF_FAILED(server != NULL, -ENOMEM, NULL);

	server->pomp = pomp_ctx_new_with_loop(
		NULL, (void *)server, loop);
	if (!server->pomp) {
		RTSP_LOGE("pomp creation failed, aborting");
		goto error;
	}

	ret = pomp_ctx_set_raw(server->pomp, rtsp_server_pomp_cb);
	if (ret < 0) {
		RTSP_LOG_ERRNO("cannot switch pomp context to raw mode", -ret);
		goto error;
	}

	/*TODO*/
	server->listen_addr_in.sin_family = AF_INET;
	server->listen_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	server->listen_addr_in.sin_port = htons(port);

	ret = pomp_ctx_listen(server->pomp,
		(const struct sockaddr *)&server->listen_addr_in,
		sizeof(server->listen_addr_in));
	if (ret < 0) {
		RTSP_LOG_ERRNO("failed to listen, aborting", -ret);
		goto error;
	}

	return server;

error:
	if (server->pomp)
		pomp_ctx_destroy(server->pomp);
	free(server);
	return NULL;
}


int rtsp_server_destroy(
	struct rtsp_server *server)
{
	RTSP_RETURN_ERR_IF_FAILED(server != NULL, -EINVAL);

	pomp_ctx_stop(server->pomp);
	pomp_ctx_destroy(server->pomp);
	free(server);

	return 0;
}


static void rtsp_server_pomp_cb(
	struct pomp_ctx *ctx,
	struct pomp_conn *conn,
	struct pomp_buffer *buf,
	void *userdata)
{
	struct rtsp_server *server = (struct rtsp_server *)userdata;
	int ret;
	size_t len;
	const void *cdata = NULL;
	char *response;
	struct pomp_buffer *resp_buf = NULL;

	RTSP_RETURN_IF_FAILED(server != NULL, -EINVAL);

	/* get request data */
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	if ((ret < 0) || (!cdata)) {
		RTSP_LOGE("failed to get request data");
		goto error;
	}

	RTSP_LOGI("received: %s", (char *)cdata);

	/* create response */
	ret = asprintf(&response, "MEUH"); /*TODO*/
	if (ret == -1) {
		RTSP_LOGE("failed to allocate response");
		goto error;
	}

	RTSP_LOGI("response: %s", response);

	/* send response */
	resp_buf = pomp_buffer_new_with_data(response, strlen(response));
	ret = pomp_conn_send_raw_buf(conn, resp_buf);
	if (ret < 0) {
		RTSP_LOG_ERRNO("failed to send response", -ret);
		goto out;
	}

out:
	if (resp_buf)
		pomp_buffer_unref(resp_buf);
	free(response);
	return;

error:
	pomp_conn_disconnect(conn);
}
