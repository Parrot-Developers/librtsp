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

#ifndef _RTSP_CLIENT_H_
#define _RTSP_CLIENT_H_

#include <rtsp/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define RTSP_CLIENT_DEFAULT_RESP_TIMEOUT_MS 4000


struct rtsp_client;


enum rtsp_client_conn_state {
	RTSP_CLIENT_CONN_STATE_DISCONNECTED = 0,
	RTSP_CLIENT_CONN_STATE_CONNECTING,
	RTSP_CLIENT_CONN_STATE_CONNECTED,
	RTSP_CLIENT_CONN_STATE_DISCONNECTING,
};


/* Request status */
enum rtsp_client_req_status {
	/* Request succeeded */
	RTSP_CLIENT_REQ_STATUS_OK = 0,
	/* Request canceled by the user */
	RTSP_CLIENT_REQ_STATUS_CANCELED,
	/* Request failed */
	RTSP_CLIENT_REQ_STATUS_FAILED,
	/* Request aborted by disconnection, no more requests can be sent */
	RTSP_CLIENT_REQ_STATUS_ABORTED,
	/* No response to request received */
	RTSP_CLIENT_REQ_STATUS_TIMEOUT,
};


struct rtsp_client_cbs {
	void (*socket_cb)(int fd, void *userdata);

	/* Called only for states CONNECTED and DISCONNECTED */
	void (*connection_state)(struct rtsp_client *client,
				 enum rtsp_client_conn_state state,
				 void *userdata);

	void (*session_removed)(struct rtsp_client *client,
				const char *session_id,
				int status,
				void *userdata);

	void (*options_resp)(struct rtsp_client *client,
			     enum rtsp_client_req_status req_status,
			     int status,
			     uint32_t methods,
			     void *userdata,
			     void *req_userdata);

	void (*describe_resp)(struct rtsp_client *client,
			      enum rtsp_client_req_status req_status,
			      int status,
			      const char *content_base,
			      const char *sdp,
			      void *userdata,
			      void *req_userdata);

	void (*setup_resp)(struct rtsp_client *client,
			   const char *session_id,
			   enum rtsp_client_req_status req_status,
			   int status,
			   uint16_t src_stream_port,
			   uint16_t src_control_port,
			   int ssrc_valid,
			   uint32_t ssrc,
			   void *userdata,
			   void *req_userdata);

	void (*play_resp)(struct rtsp_client *client,
			  const char *session_id,
			  enum rtsp_client_req_status req_status,
			  int status,
			  const struct rtsp_range *range,
			  float scale,
			  int seq_valid,
			  uint16_t seq,
			  int rtptime_valid,
			  uint32_t rtptime,
			  void *userdata,
			  void *req_userdata);

	void (*pause_resp)(struct rtsp_client *client,
			   const char *session_id,
			   enum rtsp_client_req_status req_status,
			   int status,
			   const struct rtsp_range *range,
			   void *userdata,
			   void *req_userdata);

	void (*teardown_resp)(struct rtsp_client *client,
			      const char *session_id,
			      enum rtsp_client_req_status req_status,
			      int status,
			      void *userdata,
			      void *req_userdata);

	void (*announce)(struct rtsp_client *client,
			 const char *content_base,
			 const char *sdp,
			 void *userdata);
};


RTSP_API int rtsp_client_new(struct pomp_loop *loop,
			     const char *software_name,
			     const struct rtsp_client_cbs *cbs,
			     void *userdata,
			     struct rtsp_client **ret_obj);


RTSP_API int rtsp_client_destroy(struct rtsp_client *client);


RTSP_API int rtsp_client_connect(struct rtsp_client *client, const char *addr);


RTSP_API int rtsp_client_disconnect(struct rtsp_client *client);


RTSP_API int rtsp_client_options(struct rtsp_client *client,
				 void *req_userdata,
				 unsigned int timeout_ms);


RTSP_API int rtsp_client_describe(struct rtsp_client *client,
				  const char *path,
				  void *req_userdata,
				  unsigned int timeout_ms);


RTSP_API int rtsp_client_setup(struct rtsp_client *client,
			       const char *content_base,
			       const char *resource_url,
			       const char *session_id,
			       enum rtsp_delivery delivery,
			       enum rtsp_lower_transport lower_transport,
			       uint16_t dst_stream_port,
			       uint16_t dst_control_port,
			       void *req_userdata,
			       unsigned int timeout_ms);


RTSP_API int rtsp_client_play(struct rtsp_client *client,
			      const char *session_id,
			      const struct rtsp_range *range,
			      float scale,
			      void *req_userdata,
			      unsigned int timeout_ms);


RTSP_API int rtsp_client_pause(struct rtsp_client *client,
			       const char *session_id,
			       const struct rtsp_range *range,
			       void *req_userdata,
			       unsigned int timeout_ms);


RTSP_API int rtsp_client_teardown(struct rtsp_client *client,
				  const char *session_id,
				  void *req_userdata,
				  unsigned int timeout_ms);


RTSP_API int rtsp_client_cancel(struct rtsp_client *client);


RTSP_API const char *
rtsp_client_conn_state_str(enum rtsp_client_conn_state val);


RTSP_API const char *
rtsp_client_req_status_str(enum rtsp_client_req_status val);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTSP_CLIENT_H_ */
