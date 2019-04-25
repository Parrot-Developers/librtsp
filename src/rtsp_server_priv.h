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

#ifndef _RTSP_SERVER_PRIV_H_
#define _RTSP_SERVER_PRIV_H_

#include "rtsp_priv.h"


#define RTSP_SERVER_DEFAULT_SOFTWARE_NAME "librtsp_server"
#define RTSP_SERVER_SESSION_ID_LENGTH 8
#define RTSP_SERVER_DEFAULT_REPLY_TIMEOUT_MS 1000
#define RTSP_SERVER_DEFAULT_SESSION_TIMEOUT_MS 60000


struct rtsp_server_session_media {
	struct rtsp_server_session *session;
	char *path;
	void *userdata;

	struct list_node node;
};


struct rtsp_server_session {
	struct rtsp_server *server;
	char *session_id;
	unsigned int timeout_ms;
	struct pomp_timer *timer;
	int playing;
	struct rtsp_range range;
	float scale;

	/* Operation in progress */
	enum rtsp_method_type op_in_progress;

	/* Medias */
	unsigned int media_count;
	struct list_node medias;

	struct list_node node;
};


struct rtsp_server_pending_request_media {
	struct rtsp_server_session_media *media;
	int replied;

	struct list_node node;
};


struct rtsp_server_pending_request {
	struct pomp_conn *conn;
	struct rtsp_request_header request_header;
	struct rtsp_response_header response_header;
	uint64_t timeout;
	int request_first_reply;
	int in_callback;
	int replied;

	/* Medias */
	unsigned int media_count;
	struct list_node medias;

	struct list_node node;
};


struct rtsp_server {
	struct sockaddr_in listen_addr_in;
	struct pomp_loop *loop;
	struct pomp_ctx *pomp;
	struct pomp_timer *timer;
	unsigned int max_msg_size;
	struct rtsp_server_cbs cbs;
	void *cbs_userdata;

	char *software_name;

	int pending_content_length;
	int reply_timeout_ms;

	/* Sessions */
	int session_timeout_ms;
	unsigned int session_count;
	struct list_node sessions;

	/* Pending requests */
	struct pomp_buffer *request_buf;
	unsigned int pending_request_count;
	struct list_node pending_requests;

	/* Announce requests */
	unsigned int cseq;

	struct rtsp_message_parser_ctx parser_ctx;
};


struct rtsp_server_session *rtsp_server_session_add(struct rtsp_server *server,
						    unsigned int timeout_ms);


int rtsp_server_session_remove(struct rtsp_server *server,
			       struct rtsp_server_session *session);


void rtsp_server_session_remove_idle(void *userdata);


int rtsp_server_session_reset_timeout(struct rtsp_server_session *session);


struct rtsp_server_session *rtsp_server_session_find(struct rtsp_server *server,
						     const char *session_id);


struct rtsp_server_session_media *
rtsp_server_session_media_add(struct rtsp_server *server,
			      struct rtsp_server_session *session,
			      const char *path);


int rtsp_server_session_media_remove(struct rtsp_server *server,
				     struct rtsp_server_session *session,
				     struct rtsp_server_session_media *media);


struct rtsp_server_session_media *
rtsp_server_session_media_find(struct rtsp_server *server,
			       struct rtsp_server_session *session,
			       const char *path);


struct rtsp_server_pending_request *
rtsp_server_pending_request_add(struct rtsp_server *server,
				struct pomp_conn *conn,
				unsigned int timeout);


int rtsp_server_pending_request_remove(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request);


int rtsp_server_pending_request_find(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request);


struct rtsp_server_pending_request_media *rtsp_server_pending_request_media_add(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request,
	struct rtsp_server_session_media *media);


int rtsp_server_pending_request_media_remove(
	struct rtsp_server *server,
	struct rtsp_server_pending_request *request,
	struct rtsp_server_pending_request_media *media);


void rtsp_server_session_timer_cb(struct pomp_timer *timer, void *userdata);


#endif /* !_RTSP_SERVER_PRIV_H_ */
