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


static inline void clear_remote_info(struct rtsp_client *client)
{
	rtsp_url_free(client->remote.url);
	client->remote.url = NULL;
	xfree((void **)&client->remote.url_str);
	rtsp_authorization_header_free(&client->auth);
	rtsp_authorization_header_free(&client->server_auth);
}


static int set_socket_rxbuf_size(struct rtsp_client *client,
				 struct tskt_socket *sock)
{
	int ret;

	if (client->sock_params.rxbuf_size == 0)
		return 0;

	ret = tskt_socket_set_rxbuf_size(sock, client->sock_params.rxbuf_size);
	if (ret < 0) {
		ULOGW_ERRNO(-ret, "tskt_socket_set_rxbuf_size");
		return ret;
	}
	ret = tskt_socket_get_rxbuf_size(sock);
	if (ret < 0) {
		ULOGW_ERRNO(-ret, "tskt_socket_get_rxbuf_size");
		return ret;
	}
	if ((size_t)ret != 2 * client->sock_params.rxbuf_size) {
		ULOGW("failed to set rx buffer size: got %d, expecting %zu",
		      ret / 2,
		      client->sock_params.rxbuf_size);
		return -ENOSYS;
	}

	return 0;
}


static int set_socket_txbuf_size(struct rtsp_client *client,
				 struct tskt_socket *sock)
{
	int ret;

	if (client->sock_params.txbuf_size == 0)
		return 0;

	ret = tskt_socket_set_txbuf_size(sock, client->sock_params.txbuf_size);
	if (ret < 0) {
		ULOGW_ERRNO(-ret, "tskt_socket_set_txbuf_size");
		return ret;
	}
	ret = tskt_socket_get_txbuf_size(sock);
	if (ret < 0) {
		ULOGW_ERRNO(-ret, "tskt_socket_get_txbuf_size");
		return ret;
	}
	if ((size_t)ret != 2 * client->sock_params.txbuf_size) {
		ULOGW("failed to set tx buffer size: got %d, expecting %zu",
		      ret / 2,
		      client->sock_params.txbuf_size);
		return -ENOSYS;
	}

	return 0;
}


static int set_socket_class_selector(struct rtsp_client *client,
				     struct tskt_socket *sock)
{
	int ret;

	if (client->sock_params.class_selector == UINT32_MAX)
		return 0;

	ret = tskt_socket_set_class_selector(
		sock, client->sock_params.class_selector);
	if (ret < 0) {
		ULOGW_ERRNO(-ret, "tskt_socket_set_class_selector");
		return ret;
	}

	return 0;
}


static void tskt_client_socket_created_cb(struct tskt_client *self,
					  struct tskt_socket *sock,
					  int fd,
					  void *userdata)
{
	UNUSED(self);

	struct rtsp_client *client = userdata;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	(void)set_socket_rxbuf_size(client, sock);
	(void)set_socket_txbuf_size(client, sock);
	(void)set_socket_class_selector(client, sock);

	if (client->cbs.socket_cb)
		(*client->cbs.socket_cb)(fd, client->cbs_userdata);
}


static struct tskt_socket *
tskt_client_socket_upgrade_cb(struct tskt_client *ctx,
			      struct tskt_socket *sock,
			      void *userdata)
{
	int err;
	struct rtsp_client *client = userdata;
	struct tskt_socket *tmp_sock = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(client == NULL, EINVAL, NULL);

	if (rtsp_url_get_scheme(client->remote.url) != RTSP_URL_SCHEME_TCP_TLS)
		return sock;

	/* create TLS socket */
	err = ttls_socket_new_with_ctx(client->ssl_ctx, sock, &tmp_sock);
	if (err < 0) {
		ULOG_ERRNO("ttls_socket_new_with_ctx", -err);
		return NULL;
	}

	return tmp_sock;
}


static char *make_uri(struct rtsp_client *client, const char *path)
{
	char *tmp;
	int ret;

	if (!client)
		return NULL;
	if (!path)
		return xstrdup(client->remote.url_str);

	ret = asprintf(&tmp, "%s/%s", client->remote.url_str, path);
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		return NULL;
	}
	return tmp;
}


static int format_request_uri(const struct rtsp_client *client,
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
		       (get_last_char(content_base, PATH_MAX) == '/') ? ""
								      : "/",
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
	char *content_base = NULL;

	if (!client || !uri)
		return NULL;

	size_t url_len = strnlen(client->remote.url_str, PATH_MAX);
	if ((url_len == 0) || (url_len >= PATH_MAX))
		return NULL;

	/* If URI is an absolute rtsp:// URI, return it */
	if (strncmp(uri, RTSP_SCHEME_TCP, strlen(RTSP_SCHEME_TCP)) == 0)
		return strdup(uri);

	if (uri[0] == '/')
		uri = &uri[1];

	/* Otherwise, alloc a content_base in the form
	 * client->remote.url_str/uri without duplicate '/' */
	bool needs_slash =
		(get_last_char(client->remote.url_str, PATH_MAX) != '/');

	ret = asprintf(&content_base,
		       "%s%s%s",
		       client->remote.url_str,
		       needs_slash ? "/" : "",
		       uri);
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		return NULL;
	}
	return content_base;
}


static void set_connection_state(struct rtsp_client *client,
				 enum rtsp_client_conn_state new_state)
{
	if (client->conn_state == new_state)
		return;

	ULOGD("connection_state: %s to %s",
	      rtsp_client_conn_state_str(client->conn_state),
	      rtsp_client_conn_state_str(new_state));

	client->conn_state = new_state;

	(*client->cbs.connection_state)(
		client, client->conn_state, client->cbs_userdata);
}


static int send_raw_buf(struct rtsp_client *client, struct pomp_buffer *buf)
{
	int res;
	struct tpkt_packet *pkt = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(buf == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->sock == NULL, EPROTO);

	res = tpkt_new_from_buffer(buf, &pkt);
	if (res < 0) {
		ULOG_ERRNO("tpkt_new_from_buffer", -res);
		return res;
	}

	/* Send the request */
	res = tskt_client_send_pkt(client->tclient, pkt);
	if (res < 0) {
		(void)tpkt_unref(pkt);
		if (res != -EAGAIN)
			ULOG_ERRNO("tskt_client_send_pkt", -res);
		return res;
	}
	(void)tpkt_unref(pkt);
	return 0;
}


static int send_request(struct rtsp_client *client,
			const char *content,
			unsigned int timeout_ms)
{
	int res = 0;
	struct rtsp_string request = {};

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	ULOGI("send RTSP request %s: cseq=%d session=%s",
	      rtsp_method_type_str(client->request.header.method),
	      client->request.header.cseq,
	      client->request.header.session_id
		      ? client->request.header.session_id
		      : "-");

	res = pomp_buffer_get_data(client->request.buf,
				   (void **)&request.str,
				   NULL,
				   &request.max_len);
	if (res < 0) {
		ULOG_ERRNO("pomp_buffer_get_data", -res);
		return res;
	}

	res = rtsp_request_header_write(&client->request.header, &request);
	if (res < 0) {
		ULOG_ERRNO("rtsp_request_header_write", -res);
		return res;
	}

	if (content != NULL) {
		res = rtsp_sprintf(&request, "%s", content);
		if (res < 0) {
			ULOG_ERRNO("rtsp_sprintf", -res);
			return res;
		}
	}

	/* Set buffer length */
	res = pomp_buffer_set_len(client->request.buf, request.len);
	if (res < 0) {
		ULOG_ERRNO("pomp_buffer_set_len", -res);
		return res;
	}

	/* Send the request */
	res = send_raw_buf(client, client->request.buf);
	if (res < 0) {
		if (res != -EAGAIN)
			ULOG_ERRNO("send_raw_buf", -res);
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

static int clear_pending_keep_alive_timer(struct rtsp_client *client)
{
	const struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	list_walk_entry_forward(&client->sessions, session, node)
	{
		if (!session->keep_alive_in_progress)
			continue;

		/* Clear the response timeout timer related to
		 * keep alive */
		int err = pomp_timer_clear(client->request.timer);
		if (err < 0)
			ULOG_ERRNO("pomp_timer_clear", -err);
		return 1;
	}

	return 0;
}

static int reset_keep_alive_timer(struct rtsp_client_session *session,
				  int timer_msec)
{
	int res;

	ULOG_ERRNO_RETURN_ERR_IF(session == NULL, EINVAL);

	if (timer_msec == 0)
		return 0;

	if (timer_msec > 0) {
		res = pomp_timer_set(session->timer, timer_msec);
		if (res < 0)
			ULOG_ERRNO("pomp_timer_set", -res);
		return res;
	} else {
		res = pomp_timer_clear(session->timer);
		if (res < 0)
			ULOG_ERRNO("pomp_timer_clear", -res);
		return res;
	}
}


static int generate_authorization_header(struct rtsp_client *client)
{
	int ret;
	const char *pass = NULL;
	struct rtsp_authorization_header *auth = NULL;

	if (client->auth == NULL)
		return 0;

	/* Cannot reply to Digest without nonce */
	if (client->auth->type == RTSP_AUTH_TYPE_DIGEST &&
	    client->auth->nonce == NULL)
		return -EAGAIN;

	ULOG_ERRNO_RETURN_ERR_IF(client->request.header.authorization != NULL,
				 EEXIST);

	pass = rtsp_url_get_pass(client->remote.url);
	if (!pass)
		return -EINVAL;

	auth = rtsp_authorization_header_new();
	if (!auth)
		return -ENOMEM;

	/* Copy current auth state */
	rtsp_authorization_header_copy(client->auth, auth);

	switch (client->auth->type) {
	case RTSP_AUTH_TYPE_BASIC:
		if (!auth->credentials) {
			ret = rtsp_auth_generate_basic_response(auth, pass);
			if (ret < 0) {
				ULOG_ERRNO("rtsp_auth_generate_basic_response",
					   -ret);
				goto error;
			}
		}
		break;
	case RTSP_AUTH_TYPE_DIGEST:
		dup_field(&auth->uri, client->request.header.uri);
		ret = rtsp_auth_generate_digest_response(
			auth, pass, client->request.header.method);
		if (ret < 0) {
			ULOG_ERRNO("rtsp_auth_generate_digest_response", -ret);
			goto error;
		}
		break;
	default:
		ret = -EINVAL;
		ULOG_ERRNO("invalid authorization type: %d",
			   -ret,
			   client->auth->type);
		goto error;
	}

	/* Update client->auth fields like nc, cnonce, etc. */
	rtsp_authorization_header_copy_client_fields(auth, client->auth);

	client->request.header.authorization = auth;
	return 0;

error:
	rtsp_authorization_header_free(&auth);
	return ret;
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
		 * a keep-alive, retry but after several failed attempts,
		 * remove the session */
		ULOGI("trying to send a keep-alive while not connected");
		session->failed_keep_alive++;
		if (session->failed_keep_alive >=
		    RTSP_CLIENT_MAX_FAILED_KEEP_ALIVE) {
			ULOGW("%d failed keep alive attempts, removing session",
			      session->failed_keep_alive);
			res = -EPIPE;
			rtsp_client_remove_session_internal(
				client, session->id, res, 0);
			return res;
		} else {
			reset_keep_alive_timer(
				session,
				session->timeout_ms /
					RTSP_CLIENT_MAX_FAILED_KEEP_ALIVE);
			return -EAGAIN;
		}
	}

	/* Keep alive already in progress (waiting for reply) */
	if (session->keep_alive_in_progress)
		return -EBUSY;

	/* Another request is pending, retry later */
	if (client->request.is_pending) {
		reset_keep_alive_timer(session, session->timeout_ms / 2);
		return -EBUSY;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.header.method = RTSP_METHOD_TYPE_GET_PARAMETER;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session->id);

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	session->keep_alive_in_progress = 1;
	client->request.is_pending = 1;
	client->cseq++;

	return 0;
}


void rtsp_client_pomp_timer_cb(struct pomp_timer *timer, void *userdata)
{
	UNUSED(timer);

	int res;
	struct rtsp_client_session *session = userdata;

	ULOG_ERRNO_RETURN_IF(session == NULL, EINVAL);

	res = send_keep_alive(
		session->client, session, RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
	if (res < 0)
		ULOG_ERRNO("send_keep_alive", -res);
}


static int send_teardown(struct rtsp_client *client,
			 const char *resource_url,
			 const char *session_id,
			 const struct rtsp_header_ext *ext,
			 size_t ext_count,
			 void *req_userdata,
			 unsigned int timeout_ms,
			 int internal)
{
	int res = 0;
	int keep_alive_in_progress = 0;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.teardown_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		client->methods_allowed != 0 &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_TEARDOWN),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;
	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	/* If there is a pending request that is not a keep alive, remain in
	 * busy state */
	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_TEARDOWN;

	if (resource_url != NULL) {
		res = format_request_uri(client,
					 session->content_base,
					 resource_url,
					 &client->request.header.uri);
		if (res < 0)
			return res;
	} else {
		client->request.header.uri = xstrdup(session->content_base);
	}

	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	session->internal_teardown = internal;
	client->request.is_internal = internal;
	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


static inline bool channel_pair_cmp(const struct rtsp_channel_pair *a,
				    const struct rtsp_channel_pair *b)
{
	if (!a && !b)
		return true;
	if (!a || !b)
		return false;
	return (a->rtp == b->rtp && a->rtcp == b->rtcp);
}


static inline size_t count_channel_unused(const bool *channel_used,
					  size_t count)
{
	size_t unused = 0;
	for (size_t i = 0; i < count; i++) {
		if (!channel_used[i])
			unused++;
	}
	return unused;
}


static inline int check_channel_unused(bool *channel_used, uint8_t channel)
{
	if (channel_used[channel]) {
		int res = -EEXIST;
		ULOG_ERRNO("cannot setup stream: channel (%u) already used",
			   -res,
			   channel);
		return res;
	}
	return 0;
}


static inline void
update_interleaved_channels(struct rtsp_client *client,
			    const struct rtsp_channel_pair *requested,
			    const struct rtsp_channel_pair *returned)
{
	if (!channel_pair_cmp(requested, returned)) {
		ULOGW("SETUP: server returned different interleaved channels "
		      "(requested %u/%u, got %u/%u)",
		      requested->rtp,
		      requested->rtcp,
		      returned->rtp,
		      returned->rtcp);
		set_channel_pair_used(client->channel_used, requested, false);
	}
	set_channel_pair_used(client->channel_used, returned, true);
}


static void setup_request_complete(struct rtsp_client *client,
				   struct rtsp_client_session *session,
				   const char *session_id,
				   const char *req_uri,
				   const struct rtsp_channel_pair *req_channels,
				   enum rtsp_client_req_status status,
				   const struct rtsp_response_header *resp_h,
				   void *req_userdata)
{
	int err;
	char *uri = xstrdup(req_uri);
	char *path = NULL;
	if (status == RTSP_CLIENT_REQ_STATUS_OK) {
		bool is_tcp = (resp_h->transport->lower_transport ==
			       RTSP_LOWER_TRANSPORT_TCP);
		const struct rtsp_client_session_media *media = NULL;
		uint16_t stream_port =
			is_tcp ? resp_h->transport->interleaved[0].rtp
			       : resp_h->transport->src_stream_port;
		uint16_t control_port =
			is_tcp ? resp_h->transport->interleaved[0].rtcp
			       : resp_h->transport->src_control_port;
		struct rtsp_channel_pair ret_channels = {
			.rtp = (uint8_t)stream_port,
			.rtcp = (uint8_t)control_port,
		};
		if (is_tcp) {
			update_interleaved_channels(
				client, req_channels, &ret_channels);
		}
		(*client->cbs.setup_resp)(
			client,
			session_id,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			stream_port,
			control_port,
			resp_h->transport->ssrc_valid,
			resp_h->transport->ssrc,
			resp_h->ext,
			resp_h->ext_count,
			client->cbs_userdata,
			req_userdata);
		err = rtsp_url_parse_path(uri, &path);
		if (err < 0) {
			ULOG_ERRNO("rtsp_url_parse_path(%s)", -err, req_uri);
			goto out;
		}
		media = rtsp_client_session_media_add(
			client, session, path, is_tcp ? &ret_channels : NULL);
		if (media == NULL) {
			err = -EPROTO;
			ULOG_ERRNO("rtsp_client_session_media_add", -err);
			goto out;
		}
	} else {
		if (is_channel_pair_valid(req_channels)) {
			set_channel_pair_used(
				client->channel_used, req_channels, false);
		}
		(*client->cbs.setup_resp)(
			client,
			session_id,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			0,
			0,
			0,
			0,
			NULL,
			0,
			client->cbs_userdata,
			req_userdata);
	}

out:
	free(uri);
}


static void play_request_complete(struct rtsp_client *client,
				  const char *session_id,
				  enum rtsp_client_req_status status,
				  int status_code,
				  const struct rtsp_response_header *resp_h,
				  void *req_userdata)
{
	static struct rtsp_rtp_info_header rtp_info_default = {
		.url = NULL,
		.seq_valid = 0,
		.seq = 0,
		.rtptime_valid = 0,
		.rtptime = 0,
	};
	const struct rtsp_rtp_info_header *rtp_info;

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
					 NULL,
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
				 resp_h->ext,
				 resp_h->ext_count,
				 client->cbs_userdata,
				 req_userdata);
}


static void teardown_request_complete(struct rtsp_client *client,
				      struct rtsp_client_session *session,
				      const char *session_id,
				      const char *req_uri,
				      enum rtsp_client_req_status status,
				      const struct rtsp_response_header *resp_h,
				      void *req_userdata,
				      int *session_removed)
{
	int err;
	char *uri = NULL;
	char *base_uri = NULL;
	char *path = NULL;
	char *base_path = NULL;
	struct rtsp_client_session_media *media = NULL;

	if (session == NULL)
		goto error;

	uri = xstrdup(req_uri);
	base_uri = xstrdup(session->content_base);
	err = rtsp_url_parse_path(uri, &path);
	if (err < 0) {
		ULOG_ERRNO("rtsp_url_parse_path(%s)", -err, req_uri);
		goto out;
	}
	err = rtsp_url_parse_path(base_uri, &base_path);
	if (err < 0) {
		ULOG_ERRNO(
			"rtsp_url_parse_path(%s)", -err, session->content_base);
		goto out;
	}
	media = rtsp_client_session_media_find(client, session, path);
	if ((media == NULL) && (strcmp(path, base_path) != 0)) {
		ULOGE("%s: media '%s' not found", __func__, path);
		goto out;
	}

	if (!session->internal_teardown) {
		(*client->cbs.teardown_resp)(
			client,
			session->id,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			resp_h->ext,
			resp_h->ext_count,
			client->cbs_userdata,
			req_userdata);
		session->internal_teardown = 0;
		client->request.is_internal = 0;
	}

	if (media != NULL) {
		if (is_channel_pair_valid(&media->channel_pair)) {
			set_channel_pair_used(client->channel_used,
					      &media->channel_pair,
					      false);
		}
		err = rtsp_client_session_media_remove(client, session, media);
		if (err < 0)
			ULOG_ERRNO("rtsp_client_session_media_remove", -err);
		*session_removed = (session->media_count == 0);
	} else {
		*session_removed = true;
	}

	goto out;

error:
	if (!client->request.is_internal) {
		(*client->cbs.teardown_resp)(
			client,
			session_id,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			resp_h->ext,
			resp_h->ext_count,
			client->cbs_userdata,
			req_userdata);
		client->request.is_internal = 0;
	}

out:
	free(uri);
	free(base_uri);
}


static int
update_auth_from_server(struct rtsp_client *client,
			const struct rtsp_authorization_header *server)
{
	if (!server)
		return 0;

	rtsp_authorization_header_free(&client->server_auth);
	client->server_auth = rtsp_authorization_header_new();
	if (!client->server_auth)
		return -ENOMEM;

	rtsp_authorization_header_copy(server, client->server_auth);

	if (!client->auth) {
		client->auth = rtsp_authorization_header_new();
		if (!client->auth)
			return -ENOMEM;
		rtsp_authorization_header_copy(client->server_auth,
					       client->auth);
		dup_field(&client->auth->username,
			  rtsp_url_get_user(client->remote.url));
		return 0;
	}

	rtsp_authorization_header_copy_server_fields(client->server_auth,
						     client->auth);
	return 0;
}


static int request_complete(struct rtsp_client *client,
			    const struct rtsp_response_header *resp_h,
			    const char *body,
			    size_t body_len,
			    enum rtsp_client_req_status status)
{
	int res = 0;
	int err;
	int session_removed = 0;
	int internal_teardown = 0;
	enum rtsp_method_type method;
	char *req_uri;
	char *req_session_id;
	const char *content_base;
	void *req_userdata;
	struct rtsp_channel_pair req_pair = {};
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
		if (status == RTSP_CLIENT_REQ_STATUS_TIMEOUT)
			dummy.status_code = RTSP_STATUS_CODE_REQUEST_TIMEOUT;
		resp_h = &dummy;
	}

	/* Save current request info */
	method = client->request.header.method;
	if (method == RTSP_METHOD_TYPE_SETUP &&
	    client->request.header.transport_count > 0 &&
	    client->request.header.transport[0]->lower_transport ==
		    RTSP_LOWER_TRANSPORT_TCP &&
	    client->request.header.transport[0]->interleaved_count > 0) {
		req_pair.rtp =
			client->request.header.transport[0]->interleaved[0].rtp;
		req_pair.rtcp = client->request.header.transport[0]
					->interleaved[0]
					.rtcp;
	}
	err = update_auth_from_server(client, resp_h->authenticate);
	if (err < 0)
		ULOG_ERRNO("update_auth_from_server", -err);

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

	if ((resp_h->status_code == RTSP_STATUS_CODE_UNAUTHORIZED) &&
	    (resp_h->authenticate == NULL) && (client->server_auth != NULL)) {
		ULOGW("%u %s received without WWW-Authenticate; "
		      "server nonce was probably invalidated. "
		      "Clearing cached auth data.",
		      RTSP_STATUS_CODE_UNAUTHORIZED,
		      RTSP_STATUS_STRING_UNAUTHORIZED);

		/* Server sent 401 without WWW-Authenticate, nonce was
		 * probably invalidate. Clear cache. */
		rtsp_authorization_header_free(&client->server_auth);
		rtsp_authorization_header_free(&client->auth);
	}

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
			switch (method) {
			case RTSP_METHOD_TYPE_SETUP:
				ULOGE("%s: cannot create session", __func__);
				res = -ENOMEM;
				break;
			case RTSP_METHOD_TYPE_OPTIONS:
			case RTSP_METHOD_TYPE_ANNOUNCE:
			case RTSP_METHOD_TYPE_DESCRIBE:
				/* Some servers (Wowza at least) reply to
				 * ANNOUNCE with a session id. Filter it here */
				ULOGI("%s: ignoring unexpected session '%s'",
				      __func__,
				      session_id);
				break;
			default:
				ULOGE("%s: session '%s' not found",
				      __func__,
				      session_id);
				res = -ENOENT;
				break;
			}
			goto no_session;
		}
		session->keep_alive_in_progress = 0;

		/* Ensure our keep alive probes are received before the server
		   times out by sending a probe at 80% of server's timeout,
		   it will cope with a half-RTT jitter at most 20% of the
		   server's timeout.
		   To deal with more jitter, latency spike, eg. for one
		   GET_PARAMS to be delayed on server side, double the frequency
		   at which the client side emits them, so that it will cope
		   with a half-RTT jitter equal to 60% of the server's timeout
		 */
		session->timeout_ms =
			(resp_h->session_timeout > 0)
				? (800 * resp_h->session_timeout)
				: RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS;
		session->timeout_ms /= 2;

		err = reset_keep_alive_timer(session, session->timeout_ms);
		if (err < 0)
			ULOG_ERRNO("reset_keep_alive_timer", -err);

		/* Save content base to session if given */
		if (client->request.content_base && !session->content_base)
			session->content_base =
				xstrdup(client->request.content_base);
		xfree((void **)&client->request.content_base);
	}

no_session:
	switch (method) {
	case RTSP_METHOD_TYPE_OPTIONS:
		client->methods_allowed = resp_h->public_methods;
		(*client->cbs.options_resp)(
			client,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			client->methods_allowed,
			resp_h->ext,
			resp_h->ext_count,
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
			resp_h->ext,
			resp_h->ext_count,
			body_with_null,
			client->cbs_userdata,
			req_userdata);
		free(body_with_null);
		break;
	case RTSP_METHOD_TYPE_ANNOUNCE:
		(*client->cbs.announce_resp)(
			client,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			resp_h->ext,
			resp_h->ext_count,
			client->cbs_userdata,
			req_userdata);
		break;
	case RTSP_METHOD_TYPE_SETUP:
		setup_request_complete(client,
				       session,
				       session_id,
				       req_uri,
				       &req_pair,
				       status,
				       resp_h,
				       req_userdata);
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
				resp_h->ext,
				resp_h->ext_count,
				client->cbs_userdata,
				req_userdata);
		} else {
			(*client->cbs.pause_resp)(
				client,
				session_id,
				status,
				rtsp_status_to_errno(resp_h->status_code),
				NULL,
				resp_h->ext,
				resp_h->ext_count,
				client->cbs_userdata,
				req_userdata);
		}
		break;
	case RTSP_METHOD_TYPE_RECORD:
		(*client->cbs.record_resp)(
			client,
			session_id,
			status,
			rtsp_status_to_errno(resp_h->status_code),
			resp_h->ext,
			resp_h->ext_count,
			client->cbs_userdata,
			req_userdata);
		break;
	case RTSP_METHOD_TYPE_TEARDOWN:
		teardown_request_complete(client,
					  session,
					  session_id,
					  req_uri,
					  status,
					  resp_h,
					  req_userdata,
					  &session_removed);
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
				/* Disarm keep alive timer */
				reset_keep_alive_timer(session, -1);
			} else {
				err = send_keep_alive(
					session->client,
					session,
					RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
				if (err < 0)
					ULOG_ERRNO("send_keep_alive", -err);
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
				    NULL,
				    session_id,
				    NULL,
				    0,
				    NULL,
				    RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS,
				    1);
		if (err < 0) {
			ULOG_ERRNO("send_teardown", -err);
			session_removed = 1;
		}
	}
	if (session_removed) {
		if (status == RTSP_CLIENT_REQ_STATUS_TIMEOUT) {
			err = rtsp_client_remove_session_internal(
				client, session_id, -ETIMEDOUT, 0);
		} else {
			err = rtsp_client_remove_session_internal(
				client, session_id, resp_h->status_code, 0);
		}
		if (err < 0)
			ULOG_ERRNO("rtsp_client_remove_session_internal", -err);
	}
	if (status == RTSP_CLIENT_REQ_STATUS_TIMEOUT) {
		client->failed_requests++;
		if (client->failed_requests >=
		    RTSP_CLIENT_MAX_FAILED_REQUESTS) {
			ULOGW("%d failed requests (timeout), "
			      "reconnecting to %s...",
			      client->failed_requests,
			      client->remote.url_str);
			err = tskt_client_stop(client->tclient);
			if (err < 0)
				ULOG_ERRNO("tskt_client_stop", -err);
			set_connection_state(client,
					     RTSP_CLIENT_CONN_STATE_CONNECTING);
			err = tskt_client_connect(
				client->tclient,
				NULL,
				0,
				rtsp_url_get_resolved_host(client->remote.url),
				rtsp_url_get_port(client->remote.url));
			if (err < 0)
				ULOG_ERRNO("tskt_client_connect", -err);
		}
	} else {
		client->failed_requests = 0;
	}

exit:
	free(req_session_id);
	free(req_uri);
	return res;
}


static void tskt_client_event_cb(struct tskt_client *self,
				 enum tskt_client_event event,
				 struct tskt_socket *sock,
				 void *userdata)
{
	UNUSED(self);

	int res;
	struct rtsp_client *client = userdata;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	switch (event) {
	case TSKT_CLIENT_EVENT_CONNECTED:
		client->sock = sock;
		ULOGI("client connected");
		set_connection_state(client, RTSP_CLIENT_CONN_STATE_CONNECTED);

		/* If a session already exists, send a keep-alive
		 * right away; this allows quickly seeing if a
		 * session timeout has occurred on the server side
		 * during the time disconnected */
		list_walk_entry_forward(&client->sessions, session, node)
		{
			res = send_keep_alive(
				session->client,
				session,
				RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS);
			if (res < 0)
				ULOG_ERRNO("send_keep_alive", -res);
		}
		break;

	case TSKT_CLIENT_EVENT_DISCONNECTED:
		client->sock = NULL;
		if (client->conn_state ==
		    RTSP_CLIENT_CONN_STATE_DISCONNECTING) {
			/* Disconnetion initiated by the user */
			ULOGI("client disconnected");

			clear_remote_info(client);
			res = request_complete(client,
					       NULL,
					       NULL,
					       0,
					       RTSP_CLIENT_REQ_STATUS_ABORTED);
			if (res < 0)
				ULOG_ERRNO("request_complete", -res);

			set_connection_state(
				client, RTSP_CLIENT_CONN_STATE_DISCONNECTED);
		} else if (client->conn_state ==
			   RTSP_CLIENT_CONN_STATE_CONNECTED) {
			/* Disconnetion by the network, auto reconnect*/
			ULOGI("client disconnected, waiting for reconnection");

			res = request_complete(client,
					       NULL,
					       NULL,
					       0,
					       RTSP_CLIENT_REQ_STATUS_ABORTED);
			if (res < 0)
				ULOG_ERRNO("request_complete", -res);

			set_connection_state(client,
					     RTSP_CLIENT_CONN_STATE_CONNECTING);
		}

		break;

	case TSKT_CLIENT_EVENT_READY_TO_SEND:
		/* Notify client */
		if (client->cbs.ready_to_send_cb) {
			(*client->cbs.ready_to_send_cb)(client,
							client->cbs_userdata);
		}
		break;

	default:
		break;
	}
}


static int teardown_request_process(struct rtsp_client *client,
				    struct rtsp_message *msg,
				    int *status_code,
				    const char **status_string)
{
	int ret;
	int err;
	int session_removed = 0;
	char *uri = xstrdup(msg->header.req.uri);
	char *path = NULL;
	struct rtsp_client_session *session = NULL;
	struct rtsp_client_session_media *media = NULL;

	*status_code = RTSP_STATUS_CODE_OK;
	*status_string = RTSP_STATUS_STRING_OK;

	session =
		rtsp_client_get_session(client, msg->header.req.session_id, 0);
	if (session == NULL) {
		ret = -ENOENT;
		ULOGW("%s: session not found", __func__);
		*status_code = RTSP_STATUS_CODE_SESSION_NOT_FOUND;
		goto out;
	}

	ret = rtsp_url_parse_path(uri, &path);
	if (ret < 0) {
		ULOG_ERRNO(
			"rtsp_url_parse_path(%s)", -ret, msg->header.req.uri);
		goto out;
	} else if (path == NULL) {
		ret = -EINVAL;
		ULOGE("invalid RTSP URL: '%s', path is missing",
		      msg->header.req.uri);
		goto out;
	} else if (strcmp(msg->header.req.uri, session->content_base) != 0) {
		media = rtsp_client_session_media_find(client, session, path);
		if (media == NULL)
			ULOGW("%s: media not found", __func__);
	}

	(*client->cbs.teardown)(client,
				media ? media->path : path,
				msg->header.req.session_id,
				msg->header.req.ext,
				msg->header.req.ext_count,
				client->cbs_userdata);

	if (media != NULL) {
		if (is_channel_pair_valid(&media->channel_pair)) {
			set_channel_pair_used(client->channel_used,
					      &media->channel_pair,
					      false);
		}
		err = rtsp_client_session_media_remove(client, session, media);
		if (err < 0)
			ULOG_ERRNO("rtsp_client_session_media_remove", -err);
		session_removed = (session->media_count == 0);
	}
	if (media == NULL || session_removed) {
		ret = rtsp_client_remove_session_internal(
			client,
			msg->header.req.session_id,
			RTSP_STATUS_CODE_OK,
			0);
		if (ret < 0)
			ULOG_ERRNO("rtsp_client_remove_session_internal", -ret);
	}

	ret = 0;

out:
	free(uri);
	return ret;
}


static int rtsp_client_interleaved_process(struct rtsp_client *client,
					   struct rtsp_message *msg)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg->type != RTSP_MESSAGE_TYPE_INTERLEAVED,
				 EINVAL);

	if (client->cbs.interleaved_data_cb) {
		(*client->cbs.interleaved_data_cb)(client,
						   msg->interleaved.channel,
						   msg->interleaved.data,
						   msg->interleaved.len,
						   client->cbs_userdata);
	}
	return 0;
}


static int rtsp_client_request_process(struct rtsp_client *client,
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
	struct pomp_buffer *resp_buf = NULL;
	struct rtsp_string resp_str;
	struct timespec cur_ts = {0, 0};

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
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
					msg->header.req.ext,
					msg->header.req.ext_count,
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
	case RTSP_METHOD_TYPE_TEARDOWN:
		ret = teardown_request_process(
			client, msg, &status_code, &status_string);
		if (ret < 0)
			goto out;
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
	time_get_monotonic(&cur_ts);
	resp.header.resp.date = cur_ts.tv_sec;

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

	/* Send the response */
	res = send_raw_buf(client, resp_buf);
	if (res < 0) {
		if (res != -EAGAIN)
			ULOG_ERRNO("send_raw_buf", -res);
		goto out;
	}

out:
	rtsp_response_header_clear(&resp.header.resp);
	if (resp_buf != NULL)
		pomp_buffer_unref(resp_buf);
	if (ret == 0 && not_implem)
		ret = -ENOSYS;
	return ret;
}


static int rtsp_client_response_process(struct rtsp_client *client,
					const struct rtsp_message *msg)
{
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(msg == NULL, EINVAL);

	/* Note: VLC server doesn't repeat the cseq in error case */
	if (msg->header.resp.cseq != client->request.header.cseq) {

		list_walk_entry_forward(&client->sessions, session, node)
		{
			if (!session->keep_alive_in_progress)
				continue;

			/* We suppose that this is in fact the response to a
			 * pending keep alive; drop the response */
			session->keep_alive_in_progress = 0;
			ULOGW("%s: dropping RTSP response cseq=%d session=%s"
			      " (probably %s)",
			      __func__,
			      msg->header.resp.cseq,
			      session->id,
			      rtsp_method_type_str(
				      RTSP_METHOD_TYPE_GET_PARAMETER));

			/* Try to send a keep alive later as this one has been
			 * dropped. Moreover the following response may not
			 * be related to this session. */
			reset_keep_alive_timer(session,
					       session->timeout_ms / 2);
			return 0;
		}

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


static void tskt_client_data_cb(struct tskt_client *self,
				struct tpkt_packet *pkt,
				void *userdata)
{
	int res;
	int err;
	size_t len = 0;
	const void *cdata = NULL;
	struct rtsp_message msg;
	struct pomp_buffer *buf = NULL;
	memset(&msg, 0x0, sizeof(msg));
	struct rtsp_client *client = userdata;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_IF(pkt == NULL, EINVAL);

	buf = tpkt_get_buffer(pkt);
	if (buf == NULL) {
		ULOGE("tpkt_get_buffer");
		return;
	}

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
		if (msg.type == RTSP_MESSAGE_TYPE_INTERLEAVED) {
			err = rtsp_client_interleaved_process(client, &msg);
			if (err < 0)
				ULOG_ERRNO("rtsp_client_interleaved_process",
					   -err);
		} else if (msg.type == RTSP_MESSAGE_TYPE_REQUEST) {
			err = rtsp_client_request_process(client, &msg);
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
	UNUSED(timer);

	struct rtsp_client *client = userdata;
	int ret = 0;

	ULOG_ERRNO_RETURN_IF(client == NULL, EINVAL);

	ret = request_complete(
		client, NULL, NULL, 0, RTSP_CLIENT_REQ_STATUS_TIMEOUT);
	if (ret < 0)
		ULOG_ERRNO("request_complete", -ret);
}


static void resolv_timer_cb(struct pomp_timer *timer, void *userdata)
{
	UNUSED(timer);

	struct rtsp_client *client = userdata;
	int err = 0;

	ULOGE("failed to resolve hostname '%s'"
	      " (request timed out after %.2fs)",
	      rtsp_url_get_host(client->remote.url),
	      (float)RTSP_CLIENT_RESOLV_TIMEOUT_MS / 1000.);

	err = tskt_resolv_cancel(client->resolv.resolv, client->resolv.req_id);
	if (err < 0)
		ULOG_ERRNO("tskt_resolv_cancel", -err);

	(void)rtsp_client_disconnect(client);
}


static const struct tskt_client_cbs tclient_cbs = {
	.event_cb = tskt_client_event_cb,
	.data_cb = tskt_client_data_cb,
	.socket_created_cb = tskt_client_socket_created_cb,
	.socket_upgrade_cb = tskt_client_socket_upgrade_cb,
};


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
	client->sock_params.class_selector = UINT32_MAX;

	list_init(&client->sessions);

	/* Create a timer for response timeout */
	client->request.timer =
		pomp_timer_new(loop, &rtsp_client_resp_timeout_cb, client);
	if (client->request.timer == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_timer_new", -res);
		goto error;
	}

	client->resolv.timer =
		pomp_timer_new(client->loop, &resolv_timer_cb, client);
	if (client->resolv.timer == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_timer_new", -res);
		goto error;
	}

	client->software_name =
		software_name ? strdup(software_name)
			      : strdup(RTSP_CLIENT_DEFAULT_SOFTWARE_NAME);
	if (client->software_name == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("strdup", -res);
		goto error;
	}

	res = tskt_client_new(
		client->loop, tclient_cbs, client, &client->tclient);
	if (res < 0) {
		ULOG_ERRNO("tskt_client_new", -res);
		goto error;
	}

	res = tskt_resolv_new(&client->resolv.resolv);
	if (res < 0) {
		ULOG_ERRNO("tskt_resolv_new", -res);
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

	if (client->request.timer != NULL) {
		err = pomp_timer_clear(client->request.timer);
		if (err < 0)
			ULOG_ERRNO("pomp_timer_clear", -err);
	}

	/* Before removing any session, the pomp context must be stopped
	 * to trigger a POMP_EVENT_DISCONNECTED event and complete any
	 * pending request with a RTSP_CLIENT_REQ_STATUS_ABORTED status */
	if (client->tclient != NULL) {
		err = tskt_client_stop(client->tclient);
		if (err < 0)
			ULOG_ERRNO("tskt_client_stop", -err);
	}

	rtsp_client_remove_all_sessions(client);

	if (client->request.timer != NULL) {
		err = pomp_timer_destroy(client->request.timer);
		if (err < 0)
			ULOG_ERRNO("pomp_timer_destroy", -err);
	}

	if (client->tclient != NULL) {
		err = tskt_client_destroy(client->tclient);
		if (err < 0)
			ULOG_ERRNO("tskt_client_destroy", -err);
	}

	if (client->ssl_ctx != NULL) {
		SSL_CTX_free(client->ssl_ctx);
		client->ssl_ctx = NULL;
	}

	if (client->ttls_init) {
		int err = ttls_deinit();
		if (err < 0)
			ULOG_ERRNO("ttls_deinit", -err);
	}

	if (client->resolv.resolv != NULL)
		tskt_resolv_unref(client->resolv.resolv);
	if (client->request.buf != NULL)
		pomp_buffer_unref(client->request.buf);
	if (client->response.buf != NULL)
		pomp_buffer_unref(client->response.buf);

	if (client->resolv.timer != NULL) {
		err = pomp_timer_clear(client->resolv.timer);
		if (err < 0)
			ULOG_ERRNO("pomp_timer_clear", -err);
		err = pomp_timer_destroy(client->resolv.timer);
		if (err < 0)
			ULOG_ERRNO("pomp_timer_destroy", -err);
	}

	free(client->request.uri);
	free(client->request.content_base);
	rtsp_request_header_clear(&client->request.header);
	rtsp_message_clear(&client->parser_ctx.msg);

	clear_remote_info(client);
	free(client->software_name);
	free(client);

	return 0;
}


static void tskt_resolv_cb(struct tskt_resolv *self,
			   int id,
			   enum tskt_resolv_error result,
			   int naddrs,
			   const char *const *addrs,
			   void *userdata)
{
	UNUSED(self);
	UNUSED(id);
	UNUSED(naddrs);

	int res = 0;
	int err;
	const char *host = NULL;
	uint16_t port = 0;
	struct rtsp_client *client = (struct rtsp_client *)userdata;

	host = rtsp_url_get_host(client->remote.url);
	port = rtsp_url_get_port(client->remote.url);

	err = pomp_timer_clear(client->resolv.timer);
	if (err < 0)
		ULOG_ERRNO("pomp_timer_clear", -err);

	if (result != TSKT_RESOLV_ERROR_OK) {
		ULOGE("failed to resolve hostname '%s'"
		      " (error code: %d)",
		      host,
		      result);
		goto error;
	}

	ULOGI("successfully resolved hostname '%s' to %s", host, addrs[0]);

	res = rtsp_url_set_resolved_host(client->remote.url, addrs[0]);
	if (res < 0) {
		ULOG_ERRNO("rtsp_url_set_resolved_host", -res);
		goto error;
	}

	res = tskt_client_connect(
		client->tclient,
		NULL,
		0,
		rtsp_url_get_resolved_host(client->remote.url),
		port);
	if (res < 0) {
		ULOG_ERRNO("tskt_client_connect", -res);
		goto error;
	}

	return;

error:
	set_connection_state(client, RTSP_CLIENT_CONN_STATE_DISCONNECTED);
}


int rtsp_client_connect(struct rtsp_client *client, const char *addr)
{
	int res = 0;
	const char *host = NULL;
	const char *user = NULL;
	uint16_t port = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(addr == NULL, EINVAL);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_DISCONNECTED)
		return -EBUSY;

	clear_remote_info(client);

	/* Parse the URL */
	res = rtsp_url_parse(addr, &client->remote.url);
	if (res < 0) {
		ULOG_ERRNO("rtsp_url_parse(%s)", -res, addr);
		goto error;
	}

	host = rtsp_url_get_host(client->remote.url);
	port = rtsp_url_get_port(client->remote.url);
	user = rtsp_url_get_user(client->remote.url);

	rtsp_url_strip_credentials(addr, &client->remote.url_str);

	ULOGI("connecting to address %s port %d", host, port);
	set_connection_state(client, RTSP_CLIENT_CONN_STATE_CONNECTING);

	if (rtsp_url_get_scheme(client->remote.url) ==
	    RTSP_URL_SCHEME_TCP_TLS) {
		if (!client->ttls_init) {
			OPENSSL_init_ssl(0, NULL);
			res = ttls_init();
			if (res < 0) {
				ULOG_ERRNO("ttls_init", -res);
				goto error;
			}
			client->ttls_init = true;
		}
		if (client->ssl_ctx == NULL) {
			/* create TLS client context */
			client->ssl_ctx = SSL_CTX_new(TLS_client_method());
			if (client->ssl_ctx == NULL) {
				res = -EPROTO;
				ULOG_ERRNO("SSL_CTX_new", -res);
				goto error;
			}
		}
	}

	res = pomp_timer_set(client->resolv.timer,
			     RTSP_CLIENT_RESOLV_TIMEOUT_MS);
	if (res < 0) {
		ULOG_ERRNO("pomp_timer_set", -res);
		goto error;
	}

	res = tskt_resolv_getaddrinfo(client->resolv.resolv,
				      host,
				      client->loop,
				      &tskt_resolv_cb,
				      client,
				      &client->resolv.req_id);
	if (res < 0) {
		ULOG_ERRNO("tskt_resolv_getaddrinfo", -res);
		goto error;
	}

	return 0;

error:
	set_connection_state(client, RTSP_CLIENT_CONN_STATE_DISCONNECTED);
	clear_remote_info(client);
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

	set_connection_state(client, RTSP_CLIENT_CONN_STATE_DISCONNECTING);

	/* Before removing any session, the pomp context must be stopped
	 * to trigger a POMP_EVENT_DISCONNECTED event and complete any
	 * pending request with a RTSP_CLIENT_REQ_STATUS_ABORTED status */
	res = tskt_client_stop(client->tclient);
	if (res < 0) {
		ULOG_ERRNO("tskt_client_stop", -res);
		return res;
	}

	if (already_disconnected) {
		ULOGI("client disconnected (already disconnected)");
		clear_remote_info(client);
		request_complete(
			client, NULL, NULL, 0, RTSP_CLIENT_REQ_STATUS_ABORTED);
		set_connection_state(client,
				     RTSP_CLIENT_CONN_STATE_DISCONNECTED);
	}

	rtsp_client_remove_all_sessions(client);

	return 0;
}


const struct rtsp_url *
rtsp_client_get_remote_url(const struct rtsp_client *client)
{
	ULOG_ERRNO_RETURN_VAL_IF(client == NULL, EINVAL, NULL);

	return client->remote.url;
}


int rtsp_client_options(struct rtsp_client *client,
			const struct rtsp_header_ext *ext,
			size_t ext_count,
			void *req_userdata,
			unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.options_resp == NULL, ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_OPTIONS;
	client->request.header.uri = xstrdup("*");
	client->request.header.cseq = client->cseq;
	/* client->request.header.authorization not needed for OPTIONS */
	client->request.header.user_agent = xstrdup(client->software_name);
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_describe(struct rtsp_client *client,
			 const char *path,
			 const struct rtsp_header_ext *ext,
			 size_t ext_count,
			 void *req_userdata,
			 unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.describe_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_DESCRIBE),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_DESCRIBE;
	client->request.header.uri = make_uri(client, path);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0 && res != -EAGAIN) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.accept = xstrdup(RTSP_CONTENT_TYPE_SDP);
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_announce(struct rtsp_client *client,
			 const char *path,
			 const char *session_description,
			 const struct rtsp_header_ext *ext,
			 size_t ext_count,
			 void *req_userdata,
			 unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_description == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_description[0] == '\0', EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.announce_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_ANNOUNCE),
		ENOSYS);
	size_t session_description_len =
		strnlen(session_description, RTSP_SESSION_DESCRIPTION_MAX_LEN);
	ULOG_ERRNO_RETURN_ERR_IF(session_description_len >=
					 RTSP_SESSION_DESCRIPTION_MAX_LEN,
				 EINVAL);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_ANNOUNCE;
	client->request.header.uri = make_uri(client, path);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0 && res != -EAGAIN) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.content_type = xstrdup(RTSP_CONTENT_TYPE_SDP);
	client->request.header.content_length = session_description_len;
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, session_description, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


static int allocate_interleaved_channels(struct rtsp_client *client,
					 struct rtsp_channel_pair *pair)
{
	int res;
	const size_t size = SIZEOF_ARRAY(client->channel_used);
	const struct rtsp_channel_pair req_pair = {
		.rtp = pair->rtp,
		.rtcp = pair->rtcp,
	};

	if (is_channel_pair_valid(&req_pair)) {
		res = check_channel_unused(client->channel_used, req_pair.rtp);
		if (res < 0)
			return res;
		res = check_channel_unused(client->channel_used, req_pair.rtcp);
		if (res < 0)
			return res;
	} else {
		/* Use next available channels */
		for (uint8_t i = 0; i < size - 1; i += 2) {
			if (!client->channel_used[i] &&
			    !client->channel_used[i + 1]) {
				pair->rtp = i;
				pair->rtcp = i + 1;
				break;
			}
		}
	}

	set_channel_pair_used(client->channel_used, pair, true);
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
		      enum rtsp_transport_method method,
		      const struct rtsp_header_ext *ext,
		      size_t ext_count,
		      void *req_userdata,
		      unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;
	char *_content_base = NULL;
	struct rtsp_transport_header *th;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(content_base == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(resource_url == NULL, EINVAL);
	if (lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
		ULOG_ERRNO_RETURN_ERR_IF(
			(client_stream_port != 0) &&
				(client_control_port != 0) &&
				(client_stream_port == client_control_port),
			EINVAL);
		ULOG_ERRNO_RETURN_ERR_IF(client_stream_port > UINT8_MAX,
					 EINVAL);
		ULOG_ERRNO_RETURN_ERR_IF(client_control_port > UINT8_MAX,
					 EINVAL);
	} else {
		ULOG_ERRNO_RETURN_ERR_IF(client_stream_port == 0, EINVAL);
		ULOG_ERRNO_RETURN_ERR_IF(client_control_port == 0, EINVAL);
	}
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.setup_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_SETUP),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	/* If there is a pending request that is not a keep alive, remain in
	 * busy state */
	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	res = rtsp_url_strip_credentials(content_base, &_content_base);
	if (res < 0) {
		ULOG_ERRNO("rtsp_url_strip_credentials", -res);
		goto out;
	}

	/* If a session id is passed, make sure that we know the session,
	 * and that its content_base is the right one */
	if (session_id != NULL) {
		const struct rtsp_client_session *session =
			rtsp_client_get_session(client, session_id, 0);
		if (!session) {
			ULOGE("%s: session not found", __func__);
			res = -ENOENT;
			goto out;
		}
		if (strcmp(session->content_base, _content_base) != 0) {
			ULOGE("%s: invalid content base", __func__);
			res = -EINVAL;
			goto out;
		}
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_SETUP;

	res = format_request_uri(
		client, _content_base, resource_url, &client->request.uri);
	if (res < 0)
		goto out;

	client->request.content_base = xstrdup(_content_base);

	client->request.header.uri = xstrdup(client->request.uri);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);

	th = rtsp_transport_header_new();
	client->request.header.transport[0] = th;
	client->request.header.transport_count = 1;
	th->transport_protocol = strdup(RTSP_TRANSPORT_PROTOCOL_RTP);
	th->transport_profile = strdup(RTSP_TRANSPORT_PROFILE_AVP);
	th->lower_transport = lower_transport;
	th->delivery = delivery;
	th->method = method;
	if (lower_transport == RTSP_LOWER_TRANSPORT_TCP) {
		struct rtsp_channel_pair pair = {
			.rtp = (uint8_t)client_stream_port,
			.rtcp = (uint8_t)client_control_port,
		};

		if (count_channel_unused(client->channel_used,
					 SIZEOF_ARRAY(client->channel_used)) <
		    2) {
			res = -E2BIG;
			ULOG_ERRNO(
				"cannot setup stream: too many"
				" interleaved channels",
				-res);
			return res;
		}

		res = allocate_interleaved_channels(client, &pair);
		if (res < 0) {
			ULOG_ERRNO("allocate_interleaved_channels", -res);
			return res;
		}

		th->interleaved_count = 1;
		th->interleaved[0] = pair;
	} else {
		th->dst_stream_port = client_stream_port;
		th->dst_control_port = client_control_port;
	}
	client->request.header.session_id = xstrdup(session_id);
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		goto out;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		goto out;

	client->request.is_pending = 1;
	client->cseq++;

	res = 0;

out:
	free(_content_base);
	return res;
}


int rtsp_client_play(struct rtsp_client *client,
		     const char *session_id,
		     const struct rtsp_range *range,
		     float scale,
		     const struct rtsp_header_ext *ext,
		     size_t ext_count,
		     void *req_userdata,
		     unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;
	const struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.play_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_PLAY),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	/* If there is a pending request that is not a keep alive, remain in
	 * busy state */
	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_PLAY;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	client->request.header.range = *range;
	client->request.header.scale = scale;
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_pause(struct rtsp_client *client,
		      const char *session_id,
		      const struct rtsp_range *range,
		      const struct rtsp_header_ext *ext,
		      size_t ext_count,
		      void *req_userdata,
		      unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;
	const struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.pause_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_PAUSE),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;
	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	/* If there is a pending request that is not a keep alive, remain in
	 * busy state */
	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_PAUSE;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	client->request.header.range = *range;
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_record(struct rtsp_client *client,
		       const char *session_id,
		       const struct rtsp_range *range,
		       const struct rtsp_header_ext *ext,
		       size_t ext_count,
		       void *req_userdata,
		       unsigned int timeout_ms)
{
	int res = 0;
	int keep_alive_in_progress = 0;
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session_id == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(range == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(client->cbs.record_resp == NULL, ENOSYS);
	ULOG_ERRNO_RETURN_ERR_IF(
		(client->methods_allowed != 0) &&
			!(client->methods_allowed & RTSP_METHOD_FLAG_RECORD),
		ENOSYS);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	keep_alive_in_progress = clear_pending_keep_alive_timer(client);
	if (keep_alive_in_progress < 0)
		return keep_alive_in_progress;

	/* If there is a pending request that is not a keep alive, remain in
	 * busy state */
	if (client->request.is_pending && !keep_alive_in_progress)
		return -EBUSY;

	/* Make sure that we know the session */
	session = rtsp_client_get_session(client, session_id, 0);
	if (session == NULL) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Set request header */
	rtsp_request_header_clear(&client->request.header);
	client->request.userdata = req_userdata;
	client->request.header.method = RTSP_METHOD_TYPE_RECORD;
	client->request.header.uri = xstrdup(session->content_base);
	client->request.header.cseq = client->cseq;
	res = generate_authorization_header(client);
	if (res < 0) {
		ULOG_ERRNO("generate_authorization_header", -res);
		return res;
	}
	client->request.header.user_agent = xstrdup(client->software_name);
	client->request.header.session_id = xstrdup(session_id);
	client->request.header.range = *range;
	res = rtsp_request_header_copy_ext(
		&client->request.header, ext, ext_count);
	if (res < 0)
		return res;

	/* Send the request */
	res = send_request(client, NULL, timeout_ms);
	if (res < 0)
		return res;

	client->request.is_pending = 1;
	client->cseq++;
	return 0;
}


int rtsp_client_send_interleaved(struct rtsp_client *client,
				 uint8_t channel,
				 const uint8_t *data,
				 size_t len)
{
	int res;
	struct pomp_buffer *pomp_buf = NULL;
	struct rtsp_interleaved_info info = {};

	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(data == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(len == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(len > UINT16_MAX, EINVAL);

	if (client->conn_state != RTSP_CLIENT_CONN_STATE_CONNECTED)
		return -EPIPE;

	if (!client->channel_used[channel])
		return -ENOENT;

	info.channel = (uint8_t)channel;
	info.data = data;
	info.len = (uint16_t)len;

	res = rtsp_build_interleaved(&info, &pomp_buf);
	if (res < 0) {
		ULOG_ERRNO("rtsp_build_interleaved", -res);
		goto out;
	}

	/* Send the request */
	res = send_raw_buf(client, pomp_buf);
	if (res < 0) {
		if (res != -EAGAIN)
			ULOG_ERRNO("send_raw_buf", -res);
		goto out;
	}

out:
	if (pomp_buf != NULL)
		pomp_buffer_unref(pomp_buf);
	return res;
}


int rtsp_client_teardown(struct rtsp_client *client,
			 const char *resource_url,
			 const char *session_id,
			 const struct rtsp_header_ext *ext,
			 size_t ext_count,
			 void *req_userdata,
			 unsigned int timeout_ms)
{
	return send_teardown(client,
			     resource_url,
			     session_id,
			     ext,
			     ext_count,
			     req_userdata,
			     timeout_ms,
			     0);
}

int rtsp_client_remove_session(struct rtsp_client *client,
			       const char *session_id)
{
	return rtsp_client_remove_session_internal(
		client, session_id, RTSP_STATUS_CODE_REQUEST_TIMEOUT, 0);
}


int rtsp_client_cancel(struct rtsp_client *client)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	return request_complete(
		client, NULL, NULL, 0, RTSP_CLIENT_REQ_STATUS_CANCELED);
}


int rtsp_client_set_socket_txbuf_size(struct rtsp_client *client, size_t size)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	client->sock_params.txbuf_size = size;

	if (client->sock == NULL)
		return 0;

	return set_socket_txbuf_size(client, client->sock);
}


int rtsp_client_set_socket_rxbuf_size(struct rtsp_client *client, size_t size)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	client->sock_params.rxbuf_size = size;

	if (client->sock == NULL)
		return 0;

	return set_socket_rxbuf_size(client, client->sock);
}


int rtsp_client_set_socket_class_selector(struct rtsp_client *client,
					  uint32_t class_selector)
{
	ULOG_ERRNO_RETURN_ERR_IF(client == NULL, EINVAL);

	client->sock_params.class_selector = class_selector;

	if (client->sock == NULL)
		return 0;

	return set_socket_class_selector(client, client->sock);
}
