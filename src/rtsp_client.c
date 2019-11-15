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

#include "rtsp_client_priv.h"

#define ULOG_TAG rtsp_client
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_client);


static void pomp_socket_cb(struct pomp_ctx *ctx,
			   int fd,
			   enum pomp_socket_kind kind,
			   void *userdata)
{
	struct rtsp_client *client = userdata;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	if (client->cbs.socket_cb)
		(*client->cbs.socket_cb)(fd, client->cbs_userdata);
}


static char *make_uri(struct rtsp_client *client, const char *path)
{
	char *tmp;
	int ret;

	if (!client)
		return NULL;
	if (!path)
		return xstrdup(client->addr);

	ret = asprintf(&tmp, "%s/%s", client->addr, path);
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		return NULL;
	}
	return tmp;
}


static int format_request_uri(struct rtsp_client *client,
			      const char *content_base,
			      const char *control_url,
			      char **ret_uri)
{
	int res = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(control_url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_uri == NULL, EINVAL);

	/* Full request URI */
	if (strncmp(control_url, RTSP_SCHEME_TCP, strlen(RTSP_SCHEME_TCP)) ==
	    0) {
		*ret_uri = strdup(control_url);
		return 0;
	}

	ULOG_ERRNO_RETURN_ERR_IF(content_base == NULL, EINVAL);

	/* Format from content_base */
	res = asprintf(ret_uri,
		       "%s%s%s",
		       content_base,
		       (get_last_char(content_base) == '/') ? "" : "/",
		       control_url);
	if (res <= 0) {
		res = -ENOMEM;
		ULOG_ERRNO("asprintf", -res);
		return res;
	}

	return 0;
}


static char *client_uri_to_content_base(struct rtsp_client *client,
					const char *uri)
{
	int ret;
	int needSlash;
	char *content_base;

	if (!client || !uri)
		return NULL;

	/* If URI is an absolute rtsp:// URI, return it */
	if (strncmp(uri, RTSP_SCHEME_TCP, strlen(RTSP_SCHEME_TCP)) == 0)
		return strdup(uri);

	/* Otherwise, alloc a content_base in the form client->addr/uri
	 * without duplicate '/' */
	needSlash = client->addr[strlen(client->addr) - 1] != '/';
	if (uri[0] == '/')
		uri = &uri[1];
	if (needSlash)
		ret = asprintf(&content_base, "%s/%s", client->addr, uri);
	else
		ret = asprintf(&content_base, "%s%s", client->addr, uri);
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		return NULL;
	}
	return content_base;
}


static int send_request(struct rtsp_client *client, unsigned int timeout_ms)
{
	int res = 0;
	struct rtsp_string str;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	ULOGI("send RTSP request %s: cseq=%d session=%s",
	      rtsp_method_type_str(client->request.header.method),
	      client->request.header.cseq,
	      client->request.header.session_id
		      ? client->request.header.session_id
		      : "-");

	memset(&str, 0, sizeof(str));
	res = pomp_buffer_get_data(
		client->request.buf, (void **)&str.str, NULL, &str.max_len);
	if (res < 0) {
		ULOG_ERRNO("pomp_buffer_get_data", -res);
		return res;
	}

	res = rtsp_request_header_write(&client->request.header, &str);
	if (res < 0)
		return res;

	/* Set buffer length */
	res = pomp_buffer_set_len(client->request.buf, str.len);
	if (res < 0) {
		ULOG_ERRNO("pomp_buffer_set_len", -res);
		return res;
	}

	/* Send the request */
	res = pomp_ctx_send_raw_buf(client->ctx, client->request.buf);
	if (res < 0) {
		ULOG_ERRNO("pomp_ctx_send_raw_buf", -res);
		return res;
	}

	/* Set a timer for response timeout */
	if (timeout_ms > 0) {
		res = pomp_timer_set(client->request.timer, timeout_ms);
		if (res < 0) {
			ULOG_ERRNO("pomp_timer_set", -res);
			return res;
		}
	}

	return 0;
}


static int reset_keep_alive_timer(struct rtsp_client_session *session,
				  unsigned int timer_sec)
{
	int res;

	ULOG_ERRNO_RETURN_ERR_IF(session == NULL, EINVAL);

	if (timer_sec == 0 ||
	    session->client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return 0;

	res = pomp_timer_set(session->timer, timer_sec * 1000);
	if (res < 0)
		ULOG_ERRNO("pomp_timer_set", -res);

	return res;
}


static int send_keep_alive(struct rtsp_client *client,
			   struct rtsp_client_session *session,
			   unsigned int timeout_ms)
{
	int res = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF((client->methods_allowed != 0) &&
					 !(client->methods_allowed &
					   RTSP_METHOD_FLAG_GET_PARAMETER),
				 ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED) {
		/* If we are still not reconnected when trying to send
		 * a keep-alive, remove the session */
		ULOGI("trying to send a keep-alive while not connected, "
		      "remove the session");
		res = -EPIPE;
		rtsp_client_remove_session(client, session->id, res, 0);
		return res;
	}

	/* Keep alive already in progress (waiting for reply) */
	if (session->keep_alive_in_progress)
		return -EBUSY;

	/* Another request is pending, retry in 2 seconds */
	if (client->request.is_pending) {
		reset_keep_alive_timer(session, 2);
		return -EBUSY;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_GET_PARAMETER;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session->id);

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	session->keep_alive_in_progress = 1;
	client->request.is_pending = 1;
	client->cseq++;

	return 0;
}


void rtsp_client_pomp_timer_cb(struct pomp_timer *timer, void *userdata)
{
	int res;
	struct rtsp_client_session *session = userdata;

	ULOG_ERRNO_RETURN_IF(session == NULL, EINVAL);

	res = send_keep_alive(
		session->client, session, RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("send_keep_alive", -res);
}


static int send_teardown(struct rtsp_client *client,
			 const char *session_id,
			 void *req_userdata,
			 unsigned int timeout_ms,
			 int internal)
{
	int res = 0;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		client->methods_allowed != 0 &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_TEARDOWN),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;
	if (client->request.is_pending)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_TEARDOWN;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	client->request.userdata = req_userdata;

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	session->internal_teardown = internal;
	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


static void play_request_complete(struct rtsp_client *client,
				  const char *session_id,
				  enum rtsp_client_req_status status,
				  int status_code,
				  struct rtsp_response_header *resp_h,
				  void *req_userdata)
{
	static struct rtsp_rtp_info_header rtp_info_default = {
		.url = NULL,
		.seq_valid = 0,
		.seq = 0,
		.rtptime_valid = 0,
		.rtptime = 0,
	};
	struct rtsp_rtp_info_header *rtp_info;

	if (status != RTSP_CLIENT_REQ_STATUS_OK) {
		(*client->cbs.play_resp)(client,
					 session_id,
					 status,
					 rtsp_status_to_errno(status_code),
					 NULL,
					 0.0,
					 0,
					 0,
					 0,
					 0,
					 client->cbs_userdata,
					 req_userdata);
		return;
	}

	rtp_info = (resp_h->rtp_info_count == 0) ? &rtp_info_default
						 : resp_h->rtp_info[0];
	(*client->cbs.play_resp)(client,
				 session_id,
				 status,
				 rtsp_status_to_errno(status_code),
				 &resp_h->range,
				 resp_h->scale,
				 rtp_info->seq_valid,
				 rtp_info->seq,
				 rtp_info->rtptime_valid,
				 rtp_info->rtptime,
				 client->cbs_userdata,
				 req_userdata);
}


static int request_complete(struct rtsp_client *client,
			    struct rtsp_response_header *resp_h,
			    char *body,
			    size_t body_len,
			    enum rtsp_client_req_status status)
{
	int res = 0, err, session_removed = 0, internal_teardown = 0;
	enum rtsp_method_type method;
	char *req_uri;
	char *req_session_id;
	char *content_base;
	void *req_userdata;
	const char *method_str;
	char *body_with_null;
	const char *session_id = NULL;

	struct rtsp_client_session *session = NULL;

	struct rtsp_response_header dummy;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	if (!client->request.is_pending)
		return 0;

	req_session_id = xstrdup(client->request.header.session_id);

	if (!resp_h) {
		memset(&dummy, 0, sizeof(dummy));
		dummy.session_id = req_session_id;
		resp_h = &dummy;
	}

	/* Save current request info */
	method = client->request.header.method;
	req_userdata = client->request.userdata;

	/* Reset request/response buffer */
	req_uri = xstrdup(client->request.header.uri);
	rtsp_request_header_clear(&client->request.header);
	client->request.is_pending = 0;
	free(client->request.uri);
	client->request.uri = NULL;
	client->request.userdata = NULL;

	/* Clear the response timeout timer */
	err = pomp_timer_clear(client->request.timer);
	if (err < 0)
		ULOG_ERRNO("pomp_timer_clear", -err);

	/* Set status */
	if ((status == RTSP_CLIENT_REQ_STATUS_OK) &&
	    (RTSP_STATUS_CLASS(resp_h->status_code) !=
	     RTSP_STATUS_CLASS_SUCCESS))
		status = RTSP_CLIENT_REQ_STATUS_FAILED;

	if (resp_h->status_code == RTSP_STATUS_CODE_SESSION_NOT_FOUND)
		session_removed = 1;

	session_id = req_session_id;
	if (resp_h->session_id)
		session_id = resp_h->session_id;

	ULOGI("response to RTSP request %s: "
	      "status=%d(%s) cseq=%d session=%s req_status=%s",
	      rtsp_method_type_str(method),
	      resp_h->status_code,
	      resp_h->status_string ? resp_h->status_string : "-",
	      resp_h->cseq,
	      session_id ? session_id : "-",
	      rtsp_client_req_status_str(status));

	if (session_id) {
		session = rtsp_client_get_session(
			client,
			session_id,
			(method == RTSP_METHOD_TYPE_SETUP) ? 1 : 0);
		if (!session) {
			if (method == RTSP_METHOD_TYPE_SETUP) {
				ULOGE("%s: cannot create session", __func__);
				res = -ENOMEM;
			} else {
				ULOGE("%s: session not found", __func__);
				res = -ENOENT;
			}
			goto exit;
		}
		session->keep_alive_in_progress = 0;

		/* Set the timer to 80% of the timeout value */
		session->timeout = resp_h->session_timeout;
		err = reset_keep_alive_timer(session,
					     session->timeout * 8 / 10);
		if (err < 0)
			ULOG_ERRNO("reset_keep_alive_timer", -err);

		/* Save content base to session if given */
		if (client->request.content_base && !session->content_base)
			session->content_base =
				xstrdup(client->request.content_base);
		xfree((void **)&client->request.content_base);
	}

	switch (method) {
	case RTSP_METHOD_TYPE_OPTIONS:
		client->methods_allowed = resp_h->public_methods;
		(*client->cbs.options_resp)(
			client,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			client->methods_allowed,
			client->cbs_userdata,
			req_userdata);
		break;
	case RTSP_METHOD_TYPE_DESCRIBE:
		body_with_null = calloc(body_len + 1, 1);
		if (!body_with_null) {
			res = -ENOMEM;
			goto exit;
		}
		memcpy(body_with_null, body, body_len);
		if (resp_h->content_base)
			content_base = resp_h->content_base;
		else if (resp_h->content_location)
			content_base = resp_h->content_location;
		else
			content_base = req_uri;
		(*client->cbs.describe_resp)(
			client,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			content_base,
			body_with_null,
			client->cbs_userdata,
			req_userdata);
		free(body_with_null);
		break;
	case RTSP_METHOD_TYPE_SETUP:
		if (status == RTSP_CLIENT_REQ_STATUS_OK) {
			(*client->cbs.setup_resp)(
				client,
				session_id,
				status,
				rtsp_status_to_errno(resp_h->status_code),
				resp_h->transport->src_stream_port,
				resp_h->transport->src_control_port,
				resp_h->transport->ssrc_valid,
				resp_h->transport->ssrc,
				client->cbs_userdata,
				req_userdata);
		} else {
			(*client->cbs.setup_resp)(
				client,
				session_id,
				status,
				rtsp_status_to_errno(resp_h->status_code),
				0,
				0,
				0,
				0,
				client->cbs_userdata,
				req_userdata);
		}
		break;
	case RTSP_METHOD_TYPE_PLAY:
		play_request_complete(client,
				      session_id,
				      status,
				      resp_h->status_code,
				      resp_h,
				      req_userdata);
		break;
	case RTSP_METHOD_TYPE_PAUSE:
		if (status == RTSP_CLIENT_REQ_STATUS_OK) {
			(*client->cbs.pause_resp)(
				client,
				session_id,
				status,
				rtsp_status_to_errno(resp_h->status_code),
				&resp_h->range,
				client->cbs_userdata,
				req_userdata);
		} else {
			(*client->cbs.pause_resp)(
				client,
				session_id,
				status,
				rtsp_status_to_errno(resp_h->status_code),
				NULL,
				client->cbs_userdata,
				req_userdata);
		}
		break;
	case RTSP_METHOD_TYPE_TEARDOWN:
		session_removed = 1;
		if (session == NULL)
			break;
		if (!session->internal_teardown) {
			(*client->cbs.teardown_resp)(
				client,
				session_id,
				status,
				rtsp_status_to_errno(resp_h->status_code),
				client->cbs_userdata,
				req_userdata);
			session->internal_teardown = 0;
		}
		break;
	case RTSP_METHOD_TYPE_GET_PARAMETER:
		if (session == NULL)
			break;
		/* Timeout on a keep alive: retry a new keep alive and after
		 * several failed attempts, initiate a teardown */
		if (status == RTSP_CLIENT_REQ_STATUS_TIMEOUT) {
			session->failed_keep_alive++;
			if (session->failed_keep_alive >=
			    RTSP_CLIENT_MAX_FAILED_KEEP_ALIVE) {
				ULOGW("%d failed keep alive attempts, "
				      "sending teardown request",
				      session->failed_keep_alive);
				internal_teardown = 1;
			} else {
				err = reset_keep_alive_timer(session, 1);
				if (err < 0) {
					ULOG_ERRNO("reset_keep_alive_timer",
						   -err);
				}
			}
		} else {
			session->failed_keep_alive = 0;
		}
		break;
	default:
		method_str = rtsp_method_type_str(method);
		if (method_str != NULL)
			ULOGE("unsupported request: %s", method_str);
		else
			ULOGE("unknown request: %d", method);
		break;
	}

	if (internal_teardown) {
		err = send_teardown(client,
				    session_id,
				    NULL,
				    RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS,
				    1);
		if (err < 0)
			ULOG_ERRNO("send_teardown", -err);
	}
	if (session_removed) {
		if (status == RTSP_CLIENT_REQ_STATUS_TIMEOUT) {
			err = rtsp_client_remove_session(
				client, session_id, -ETIMEDOUT, 0);
		} else {
			err = rtsp_client_remove_session(
				client, session_id, resp_h->status_code, 0);
		}
		if (err < 0)
			ULOG_ERRNO("rtsp_client_remove_session", -err);
	}

exit:
	free(req_session_id);
	free(req_uri);
	return res;
}


static void rtsp_client_pomp_event_cb(struct pomp_ctx *ctx,
				      enum pomp_event event,
				      struct pomp_conn *conn,
				      const struct pomp_msg *msg,
				      void *userdata)
{
	int res;
	struct rtsp_client *client = userdata;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	switch (event) {
	case POMP_EVENT_CONNECTED:
		ULOGI("client connected");
		client->conn_state = RTSP_CLIENT_CONN_STATE_CONNECTED;
		if (client->user_connecting) {
			/* Connection initiated by the user */
			client->user_connecting = 0;
			(*client->cbs.connection_state)(client,
							client->conn_state,
							client->cbs_userdata);
		}
		/* If a session already exists, send a keep-alive
		 * in 2 seconds; this allows quickly seeing if a
		 * session timeout has occurred on the server side
		 * during the time disconnected */
		list_walk_entry_forward(&client->sessions, session, node)
		{
			res = reset_keep_alive_timer(session, 2);
			if (res < 0)
				ULOG_ERRNO("reset_keep_alive_timer", -res);
		}
		break;

	case POMP_EVENT_DISCONNECTED:
		if (client->conn_state ==
		    RTSP_CLIENT_CONN_STATE_DISCONNECTING) {
			/* Disconnetion initiated by the user */
			ULOGI("client disconnected");
			client->conn_state =
				RTSP_CLIENT_CONN_STATE_DISCONNECTED;

			xfree((void **)&client->addr);
			res = request_complete(client,
					       NULL,
					       NULL,
					       0,
					       RTSP_CLIENT_REQ_STATUS_ABORTED);
			if (res < 0)
				ULOG_ERRNO("request_complete", -res);

			(*client->cbs.connection_state)(client,
							client->conn_state,
							client->cbs_userdata);
		} else if (client->conn_state ==
			   RTSP_CLIENT_CONN_STATE_CONNECTED) {
			/* Disconnetion by the network, auto reconnect*/
			ULOGI("client disconnected, waiting for reconnection");
			client->conn_state = RTSP_CLIENT_CONN_STATE_CONNECTING;
			client->user_connecting = 0;
			res = request_complete(client,
					       NULL,
					       NULL,
					       0,
					       RTSP_CLIENT_REQ_STATUS_ABORTED);
			if (res < 0)
				ULOG_ERRNO("request_complete", -res);
		}

		break;

	default:
	case POMP_EVENT_MSG:
		/* Never received for raw context */
		break;
	}
}


static int rtsp_client_request_process(struct rtsp_client *client,
				       struct pomp_conn *conn,
				       struct rtsp_message *msg)
{
	int ret = 0;
	int not_implem = 0;
	ssize_t res;
	int status_code;
	const char *status_string;
	char *body_with_null;
	char *content_base;
	struct rtsp_message resp;
	struct pomp_buffer *resp_buf;
	struct rtsp_string resp_str;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(conn == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);

	memset(&resp, 0, sizeof(resp));
	memset(&resp_str, 0, sizeof(resp_str));

	ULOGI("received RTSP request %s: cseq=%d session=%s",
	      rtsp_method_type_str(msg->header.req.method),
	      msg->header.req.cseq,
	      msg->header.req.session_id ? msg->header.req.session_id : "-");

	switch (msg->header.req.method) {
	case RTSP_METHOD_TYPE_ANNOUNCE:
		body_with_null = calloc(msg->body_len + 1, 1);
		ULOG_ERRNO_RETURN_ERR_IF(body_with_null == NULL, ENOMEM);
		memcpy(body_with_null, msg->body, msg->body_len);
		content_base =
			client_uri_to_content_base(client, msg->header.req.uri);
		(*client->cbs.announce)(client,
					content_base,
					body_with_null,
					client->cbs_userdata);
		free(content_base);
		free(body_with_null);
		status_code = RTSP_STATUS_CODE_OK;
		status_string = RTSP_STATUS_STRING_OK;
		break;
	case RTSP_METHOD_TYPE_GET_PARAMETER:
		if (msg->body_len == 0) {
			status_code = RTSP_STATUS_CODE_OK;
			status_string = RTSP_STATUS_STRING_OK;
		} else {
			ULOGW("non-empty get parameter in RTSP client");
			status_code = RTSP_STATUS_CODE_NOT_IMPLEMENTED;
			status_string = RTSP_STATUS_STRING_NOT_IMPLEMENTED;
		}
		break;
	default:
		ULOGW("received unhandled %s request in RTSP client",
		      rtsp_method_type_str(msg->header.req.method));
		status_code = RTSP_STATUS_CODE_NOT_IMPLEMENTED;
		status_string = RTSP_STATUS_STRING_NOT_IMPLEMENTED;
		not_implem = 1;
		break;
	}

	resp.header.resp.cseq = msg->header.req.cseq;
	resp.header.resp.status_code = status_code;
	resp.header.resp.status_string = strdup(status_string);
	time(&resp.header.resp.date);

	ULOGI("send RTSP response to %s: status=%d(%s) cseq=%d session=%s",
	      rtsp_method_type_str(msg->header.req.method),
	      resp.header.resp.status_code,
	      resp.header.resp.status_string ? resp.header.resp.status_string
					     : "-",
	      resp.header.resp.cseq,
	      msg->header.req.session_id ? msg->header.req.session_id : "-");

	resp_buf = pomp_buffer_new(PIPE_BUF - 1);
	if (resp_buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = pomp_buffer_get_data(
		resp_buf, (void **)&resp_str.str, NULL, &resp_str.max_len);
	if (ret < 0)
		goto out;

	res = rtsp_response_header_write(&resp.header.resp, &resp_str);
	if (res < 0) {
		ret = res;
		goto out;
	}
	ret = pomp_buffer_set_len(resp_buf, resp_str.len);
	if (ret < 0) {
		ULOG_ERRNO("pomp_buffer_set_len", -ret);
		goto out;
	}

	ret = pomp_conn_send_raw_buf(conn, resp_buf);
	if (ret < 0)
		ULOG_ERRNO("pomp_conn_send_raw_buf", -ret);

out:
	rtsp_response_header_clear(&resp.header.resp);
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	if (ret == 0 && not_implem)
		ret = -ENOSYS;
	return ret;
}


static int rtsp_client_response_process(struct rtsp_client *client,
					struct rtsp_message *msg)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);

	/* Note: VLC server doesn't repeat the cseq in error case */
	if (msg->header.resp.cseq != client->request.header.cseq) {
		ULOGE("%s: unexpected CSeq (req: %d, resp: %d)",
		      __func__,
		      client->request.header.cseq,
		      msg->header.resp.cseq);
		return -EPROTO;
	}

	return request_complete(client,
				&msg->header.resp,
				msg->body,
				msg->body_len,
				RTSP_CLIENT_REQ_STATUS_OK);
}


static void rtsp_client_pomp_raw_cb(struct pomp_ctx *ctx,
				    struct pomp_conn *conn,
				    struct pomp_buffer *buf,
				    void *userdata)
{
	struct rtsp_client *client = userdata;
	int res, err;
	size_t len = 0;
	const void *cdata = NULL;
	struct rtsp_message msg;
	memset(&msg, 0x0, sizeof(msg));

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	/* Get the message data */
	res = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	if (res < 0) {
		ULOG_ERRNO("pomp_buffer_get_cdata", -res);
		return;
	}

	/* Add the data to the buffer */
	res = pomp_buffer_append_data(client->response.buf, cdata, len);
	if (res < 0) {
		ULOG_ERRNO("pomp_buffer_append_data", -res);
		return;
	}

	/* Iterate over complete messages */
	while ((res = rtsp_get_next_message(client->response.buf,
					    &msg,
					    &client->parser_ctx)) == 0) {
		if (msg.type == RTSP_MESSAGE_TYPE_REQUEST) {
			err = rtsp_client_request_process(client, conn, &msg);
			if (err < 0)
				ULOG_ERRNO("rtsp_client_request_process", -err);
		} else {
			err = rtsp_client_response_process(client, &msg);
			if (err < 0) {
				ULOG_ERRNO("rtsp_client_response_process",
					   -err);
			}
		}

		rtsp_buffer_remove_first_bytes(client->response.buf,
					       msg.total_len);
	}

	if (res != -EAGAIN)
		ULOG_ERRNO("rtsp_get_next_message", -res);

	rtsp_buffer_remove_first_bytes(client->response.buf, msg.total_len);
}


static void rtsp_client_resp_timeout_cb(struct pomp_timer *timer,
					void *userdata)
{
	struct rtsp_client *client = userdata;
	int ret = 0;
	enum rtsp_method_type method;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	method = client->request.header.method;

	ret = request_complete(
		client, NULL, NULL, 0, RTSP_CLIENT_REQ_STATUS_TIMEOUT);
	if (ret < 0)
		ULOG_ERRNO("request_complete", -ret);
}


int rtsp_client_new(struct pomp_loop *loop,
		    const char *software_name,
		    const struct rtsp_client_cbs *cbs,
		    void *userdata,
		    struct rtsp_client **ret_obj)
{
	int res = 0;
	struct rtsp_client *client = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(loop == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->connection_state == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->session_removed == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->options_resp == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->describe_resp == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->setup_resp == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->play_resp == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->pause_resp == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->teardown_resp == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cbs->announce == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);

	/* Allocate structure */
	client = calloc(1, sizeof(*client));
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, ENOMEM);

	/* Initialize structure */
	client->loop = loop;
	client->cbs = *cbs;
	client->cbs_userdata = userdata;
	client->cseq = 1;

	list_init(&client->sessions);

	/* Create a timer for response timeout */
	client->request.timer =
		pomp_timer_new(loop, &rtsp_client_resp_timeout_cb, client);
	if (client->request.timer == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_timer_new", -res);
		goto error;
	}

	client->software_name =
		(software_name) ? strdup(software_name)
				: strdup(RTSP_CLIENT_DEFAULT_SOFTWARE_NAME);
	if (client->software_name == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("strdup", -res);
		goto error;
	}

	client->ctx = pomp_ctx_new_with_loop(
		&rtsp_client_pomp_event_cb, client, client->loop);
	if (client->ctx == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_ctx_new_with_loop", -res);
		goto error;
	}

	res = pomp_ctx_setup_keepalive(client->ctx, 0, 0, 0, 0);
	if (res < 0) {
		ULOG_ERRNO("pomp_ctx_setup_keepalive", -res);
		goto error;
	}

	res = pomp_ctx_set_socket_cb(client->ctx, &pomp_socket_cb);
	if (res < 0) {
		ULOG_ERRNO("pomp_ctx_set_socket_cb", -res);
		goto error;
	}

	res = pomp_ctx_set_raw(client->ctx, &rtsp_client_pomp_raw_cb);
	if (res < 0) {
		ULOG_ERRNO("pomp_ctx_set_raw", -res);
		goto error;
	}

	/* Initialize request */
	client->request.buf = pomp_buffer_new(PIPE_BUF - 1);
	if (client->request.buf == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_buffer_new", -res);
		goto error;
	}

	/* Initialize response */
	client->response.buf = pomp_buffer_new(PIPE_BUF - 1);
	if (client->response.buf == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_buffer_new", -res);
		goto error;
	}

	*ret_obj = client;
	return 0;
error:
	rtsp_client_destroy(client);
	*ret_obj = NULL;
	return res;
}


int rtsp_client_destroy(struct rtsp_client *client)
{
	int err;

	if (client == NULL)
		return 0;

	err = pomp_timer_destroy(client->request.timer);
	if (err < 0)
		ULOG_ERRNO("pomp_timer_destroy", -err);

	rtsp_client_remove_all_sessions(client);

	err = pomp_ctx_destroy(client->ctx);
	if (err < 0)
		ULOG_ERRNO("pomp_ctx_destroy", -err);

	pomp_buffer_unref(client->request.buf);
	pomp_buffer_unref(client->response.buf);

	rtsp_request_header_clear(&client->request.header);
	rtsp_message_clear(&client->parser_ctx.msg);

	free(client->addr);
	free(client->software_name);
	free(client);

	return 0;
}


int rtsp_client_connect(struct rtsp_client *client, const char *addr)
{
	int res = 0;
	char *url_tmp = NULL;
	char *server_addr = NULL;
	uint16_t server_port = RTSP_DEFAULT_PORT;
	struct sockaddr_in remote_addr_in;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(addr == NULL, EINVAL);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_DISCONNECTED)
		return -EBUSY;

	xfree((void **)&client->addr);

	client->addr = strdup(addr);
	if (client->addr == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("strdup", -res);
		goto error;
	}

	/* Parse the URL */
	url_tmp = strdup(addr);
	if (url_tmp == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("strdup", -res);
		goto error;
	}

	res = rtsp_url_parse(url_tmp, &server_addr, &server_port, NULL);
	if (res < 0)
		goto error;

	/* Check the URL validity */
	if (!server_addr || strlen(server_addr) == 0) {
		res = -EINVAL;
		ULOGE("invalid server host: %s", addr);
		goto error;
	}
	if (server_port == 0) {
		res = -EINVAL;
		ULOGE("invalid server port: %s", addr);
		goto error;
	}

	/* Set address */
	res = inet_pton(AF_INET, server_addr, &remote_addr_in.sin_addr);
	if (res <= 0) {
		res = -errno;
		ULOG_ERRNO("inet_pton('%s')", -res, server_addr);
		goto error;
	}
	remote_addr_in.sin_family = AF_INET;
	remote_addr_in.sin_port = htons(server_port);

	ULOGI("connecting to address %s port %d", server_addr, server_port);
	client->conn_state = RTSP_CLIENT_CONN_STATE_CONNECTING;
	client->user_connecting = 1;

	res = pomp_ctx_connect(client->ctx,
			       (const struct sockaddr *)&remote_addr_in,
			       sizeof(remote_addr_in));
	if (res < 0) {
		ULOG_ERRNO("pomp_ctx_connect", -res);
		goto error;
	}

	free(url_tmp);
	return 0;

error:
	client->conn_state = RTSP_CLIENT_CONN_STATE_DISCONNECTED;
	client->user_connecting = 0;
	xfree((void **)&client->addr);
	free(url_tmp);
	return res;
}


int rtsp_client_disconnect(struct rtsp_client *client)
{
	int res = 0;
	int already_disconnected = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	if (client->conn_state == RTSP_CLIENT_CONN_STATE_DISCONNECTED)
		return -EPROTO;
	if (client->conn_state == RTSP_CLIENT_CONN_STATE_DISCONNECTING)
		return 0;
	if (client->conn_state == RTSP_CLIENT_CONN_STATE_CONNECTING)
		already_disconnected = 1;

	client->conn_state = RTSP_CLIENT_CONN_STATE_DISCONNECTING;

	rtsp_client_remove_all_sessions(client);

	res = pomp_ctx_stop(client->ctx);
	if (res < 0) {
		ULOG_ERRNO("pomp_ctx_stop", -res);
		return res;
	}

	if (already_disconnected) {
		ULOGI("client disconnected (already disconnected)");
		client->conn_state = RTSP_CLIENT_CONN_STATE_DISCONNECTED;
		xfree((void **)&client->addr);
		request_complete(
			client, NULL, NULL, 0, RTSP_CLIENT_REQ_STATUS_ABORTED);
		(*client->cbs.connection_state)(
			client, client->conn_state, client->cbs_userdata);
	}

	return 0;
}


int rtsp_client_options(struct rtsp_client *client,
			void *req_userdata,
			unsigned int timeout_ms)
{
	int res = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	if (client->request.is_pending)
		return -EBUSY;

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_OPTIONS;
	client->request.header.uri = xstrdup("*");
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.userdata = req_userdata;

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_describe(struct rtsp_client *client,
			 const char *path,
			 void *req_userdata,
			 unsigned int timeout_ms)
{
	int res = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_DESCRIBE),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	if (client->request.is_pending)
		return -EBUSY;

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_DESCRIBE;
	client->request.header.uri = make_uri(client, path);
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.accept = xstrdup(RTSP_CONTENT_TYPE_SDP);
	client->request.userdata = req_userdata;

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_setup(struct rtsp_client *client,
		      const char *content_base,
		      const char *resource_url,
		      const char *session_id,
		      enum rtsp_delivery delivery,
		      enum rtsp_lower_transport lower_transport,
		      uint16_t client_stream_port,
		      uint16_t client_control_port,
		      void *req_userdata,
		      unsigned int timeout_ms)
{
	int res = 0;
	struct rtsp_transport_header *th;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(content_base == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(resource_url == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client_stream_port == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client_control_port == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_SETUP),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	if (client->request.is_pending)
		return -EBUSY;

	/* If a session id is passed, make sure that we know the session,
	 * and that its content_base is the right one */
	if (session_id != NULL) {
		struct rtsp_client_session *session =
			rtsp_client_get_session(client, session_id, 0);
		if (!session) {
			ULOGE("%s: session not found", __func__);
			return -ENOENT;
		}
		if (strcmp(session->content_base, content_base) != 0) {
			ULOGE("%s: invalid content base", __func__);
			return -EINVAL;
		}
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_SETUP;

	res = format_request_uri(
		client, content_base, resource_url, &client->request.uri);
	if (res < 0)
		return res;

	client->request.content_base = xstrdup(content_base);

	client->request.header.uri = xstrdup(client->request.uri);
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);

	th = rtsp_transport_header_new();
	client->request.header.transport[0] = th;
	client->request.header.transport_count = 1;
	client->request.header.transport[0]->transport_protocol =
		strdup(RTSP_TRANSPORT_PROTOCOL_RTP);
	client->request.header.transport[0]->transport_profile =
		strdup(RTSP_TRANSPORT_PROFILE_AVP);
	client->request.header.transport[0]->lower_transport = lower_transport;
	client->request.header.transport[0]->delivery = delivery;
	client->request.header.transport[0]->dst_stream_port =
		client_stream_port;
	client->request.header.transport[0]->dst_control_port =
		client_control_port;
	client->request.header.session_id = xstrdup(session_id);
	client->request.userdata = req_userdata;

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_play(struct rtsp_client *client,
		     const char *session_id,
		     const struct rtsp_range *range,
		     float scale,
		     void *req_userdata,
		     unsigned int timeout_ms)
{
	int res = 0;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_PLAY),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	if (client->request.is_pending)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_PLAY;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	client->request.header.range = *range;
	client->request.header.scale = scale;
	client->request.userdata = req_userdata;

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_pause(struct rtsp_client *client,
		      const char *session_id,
		      const struct rtsp_range *range,
		      void *req_userdata,
		      unsigned int timeout_ms)
{
	int res = 0;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_PAUSE),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;
	if (client->request.is_pending)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_PAUSE;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	client->request.header.range = *range;
	client->request.userdata = req_userdata;

	/* Send the request */
	res = send_request(client, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_teardown(struct rtsp_client *client,
			 const char *session_id,
			 void *req_userdata,
			 unsigned int timeout_ms)
{
	return send_teardown(client, session_id, req_userdata, timeout_ms, 0);
}


int rtsp_client_cancel(struct rtsp_client *client)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	return request_complete(
		client, NULL, NULL, 0, RTSP_CLIENT_REQ_STATUS_CANCELED);
}
