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

#ifndef _RTSP_SERVER_H_
#define _RTSP_SERVER_H_

#include <rtsp/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Reason which brings a RTSP teardown */
enum rtsp_server_teardown_reason {
	RTSP_SERVER_TEARDOWN_REASON_CLIENT_REQUEST = 0,
	RTSP_SERVER_TEARDOWN_REASON_SESSION_TIMEOUT,
	RTSP_SERVER_TEARDOWN_REASON_FORCED_TEARDOWN,
};


struct rtsp_server;


struct rtsp_server_cbs {
	void (*socket_cb)(int fd, void *userdata);

	void (*describe)(struct rtsp_server *server,
			 const char *server_address,
			 const char *path,
			 void *request_ctx,
			 void *userdata);

	void (*setup)(struct rtsp_server *server,
		      const char *path,
		      const char *session_id,
		      void *request_ctx,
		      void *media_ctx,
		      enum rtsp_delivery delivery,
		      enum rtsp_lower_transport lower_transport,
		      const char *src_address,
		      const char *dst_address,
		      uint16_t dst_stream_port,
		      uint16_t dst_control_port,
		      void *userdata);

	void (*play)(struct rtsp_server *server,
		     const char *session_id,
		     void *request_ctx,
		     void *media_ctx,
		     const struct rtsp_range *range,
		     float scale,
		     void *stream_userdata,
		     void *userdata);

	void (*pause)(struct rtsp_server *server,
		      const char *session_id,
		      void *request_ctx,
		      void *media_ctx,
		      const struct rtsp_range *range,
		      void *stream_userdata,
		      void *userdata);

	void (*teardown)(struct rtsp_server *server,
			 const char *session_id,
			 enum rtsp_server_teardown_reason reason,
			 void *request_ctx,
			 void *media_ctx,
			 void *stream_userdata,
			 void *userdata);


	void (*request_timeout)(struct rtsp_server *server,
				void *request_ctx,
				enum rtsp_method_type method,
				void *userdata);
};


RTSP_API int rtsp_server_new(const char *software_name,
			     uint16_t port,
			     int reply_timeout_ms,
			     int session_timeout_ms,
			     struct pomp_loop *loop,
			     const struct rtsp_server_cbs *cbs,
			     void *userdata,
			     struct rtsp_server **ret_obj);


RTSP_API int rtsp_server_destroy(struct rtsp_server *server);


RTSP_API int rtsp_server_reply_to_describe(struct rtsp_server *server,
					   void *request_ctx,
					   int status,
					   char *session_description);


RTSP_API int rtsp_server_reply_to_setup(struct rtsp_server *server,
					void *request_ctx,
					void *media_ctx,
					int status,
					uint16_t src_stream_port,
					uint16_t src_control_port,
					int ssrc_valid,
					uint32_t ssrc,
					void *stream_userdata);


RTSP_API int rtsp_server_reply_to_play(struct rtsp_server *server,
				       void *request_ctx,
				       void *media_ctx,
				       int status,
				       struct rtsp_range *range,
				       float scale,
				       int seq_valid,
				       uint16_t seq,
				       int rtptime_valid,
				       uint32_t rtptime);


RTSP_API int rtsp_server_reply_to_pause(struct rtsp_server *server,
					void *request_ctx,
					void *media_ctx,
					int status,
					struct rtsp_range *range);


RTSP_API int rtsp_server_reply_to_teardown(struct rtsp_server *server,
					   void *request_ctx,
					   void *media_ctx,
					   int status);


RTSP_API int rtsp_server_announce(struct rtsp_server *server,
				  char *uri,
				  char *session_description);


RTSP_API int rtsp_server_force_session_teardown(struct rtsp_server *server,
						const char *session_id);


RTSP_API const char *
rtsp_server_teardown_reason_str(enum rtsp_server_teardown_reason val);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTSP_SERVER_H_ */
