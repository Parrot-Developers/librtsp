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

#include "rtsp_server_priv.h"

#define ULOG_TAG rtsp_server
#include <ulog.h>


struct rtsp_server_pending_request *
rtsp_server_pending_request_add(struct rtsp_server *server,
				struct pomp_conn *conn,
				unsigned int timeout)
{
	struct timespec cur_ts = {0, 0};
	struct rtsp_server_pending_request *request = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(server == NULL, EINVAL, NULL);

	request = calloc(1, sizeof(*request));
	ULOG_ERRNO_RETURN_VAL_IF(request == NULL, ENOMEM, NULL);
	list_node_unref(&request->node);
	request->conn = conn;
	request->request_first_reply = 1;
	list_init(&request->medias);

	time_get_monotonic(&cur_ts);
	time_timespec_to_us(&cur_ts, &request->timeout);
	request->timeout =
		(timeout > 0) ? request->timeout + (uint64_t)timeout * 1000 : 0;

	/* Add to the list */
	list_add_before(&server->pending_requests, &request->node);
	server->pending_request_count++;

	return request;
}


int rtsp_server_pending_request_remove(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request)
{
	int found = 0;
	struct rtsp_server_pending_request *_request = NULL;
	struct rtsp_server_pending_request_media *media = NULL;
	struct rtsp_server_pending_request_media *tmp_media = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);

	list_walk_entry_forward(&server->pending_requests, _request, node)
	{
		if (_request == request) {
			found = 1;
			break;
		}
	}

	if (!found) {
		ULOGE("%s: pending request not found", __func__);
		return -ENOENT;
	}

	/* Remove from the list */
	list_del(&request->node);
	server->pending_request_count--;

	/* Remove all medias */
	list_walk_entry_forward_safe(&request->medias, media, tmp_media, node)
	{
		rtsp_server_pending_request_media_remove(
			server, request, media);
	}

	rtsp_request_header_clear(&request->request_header);
	rtsp_response_header_clear(&request->response_header);
	free(request);

	return 0;
}


int rtsp_server_pending_request_find(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request)
{
	int found = 0;
	struct rtsp_server_pending_request *_request = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);

	list_walk_entry_forward(&server->pending_requests, _request, node)
	{
		if (_request == request) {
			found = 1;
			break;
		}
	}

	return (found) ? 0 : -ENOENT;
}


struct rtsp_server_pending_request_media *rtsp_server_pending_request_media_add(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request,
	struct rtsp_server_session_media *media)
{
	struct rtsp_server_pending_request_media *m = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(server == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(request == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(media == NULL, EINVAL, NULL);

	m = calloc(1, sizeof(*m));
	ULOG_ERRNO_RETURN_VAL_IF(m == NULL, ENOMEM, NULL);
	list_node_unref(&m->node);
	m->media = media;

	/* Add to the list */
	list_add_before(&request->medias, &m->node);
	request->media_count++;

	return m;
}


int rtsp_server_pending_request_media_remove(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request,
	struct rtsp_server_pending_request_media *media)
{
	int found = 0;
	struct rtsp_server_pending_request_media *_media = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media == NULL, EINVAL);

	list_walk_entry_forward(&request->medias, _media, node)
	{
		if (_media == media) {
			found = 1;
			break;
		}
	}

	if (!found) {
		ULOGE("%s: media not found", __func__);
		return -ENOENT;
	}

	/* Remove from the list */
	list_del(&media->node);
	request->media_count--;

	free(media);

	return 0;
}
