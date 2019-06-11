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


struct rtsp_server_session *rtsp_server_session_add(struct rtsp_server *server,
						    unsigned int timeout_ms,
						    const char *uri)
{
	int ret;
	int found;
	struct rtsp_server_session *session = NULL, *_session = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(server == NULL, EINVAL, NULL);

	session = calloc(1, sizeof(*session));
	ULOG_ERRNO_RETURN_VAL_IF(session == NULL, ENOMEM, NULL);
	list_node_unref(&session->node);
	list_init(&session->medias);
	session->server = server;
	session->timeout_ms = timeout_ms;
	if (session->timeout_ms > 0) {
		session->timer = pomp_timer_new(server->loop,
						&rtsp_server_session_timer_cb,
						(void *)session);
		if (!session->timer) {
			ULOG_ERRNO("pomp_timer_new", ENOMEM);
			goto error;
		}
		ret = rtsp_server_session_reset_timeout(session);
		if (ret < 0)
			goto error;
	}

	do {
		/* Generate a session id */
		uint64_t id64;
		ret = futils_random64(&id64);
		if (ret < 0) {
			ULOG_ERRNO("futils_random64", -ret);
			goto error;
		}
		ret = asprintf(&session->session_id, "%016" PRIx64, id64);
		if ((ret < 0) || (session->session_id == NULL)) {
			ULOG_ERRNO("asprintf", ENOMEM);
			goto error;
		}

		/* Check that this session id does not already exist */
		found = 0;
		list_walk_entry_forward(&server->sessions, _session, node)
		{
			if (strncmp(_session->session_id,
				    session->session_id,
				    RTSP_SERVER_SESSION_ID_LENGTH) == 0) {
				found = 1;
				xfree((void **)&session->session_id);
				break;
			}
		}
	} while (found);

	/* store the URI */
	session->uri = xstrdup(uri);

	/* Add to the list */
	list_add_before(&server->sessions, &session->node);
	server->session_count++;

	return session;

error:
	if (session) {
		if (session->timer != NULL) {
			ret = pomp_timer_destroy(session->timer);
			if (ret < 0)
				ULOG_ERRNO("pomp_timer_destroy", -ret);
		}
		free(session->session_id);
		free(session);
	}

	return NULL;
}


int rtsp_server_session_remove(struct rtsp_server *server,
			       struct rtsp_server_session *session)
{
	int found = 0, ret;
	struct rtsp_server_session *_session = NULL;
	struct rtsp_server_session_media *media = NULL, *tmp_media = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session == NULL, EINVAL);

	list_walk_entry_forward(&server->sessions, _session, node)
	{
		if (_session == session) {
			found = 1;
			break;
		}
	}

	if (!found) {
		ULOGE("%s: session not found", __func__);
		return -ENOENT;
	}

	/* Remove from the list */
	list_del(&session->node);
	server->session_count--;

	/* Remove all medias */
	list_walk_entry_forward_safe(&session->medias, media, tmp_media, node)
	{
		rtsp_server_session_media_remove(server, session, media);
	}

	if (session->timer != NULL) {
		ret = pomp_timer_destroy(session->timer);
		if (ret < 0)
			ULOG_ERRNO("pomp_timer_destroy", -ret);
	}
	ret = pomp_loop_idle_remove(
		server->loop, &rtsp_server_session_remove_idle, session);
	if (ret < 0)
		ULOG_ERRNO("pomp_loop_idle_remove", -ret);
	free(session->session_id);
	free(session->uri);
	free(session);

	return 0;
}


int rtsp_server_session_reset_timeout(struct rtsp_server_session *session)
{
	int ret;

	ULOG_ERRNO_RETURN_ERR_IF(session == NULL, EINVAL);

	/* Set the timer to >= 20% more than the advertised session timeout
	 * because some players (like VLC) will only send GET_PARAMETER
	 * request every 'timeout_ms' ms, which can cause timeouts here
	 * otherwise due to latency */
	ret = pomp_timer_set(session->timer,
			     ((12 * session->timeout_ms) + 9) / 10);
	if (ret < 0)
		ULOG_ERRNO("pomp_timer_set", -ret);

	return ret;
}


struct rtsp_server_session *rtsp_server_session_find(struct rtsp_server *server,
						     const char *session_id)
{
	int found = 0;
	struct rtsp_server_session *session = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(server == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(session_id == NULL, EINVAL, NULL);

	list_walk_entry_forward(&server->sessions, session, node)
	{
		if (session->session_id != NULL &&
		    strncmp(session->session_id,
			    session_id,
			    RTSP_SERVER_SESSION_ID_LENGTH) == 0) {
			found = 1;
			break;
		}
	}

	return (found) ? session : NULL;
}


struct rtsp_server_session_media *
rtsp_server_session_media_add(struct rtsp_server *server,
			      struct rtsp_server_session *session,
			      const char *path)
{
	struct rtsp_server_session_media *media = NULL, *_media;

	ULOG_ERRNO_RETURN_VAL_IF(server == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(session == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(path == NULL, EINVAL, NULL);

	_media = rtsp_server_session_media_find(server, session, path);
	ULOG_ERRNO_RETURN_VAL_IF(_media != NULL, EEXIST, NULL);

	media = calloc(1, sizeof(*media));
	ULOG_ERRNO_RETURN_VAL_IF(media == NULL, ENOMEM, NULL);
	list_node_unref(&media->node);
	media->session = session;
	media->path = strdup(path);

	/* Add to the list */
	list_add_before(&session->medias, &media->node);
	session->media_count++;

	return media;
}


int rtsp_server_session_media_remove(struct rtsp_server *server,
				     struct rtsp_server_session *session,
				     struct rtsp_server_session_media *media)
{
	int found = 0;
	struct rtsp_server_session_media *_media = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(server == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(session == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media == NULL, EINVAL);

	list_walk_entry_forward(&session->medias, _media, node)
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
	session->media_count--;

	free(media->path);
	free(media);

	return 0;
}


struct rtsp_server_session_media *
rtsp_server_session_media_find(struct rtsp_server *server,
			       struct rtsp_server_session *session,
			       const char *path)
{
	int found = 0;
	struct rtsp_server_session_media *media = NULL;

	ULOG_ERRNO_RETURN_VAL_IF(server == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(session == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(path == NULL, EINVAL, NULL);

	list_walk_entry_forward(&session->medias, media, node)
	{
		if (strcmp(media->path, path) == 0) {
			found = 1;
			break;
		}
	}

	return (found) ? media : NULL;
}
