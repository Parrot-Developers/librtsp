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

#ifndef _RTSP_CLIENT_PRIV_H_
#define _RTSP_CLIENT_PRIV_H_

#include "rtsp_priv.h"


#define RTSP_CLIENT_DEFAULT_SOFTWARE_NAME "librtsp_client"
#define RTSP_CLIENT_MAX_FAILED_KEEP_ALIVE 5


enum rtsp_client_state {
	RTSP_CLIENT_STATE_IDLE = 0,
	RTSP_CLIENT_STATE_OPTIONS_WAITING_REPLY,
	RTSP_CLIENT_STATE_OPTIONS_OK,
	RTSP_CLIENT_STATE_DESCRIBE_WAITING_REPLY,
	RTSP_CLIENT_STATE_DESCRIBE_OK,
	RTSP_CLIENT_STATE_SETUP_WAITING_REPLY,
	RTSP_CLIENT_STATE_SETUP_OK,
	RTSP_CLIENT_STATE_PLAY_WAITING_REPLY,
	RTSP_CLIENT_STATE_PLAY_OK,
	RTSP_CLIENT_STATE_PAUSE_WAITING_REPLY,
	RTSP_CLIENT_STATE_PAUSE_OK,
	RTSP_CLIENT_STATE_TEARDOWN_WAITING_REPLY,
	RTSP_CLIENT_STATE_TEARDOWN_OK,
	RTSP_CLIENT_STATE_KEEPALIVE_WAITING_REPLY,
};


struct rtsp_client_session {
	char *id;
	struct pomp_timer *timer;
	struct rtsp_client *client;
	char *content_base;
	unsigned int timeout_ms;
	unsigned int failed_keep_alive;
	int keep_alive_in_progress;
	int internal_teardown;
	struct list_node node;
};


struct rtsp_client {
	struct pomp_loop *loop;
	struct pomp_ctx *ctx;
	struct rtsp_client_cbs cbs;
	void *cbs_userdata;
	char *software_name;
	uint16_t port;

	/* States */
	enum rtsp_client_conn_state conn_state;
	int user_connecting;
	char *addr;
	unsigned int cseq;
	uint32_t methods_allowed;
	struct list_node sessions;

	struct {
		struct rtsp_request_header header;
		struct pomp_buffer *buf;
		int is_pending;
		char *uri;
		char *content_base;
		void *userdata;
		struct pomp_timer *timer;
	} request;

	struct {
		struct pomp_buffer *buf;
	} response;

	struct rtsp_message_parser_ctx parser_ctx;
};


struct rtsp_client_session *rtsp_client_get_session(struct rtsp_client *client,
						    const char *session_id,
						    int add);


int rtsp_client_remove_session_internal(struct rtsp_client *client,
					const char *session_id,
					int status_code,
					int nexist_ok);


void rtsp_client_remove_all_sessions(struct rtsp_client *client);


void rtsp_client_pomp_timer_cb(struct pomp_timer *timer, void *userdata);


#endif /* !_RTSP_CLIENT_PRIV_H_ */
