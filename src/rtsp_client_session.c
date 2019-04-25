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


struct rtsp_client_session *rtsp_client_get_session(struct rtsp_client *client,
						    const char *session_id,
						    int add)
{
	struct rtsp_client_session *session;

	ULOG_ERRNO_RETURN_VAL_IF(client == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(session_id == NULL, EINVAL, NULL);

	/* Search for a session with the same id */
	list_walk_entry_forward(&client->sessions, session, node)
	{
		if (strcmp(session->id, session_id) == 0)
			return session;
	}

	/* If not found and no add requested, just return NULL */
	if (!add)
		return NULL;

	/* Otherwise create and add the session */
	session = calloc(1, sizeof(struct rtsp_client_session));
	if (!session) {
		ULOG_ERRNO("calloc", ENOMEM);
		return NULL;
	}
	session->id = xstrdup(session_id);
	if (!session->id) {
		ULOG_ERRNO("xstrdup", ENOMEM);
		goto error;
	}
	session->client = client;
	session->timer = pomp_timer_new(
		client->loop, rtsp_client_pomp_timer_cb, session);
	if (!session->timer) {
		ULOG_ERRNO("pomp_timer_new", ENOMEM);
		goto error;
	}

	list_add_before(&client->sessions, &session->node);

	ULOGI("client session %s added", session->id);

	return session;
error:
	free(session->id);
	free(session);
	return NULL;
}


int rtsp_client_remove_session(struct rtsp_client *client,
			       const char *session_id,
			       int status_code,
			       int nexist_ok)
{
	struct rtsp_client_session *session;
	int found = 0;
	int status = 0;

	if (!client || !session_id)
		return -EINVAL;

	list_walk_entry_forward(&client->sessions, session, node)
	{
		if (strcmp(session->id, session_id) != 0)
			continue;
		found = 1;
		break;
	}

	if (!found)
		return nexist_ok ? 0 : -ENOENT;

	/* Convert RTSP status code to errno */
	status = rtsp_status_to_errno(status_code);

	ULOGI("client session %s removed", session->id);

	(*client->cbs.session_removed)(
		client, session->id, status, client->cbs_userdata);

	list_del(&session->node);
	pomp_timer_clear(session->timer);
	pomp_timer_destroy(session->timer);
	free(session->content_base);
	free(session->id);
	free(session);
	return 0;
}


void rtsp_client_remove_all_sessions(struct rtsp_client *client)
{
	struct rtsp_client_session *session, *tmp;

	if (!client)
		return;

	list_walk_entry_forward_safe(&client->sessions, session, tmp, node)
	{
		ULOGI("client session %s removed", session->id);
		(*client->cbs.session_removed)(
			client, session->id, 0, client->cbs_userdata);
		list_del(&session->node);
		pomp_timer_clear(session->timer);
		pomp_timer_destroy(session->timer);
		free(session->content_base);
		free(session->id);
		free(session);
	}
}
