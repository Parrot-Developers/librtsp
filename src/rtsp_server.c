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
ULOG_DECLARE_TAG(rtsp_server);


/* codecheck_ignore[COMPLEX_MACRO] */
#define RTSP_ENUM_CASE(_prefix, _name)                                         \
	case _prefix##_name:                                                   \
		return #_name


const char *
rtsp_server_teardown_reason_str(enum rtsp_server_teardown_reason val)
{
	/* clang-format off */
	switch (val) {
	RTSP_ENUM_CASE(RTSP_SERVER_TEARDOWN_REASON_, CLIENT_REQUEST);
	RTSP_ENUM_CASE(RTSP_SERVER_TEARDOWN_REASON_, SESSION_TIMEOUT);
	RTSP_ENUM_CASE(RTSP_SERVER_TEARDOWN_REASON_, FORCED_TEARDOWN);
	default: return "UNKNOWN";
	}
	/* clang-format on */
}


static void pomp_socket_cb(struct pomp_ctx *ctx,
			   int fd,
			   enum pomp_socket_kind kind,
			   void *userdata)
{
	struct rtsp_server *server = userdata;

	ULOG_ERRNO_RETURN_IF(server == NULL, EINVAL);

	if (server->cbs.socket_cb)
		(*server->cbs.socket_cb)(fd, server->cbs_userdata);
}


static int error_response(struct rtsp_server *server,
			  struct rtsp_server_pending_request *request,
			  int status)
{
	int ret = 0;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	int status_code = 0;
	const char *status_string = NULL;
	struct timespec cur_ts = {0, 0};

	memset(&response, 0, sizeof(response));

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == 0, EINVAL);

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		goto out;
	}

	/* Map status to status codes and status strings */
	rtsp_status_get(status, &status_code, &status_string);

	ULOG_ERRNO_RETURN_ERR_IF(status_code == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status_string == NULL, EINVAL);

	request->response_header.status_code = status_code;
	request->response_header.status_string = strdup(status_string);
	request->response_header.cseq = request->request_header.cseq;
	request->response_header.server = strdup(server->software_name);
	time_get_monotonic(&cur_ts);
	request->response_header.date = cur_ts.tv_sec;

	/* Create the response */
	response.max_len = server->max_msg_size;
	response.str = calloc(server->max_msg_size, 1);
	if (response.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	ret = rtsp_response_header_write(&request->response_header, &response);
	if (ret < 0)
		goto out;

	if (response.len > 0) {
		/* Send the response */
		ULOGI("send RTSP response to %s: "
		      "status=%d(%s) cseq=%d session=%s",
		      rtsp_method_type_str(request->request_header.method),
		      request->response_header.status_code,
		      request->response_header.status_string
			      ? request->response_header.status_string
			      : "-",
		      request->response_header.cseq,
		      request->request_header.session_id
			      ? request->request_header.session_id
			      : "-");
		resp_buf =
			pomp_buffer_new_with_data(response.str, response.len);
		ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
		if (ret < 0) {
			ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
			goto out;
		}
	}

out:
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


static void rtsp_server_timer_cb(struct pomp_timer *timer, void *userdata)
{
	int ret;
	struct rtsp_server *server = (struct rtsp_server *)userdata;
	struct rtsp_server_pending_request *request = NULL, *tmp_request = NULL;
	struct timespec cur_ts = {0, 0};
	uint64_t cur_time = 0;

	time_get_monotonic(&cur_ts);
	time_timespec_to_us(&cur_ts, &cur_time);

	/* Remove pending requests on timeout */
	list_walk_entry_forward_safe(
		&server->pending_requests, request, tmp_request, node)
	{
		if ((request->timeout > 0) && (cur_time > request->timeout)) {
			ULOGI("timeout on %s request, removing",
			      rtsp_method_type_str(
				      request->request_header.method));

			/* Reply with an error */
			ret = error_response(
				server,
				request,
				RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR);
			if (ret < 0)
				ULOG_ERRNO("error_response", -ret);

			(*server->cbs.request_timeout)(
				server,
				(void *)request,
				request->request_header.method,
				server->cbs_userdata);
			ret = rtsp_server_pending_request_remove(server,
								 request);
			if (ret < 0) {
				ULOG_ERRNO("rtsp_server_pending_request_remove",
					   -ret);
			}
		}
	}
}


void rtsp_server_session_timer_cb(struct pomp_timer *timer, void *userdata)
{
	int ret;
	struct rtsp_server_session *session = userdata;
	struct rtsp_server_session_media *media = NULL;
	struct rtsp_server *server = NULL;

	ULOG_ERRNO_RETURN_IF(session == NULL, EINVAL);
	server = session->server;

	/* Remove session on timeout */
	ULOGI("timeout on session '%s', removing", session->session_id);

	list_walk_entry_forward(&session->medias, media, node)
	{
		(*server->cbs.teardown)(
			server,
			media->path,
			session->session_id,
			RTSP_SERVER_TEARDOWN_REASON_SESSION_TIMEOUT,
			NULL,
			0,
			NULL,
			(void *)media,
			media->userdata,
			server->cbs_userdata);
	}

	ret = rtsp_server_session_remove(server, session);
	if (ret < 0)
		ULOG_ERRNO("rtsp_server_session_remove", -ret);
}


void rtsp_server_session_remove_idle(void *userdata)
{
	struct rtsp_server_session *session = userdata;

	ULOG_ERRNO_RETURN_IF(session == NULL, EINVAL);

	int err = rtsp_server_session_remove(session->server, session);
	if (err < 0)
		ULOG_ERRNO("rtsp_server_session_remove", -err);
}


static void rtsp_server_pomp_event_cb(struct pomp_ctx *ctx,
				      enum pomp_event event,
				      struct pomp_conn *conn,
				      const struct pomp_msg *msg,
				      void *userdata)
{
	struct rtsp_server *server = userdata;
	const struct sockaddr *peer_addr = NULL;
	uint32_t addrlen = 0;
	char addr[INET_ADDRSTRLEN] = "";
	struct rtsp_server_pending_request *request = NULL;

	peer_addr = pomp_conn_get_peer_addr(conn, &addrlen);
	if ((peer_addr->sa_family == AF_INET) &&
	    (addrlen == sizeof(struct sockaddr_in))) {
		const struct sockaddr_in *peer_addr_in =
			(const struct sockaddr_in *)peer_addr;
		inet_ntop(AF_INET,
			  &peer_addr_in->sin_addr,
			  addr,
			  INET_ADDRSTRLEN);
	}

	switch (event) {
	case POMP_EVENT_CONNECTED:
		if (strlen(addr))
			ULOGI("client connected (%s)", addr);
		else
			ULOGI("client connected");
		break;

	case POMP_EVENT_DISCONNECTED:
		if (strlen(addr))
			ULOGI("client disconnected (%s)", addr);
		else
			ULOGI("client disconnected");
		/* Flag the connection as not available on all pending
		 * requests on this connection */
		list_walk_entry_forward(
			&server->pending_requests, request, node)
		{
			if (request->conn == conn)
				request->conn = NULL;
		}
		break;

	default:
	case POMP_EVENT_MSG:
		/* Never received for raw context */
		break;
	}
}


static int rtsp_server_options(struct rtsp_server *server,
			       struct rtsp_server_pending_request *request,
			       int *status)
{
	int ret = 0;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	struct timespec cur_ts = {0, 0};

	memset(&response, 0, sizeof(response));

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		goto out;
	}

	request->response_header.status_code = RTSP_STATUS_CODE_OK;
	request->response_header.status_string = strdup(RTSP_STATUS_STRING_OK);
	request->response_header.cseq = request->request_header.cseq;
	request->response_header.server = strdup(server->software_name);
	time_get_monotonic(&cur_ts);
	request->response_header.date = cur_ts.tv_sec;
	request->response_header.public_methods =
		RTSP_METHOD_FLAG_DESCRIBE | RTSP_METHOD_FLAG_SETUP |
		RTSP_METHOD_FLAG_TEARDOWN | RTSP_METHOD_FLAG_PLAY |
		RTSP_METHOD_FLAG_PAUSE | RTSP_METHOD_FLAG_GET_PARAMETER;

	/* Create the response */
	response.max_len = server->max_msg_size;
	response.str = calloc(server->max_msg_size, 1);
	if (response.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	ret = rtsp_response_header_write(&request->response_header, &response);
	if (ret < 0)
		goto out;

	if (response.len > 0) {
		/* Send the response */
		ULOGI("send RTSP response to %s: "
		      "status=%d(%s) cseq=%d session=%s",
		      rtsp_method_type_str(request->request_header.method),
		      request->response_header.status_code,
		      request->response_header.status_string
			      ? request->response_header.status_string
			      : "-",
		      request->response_header.cseq,
		      request->response_header.session_id
			      ? request->response_header.session_id
			      : "-");
		resp_buf =
			pomp_buffer_new_with_data(response.str, response.len);
		ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
		if (ret < 0) {
			ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
			goto out;
		}
	}

out:
	if (ret == 0)
		rtsp_server_pending_request_remove(server, request);
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


static int rtsp_server_describe(struct rtsp_server *server,
				struct rtsp_server_pending_request *request,
				int *status)
{
	int ret = 0;
	char *uri = NULL, *host = NULL, *path = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	uri = xstrdup(request->request_header.uri);
	ret = rtsp_url_parse(uri, &host, NULL, &path);
	if (ret < 0)
		goto out;
	request->in_callback = 1;
	(*server->cbs.describe)(server,
				host,
				path,
				request->request_header.ext,
				request->request_header.ext_count,
				(void *)request,
				server->cbs_userdata);

out:
	request->in_callback = 0;
	if ((ret == 0) && (request->replied))
		rtsp_server_pending_request_remove(server, request);
	free(uri);
	return ret;
}


static int rtsp_server_setup(struct rtsp_server *server,
			     const char *dst_address,
			     struct rtsp_server_pending_request *request,
			     int *status)
{
	int ret = 0;
	char *uri = NULL, *host = NULL, *path = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL;
	int session_created = 0;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst_address == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(dst_address[0] == '\0', EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	if (request->request_header.transport_count != 1) {
		/* TODO: support multiple transports */
		ULOGE("%s: unsupported transport count", __func__);
		ret = -ENOSYS;
		goto out;
	}

	uri = xstrdup(request->request_header.uri);
	ret = rtsp_url_parse(uri, &host, NULL, &path);
	if (ret < 0)
		goto out;

	if (request->request_header.session_id == NULL) {
		/* New session */
		session = rtsp_server_session_add(
			server, server->session_timeout_ms, host);
		if (session == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		session_created = 1;
	} else {
		/* Existing session */
		session = rtsp_server_session_find(
			server, request->request_header.session_id);
		if (session == NULL) {
			ret = -ENOENT;
			ULOGW("%s: session not found", __func__);
			*status = RTSP_STATUS_CODE_SESSION_NOT_FOUND;
			goto out;
		}
	}

	media = rtsp_server_session_media_add(
		server, session, request->request_header.uri, path);
	if (media == NULL) {
		ret = -EPROTO;
		goto out;
	}

	rtsp_server_session_reset_timeout(session);

	/* TODO: source address? */
	request->in_callback = 1;
	session->op_in_progress = request->request_header.method;
	(*server->cbs.setup)(
		server,
		path,
		session->session_id,
		request->request_header.ext,
		request->request_header.ext_count,
		(void *)request,
		(void *)media,
		request->request_header.transport[0]->delivery,
		request->request_header.transport[0]->lower_transport,
		"0.0.0.0",
		dst_address,
		request->request_header.transport[0]->dst_stream_port,
		request->request_header.transport[0]->dst_control_port,
		server->cbs_userdata);

out:
	request->in_callback = 0;
	if ((ret == 0) && (request->replied)) {
		if (session)
			session->op_in_progress = RTSP_METHOD_TYPE_UNKNOWN;
		rtsp_server_pending_request_remove(server, request);
	}
	free(uri);
	if ((ret != 0) && (session_created)) {
		/* TODO check if only session media must be removed */
		rtsp_server_session_remove(server, session);
	}
	return ret;
}


static int rtsp_server_play(struct rtsp_server *server,
			    struct rtsp_server_pending_request *request,
			    int *status)
{
	int ret = 0;
	char *uri = NULL, *path = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL;
	struct rtsp_server_pending_request_media *req_media;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request->request_header.session_id == NULL,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	uri = xstrdup(request->request_header.uri);
	ret = rtsp_url_parse(uri, NULL, NULL, &path);
	if (ret < 0)
		goto out;
	/* TODO: check that the path corresponds to the session */

	session = rtsp_server_session_find(server,
					   request->request_header.session_id);
	if ((session == NULL) || (session->media_count == 0)) {
		ULOGW("%s: session not found", __func__);
		*status = RTSP_STATUS_CODE_SESSION_NOT_FOUND;
		ret = -ENOENT;
		goto out;
	}

	rtsp_server_session_reset_timeout(session);

	request->in_callback = 1;
	session->op_in_progress = request->request_header.method;
	list_walk_entry_forward(&session->medias, media, node)
	{
		req_media = rtsp_server_pending_request_media_add(
			server, request, media);
		if (req_media == NULL) {
			ret = -ENOMEM;
			goto out;
		}
	}
	list_walk_entry_forward(&request->medias, req_media, node)
	{
		(*server->cbs.play)(server,
				    session->session_id,
				    request->request_header.ext,
				    request->request_header.ext_count,
				    (void *)request,
				    (void *)req_media->media,
				    &request->request_header.range,
				    request->request_header.scale,
				    req_media->media->userdata,
				    server->cbs_userdata);
	}

	request->request_first_reply = 1;

out:
	request->in_callback = 0;
	if ((ret == 0) && (request->replied)) {
		if (session)
			session->op_in_progress = RTSP_METHOD_TYPE_UNKNOWN;
		rtsp_server_pending_request_remove(server, request);
	}
	free(uri);
	return ret;
}


static int rtsp_server_pause(struct rtsp_server *server,
			     struct rtsp_server_pending_request *request,
			     int *status)
{
	int ret = 0;
	char *uri = NULL, *path = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL;
	struct rtsp_server_pending_request_media *req_media;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request->request_header.session_id == NULL,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	uri = xstrdup(request->request_header.uri);
	ret = rtsp_url_parse(uri, NULL, NULL, &path);
	if (ret < 0)
		goto out;
	/* TODO: check that the path corresponds to the session */

	session = rtsp_server_session_find(server,
					   request->request_header.session_id);
	if ((session == NULL) || (session->media_count == 0)) {
		ULOGW("%s: session not found", __func__);
		*status = RTSP_STATUS_CODE_SESSION_NOT_FOUND;
		ret = -ENOENT;
		goto out;
	}

	rtsp_server_session_reset_timeout(session);

	request->in_callback = 1;
	session->op_in_progress = request->request_header.method;
	list_walk_entry_forward(&session->medias, media, node)
	{
		req_media = rtsp_server_pending_request_media_add(
			server, request, media);
		if (req_media == NULL) {
			ret = -ENOMEM;
			goto out;
		}
	}
	list_walk_entry_forward(&request->medias, req_media, node)
	{
		(*server->cbs.pause)(server,
				     session->session_id,
				     request->request_header.ext,
				     request->request_header.ext_count,
				     (void *)request,
				     (void *)req_media->media,
				     &request->request_header.range,
				     req_media->media->userdata,
				     server->cbs_userdata);
	}

	request->request_first_reply = 1;

out:
	request->in_callback = 0;
	if ((ret == 0) && (request->replied)) {
		if (session)
			session->op_in_progress = RTSP_METHOD_TYPE_UNKNOWN;
		rtsp_server_pending_request_remove(server, request);
	}
	free(uri);
	return ret;
}


static int rtsp_server_teardown(struct rtsp_server *server,
				struct rtsp_server_pending_request *request,
				int *status)
{
	int ret = 0;
	char *uri = NULL, *path = NULL, *p = NULL;
	bool media_found = false;
	bool is_prefix = false;
	size_t prefix_len = 0;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL, *tmpmedia = NULL;
	struct rtsp_server_pending_request_media *req_media;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request->request_header.session_id == NULL,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	uri = xstrdup(request->request_header.uri);
	ret = rtsp_url_parse(uri, NULL, NULL, &path);
	if (ret < 0)
		goto out;

	/* Parse the path */
	p = strchr(path, '/');
	if (p == NULL) {
		is_prefix = true;
		prefix_len = strlen(path);
	}

	session = rtsp_server_session_find(server,
					   request->request_header.session_id);
	if ((session == NULL) || (session->media_count == 0)) {
		ULOGW("%s: session not found", __func__);
		*status = RTSP_STATUS_CODE_SESSION_NOT_FOUND;
		ret = -ENOENT;
		goto out;
	}

	/* Check if the path corresponds to a media in the session */
	media = rtsp_server_session_media_find(server, session, path);
	if (media != NULL)
		media_found = true;

	rtsp_server_session_reset_timeout(session);

	request->in_callback = 1;
	session->op_in_progress = request->request_header.method;
	list_walk_entry_forward(&session->medias, media, node)
	{
		if (media_found && strcmp(path, media->path) != 0) {
			/* Filter-out non matching media */
			continue;
		}
		if (!media_found && is_prefix &&
		    strncmp(path, media->path, prefix_len) != 0) {
			/* Filter-out non matching prefix */
			continue;
		}
		req_media = rtsp_server_pending_request_media_add(
			server, request, media);
		if (req_media == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		media->is_tearing_down = true;
	}
	if (list_is_empty(&request->medias)) {
		ret = -ENOENT;
		ULOGE("path '%s' not found", path);
		goto out;
	} else if (is_prefix) {
		ULOGI("path '%s' is a prefix, "
		      "tearing down all matching resources",
		      path);
	}
	list_walk_entry_forward(&request->medias, req_media, node)
	{
		(*server->cbs.teardown)(
			server,
			media->path,
			session->session_id,
			RTSP_SERVER_TEARDOWN_REASON_CLIENT_REQUEST,
			request->request_header.ext,
			request->request_header.ext_count,
			(void *)request,
			(void *)req_media->media,
			req_media->media->userdata,
			server->cbs_userdata);
	}

out:
	request->in_callback = 0;
	if ((ret == 0) && (request->replied)) {
		if (session)
			session->op_in_progress = RTSP_METHOD_TYPE_UNKNOWN;
		rtsp_server_pending_request_remove(server, request);
		bool remaining = false;
		list_walk_entry_forward_safe(
			&session->medias, media, tmpmedia, node)
		{
			if (!media->is_tearing_down) {
				remaining = true;
			} else {
				rtsp_server_session_media_remove(
					server, session, media);
			}
		}
		if (!remaining) {
			ULOGI("all media torn down, removing session");
			rtsp_server_session_remove(server, session);
		}
	}
	free(uri);
	return ret;
}


static int
rtsp_server_get_parameter(struct rtsp_server *server,
			  struct rtsp_server_pending_request *request,
			  int *status)
{
	int ret = 0;
	char *uri = NULL, *path = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request->request_header.session_id == NULL,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(status == NULL, EINVAL);

	memset(&response, 0, sizeof(response));

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		goto out;
	}

	uri = xstrdup(request->request_header.uri);
	ret = rtsp_url_parse(uri, NULL, NULL, &path);
	if (ret < 0)
		goto out;
	/* TODO: check that the path corresponds to the session */

	session = rtsp_server_session_find(server,
					   request->request_header.session_id);
	if ((session == NULL) || (session->media_count == 0)) {
		ULOGW("%s: session not found", __func__);
		*status = RTSP_STATUS_CODE_SESSION_NOT_FOUND;
		ret = -ENOENT;
		goto out;
	}

	rtsp_server_session_reset_timeout(session);

	request->response_header.status_code = RTSP_STATUS_CODE_OK;
	request->response_header.status_string = strdup(RTSP_STATUS_STRING_OK);
	request->response_header.cseq = request->request_header.cseq;
	request->response_header.server = strdup(server->software_name);
	time_get_monotonic(&cur_ts);
	request->response_header.date = cur_ts.tv_sec;
	request->response_header.session_id =
		strdup(request->request_header.session_id);
	request->response_header.session_timeout = session->timeout_ms / 1000;

	/* Create the response */
	response.max_len = server->max_msg_size;
	response.str = calloc(server->max_msg_size, 1);
	if (response.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	ret = rtsp_response_header_write(&request->response_header, &response);
	if (ret < 0)
		goto out;

	if (response.len > 0) {
		/* Send the response */
		ULOGI("send RTSP response to %s: "
		      "status=%d(%s) cseq=%d session=%s",
		      rtsp_method_type_str(request->request_header.method),
		      request->response_header.status_code,
		      request->response_header.status_string
			      ? request->response_header.status_string
			      : "-",
		      request->response_header.cseq,
		      request->response_header.session_id
			      ? request->response_header.session_id
			      : "-");
		resp_buf =
			pomp_buffer_new_with_data(response.str, response.len);
		ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
		if (ret < 0) {
			ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
			goto out;
		}
	}

out:
	if (ret == 0)
		rtsp_server_pending_request_remove(server, request);
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	free(uri);
	return ret;
}


static int rtsp_server_request_process(struct rtsp_server *server,
				       struct pomp_conn *conn,
				       struct rtsp_message *msg)
{
	int ret = 0, err = 0, status = 0;
	const struct sockaddr *peer_addr = NULL;
	uint32_t addrlen = 0;
	char dst_address[INET_ADDRSTRLEN] = "";
	struct rtsp_server_pending_request *request = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(conn == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);

	peer_addr = pomp_conn_get_peer_addr(conn, &addrlen);
	if ((peer_addr->sa_family == AF_INET) &&
	    (addrlen == sizeof(struct sockaddr_in))) {
		const struct sockaddr_in *peer_addr_in =
			(const struct sockaddr_in *)peer_addr;
		inet_ntop(AF_INET,
			  &peer_addr_in->sin_addr,
			  dst_address,
			  INET_ADDRSTRLEN);
	}

	request = rtsp_server_pending_request_add(
		server, conn, server->reply_timeout_ms);
	if (request == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	rtsp_request_header_copy(&msg->header.req, &request->request_header);

	ULOGI("received RTSP request %s: cseq=%d session=%s",
	      rtsp_method_type_str(request->request_header.method),
	      request->request_header.cseq,
	      request->request_header.session_id
		      ? request->request_header.session_id
		      : "-");

	switch (request->request_header.method) {
	default:
	case RTSP_METHOD_TYPE_UNKNOWN:
		ULOGE("%s: unknown method", __func__);
		break;
	case RTSP_METHOD_TYPE_OPTIONS:
		err = rtsp_server_options(server, request, &status);
		break;
	case RTSP_METHOD_TYPE_DESCRIBE:
		err = rtsp_server_describe(server, request, &status);
		break;
	case RTSP_METHOD_TYPE_ANNOUNCE:
		/* TODO */
		break;
	case RTSP_METHOD_TYPE_SETUP:
		err = rtsp_server_setup(server, dst_address, request, &status);
		break;
	case RTSP_METHOD_TYPE_PLAY:
		err = rtsp_server_play(server, request, &status);
		break;
	case RTSP_METHOD_TYPE_PAUSE:
		err = rtsp_server_pause(server, request, &status);
		break;
	case RTSP_METHOD_TYPE_TEARDOWN:
		err = rtsp_server_teardown(server, request, &status);
		break;
	case RTSP_METHOD_TYPE_GET_PARAMETER:
		err = rtsp_server_get_parameter(server, request, &status);
		break;
	case RTSP_METHOD_TYPE_SET_PARAMETER:
		/* TODO */
		break;
	case RTSP_METHOD_TYPE_REDIRECT:
		/* TODO */
		break;
	case RTSP_METHOD_TYPE_RECORD:
		/* TODO */
		break;
	}

out:
	if ((err < 0) && (request != NULL)) {
		/* Reply with an error */
		error_response(
			server,
			request,
			(RTSP_STATUS_CLASS(status) > RTSP_STATUS_CLASS_SUCCESS)
				? status
				: err);
		rtsp_server_pending_request_remove(server, request);
	}
	return ret;
}


static int rtsp_server_response_process(struct rtsp_server *server,
					struct rtsp_message *msg)
{
	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);

	ULOGI("response to RTSP request %s: status=%d(%s) cseq=%d session=%s",
	      rtsp_method_type_str(msg->header.req.method),
	      msg->header.resp.status_code,
	      msg->header.resp.status_string ? msg->header.resp.status_string
					     : "-",
	      msg->header.resp.cseq,
	      msg->header.resp.session_id ? msg->header.resp.session_id : "-");

	return 0;
}


static void rtsp_server_pomp_cb(struct pomp_ctx *ctx,
				struct pomp_conn *conn,
				struct pomp_buffer *buf,
				void *userdata)
{
	struct rtsp_server *server = (struct rtsp_server *)userdata;
	int ret;
	size_t len = 0;
	const void *cdata = NULL;
	struct rtsp_message msg;
	memset(&msg, 0x0, sizeof(msg));

	ULOG_ERRNO_RETURN_IF(server == NULL, EINVAL);

	/* Get the message data */
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	if ((ret < 0) || (!cdata)) {
		ULOG_ERRNO("pomp_buffer_get_cdata", -ret);
		return;
	}

	/* Add the data to the buffer */
	ret = pomp_buffer_append_data(server->request_buf, cdata, len);
	if (ret < 0) {
		ULOG_ERRNO("pomp_buffer_append_data", -ret);
		return;
	}

	/* Iterate over complete messages */
	while ((ret = rtsp_get_next_message(
			server->request_buf, &msg, &server->parser_ctx)) == 0) {
		if (msg.type == RTSP_MESSAGE_TYPE_REQUEST)
			(void)rtsp_server_request_process(server, conn, &msg);
		else
			(void)rtsp_server_response_process(server, &msg);
		rtsp_buffer_remove_first_bytes(server->request_buf,
					       msg.total_len);
	}

	if (ret != -EAGAIN)
		ULOG_ERRNO("rtsp_get_next_message", -ret);


	rtsp_buffer_remove_first_bytes(server->request_buf, msg.total_len);
}


static int rtsp_server_send_teardown(struct rtsp_server *server,
				     const char *uri,
				     const char *session_id,
				     const struct rtsp_header_ext *ext,
				     size_t ext_count)
{
	struct rtsp_request_header header;
	struct pomp_buffer *req_buf;
	struct rtsp_string request;
	int ret = 0;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(uri == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(uri[0] == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);

	memset(&header, 0, sizeof(header));
	memset(&request, 0, sizeof(request));

	header.method = RTSP_METHOD_TYPE_TEARDOWN;
	header.cseq = server->cseq++;
	header.session_id = strdup(session_id);
	if (header.session_id == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("strdup", -ret);
		goto out;
	}
	ret = rtsp_request_header_copy_ext(&header, ext, ext_count);
	if (ret < 0)
		goto out;
	ret = asprintf(&header.uri, "%s", uri);
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		goto out;
	}

	/* Create the request */
	memset(&request, 0, sizeof(request));
	request.max_len = server->max_msg_size;
	request.str = calloc(server->max_msg_size, 1);
	if (request.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	ret = rtsp_request_header_write(&header, &request);
	if (ret < 0)
		goto out;

	if (request.len > 0) {
		/* Send the request */
		ULOGI("send RTSP request %s: cseq=%d session=%s",
		      rtsp_method_type_str(header.method),
		      header.cseq,
		      header.session_id ? header.session_id : "-");
		req_buf = pomp_buffer_new_with_data(request.str, request.len);
		ret = pomp_ctx_send_raw_buf(server->pomp, req_buf);
		pomp_buffer_unref(req_buf);
	}

out:
	free(header.session_id);
	free(header.uri);
	free(request.str);
	return ret;
}


int rtsp_server_new(const char *software_name,
		    uint16_t port,
		    int reply_timeout_ms,
		    int session_timeout_ms,
		    struct pomp_loop *loop,
		    const struct rtsp_server_cbs *cbs,
		    void *userdata,
		    struct rtsp_server **ret_obj)
{
	int ret;
	struct rtsp_server *server = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(port == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(loop == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->describe == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->setup == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->play == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->pause == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->teardown == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->request_timeout == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);

	server = calloc(1, sizeof(*server));
	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, ENOMEM);
	server->cseq = 1;
	server->max_msg_size = PIPE_BUF - 1;
	server->loop = loop;
	server->cbs = *cbs;
	server->cbs_userdata = userdata;
	server->reply_timeout_ms =
		(reply_timeout_ms == 0) ? RTSP_SERVER_DEFAULT_REPLY_TIMEOUT_MS
					: reply_timeout_ms;
	server->session_timeout_ms =
		(session_timeout_ms == 0)
			? RTSP_SERVER_DEFAULT_SESSION_TIMEOUT_MS
			: session_timeout_ms;
	list_init(&server->sessions);
	list_init(&server->pending_requests);

	server->software_name =
		(software_name) ? strdup(software_name)
				: strdup(RTSP_SERVER_DEFAULT_SOFTWARE_NAME);
	if (!server->software_name) {
		ret = -ENOMEM;
		ULOG_ERRNO("strdup", -ret);
		goto error;
	}

	server->pomp = pomp_ctx_new_with_loop(
		&rtsp_server_pomp_event_cb, (void *)server, server->loop);
	if (!server->pomp) {
		ret = -ENOMEM;
		ULOG_ERRNO("pomp_ctx_new_with_loop", -ret);
		goto error;
	}

	/* Set tcp keepalive timeout to 30 seconds and 10 retries
	 * for dead peer detection,
	 * the 5 seconds and 2 retries default of pomp_ctx
	 * may be too aggressive for wireless connections */
	ret = pomp_ctx_setup_keepalive(server->pomp, 1, 30, 1, 10);
	if (ret < 0) {
		ULOG_ERRNO("pomp_ctx_setup_keepalive", -ret);
		goto error;
	}

	ret = pomp_ctx_set_socket_cb(server->pomp, &pomp_socket_cb);
	if (ret < 0) {
		ULOG_ERRNO("pomp_ctx_set_socket_cb", -ret);
		goto error;
	}

	ret = pomp_ctx_set_raw(server->pomp, &rtsp_server_pomp_cb);
	if (ret < 0) {
		ULOG_ERRNO("pomp_ctx_set_raw", -ret);
		goto error;
	}

	server->timer = pomp_timer_new(
		server->loop, &rtsp_server_timer_cb, (void *)server);
	if (!server->timer) {
		ret = -ENOMEM;
		ULOG_ERRNO("pomp_timer_new", -ret);
		goto error;
	}
	ret = pomp_timer_set_periodic(server->timer, 1000, 1000);
	if (ret < 0) {
		ULOG_ERRNO("pomp_timer_set_periodic", -ret);
		goto error;
	}

	/* TODO */
	server->listen_addr_in.sin_family = AF_INET;
	server->listen_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	server->listen_addr_in.sin_port = htons(port);

	ret = pomp_ctx_listen(server->pomp,
			      (const struct sockaddr *)&server->listen_addr_in,
			      sizeof(server->listen_addr_in));
	if (ret < 0) {
		ULOG_ERRNO("pomp_ctx_listen", -ret);
		goto error;
	}

	server->request_buf = pomp_buffer_new(PIPE_BUF - 1);
	if (server->request_buf == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("pomp_buffer_new", -ret);
		goto error;
	}

	*ret_obj = server;
	return 0;

error:
	rtsp_server_destroy(server);
	*ret_obj = NULL;
	return ret;
}


int rtsp_server_destroy(struct rtsp_server *server)
{
	int ret;
	struct rtsp_server_session *session = NULL, *tmp_session = NULL;
	struct rtsp_server_pending_request *request = NULL, *tmp_request = NULL;

	if (server == NULL)
		return 0;

	if (server->timer) {
		ret = pomp_timer_destroy(server->timer);
		if (ret < 0) {
			if (ret != -EBUSY)
				ULOG_ERRNO("pomp_timer_destroy", -ret);
			return ret;
		}
	}

	if (server->pomp) {
		/* TODO: to be done through a disconnect function */
		pomp_ctx_stop(server->pomp);
		ret = pomp_ctx_destroy(server->pomp);
		if (ret < 0) {
			if (ret != -EBUSY)
				ULOG_ERRNO("pomp_ctx_destroy", -ret);
			return ret;
		}
	}

	/* Remove all pending requests */
	list_walk_entry_forward_safe(
		&server->pending_requests, request, tmp_request, node)
	{
		(*server->cbs.request_timeout)(server,
					       (void *)request,
					       request->request_header.method,
					       server->cbs_userdata);
		ret = rtsp_server_pending_request_remove(server, request);
		if (ret < 0)
			ULOG_ERRNO("rtsp_server_pending_request_remove", -ret);
	}

	/* Remove all sessions */
	list_walk_entry_forward_safe(
		&server->sessions, session, tmp_session, node)
	{
		ret = rtsp_server_session_remove(server, session);
		if (ret < 0)
			ULOG_ERRNO("rtsp_server_session_remove", -ret);
	}

	rtsp_message_clear(&server->parser_ctx.msg);

	if (server->request_buf)
		pomp_buffer_unref(server->request_buf);
	free(server->software_name);
	free(server);

	return 0;
}


int rtsp_server_reply_to_describe(struct rtsp_server *server,
				  void *request_ctx,
				  int status,
				  const struct rtsp_header_ext *ext,
				  size_t ext_count,
				  char *session_description)
{
	int ret = 0;
	struct rtsp_server_pending_request *request = NULL;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	int status_code = 0, error_status = 0;
	const char *status_string = NULL;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request_ctx == NULL, EINVAL);

	memset(&response, 0, sizeof(response));

	request = request_ctx;
	ret = rtsp_server_pending_request_find(server, request);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_server_pending_request_find", -ret);
		request = NULL;
		goto out;
	}

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		goto out;
	}

	if ((status < 0) ||
	    (RTSP_STATUS_CLASS(status) > RTSP_STATUS_CLASS_SUCCESS)) {
		error_status = status;
		goto out;
	}

	if ((session_description == NULL) || (session_description[0] == '\0')) {
		ret = -EINVAL;
		ULOGE("%s: invalid session description", __func__);
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	/* Map status to status codes and status strings */
	rtsp_status_get(status, &status_code, &status_string);
	if ((status_code == 0) || (status_string == NULL)) {
		ret = -EPROTO;
		ULOGE("%s: invalid status", __func__);
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	request->response_header.status_code = status_code;
	request->response_header.status_string = strdup(status_string);
	request->response_header.cseq = request->request_header.cseq;
	request->response_header.server = strdup(server->software_name);
	time_get_monotonic(&cur_ts);
	request->response_header.date = cur_ts.tv_sec;
	request->response_header.content_length = strlen(session_description);
	request->response_header.content_type = strdup(RTSP_CONTENT_TYPE_SDP);
	/* TODO */
	request->response_header.content_base =
		strdup(request->request_header.uri);
	ret = rtsp_response_header_copy_ext(
		&request->response_header, ext, ext_count);
	if (ret < 0) {
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	/* Create the response */
	response.max_len = server->max_msg_size;
	response.str = calloc(server->max_msg_size, 1);
	if (response.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	ret = rtsp_response_header_write(&request->response_header, &response);
	if (ret < 0) {
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	ret = rtsp_sprintf(&response, "%s", session_description);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_sprintf", -ret);
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	if (response.len > 0) {
		/* Send the response */
		ULOGI("send RTSP response to %s: "
		      "status=%d(%s) cseq=%d session=%s",
		      rtsp_method_type_str(request->request_header.method),
		      request->response_header.status_code,
		      request->response_header.status_string
			      ? request->response_header.status_string
			      : "-",
		      request->response_header.cseq,
		      request->response_header.session_id
			      ? request->response_header.session_id
			      : "-");
		resp_buf =
			pomp_buffer_new_with_data(response.str, response.len);
		ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
		if (ret < 0) {
			ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
			goto out;
		}
	}

out:
	if (error_status != 0) {
		/* Reply with an error */
		error_response(server, request, error_status);
	}
	if (request != NULL) {
		request->replied = 1;
		if (!request->in_callback)
			rtsp_server_pending_request_remove(server, request);
	}
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


int rtsp_server_reply_to_setup(struct rtsp_server *server,
			       void *request_ctx,
			       void *media_ctx,
			       int status,
			       uint16_t src_stream_port,
			       uint16_t src_control_port,
			       int ssrc_valid,
			       uint32_t ssrc,
			       const struct rtsp_header_ext *ext,
			       size_t ext_count,
			       void *stream_userdata)
{
	int ret = 0, failed = 0;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL;
	struct rtsp_server_pending_request *request = NULL;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	int status_code = 0, error_status = 0;
	const char *status_string = NULL;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request_ctx == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media_ctx == NULL, EINVAL);

	memset(&response, 0, sizeof(response));

	request = request_ctx;
	media = media_ctx;
	session = media->session;

	ret = rtsp_server_pending_request_find(server, request);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_server_pending_request_find", -ret);
		request = NULL;
		goto out;
	}

	if (session == NULL) {
		ret = -EINVAL;
		goto out;
	}
	media->userdata = stream_userdata;

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		failed = 1;
		goto out;
	}

	if ((status < 0) ||
	    (RTSP_STATUS_CLASS(status) > RTSP_STATUS_CLASS_SUCCESS)) {
		failed = 1;
		error_status = status;
		goto out;
	}

	if ((src_stream_port == 0) || (src_control_port == 0)) {
		ULOGE("%s: invalid source ports", __func__);
		ret = -EINVAL;
		failed = 1;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	/* Map status to status codes and status strings */
	rtsp_status_get(status, &status_code, &status_string);
	if ((status_code == 0) || (status_string == NULL)) {
		ULOGE("%s: invalid status", __func__);
		ret = -EPROTO;
		failed = 1;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	request->response_header.status_code = status_code;
	request->response_header.status_string = strdup(status_string);
	request->response_header.cseq = request->request_header.cseq;
	request->response_header.server = strdup(server->software_name);
	time_get_monotonic(&cur_ts);
	request->response_header.date = cur_ts.tv_sec;
	request->response_header.session_id = strdup(session->session_id);
	request->response_header.session_timeout = session->timeout_ms / 1000;
	request->response_header.transport = rtsp_transport_header_new();
	ULOG_ERRNO_RETURN_ERR_IF(request->response_header.transport == NULL,
				 ENOMEM);
	request->response_header.transport->transport_protocol =
		strdup(RTSP_TRANSPORT_PROTOCOL_RTP);
	request->response_header.transport->transport_profile =
		strdup(RTSP_TRANSPORT_PROFILE_AVP);
	request->response_header.transport->lower_transport =
		request->request_header.transport[0]->lower_transport;
	request->response_header.transport->delivery =
		request->request_header.transport[0]->delivery;
	request->response_header.transport->method = RTSP_TRANSPORT_METHOD_PLAY;
	request->response_header.transport->dst_stream_port =
		request->request_header.transport[0]->dst_stream_port;
	request->response_header.transport->dst_control_port =
		request->request_header.transport[0]->dst_control_port;
	request->response_header.transport->src_stream_port = src_stream_port;
	request->response_header.transport->src_control_port = src_control_port;
	request->response_header.transport->ssrc_valid = ssrc_valid;
	request->response_header.transport->ssrc = ssrc;
	ret = rtsp_response_header_copy_ext(
		&request->response_header, ext, ext_count);
	if (ret < 0) {
		failed = 1;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	/* Create the response */
	response.max_len = server->max_msg_size;
	response.str = calloc(server->max_msg_size, 1);
	if (response.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		failed = 1;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	ret = rtsp_response_header_write(&request->response_header, &response);
	if (ret < 0) {
		failed = 1;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		goto out;
	}

	if (response.len > 0) {
		/* Send the response */
		ULOGI("send RTSP response to %s: "
		      "status=%d(%s) cseq=%d session=%s",
		      rtsp_method_type_str(request->request_header.method),
		      request->response_header.status_code,
		      request->response_header.status_string
			      ? request->response_header.status_string
			      : "-",
		      request->response_header.cseq,
		      request->response_header.session_id
			      ? request->response_header.session_id
			      : "-");
		resp_buf =
			pomp_buffer_new_with_data(response.str, response.len);
		ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
		if (ret < 0) {
			ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
			failed = 1;
			goto out;
		}
	}

out:
	if (error_status != 0) {
		/* Reply with an error */
		error_response(server, request, error_status);
	}
	if ((failed) && (session) && (session->media_count == 1)) {
		/* This is a setup for the first media of the session; if the
		 * setup failed, the client will never know of the session's
		 * existence, so it can be safely removed now instead of having
		 * a zombie session waiting for its timeout. The remove is done
		 * in an idle callback as reply_to_setup() can be called from
		 * the setup callback which can still use references on the
		 * session. */
		ULOGI("avoiding zombie session (setup failed on 1st media)");
		int err = pomp_loop_idle_add(server->loop,
					     &rtsp_server_session_remove_idle,
					     session);
		if (err < 0)
			ULOG_ERRNO("pomp_loop_idle_add", -err);
	}
	if (request != NULL) {
		request->replied = 1;
		if (!request->in_callback) {
			if (session)
				session->op_in_progress =
					RTSP_METHOD_TYPE_UNKNOWN;
			rtsp_server_pending_request_remove(server, request);
		}
	}
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


int rtsp_server_reply_to_play(struct rtsp_server *server,
			      void *request_ctx,
			      void *media_ctx,
			      int status,
			      struct rtsp_range *range,
			      float scale,
			      int seq_valid,
			      uint16_t seq,
			      int rtptime_valid,
			      uint32_t rtptime,
			      const struct rtsp_header_ext *ext,
			      size_t ext_count)
{
	int ret = 0, found = 0, replied = 0;
	struct rtsp_server_pending_request *request = NULL;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL;
	struct rtsp_server_pending_request_media *req_media = NULL, *rm = NULL;
	int status_code = 0, error_status = 0;
	const char *status_string = NULL;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request_ctx == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media_ctx == NULL, EINVAL);

	memset(&response, 0, sizeof(response));

	request = request_ctx;
	media = media_ctx;
	session = media->session;

	ret = rtsp_server_pending_request_find(server, request);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_server_pending_request_find", -ret);
		request = NULL;
		goto out;
	}

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		replied = request->media_count;
		goto out;
	}

	if ((status < 0) ||
	    (RTSP_STATUS_CLASS(status) > RTSP_STATUS_CLASS_SUCCESS)) {
		error_status = status;
		replied = request->media_count;
		goto out;
	}

	if (range == NULL) {
		ret = -EINVAL;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	if (session == NULL) {
		ret = -EINVAL;
		goto out;
	}
	list_walk_entry_forward(&request->medias, req_media, node)
	{
		if (req_media->media == media) {
			found = 1;
			break;
		}
	}
	if (!found) {
		ULOGE("%s: media not found", __func__);
		ret = -ENOENT;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	if (request->request_first_reply) {
		session->range = *range;
		session->scale = scale;
		request->request_first_reply = 0;
	}

	if (memcmp(range, &session->range, sizeof(*range))) {
		ULOGE("%s: session range mismatch", __func__);
		ret = -EPROTO;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}
	if (scale != session->scale) {
		ULOGE("%s: session scale mismatch", __func__);
		ret = -EPROTO;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	if (((seq_valid) || (rtptime_valid)) &&
	    (request->response_header.rtp_info_count <
	     RTSP_RTP_INFO_MAX_COUNT)) {
		struct rtsp_rtp_info_header *rtp_info =
			rtsp_rtp_info_header_new();
		if (rtp_info == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("rtsp_rtp_info_header_new", -ret);
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}
		rtp_info->url = strdup(media->path);
		rtp_info->seq_valid = seq_valid;
		rtp_info->seq = seq;
		rtp_info->rtptime_valid = rtptime_valid;
		rtp_info->rtptime = rtptime;
		request->response_header
			.rtp_info[request->response_header.rtp_info_count++] =
			rtp_info;
	}

	req_media->replied = 1;
	list_walk_entry_forward(&request->medias, rm, node)
	{
		replied += rm->replied;
	}

	if (replied == (int)request->media_count) {
		session->playing = 1;

		/* Map status to status codes and status strings */
		rtsp_status_get(status, &status_code, &status_string);
		if ((status_code == 0) || (status_string == NULL)) {
			ULOGE("%s: invalid status", __func__);
			ret = -EPROTO;
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			goto out;
		}

		request->response_header.status_code = status_code;
		free(request->response_header.status_string);
		request->response_header.status_string = strdup(status_string);
		request->response_header.cseq = request->request_header.cseq;
		free(request->response_header.server);
		request->response_header.server = strdup(server->software_name);
		time_get_monotonic(&cur_ts);
		request->response_header.date = cur_ts.tv_sec;
		free(request->response_header.session_id);
		request->response_header.session_id =
			strdup(session->session_id);
		request->response_header.session_timeout =
			session->timeout_ms / 1000;
		request->response_header.range = session->range;
		request->response_header.scale = session->scale;
		ret = rtsp_response_header_copy_ext(
			&request->response_header, ext, ext_count);
		if (ret < 0) {
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			goto out;
		}

		/* Create the response */
		response.max_len = server->max_msg_size;
		response.str = calloc(server->max_msg_size, 1);
		if (response.str == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("calloc", -ret);
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			goto out;
		}

		ret = rtsp_response_header_write(&request->response_header,
						 &response);
		if (ret < 0) {
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			goto out;
		}

		if (response.len > 0) {
			/* Send the response */
			ULOGI("send RTSP response to %s: "
			      "status=%d(%s) cseq=%d session=%s",
			      rtsp_method_type_str(
				      request->request_header.method),
			      request->response_header.status_code,
			      request->response_header.status_string
				      ? request->response_header.status_string
				      : "-",
			      request->response_header.cseq,
			      request->response_header.session_id
				      ? request->response_header.session_id
				      : "-");
			resp_buf = pomp_buffer_new_with_data(response.str,
							     response.len);
			ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
			if (ret < 0) {
				ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
				goto out;
			}
		}
	}

out:
	if (error_status != 0) {
		/* Reply with an error */
		error_response(server, request, error_status);
	}
	if ((request != NULL) && (replied == (int)request->media_count)) {
		request->replied = 1;
		if (!request->in_callback) {
			if (session)
				session->op_in_progress =
					RTSP_METHOD_TYPE_UNKNOWN;
			rtsp_server_pending_request_remove(server, request);
		}
	}
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


int rtsp_server_reply_to_pause(struct rtsp_server *server,
			       void *request_ctx,
			       void *media_ctx,
			       int status,
			       struct rtsp_range *range,
			       const struct rtsp_header_ext *ext,
			       size_t ext_count)
{
	int ret = 0, found = 0, replied = 0;
	struct rtsp_server_pending_request *request = NULL;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL;
	struct rtsp_server_pending_request_media *req_media = NULL, *rm = NULL;
	int status_code = 0, error_status = 0;
	const char *status_string = NULL;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request_ctx == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media_ctx == NULL, EINVAL);

	memset(&response, 0, sizeof(response));

	request = request_ctx;
	media = media_ctx;
	session = media->session;

	ret = rtsp_server_pending_request_find(server, request);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_server_pending_request_find", -ret);
		request = NULL;
		goto out;
	}

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		replied = request->media_count;
		goto out;
	}

	if ((status < 0) ||
	    (RTSP_STATUS_CLASS(status) > RTSP_STATUS_CLASS_SUCCESS)) {
		error_status = status;
		replied = request->media_count;
		goto out;
	}

	if (range == NULL) {
		ret = -EINVAL;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	if (session == NULL) {
		ret = -EINVAL;
		goto out;
	}
	list_walk_entry_forward(&request->medias, req_media, node)
	{
		if (req_media->media == media) {
			found = 1;
			break;
		}
	}
	if (!found) {
		ULOGE("%s: media not found", __func__);
		ret = -ENOENT;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	if (request->request_first_reply) {
		session->range = *range;
		request->request_first_reply = 0;
	}

	if (memcmp(range, &session->range, sizeof(*range))) {
		ULOGE("%s: session range mismatch", __func__);
		ret = -EPROTO;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	req_media->replied = 1;
	list_walk_entry_forward(&request->medias, rm, node)
	{
		replied += rm->replied;
	}

	if (replied == (int)request->media_count) {
		session->playing = 0;

		/* Map status to status codes and status strings */
		rtsp_status_get(status, &status_code, &status_string);
		if ((status_code == 0) || (status_string == NULL)) {
			ULOGE("%s: invalid status", __func__);
			ret = -EPROTO;
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		request->response_header.status_code = status_code;
		free(request->response_header.status_string);
		request->response_header.status_string = strdup(status_string);
		request->response_header.cseq = request->request_header.cseq;
		free(request->response_header.server);
		request->response_header.server = strdup(server->software_name);
		time_get_monotonic(&cur_ts);
		request->response_header.date = cur_ts.tv_sec;
		free(request->response_header.session_id);
		request->response_header.session_id =
			strdup(session->session_id);
		request->response_header.session_timeout =
			session->timeout_ms / 1000;
		request->response_header.range = session->range;
		ret = rtsp_response_header_copy_ext(
			&request->response_header, ext, ext_count);
		if (ret < 0) {
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		/* Create the response */
		response.max_len = server->max_msg_size;
		response.str = calloc(server->max_msg_size, 1);
		if (response.str == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("calloc", -ret);
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		ret = rtsp_response_header_write(&request->response_header,
						 &response);
		if (ret < 0) {
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		if (response.len > 0) {
			/* Send the response */
			ULOGI("send RTSP response to %s: "
			      "status=%d(%s) cseq=%d session=%s",
			      rtsp_method_type_str(
				      request->request_header.method),
			      request->response_header.status_code,
			      request->response_header.status_string
				      ? request->response_header.status_string
				      : "-",
			      request->response_header.cseq,
			      request->response_header.session_id
				      ? request->response_header.session_id
				      : "-");
			resp_buf = pomp_buffer_new_with_data(response.str,
							     response.len);
			ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
			if (ret < 0) {
				ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
				goto out;
			}
		}
	}

out:
	if (error_status != 0) {
		/* Reply with an error */
		error_response(server, request, error_status);
	}
	if ((request != NULL) && (replied == (int)request->media_count)) {
		request->replied = 1;
		if (!request->in_callback) {
			if (session)
				session->op_in_progress =
					RTSP_METHOD_TYPE_UNKNOWN;
			rtsp_server_pending_request_remove(server, request);
		}
	}
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


int rtsp_server_reply_to_teardown(struct rtsp_server *server,
				  void *request_ctx,
				  void *media_ctx,
				  int status,
				  const struct rtsp_header_ext *ext,
				  size_t ext_count)
{
	int ret = 0, found = 0, replied = 0;
	struct rtsp_server_pending_request *request = NULL;
	struct rtsp_string response;
	struct pomp_buffer *resp_buf = NULL;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL, *tmpmedia = NULL;
	struct rtsp_server_pending_request_media *req_media = NULL, *rm = NULL;
	int status_code = 0, error_status = 0;
	const char *status_string = NULL;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(request_ctx == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media_ctx == NULL, EINVAL);

	memset(&response, 0, sizeof(response));

	request = request_ctx;
	media = media_ctx;
	session = media->session;

	ret = rtsp_server_pending_request_find(server, request);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_server_pending_request_find", -ret);
		request = NULL;
		goto out;
	}

	if (request->conn == NULL) {
		ret = -ECONNRESET;
		ULOGE("%s: cannot reply to request: connection closed",
		      __func__);
		replied = request->media_count;
		goto out;
	}

	if ((status < 0) ||
	    (RTSP_STATUS_CLASS(status) > RTSP_STATUS_CLASS_SUCCESS)) {
		error_status = status;
		replied = request->media_count;
		goto out;
	}

	if (session == NULL) {
		ret = -EINVAL;
		goto out;
	}
	list_walk_entry_forward(&request->medias, req_media, node)
	{
		if (req_media->media == media) {
			found = 1;
			break;
		}
	}
	if (!found) {
		ULOGE("%s: media not found", __func__);
		ret = -ENOENT;
		error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
		replied = request->media_count;
		goto out;
	}

	req_media->replied = 1;
	list_walk_entry_forward(&request->medias, rm, node)
	{
		replied += rm->replied;
	}

	if (replied == (int)request->media_count) {
		/* Map status to status codes and status strings */
		rtsp_status_get(status, &status_code, &status_string);
		if ((status_code == 0) || (status_string == NULL)) {
			ULOGE("%s: invalid status", __func__);
			ret = -EPROTO;
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		request->response_header.status_code = status_code;
		free(request->response_header.status_string);
		request->response_header.status_string = strdup(status_string);
		request->response_header.cseq = request->request_header.cseq;
		free(request->response_header.server);
		request->response_header.server = strdup(server->software_name);
		time_get_monotonic(&cur_ts);
		request->response_header.date = cur_ts.tv_sec;
		free(request->response_header.session_id);
		request->response_header.session_id =
			strdup(request->request_header.session_id);
		request->response_header.session_timeout =
			session->timeout_ms / 1000;
		ret = rtsp_response_header_copy_ext(
			&request->response_header, ext, ext_count);
		if (ret < 0) {
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		/* Create the response */
		response.max_len = server->max_msg_size;
		response.str = calloc(server->max_msg_size, 1);
		if (response.str == NULL) {
			ret = -ENOMEM;
			ULOG_ERRNO("calloc", -ret);
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		ret = rtsp_response_header_write(&request->response_header,
						 &response);
		if (ret < 0) {
			error_status = RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR;
			replied = request->media_count;
			goto out;
		}

		if (response.len > 0) {
			/* Send the response */
			ULOGI("send RTSP response to %s: "
			      "status=%d(%s) cseq=%d session=%s",
			      rtsp_method_type_str(
				      request->request_header.method),
			      request->response_header.status_code,
			      request->response_header.status_string
				      ? request->response_header.status_string
				      : "-",
			      request->response_header.cseq,
			      request->response_header.session_id
				      ? request->response_header.session_id
				      : "-");
			resp_buf = pomp_buffer_new_with_data(response.str,
							     response.len);
			ret = pomp_conn_send_raw_buf(request->conn, resp_buf);
			if (ret < 0) {
				ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);
				goto out;
			}
		}
	}

out:
	if (error_status != 0) {
		/* Reply with an error */
		error_response(server, request, error_status);
	}
	if ((request != NULL) && (replied == (int)request->media_count)) {
		request->replied = 1;
		if (!request->in_callback) {
			if (session)
				session->op_in_progress =
					RTSP_METHOD_TYPE_UNKNOWN;
			rtsp_server_pending_request_remove(server, request);
			bool remaining = false;
			list_walk_entry_forward_safe(
				&session->medias, media, tmpmedia, node)
			{
				if (!media->is_tearing_down) {
					remaining = true;
				} else {
					rtsp_server_session_media_remove(
						server, session, media);
				}
			}
			if (!remaining) {
				ULOGI("all media torn down, removing session");
				rtsp_server_session_remove(server, session);
			}
		}
	}
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	free(response.str);
	return ret;
}


int rtsp_server_announce(struct rtsp_server *server,
			 char *uri,
			 const struct rtsp_header_ext *ext,
			 size_t ext_count,
			 char *session_description)
{
	int ret = 0;
	struct rtsp_request_header header;
	struct pomp_buffer *req_buf;
	struct rtsp_string request;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(uri == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strlen(uri) == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_description == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(strlen(session_description) == 0, EINVAL);

	memset(&header, 0, sizeof(header));

	header.method = RTSP_METHOD_TYPE_ANNOUNCE;
	header.cseq = server->cseq++;
	header.content_length = strlen(session_description);
	header.content_type = RTSP_CONTENT_TYPE_SDP;
	time_get_monotonic(&cur_ts);
	header.date = cur_ts.tv_sec;
	header.server = server->software_name;
	ret = rtsp_request_header_copy_ext(&header, ext, ext_count);
	if (ret < 0)
		return ret;
	ret = asprintf(&header.uri, "/%s", uri);
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		return ret;
	}

	/* Create the request */
	memset(&request, 0, sizeof(request));
	request.max_len = server->max_msg_size;
	request.str = calloc(server->max_msg_size, 1);
	if (request.str == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto out;
	}

	ret = rtsp_request_header_write(&header, &request);
	if (ret < 0)
		goto out;

	CHECK_FUNC(rtsp_sprintf,
		   ret,
		   goto out,
		   &request,
		   "%s",
		   session_description);

	if (request.len > 0) {
		/* Send the request */
		ULOGI("send RTSP request %s: cseq=%d session=%s",
		      rtsp_method_type_str(header.method),
		      header.cseq,
		      header.session_id ? header.session_id : "-");
		req_buf = pomp_buffer_new_with_data(request.str, request.len);
		ret = pomp_ctx_send_raw_buf(server->pomp, req_buf);
		if (ret < 0)
			ULOG_ERRNO("pomp_ctx_send_raw_buf", -ret);
		pomp_buffer_unref(req_buf);
	}

out:
	free(header.uri);
	free(request.str);
	return ret;
}


int rtsp_server_force_teardown(struct rtsp_server *server,
			       const char *session_id,
			       const char *resource_uri,
			       const struct rtsp_header_ext *ext,
			       size_t ext_count)
{
	int ret;
	struct rtsp_server_session *session = NULL;
	struct rtsp_server_session_media *media = NULL, *_media = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);

	session = rtsp_server_session_find(server, session_id);
	if (session == NULL) {
		ULOGE("%s: session '%s' not found", __func__, session_id);
		return -ENOENT;
	}

	if (resource_uri != NULL) {
		media = rtsp_server_session_media_find(
			server, session, resource_uri);
		if (media == NULL) {
			ULOGE("%s: media not found: %s",
			      __func__,
			      resource_uri);
			return -ENOENT;
		}
	}

	switch (session->op_in_progress) {
	case RTSP_METHOD_TYPE_UNKNOWN:
		/* No op in progress, proceed with forced teardown */
		break;
	case RTSP_METHOD_TYPE_TEARDOWN:
		/* TODO: check that this teardown relates to the current
		 * resource */
		/* Teardown already in progress */
		ret = -EALREADY;
		goto out;
	default:
		/* Any other operation */
		ret = -EBUSY;
		goto out;
	}

	if (media != NULL) {
		ULOGI("force remove on session '%s', media '%s'",
		      session->session_id,
		      media->path);
	} else {
		ULOGI("force remove on session '%s'", session->session_id);
	}

	list_walk_entry_forward(&session->medias, _media, node)
	{
		if ((media != NULL) && (media != _media))
			continue;

		(*server->cbs.teardown)(
			server,
			_media->path,
			session->session_id,
			RTSP_SERVER_TEARDOWN_REASON_FORCED_TEARDOWN,
			ext,
			ext_count,
			NULL,
			(void *)_media,
			_media->userdata,
			server->cbs_userdata);
	}

	ret = rtsp_server_send_teardown(server,
					(media != NULL) ? media->uri
							: session->uri,
					session->session_id,
					ext,
					ext_count);
	if (ret < 0)
		ULOG_ERRNO("rtsp_server_send_teardown", -ret);

	if (media != NULL) {
		ret = rtsp_server_session_media_remove(server, session, media);
		if (ret < 0)
			ULOG_ERRNO("rtsp_server_session_media_remove", -ret);
	} else {
		ret = rtsp_server_session_remove(server, session);
		if (ret < 0)
			ULOG_ERRNO("rtsp_server_session_remove", -ret);
	}

out:
	return ret;
}
