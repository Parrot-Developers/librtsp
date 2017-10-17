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


static void rtsp_client_pipe_cb(
	int fd,
	uint32_t revents,
	void *userdata);


static void rtsp_client_pomp_event_cb(
	struct pomp_ctx *ctx,
	enum pomp_event event,
	struct pomp_conn *conn,
	const struct pomp_msg *msg,
	void *userdata);


static void rtsp_client_mbox_cb(
	int fd,
	uint32_t revents,
	void *userdata);


static void rtsp_client_timer_cb(
	struct pomp_timer *timer,
	void *userdata);


static void rtsp_client_pomp_cb(
	struct pomp_ctx *ctx,
	struct pomp_conn *conn,
	struct pomp_buffer *buf,
	void *userdata);


struct rtsp_client *rtsp_client_new(
	const char *user_agent,
	struct pomp_loop *loop)
{
	int ret;
	int mutex_created = 0, cond_created = 0;
	int mbox_fd = -1;

	RTSP_RETURN_VAL_IF_FAILED(loop != NULL, -EINVAL, NULL);

	struct rtsp_client *client = calloc(1, sizeof(*client));
	RTSP_RETURN_VAL_IF_FAILED(client != NULL, -ENOMEM, NULL);
	client->tcp_state = RTSP_TCP_STATE_IDLE;
	client->client_state = RTSP_CLIENT_STATE_IDLE;
	client->max_msg_size = PIPE_BUF - 1;
	client->loop = loop;
	client->connect_pipe[0] = -1;
	client->connect_pipe[1] = -1;

	client->user_agent = (user_agent) ?
		strdup(user_agent) : strdup(RTSP_CLIENT_DEFAULT_USER_AGENT);
	if (!client->user_agent) {
		RTSP_LOGE("string allocation failed, aborting");
		goto error;
	}

	ret = pipe(client->connect_pipe);
	if (ret != 0) {
		RTSP_LOG_ERRNO("pipe creation failed", -ret);
		goto error;
	}

	ret = pomp_loop_add(client->loop, client->connect_pipe[0],
		POMP_FD_EVENT_IN, rtsp_client_pipe_cb, (void *)client);
	if (ret != 0) {
		RTSP_LOG_ERRNO("failed to add pipe fd to loop", -ret);
		goto error;
	}

	client->mbox = mbox_new(client->max_msg_size);
	if (client->mbox == NULL) {
		RTSP_LOGE("mbox creation failed");
		goto error;
	}

	mbox_fd = mbox_get_read_fd(client->mbox);
	ret = pomp_loop_add(client->loop, mbox_fd, POMP_FD_EVENT_IN,
		rtsp_client_mbox_cb, (void *)client);
	if (ret != 0) {
		RTSP_LOG_ERRNO("failed to add mbox fd to loop", -ret);
		goto error;
	}

	client->pomp = pomp_ctx_new_with_loop(
		rtsp_client_pomp_event_cb,
		(void *)client, client->loop);
	if (!client->pomp) {
		RTSP_LOGE("pomp creation failed, aborting");
		ret = -1;
		goto error;
	}

	ret = pomp_ctx_set_raw(client->pomp, rtsp_client_pomp_cb);
	if (ret < 0) {
		RTSP_LOG_ERRNO(
			"cannot switch pomp context to raw mode", -ret);
		goto error;
	}

	client->timer = pomp_timer_new(client->loop,
		rtsp_client_timer_cb, (void *)client);
	if (!client->timer) {
		RTSP_LOGE("pomp timer creation failed, aborting");
		ret = -1;
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
	if (client->timer)
		pomp_timer_destroy(client->timer);
	if (client->pomp)
		pomp_ctx_destroy(client->pomp);
	if (mutex_created)
		pthread_mutex_destroy(&client->mutex);
	if (cond_created)
		pthread_cond_destroy(&client->cond);
	if (client->mbox)
		mbox_destroy(client->mbox);
	if (client->connect_pipe[0] != -1) {
		do
			ret = close(client->connect_pipe[0]);
		while ((ret == -1) && (errno == EINTR));
		client->connect_pipe[0] = -1;
	}
	if (client->connect_pipe[1] != -1) {
		do
			ret = close(client->connect_pipe[1]);
		while ((ret == -1) && (errno == EINTR));
		client->connect_pipe[1] = -1;
	}
	free(client->user_agent);
	free(client);
	return NULL;
}


int rtsp_client_destroy(
	struct rtsp_client *client)
{
	int ret, mbox_fd;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	ret = pomp_timer_destroy(client->timer);
	if (ret != 0) {
		if (ret != -EBUSY)
			RTSP_LOG_ERRNO("failed to destroy timer context", -ret);
		return ret;
	}

	ret = pomp_ctx_destroy(client->pomp);
	if (ret != 0) {
		if (ret != -EBUSY)
			RTSP_LOG_ERRNO("failed to destroy pomp context", -ret);
		return ret;
	}

	mbox_fd = mbox_get_read_fd(client->mbox);
	ret = pomp_loop_remove(client->loop, mbox_fd);
	if (ret != 0)
		RTSP_LOG_ERRNO("failed to remove mbox fd from loop", -ret);
	mbox_destroy(client->mbox);

	ret = pomp_loop_remove(client->loop, client->connect_pipe[0]);
	if (ret != 0)
		RTSP_LOG_ERRNO("failed to remove pipe fd from loop", -ret);
	if (client->connect_pipe[0] != -1) {
		do
			ret = close(client->connect_pipe[0]);
		while ((ret == -1) && (errno == EINTR));
		client->connect_pipe[0] = -1;
	}
	if (client->connect_pipe[1] != -1) {
		do
			ret = close(client->connect_pipe[1]);
		while ((ret == -1) && (errno == EINTR));
		client->connect_pipe[1] = -1;
	}

	pthread_mutex_destroy(&client->mutex);
	pthread_cond_destroy(&client->cond);

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
	free(client->pending_content);
	rtsp_response_header_free(&client->current_header);
	free(client);

	return 0;
}


int rtsp_client_connect(
	struct rtsp_client *client,
	const char *url)
{
	int ret = 0, _ret;
	char *p, *temp, *u = NULL;
	char *buf = "x";
	ssize_t err;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(url != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(strlen(url), -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(strncmp(url, RTSP_SCHEME_TCP,
		strlen(RTSP_SCHEME_TCP)) == 0, -EINVAL);

	if (client->url != NULL) {
		RTSP_LOGE("client already connected");
		ret = -EISCONN;
		goto out;
	}

	client->url = strdup(url);
	if (!client->url) {
		RTSP_LOGE("string allocation failed, aborting");
		ret = -ENOMEM;
		goto out;
	}

	/* parse the URL */
	client->server_port = RTSP_DEFAULT_PORT;
	u = strdup(client->url + 7);
	if (!u) {
		RTSP_LOGE("string allocation failed, aborting");
		ret = -ENOMEM;
		goto out;
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
			ret = -ENOMEM;
			goto out;
		}

		/* absolute path */
		p = strtok_r(NULL, "/", &temp);
		if (p) {
			client->abs_path = strdup(p);
			if (!client->abs_path) {
				RTSP_LOGE("string allocation failed, aborting");
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	xfree((void **)&u);

	/* check the URL validity */
	if ((!client->server_host) || (!strlen(client->server_host))) {
		RTSP_LOGE("invalid server host, aborting");
		ret = -EINVAL;
		goto out;
	}
	if (client->server_port == 0) {
		RTSP_LOGE("invalid server port, aborting");
		ret = -EINVAL;
		goto out;
	}
	if ((!client->abs_path) || (!strlen(client->abs_path))) {
		RTSP_LOGE("invalid resource path, aborting");
		ret = -EINVAL;
		goto out;
	}

	/* TODO: name resolution */
	_ret = inet_pton(AF_INET, client->server_host,
		&client->remote_addr_in.sin_addr);
	if (_ret <= 0) {
		RTSP_LOGE("failed to convert address %s, aborting",
			client->server_host);
		ret = -1;
		goto out;
	}
	client->remote_addr_in.sin_family = AF_INET;
	client->remote_addr_in.sin_port = htons(client->server_port);

	/* signal the loop to create the connection */
	do
		err = write(client->connect_pipe[1], buf, 1);
	while ((err == -1) && (errno == EINTR));

out:
	if (ret != 0) {
		xfree((void **)&client->server_host);
		xfree((void **)&client->abs_path);
		xfree((void **)&client->url);
	}
	free(u);
	return ret;
}


int rtsp_client_disconnect(
	struct rtsp_client *client,
	unsigned int timeout_ms)
{
	int ret = 0;
	char *buf = "x";
	ssize_t err;
	struct timespec ts;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	if (client->url == NULL) {
		RTSP_LOGE("client is not connected");
		return -EISCONN;
	}

	xfree((void **)&client->url);

	/* signal the loop to stop the connection */
	do
		err = write(client->connect_pipe[1], buf, 1);
	while ((err == -1) && (errno == EINTR));

	/* wait for disconnection event */
	pthread_mutex_lock(&client->mutex);
	if (timeout_ms > 0) {
		get_time_with_ms_delay(&ts, timeout_ms);
		err = pthread_cond_timedwait(
			&client->cond, &client->mutex, &ts);
	} else
		err = pthread_cond_wait(&client->cond, &client->mutex);
	pthread_mutex_unlock(&client->mutex);
	if (err == ETIMEDOUT) {
		RTSP_LOGE("timeout waiting for disconnection");
		ret = -ETIMEDOUT;
	}

	return ret;
}


int rtsp_client_options(
	struct rtsp_client *client,
	unsigned int timeout_ms)
{
	int ret = 0, err;
	char *request = NULL;
	enum rtsp_client_state client_state;
	int waiting_reply;
	struct timespec ts;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);

	/* check that the client state is valid  */
	pthread_mutex_lock(&client->mutex);
	waiting_reply = client->waiting_reply;
	pthread_mutex_unlock(&client->mutex);
	RTSP_RETURN_ERR_IF_FAILED(waiting_reply == 0, -EBUSY);

	/* create request */
	request = calloc(client->max_msg_size, 1);
	RTSP_RETURN_ERR_IF_FAILED(request != NULL, -ENOMEM);

	err = snprintf(request, client->max_msg_size,
		RTSP_METHOD_OPTIONS " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n\n",
		client->url, ++client->cseq, client->user_agent);
	if (err < 0) {
		RTSP_LOGE("failed to write request");
		ret = -1;
		goto out;
	}

	/* update the state */
	pthread_mutex_lock(&client->mutex);
	client->client_state = RTSP_CLIENT_STATE_OPTIONS_WAITING_REPLY;
	client->waiting_reply = 1;
	pthread_mutex_unlock(&client->mutex);

	/* push into the mailbox */
	err = mbox_push(client->mbox, request);
	if (err < 0) {
		RTSP_LOGE("failed to push into mbox");
		ret = -1;
		goto out;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	if (timeout_ms > 0) {
		get_time_with_ms_delay(&ts, timeout_ms);
		err = pthread_cond_timedwait(
			&client->cond, &client->mutex, &ts);
	} else
		err = pthread_cond_wait(&client->cond, &client->mutex);
	client->waiting_reply = 0;
	client_state = client->client_state;
	client->client_state = RTSP_CLIENT_STATE_IDLE;
	pthread_mutex_unlock(&client->mutex);
	if (err == ETIMEDOUT) {
		RTSP_LOGE("timeout on reply");
		ret = -ETIMEDOUT;
		goto out;
	} else if (client_state != RTSP_CLIENT_STATE_OPTIONS_OK) {
		RTSP_LOGE("failed to get reply");
		ret = -EPROTO;
		goto out;
	}

out:
	free(request);
	return ret;
}


int rtsp_client_describe(
	struct rtsp_client *client,
	char **session_description,
	unsigned int timeout_ms)
{
	int ret = 0, err;
	char *request = NULL, *sdp = NULL;
	enum rtsp_client_state client_state;
	int waiting_reply;
	struct timespec ts;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(session_description != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED((client->options == 0) ||
		(client->options & RTSP_METHOD_FLAG_DESCRIBE), -ENOSYS);

	/* check that the client state is valid  */
	pthread_mutex_lock(&client->mutex);
	waiting_reply = client->waiting_reply;
	pthread_mutex_unlock(&client->mutex);
	RTSP_RETURN_ERR_IF_FAILED(waiting_reply == 0, -EBUSY);

	/* create request */
	request = calloc(client->max_msg_size, 1);
	RTSP_RETURN_ERR_IF_FAILED(request != NULL, -ENOMEM);

	err = snprintf(request, client->max_msg_size,
		RTSP_METHOD_DESCRIBE " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_ACCEPT ": " RTSP_CONTENT_TYPE_SDP "\n\n",
		client->url, ++client->cseq, client->user_agent);
	if (err < 0) {
		RTSP_LOGE("failed to write request");
		ret = -1;
		goto out;
	}

	/* update the state */
	pthread_mutex_lock(&client->mutex);
	client->client_state = RTSP_CLIENT_STATE_DESCRIBE_WAITING_REPLY;
	client->waiting_reply = 1;
	pthread_mutex_unlock(&client->mutex);

	/* push into the mailbox */
	err = mbox_push(client->mbox, request);
	if (err < 0) {
		RTSP_LOGE("failed to push into mbox");
		ret = -1;
		goto out;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	if (timeout_ms > 0) {
		get_time_with_ms_delay(&ts, timeout_ms);
		err = pthread_cond_timedwait(
			&client->cond, &client->mutex, &ts);
	} else
		err = pthread_cond_wait(&client->cond, &client->mutex);
	sdp = client->sdp;
	client->waiting_reply = 0;
	client_state = client->client_state;
	client->client_state = RTSP_CLIENT_STATE_IDLE;
	pthread_mutex_unlock(&client->mutex);
	if (err == ETIMEDOUT) {
		RTSP_LOGE("timeout on reply");
		ret = -ETIMEDOUT;
		goto out;
	} else if (client_state != RTSP_CLIENT_STATE_DESCRIBE_OK) {
		RTSP_LOGE("failed to get reply");
		ret = -EPROTO;
		goto out;
	}

	*session_description = sdp;

out:
	free(request);
	return ret;
}


int rtsp_client_setup(
	struct rtsp_client *client,
	const char *resource_url,
	int client_stream_port,
	int client_control_port,
	int *server_stream_port,
	int *server_control_port,
	unsigned int timeout_ms)
{
	int ret = 0, err;
	int s_stream_port, s_control_port;
	char *request = NULL, *media_url = NULL;
	enum rtsp_client_state client_state;
	int waiting_reply;
	struct timespec ts;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(resource_url != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(client_stream_port != 0, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(client_control_port != 0, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(server_stream_port != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED(server_control_port != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED((client->options == 0) ||
		(client->options & RTSP_METHOD_FLAG_SETUP), -ENOSYS);

	/* check that the client state is valid  */
	pthread_mutex_lock(&client->mutex);
	waiting_reply = client->waiting_reply;
	pthread_mutex_unlock(&client->mutex);
	RTSP_RETURN_ERR_IF_FAILED(waiting_reply == 0, -EBUSY);

	/* create the media URL */
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
	request = calloc(client->max_msg_size, 1);
	RTSP_RETURN_ERR_IF_FAILED(request != NULL, -ENOMEM);

	if (client->session_id)
		err = snprintf(request, client->max_msg_size,
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
		err = snprintf(request, client->max_msg_size,
			RTSP_METHOD_SETUP " %s " RTSP_VERSION "\n"
			RTSP_HEADER_CSEQ ": %d\n"
			RTSP_HEADER_USER_AGENT ": %s\n"
			RTSP_HEADER_TRANSPORT ": " RTSP_TRANSPORT_RTPAVP
			";" RTSP_TRANSPORT_UNICAST
			";" RTSP_TRANSPORT_CLIENT_PORT "=%d-%d\n\n",
			media_url, ++client->cseq, client->user_agent,
			client_stream_port, client_control_port);
	if (err < 0) {
		RTSP_LOGE("failed to write request");
		ret = -1;
		goto out;
	}

	/* update the state */
	pthread_mutex_lock(&client->mutex);
	client->client_state = RTSP_CLIENT_STATE_SETUP_WAITING_REPLY;
	client->waiting_reply = 1;
	pthread_mutex_unlock(&client->mutex);

	/* push into the mailbox */
	err = mbox_push(client->mbox, request);
	if (err < 0) {
		RTSP_LOGE("failed to push into mbox");
		ret = -1;
		goto out;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	if (timeout_ms > 0) {
		get_time_with_ms_delay(&ts, timeout_ms);
		err = pthread_cond_timedwait(
			&client->cond, &client->mutex, &ts);
	} else
		err = pthread_cond_wait(&client->cond, &client->mutex);
	s_stream_port = client->server_stream_port;
	s_control_port = client->server_control_port;
	client->waiting_reply = 0;
	client_state = client->client_state;
	client->client_state = RTSP_CLIENT_STATE_IDLE;
	pthread_mutex_unlock(&client->mutex);
	if (err == ETIMEDOUT) {
		RTSP_LOGE("timeout on reply");
		ret = -ETIMEDOUT;
		goto out;
	} else if (client_state != RTSP_CLIENT_STATE_SETUP_OK) {
		RTSP_LOGE("failed to get reply");
		ret = -EPROTO;
		goto out;
	}

	*server_stream_port = s_stream_port;
	*server_control_port = s_control_port;

out:
	free(request);
	free(media_url);
	return ret;
}


int rtsp_client_play(
	struct rtsp_client *client,
	unsigned int timeout_ms)
{
	int ret = 0, err;
	char *request = NULL;
	enum rtsp_client_state client_state;
	int waiting_reply;
	struct timespec ts;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED((client->options == 0) ||
		(client->options & RTSP_METHOD_FLAG_PLAY), -ENOSYS);

	/* check that the client state is valid  */
	pthread_mutex_lock(&client->mutex);
	waiting_reply = client->waiting_reply;
	pthread_mutex_unlock(&client->mutex);
	RTSP_RETURN_ERR_IF_FAILED(waiting_reply == 0, -EBUSY);
	RTSP_RETURN_ERR_IF_FAILED(client->session_id != NULL, -EPERM);

	/* create request */
	request = calloc(client->max_msg_size, 1);
	RTSP_RETURN_ERR_IF_FAILED(request != NULL, -ENOMEM);

	err = snprintf(request, client->max_msg_size,
		RTSP_METHOD_PLAY " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_SESSION ": %s\n"
		RTSP_HEADER_RANGE ": npt=0.000-\n\n",
		(client->content_base) ? client->content_base : client->url,
		++client->cseq, client->user_agent, client->session_id);
		/*TODO: range*/
	if (err < 0) {
		RTSP_LOGE("failed to write request");
		ret = -1;
		goto out;
	}

	/* update the state */
	pthread_mutex_lock(&client->mutex);
	client->client_state = RTSP_CLIENT_STATE_PLAY_WAITING_REPLY;
	client->waiting_reply = 1;
	pthread_mutex_unlock(&client->mutex);

	/* push into the mailbox */
	err = mbox_push(client->mbox, request);
	if (err < 0) {
		RTSP_LOGE("failed to push into mbox");
		ret = -1;
		goto out;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	if (timeout_ms > 0) {
		get_time_with_ms_delay(&ts, timeout_ms);
		err = pthread_cond_timedwait(
			&client->cond, &client->mutex, &ts);
	} else
		err = pthread_cond_wait(&client->cond, &client->mutex);
	client->waiting_reply = 0;
	client_state = client->client_state;
	client->client_state = RTSP_CLIENT_STATE_IDLE;
	if (client_state == RTSP_CLIENT_STATE_PLAY_OK)
		client->playing = 1;
	pthread_mutex_unlock(&client->mutex);
	if (err == ETIMEDOUT) {
		RTSP_LOGE("timeout on reply");
		ret = -ETIMEDOUT;
		goto out;
	} else if (client_state != RTSP_CLIENT_STATE_PLAY_OK) {
		RTSP_LOGE("failed to get reply");
		ret = -EPROTO;
		goto out;
	}

out:
	free(request);
	return ret;
}


int rtsp_client_teardown(
	struct rtsp_client *client,
	unsigned int timeout_ms)
{
	int ret = 0, err;
	char *request = NULL;
	enum rtsp_client_state client_state;
	int waiting_reply;
	struct timespec ts;

	RTSP_RETURN_ERR_IF_FAILED(client != NULL, -EINVAL);
	RTSP_RETURN_ERR_IF_FAILED((client->options == 0) ||
		(client->options & RTSP_METHOD_FLAG_TEARDOWN), -ENOSYS);

	/* check that the client state is valid  */
	pthread_mutex_lock(&client->mutex);
	waiting_reply = client->waiting_reply;
	pthread_mutex_unlock(&client->mutex);
	RTSP_RETURN_ERR_IF_FAILED(waiting_reply == 0, -EBUSY);
	RTSP_RETURN_ERR_IF_FAILED(client->session_id != NULL, -EPERM);

	/* create request */
	request = calloc(client->max_msg_size, 1);
	RTSP_RETURN_ERR_IF_FAILED(request != NULL, -ENOMEM);

	err = snprintf(request, client->max_msg_size,
		RTSP_METHOD_TEARDOWN " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_SESSION ": %s\n\n",
		client->content_base, ++client->cseq,
		client->user_agent, client->session_id);
	if (err < 0) {
		RTSP_LOGE("failed to write request");
		ret = -1;
		goto out;
	}

	/* update the state */
	pthread_mutex_lock(&client->mutex);
	client->client_state = RTSP_CLIENT_STATE_TEARDOWN_WAITING_REPLY;
	client->waiting_reply = 1;
	pthread_mutex_unlock(&client->mutex);

	/* push into the mailbox */
	err = mbox_push(client->mbox, request);
	if (err < 0) {
		RTSP_LOGE("failed to push into mbox");
		ret = -1;
		goto out;
	}

	/* wait for response */
	pthread_mutex_lock(&client->mutex);
	if (timeout_ms > 0) {
		get_time_with_ms_delay(&ts, timeout_ms);
		err = pthread_cond_timedwait(
			&client->cond, &client->mutex, &ts);
	} else
		err = pthread_cond_wait(&client->cond, &client->mutex);
	client->waiting_reply = 0;
	client_state = client->client_state;
	client->client_state = RTSP_CLIENT_STATE_IDLE;
	if (client_state == RTSP_CLIENT_STATE_TEARDOWN_OK) {
		client->playing = 0;
		xfree((void **)&client->session_id);
	}
	pthread_mutex_unlock(&client->mutex);
	if (err == ETIMEDOUT) {
		RTSP_LOGE("timeout on reply");
		ret = -ETIMEDOUT;
		goto out;
	} else if (client_state != RTSP_CLIENT_STATE_TEARDOWN_OK) {
		RTSP_LOGE("failed to get reply");
		ret = -EPROTO;
		goto out;
	}

out:
	free(request);
	return ret;
}


static void rtsp_client_pipe_cb(
	int fd,
	uint32_t revents,
	void *userdata)
{
	struct rtsp_client *client = (struct rtsp_client *)userdata;
	char buf[10];
	int ret = 0;

	RTSP_RETURN_IF_FAILED(client != NULL, -EINVAL);

	do {
		/* read from the pipe */
		ret = read(client->connect_pipe[0], &buf, 10);
		if ((ret < 0) && (ret != -EAGAIN)) {
			RTSP_LOG_ERRNO("failed to read from pipe", -ret);
			break;
		}
	} while (ret == 0);

	ret = 0;
	if (client->url) {
		RTSP_LOGI("connecting to URL '%s'", client->url);
		ret = pomp_ctx_connect(client->pomp,
			(const struct sockaddr *)&client->remote_addr_in,
			sizeof(client->remote_addr_in));
		if (ret < 0) {
			RTSP_LOG_ERRNO("failed to connect", -ret);
			goto cleanup;
		}
	} else {
		ret = pomp_ctx_stop(client->pomp);
		if (ret < 0) {
			RTSP_LOG_ERRNO("failed to disconnect", -ret);
			goto cleanup;
		}
		if (client->tcp_state == RTSP_TCP_STATE_IDLE)
			/* the client is already disconnected */
			pthread_cond_signal(&client->cond);
	}

	return;

cleanup:
	xfree((void **)&client->server_host);
	xfree((void **)&client->abs_path);
	xfree((void **)&client->url);
}


static void rtsp_client_pomp_event_cb(
	struct pomp_ctx *ctx,
	enum pomp_event event,
	struct pomp_conn *conn,
	const struct pomp_msg *msg,
	void *userdata)
{
	struct rtsp_client *client = (struct rtsp_client *)userdata;
	char *request = NULL;
	int err;

	switch (event) {
	case POMP_EVENT_CONNECTED:
		RTSP_LOGI("client connected");
		client->tcp_state = RTSP_TCP_STATE_CONNECTED;
		request = calloc(client->max_msg_size, 1);
		if (request) {
			err = mbox_push(client->mbox, request);
			if (err < 0)
				RTSP_LOGE("failed to push into mbox");
			free(request);
		} else
			RTSP_LOGE("allocation failed");
		break;

	case POMP_EVENT_DISCONNECTED:
		RTSP_LOGI("client disconnected");
		client->tcp_state = RTSP_TCP_STATE_IDLE;
		pthread_cond_signal(&client->cond);
		break;

	default:
	case POMP_EVENT_MSG:
		/* never received for raw context */
		break;
	}
}


static void rtsp_client_mbox_cb(
	int fd,
	uint32_t revents,
	void *userdata)
{
	struct rtsp_client *client = (struct rtsp_client *)userdata;
	struct pomp_buffer *req_buf = NULL;
	char *buf;
	int ret;

	RTSP_RETURN_IF_FAILED(client != NULL, -EINVAL);

	if (client->tcp_state != RTSP_TCP_STATE_CONNECTED)
		return;

	buf = calloc(client->max_msg_size, 1);
	RTSP_RETURN_IF_FAILED(buf != NULL, -ENOMEM);

	do {
		/* read from the mailbox */
		ret = mbox_peek(client->mbox, buf);
		if ((ret < 0) && (ret != -EAGAIN)) {
			RTSP_LOG_ERRNO("failed to read from mbox", -ret);
			goto out;
		}

		if (ret != 0)
			break;

		if (!strlen(buf))
			continue;

		/* send the request */
		req_buf = pomp_buffer_new_with_data(buf, strlen(buf));
		ret = pomp_ctx_send_raw_buf(client->pomp, req_buf);
		if (ret < 0) {
			RTSP_LOG_ERRNO("failed to send request", -ret);
			goto out;
		}
	} while (ret == 0);

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(buf);
}


static void rtsp_client_timer_cb(
	struct pomp_timer *timer,
	void *userdata)
{
	struct rtsp_client *client = (struct rtsp_client *)userdata;
	int err;
	char *request = NULL;
	struct pomp_buffer *req_buf = NULL;
	int waiting_reply;

	RTSP_RETURN_IF_FAILED((client->options == 0) ||
		(client->options & RTSP_METHOD_FLAG_GET_PARAMETER), -ENOSYS);

	/* check that the client state is valid  */
	pthread_mutex_lock(&client->mutex);
	waiting_reply = client->waiting_reply;
	pthread_mutex_unlock(&client->mutex);
	RTSP_RETURN_IF_FAILED(waiting_reply == 0, -EBUSY);
	RTSP_RETURN_IF_FAILED(client->session_id != NULL, -EPERM);

	/* create request */
	request = calloc(client->max_msg_size, 1);
	RTSP_RETURN_IF_FAILED(request != NULL, -ENOMEM);

	err = snprintf(request, client->max_msg_size,
		RTSP_METHOD_GET_PARAMETER " %s " RTSP_VERSION "\n"
		RTSP_HEADER_CSEQ ": %d\n"
		RTSP_HEADER_USER_AGENT ": %s\n"
		RTSP_HEADER_SESSION ": %s\n\n",
		client->content_base, ++client->cseq,
		client->user_agent, client->session_id);
	if (err < 0) {
		RTSP_LOGE("failed to write request");
		goto out;
	}

	/* update the state */
	pthread_mutex_lock(&client->mutex);
	client->client_state = RTSP_CLIENT_STATE_KEEPALIVE_WAITING_REPLY;
	client->waiting_reply = 1;
	pthread_mutex_unlock(&client->mutex);

	/* send the request */
	req_buf = pomp_buffer_new_with_data(request, strlen(request));
	err = pomp_ctx_send_raw_buf(client->pomp, req_buf);
	if (err < 0) {
		RTSP_LOG_ERRNO("failed to send request", -err);
		goto out;
	}

out:
	if (req_buf)
		pomp_buffer_unref(req_buf);
	free(request);
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
	char *response = NULL, *body = NULL;
	struct rtsp_response_header new_header;

	RTSP_RETURN_IF_FAILED(client != NULL, -EINVAL);

	/* get response data */
	ret = pomp_buffer_get_cdata(buf, &cdata, &len, NULL);
	if ((ret < 0) || (!cdata)) {
		RTSP_LOGE("failed to get response data");
		goto error;
	}
	response = strdup((const char *)cdata);

	if (client->pending_content_length == 0) {
		/* new message, parse the response header */
		memset(&new_header, 0, sizeof(new_header));
		ret = rtsp_response_header_read(response, &new_header);
		if (ret != 0) {
			RTSP_LOGE("failed to parse response header");
			goto error;
		}

		if (new_header.cseq != (signed)client->cseq) {
			RTSP_LOGE("unexpected Cseq");
			goto error;
		}

		ret = rtsp_response_header_free(&client->current_header);
		if (ret != 0)
			goto error;

		ret = rtsp_response_header_copy(&new_header,
			&client->current_header);
		if (ret != 0)
			goto error;

		client->pending_content_length =
			client->current_header.content_length;
		client->pending_content_offset = 0;
		xfree((void **)&client->pending_content);
		if (client->pending_content_length) {
			client->pending_content =
				malloc(client->pending_content_length);
			if (!client->pending_content) {
				RTSP_LOGE("allocation failed");
				goto error;
			}
		}

		body = client->current_header.body;
	} else {
		/* continuation */
		body = response;
	}

	if ((body) && (strlen(body)) && (client->pending_content_length)) {
		/* copy the body */
		int len = ((int)strlen(body) < client->pending_content_length) ?
			(int)strlen(body) : client->pending_content_length;
		memcpy(client->pending_content + client->pending_content_offset,
			body, len);
		client->pending_content_offset += len;
		client->pending_content_length -= len;
	}

	if (client->pending_content_length == 0) {
		/* message complete */
		struct rtsp_response_header *header = &client->current_header;

		pthread_mutex_lock(&client->mutex);

		client->timeout = header->timeout;
		if (client->timeout > 0) {
			/* set the timer to 80% of the timeout value */
			ret = pomp_timer_set(client->timer,
				client->timeout * 800);
			if (ret != 0)
				RTSP_LOG_ERRNO("failed to set timer", -ret);
		}

		if (header->content_encoding) {
			xfree((void **)&client->content_encoding);
			client->content_encoding =
				xstrdup(header->content_encoding);
		}
		if (header->content_language) {
			xfree((void **)&client->content_language);
			client->content_language =
				xstrdup(header->content_language);
		}
		if (header->content_base) {
			xfree((void **)&client->content_base);
			client->content_base =
				xstrdup(header->content_base);
		}
		if (header->content_location) {
			xfree((void **)&client->content_location);
			client->content_location =
				xstrdup(header->content_location);
		}
		if (header->session_id) {
			if (client->session_id) {
				if (strcmp(header->session_id,
					client->session_id)) {
					pthread_mutex_unlock(&client->mutex);
					RTSP_LOGE("unexpected session id");
					goto error;
				}
			} else {
				client->session_id = strdup(header->session_id);
			}
		}

		if (client->client_state ==
			RTSP_CLIENT_STATE_OPTIONS_WAITING_REPLY) {
			client->options = header->options;
			client->client_state = RTSP_CLIENT_STATE_OPTIONS_OK;
		}

		if (client->client_state ==
			RTSP_CLIENT_STATE_DESCRIBE_WAITING_REPLY) {
			xfree((void **)&client->sdp);
			if ((client->pending_content) &&
				(header->content_type) &&
				(!strncmp(header->content_type,
				RTSP_CONTENT_TYPE_SDP,
				strlen(RTSP_CONTENT_TYPE_SDP))))
				client->sdp = strdup(client->pending_content);
			client->client_state = RTSP_CLIENT_STATE_DESCRIBE_OK;
		}

		if (client->client_state ==
			RTSP_CLIENT_STATE_SETUP_WAITING_REPLY) {
			client->server_stream_port =
				header->transport.server_stream_port;
			client->server_control_port =
				header->transport.server_control_port;
			client->client_state = RTSP_CLIENT_STATE_SETUP_OK;
		}

		if (client->client_state ==
			RTSP_CLIENT_STATE_KEEPALIVE_WAITING_REPLY) {
			client->waiting_reply = 0;
			client->client_state = RTSP_CLIENT_STATE_IDLE;
		}

		if (client->client_state ==
			RTSP_CLIENT_STATE_PLAY_WAITING_REPLY)
			client->client_state = RTSP_CLIENT_STATE_PLAY_OK;

		if (client->client_state ==
			RTSP_CLIENT_STATE_TEARDOWN_WAITING_REPLY)
			client->client_state = RTSP_CLIENT_STATE_TEARDOWN_OK;

		pthread_mutex_unlock(&client->mutex);

		pthread_cond_signal(&client->cond);

		xfree((void **)&client->pending_content);
		ret = rtsp_response_header_free(&client->current_header);
		if (ret != 0)
			goto error;
	}

	free(response);
	return;

error:
	pthread_cond_signal(&client->cond);
	xfree((void **)&client->pending_content);
	client->pending_content_length = 0;
	rtsp_response_header_free(&client->current_header);
	free(response);
	pomp_conn_disconnect(conn);
	return;
}
