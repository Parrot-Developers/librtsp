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
	const char *user_agent,
	struct pomp_loop *loop)
{
	int ret;
	int mutex_created = 0, cond_created = 0;
	char *p, *temp, *u = NULL;

	RTSP_RETURN_VAL_IF_FAILED(url != NULL, -EINVAL, NULL);
	RTSP_RETURN_VAL_IF_FAILED(strlen(url), -EINVAL, NULL);
	RTSP_RETURN_VAL_IF_FAILED(strncmp(url, RTSP_SCHEME_TCP,
		strlen(RTSP_SCHEME_TCP)) == 0, -EINVAL, NULL);
	RTSP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);

	struct rtsp_client *client = calloc(1, sizeof(*client));
	RTSP_RETURN_VAL_IF_FAILED(client != NULL, -ENOMEM, NULL);

	client->user_agent = (user_agent) ?
		strdup(user_agent) : strdup(RTSP_CLIENT_DEFAULT_USER_AGENT);
	if (!client->user_agent) {
		RTSP_LOGE("string allocation failed, aborting");
		goto error;
	}

	client->url = strdup(url);
	if (!client->url) {
		RTSP_LOGE("string allocation failed, aborting");
		goto error;
	}

	/* parse the URL */
	client->server_port = RTSP_DEFAULT_PORT;
	u = strdup(client->url + 7);
	if (!u) {
		RTSP_LOGE("string allocation failed, aborting");
		goto error;
	}
	p = strtok_r(u, "/", &temp);
	if (p) {
		/* host */
		char *p2 = strchr(p, ':');
		if (p2) {
			/* port */
			client->server_port = atoi(p2 + 1);
			*p2 = '\0';
		}
		client->server_host = strdup(p);
		if (!client->server_host) {
			RTSP_LOGE("string allocation failed, aborting");
			goto error;
		}

		/* absolute path */
		p = strtok_r(NULL, "/", &temp);
		if (p) {
			client->abs_path = strdup(p);
			if (!client->abs_path) {
				RTSP_LOGE("string allocation failed, aborting");
				goto error;
			}
		}
	}
	free(u);

	/* check the URL validity */
	if ((!client->server_host) || (!strlen(client->server_host))) {
		RTSP_LOGE("invalid server host, aborting");
		goto error;
	}
	if (client->server_port == 0) {
		RTSP_LOGE("invalid server port, aborting");
		goto error;
	}
	if ((!client->abs_path) || (!strlen(client->abs_path))) {
		RTSP_LOGE("invalid resource path, aborting");
		goto error;
	}

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

	/* TODO: name resolution */
	ret = inet_pton(AF_INET, client->server_host,
		&client->remote_addr_in.sin_addr);
	if (ret <= 0) {
		RTSP_LOGE("failed to convert address %s, aborting",
			client->server_host);
		goto error;
	}
	client->remote_addr_in.sin_family = AF_INET;
	client->remote_addr_in.sin_port = htons(client->server_port);

	ret = pomp_ctx_connect(client->pomp,
		(const struct sockaddr *)&client->remote_addr_in,
		sizeof(client->remote_addr_in));
	if (ret < 0) {
		RTSP_LOG_ERRNO("failed to connect, aborting", -ret);
		goto error;
	}

	ret = pthread_mutex_init(&client->mutex, NULL);
	if (ret != 0) {
		RTSP_LOGE("mutex creation failed, aborting");
		goto error;
	}
	mutex_created = 1;
	ret = pthread_cond_init(&client->cond, NULL);
	if (ret != 0) {
		RTSP_LOGE("cond creation failed, aborting");
		goto error;
	}
	cond_created = 1;

	return client;

error:
	if (mutex_created)
		pthread_mutex_destroy(&client->mutex);
	if (cond_created)
		pthread_cond_destroy(&client->cond);
	if (client->pomp)
		pomp_ctx_destroy(client->pomp);
	free(client->user_agent);
	free(client->server_host);
	free(client->abs_path);
	free(client->url);
	free(u);
	free(client);
	return NULL;
}


int rtsp_client_destroy(
	struct rtsp_client *client)
{
	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	pthread_mutex_destroy(&client->mutex);
	pthread_cond_destroy(&client->cond);
	pomp_ctx_stop(client->pomp);
	pomp_ctx_destroy(client->pomp);
	free(client->sdp);
	free(client->session_id);
	free(client->user_agent);
	free(client->server_host);
	free(client->abs_path);
	free(client->content_encoding);
	free(client->content_language);
	free(client->content_base);
	free(client->content_location);
	free(client->url);
	free(client);

	return 0;
}


int rtsp_client_options(
	struct rtsp_client *client)
{
	int ret = 0, err;
	char *request;
	struct pomp_buffer *req_buf = NULL;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	/* create request */
	err = asprintf(&request,
		RTSP_METHOD_OPTIONS " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n\n",
		client->url, ++client->cseq, client->user_agent);
	if (err == -1) {
		RTSP_LOGE("failed to allocate request");
		ret = -1;
		goto out;
	}

	/* send request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		ret = -1;
		goto out;
	}
	if (req_buf) {
		pomp_buffer_unref(req_buf);
		req_buf = NULL;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	pthread_cond_wait(&client->cond, &client->mutex);
	pthread_mutex_unlock(&client->mutex);

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(request);
	return ret;
}


int rtsp_client_describe(
	struct rtsp_client *client,
	char **session_description)
{
	int ret = 0, err;
	char *request, *sdp = NULL;
	struct pomp_buffer *req_buf = NULL;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	/* create request */
	err = asprintf(&request,
		RTSP_METHOD_DESCRIBE " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_ACCEPT ": " RTSP_CONTENT_TYPE_SDP "\n\n",
		client->url, ++client->cseq, client->user_agent);
	if (err == -1) {
		RTSP_LOGE("failed to allocate request");
		ret = -1;
		goto out;
	}

	/* send request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		ret = -1;
		goto out;
	}
	if (req_buf) {
		pomp_buffer_unref(req_buf);
		req_buf = NULL;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	free(client->sdp);
	client->sdp = NULL;
	client->wait_describe_response = 1;
	pthread_cond_wait(&client->cond, &client->mutex);
	sdp = client->sdp;
	pthread_mutex_unlock(&client->mutex);

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(request);
	if ((ret == 0) && (session_description))
		*session_description = sdp;
	return ret;
}


int rtsp_client_setup(
	struct rtsp_client *client,
	const char *resource_url,
	int client_stream_port,
	int client_control_port,
	int *server_stream_port,
	int *server_control_port)
{
	int ret = 0, err;
	int s_stream_port, s_control_port;
	char *request, *media_url = NULL;
	struct pomp_buffer *req_buf = NULL;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(resource_url != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(client_stream_port != 0, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(client_control_port != 0, -EINVAL);

	if (!strncmp(resource_url, RTSP_SCHEME_TCP, strlen(RTSP_SCHEME_TCP))) {
		media_url = strdup(resource_url);
	} else {
		if (client->content_base != NULL)
			err = asprintf(&media_url, "%s%s",
				client->content_base, resource_url);
		else if (client->content_location != NULL)
			err = asprintf(&media_url, "%s%s",
				client->content_location, resource_url);
		else if (client->url != NULL)
			err = asprintf(&media_url, "%s/%s",
				client->url, resource_url);
		else
			RTSP_RETURN_ERR_IF_FAILED(NULL, -EINVAL);
	}

	/* create request */
	if (client->session_id)
		err = asprintf(&request,
			RTSP_METHOD_SETUP " %s " RTSP_VERSION "\n"
			RTSP_HEADER_CSEQ ": %d\n"
			RTSP_HEADER_USER_AGENT ": %s\n"
			RTSP_HEADER_TRANSPORT ": " RTSP_TRANSPORT_RTPAVP
			";" RTSP_TRANSPORT_UNICAST
			";" RTSP_TRANSPORT_CLIENT_PORT "=%d-%d\n"
			RTSP_HEADER_SESSION ": %s\n\n",
			media_url, ++client->cseq, client->user_agent,
			client_stream_port, client_control_port,
			client->session_id);
	else
		err = asprintf(&request,
			RTSP_METHOD_SETUP " %s " RTSP_VERSION "\n"
			RTSP_HEADER_CSEQ ": %d\n"
			RTSP_HEADER_USER_AGENT ": %s\n"
			RTSP_HEADER_TRANSPORT ": " RTSP_TRANSPORT_RTPAVP
			";" RTSP_TRANSPORT_UNICAST
			";" RTSP_TRANSPORT_CLIENT_PORT "=%d-%d\n\n",
			media_url, ++client->cseq, client->user_agent,
			client_stream_port, client_control_port);
	if (err == -1) {
		RTSP_LOGE("failed to allocate request");
		ret = -1;
		goto out;
	}

	/* send request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		ret = -1;
		goto out;
	}
	if (req_buf) {
		pomp_buffer_unref(req_buf);
		req_buf = NULL;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	client->wait_setup_response = 1;
	pthread_cond_wait(&client->cond, &client->mutex);
	s_stream_port = client->server_stream_port;
	s_control_port = client->server_control_port;
	pthread_mutex_unlock(&client->mutex);

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(request);
	free(media_url);
	if (ret == 0) {
		if (server_stream_port)
			*server_stream_port = s_stream_port;
		if (server_control_port)
			*server_control_port = s_control_port;
	}
	return ret;
}


int rtsp_client_play(
	struct rtsp_client *client)
{
	int ret = 0, err;
	char *request;
	struct pomp_buffer *req_buf = NULL;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(client->session_id != NULL, -EPERM);

	/* create request */
	err = asprintf(&request,
		RTSP_METHOD_PLAY " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_SESSION ": %s\n"
		RTSP_HEADER_RANGE ": npt=0.000-\n\n",
		(client->content_base) ? client->content_base : client->url,
		++client->cseq, client->user_agent, client->session_id);
		/*TODO: range*/
	if (err == -1) {
		RTSP_LOGE("failed to allocate request");
		ret = -1;
		goto out;
	}

	/* send request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		ret = -1;
		goto out;
	}
	if (req_buf) {
		pomp_buffer_unref(req_buf);
		req_buf = NULL;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	pthread_cond_wait(&client->cond, &client->mutex);
	pthread_mutex_unlock(&client->mutex);

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(request);
	return ret;
}


int rtsp_client_teardown(
	struct rtsp_client *client)
{
	int ret = 0, err;
	char *request;
	struct pomp_buffer *req_buf = NULL;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(client->session_id != NULL, -EPERM);

	/* create request */
	err = asprintf(&request,
		RTSP_METHOD_TEARDOWN " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_SESSION ": %s\n\n",
		client->content_base, ++client->cseq,
		client->user_agent, client->session_id);
	if (err == -1) {
		RTSP_LOGE("failed to allocate request");
		ret = -1;
		goto out;
	}

	/* send request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		ret = -1;
		goto out;
	}
	if (req_buf) {
		pomp_buffer_unref(req_buf);
		req_buf = NULL;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	pthread_cond_wait(&client->cond, &client->mutex);
	pthread_mutex_unlock(&client->mutex);

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
	size_t len = 0;
	const void *cdata = NULL;
	char *response = NULL, *body = NULL, *p, *temp, *temp2;
	char *version, *status_code_str, *status_str;
	char *session_id = NULL;
	char *content_length = NULL, *content_type = NULL;
	char *content_encoding = NULL, *content_language = NULL;
	char *content_base = NULL, *content_location = NULL;
	int status_code, cseq, server_stream_port = 0, server_control_port = 0;

	RTSP_RETURN_IF_FAILED(client != NULL, -EINVAL);

	/* get response data */
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	if ((ret < 0) || (!cdata)) {
		RTSP_LOGE("failed to get response data");
		goto error;
	}
	response = strdup((const char *)cdata);

	/* parse the response header */
	p = strtok_r(response, "\n", &temp);
	if (!p) {
		RTSP_LOGE("invalid response data");
		goto error;
	}

	version = strtok_r(p, " ", &temp2);
	status_code_str = strtok_r(NULL, " ", &temp2);
	status_str = strtok_r(NULL, "\n", &temp2);

	if ((!version) || (strncmp(version, RTSP_VERSION,
		strlen(RTSP_VERSION)))) {
		RTSP_LOGE("invalid RTSP protocol version");
		goto error;
	}
	if ((!status_code_str) || (!status_str)) {
		RTSP_LOGE("malformed RTSP response");
		goto error;
	}
	status_code = atoi(status_code_str);
	if (RTSP_STATUS_CLASS(status_code) != RTSP_STATUS_CLASS_SUCCESS) {
		RTSP_LOGE("RTSP status %d: %s", status_code, status_str);
		goto error;
	}

	p = strtok_r(NULL, "\n", &temp);
	while (p) {
		char *field, *value, *p2;

		/* remove the '\r' before '\n' if present */
		if (p[strlen(p) - 1] == '\r')
			p[strlen(p) - 1] = '\0';

		if (strlen(p) == 0) {
			body = strtok_r(NULL, "", &temp);
			break;
		}

		p2 = strchr(p, ':');
		if (p2) {
			*p2 = '\0';
			field = p;
			value = p2 + 1;
			if (*value == ' ')
				value++;

			if (!strncmp(field, RTSP_HEADER_CSEQ,
				strlen(RTSP_HEADER_CSEQ))) {
				cseq = atoi(value);
				if (cseq != (signed)client->cseq) {
					RTSP_LOGE("unexpected Cseq");
					goto error;
				}
			} else if (!strncmp(field, RTSP_HEADER_SESSION,
				strlen(RTSP_HEADER_SESSION))) {
				char *p3 = strchr(value, ';');
				if (p3)
					*p3 = '\0';
				session_id = value;
				/*TODO: timeout*/
			} else if (!strncmp(field, RTSP_HEADER_CONTENT_LENGTH,
				strlen(RTSP_HEADER_CONTENT_LENGTH))) {
				content_length = value;
			} else if (!strncmp(field, RTSP_HEADER_CONTENT_TYPE,
				strlen(RTSP_HEADER_CONTENT_TYPE))) {
				content_type = value;
			} else if (!strncmp(field, RTSP_HEADER_CONTENT_ENCODING,
				strlen(RTSP_HEADER_CONTENT_ENCODING))) {
				content_encoding = value;
			} else if (!strncmp(field, RTSP_HEADER_CONTENT_LANGUAGE,
				strlen(RTSP_HEADER_CONTENT_LANGUAGE))) {
				content_language = value;
			} else if (!strncmp(field, RTSP_HEADER_CONTENT_BASE,
				strlen(RTSP_HEADER_CONTENT_BASE))) {
				content_base = value;
			} else if (!strncmp(field, RTSP_HEADER_CONTENT_LOCATION,
				strlen(RTSP_HEADER_CONTENT_LOCATION))) {
				content_location = value;
			} else if (!strncmp(field, RTSP_HEADER_TRANSPORT,
				strlen(RTSP_HEADER_TRANSPORT))) {
				char *transport = NULL;
				ret = rtsp_parse_transport_header(value,
					&transport, &server_stream_port,
					&server_control_port);
				if (ret != 0) {
					RTSP_LOGE("failed to parse "
						"transport header");
					goto error;
				}
			}
		}

		p = strtok_r(NULL, "\n", &temp);
	}

	pthread_mutex_lock(&client->mutex);
	if (content_encoding) {
		free(client->content_encoding);
		client->content_encoding = strdup(content_encoding);
	}
	if (content_language) {
		free(client->content_language);
		client->content_language = strdup(content_language);
	}
	if (content_base) {
		free(client->content_base);
		client->content_base = strdup(content_base);
	}
	if (content_location) {
		free(client->content_location);
		client->content_location = strdup(content_location);
	}
	if (session_id) {
		if (client->session_id) {
			if (strcmp(session_id, client->session_id)) {
				pthread_mutex_unlock(&client->mutex);
				RTSP_LOGE("unexpected session id");
				goto error;
			}
		} else {
			client->session_id = strdup(session_id);
		}
	}
	if (client->wait_describe_response) {
		free(client->sdp);
		client->sdp = NULL;
		if ((body) && (!strncmp(content_type, RTSP_CONTENT_TYPE_SDP,
			strlen(RTSP_CONTENT_TYPE_SDP))))
			client->sdp = strdup(body);
		client->wait_describe_response = 0;
	}
	if (client->wait_setup_response) {
		client->server_stream_port = server_stream_port;
		client->server_control_port = server_control_port;
		client->wait_setup_response = 0;
	}
	pthread_mutex_unlock(&client->mutex);

	pthread_cond_signal(&client->cond);
	free(response);
	return;

error:
	pthread_cond_signal(&client->cond);
	free(response);
	pomp_conn_disconnect(conn);
	return;
}
