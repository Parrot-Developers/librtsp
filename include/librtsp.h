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

#ifndef _LIBRTSP_H_
#define _LIBRTSP_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <inttypes.h>
#include <libpomp.h>


struct rtsp_server;
struct rtsp_client;


struct rtsp_server *rtsp_server_new(
	int port,
	struct pomp_loop *loop);


int rtsp_server_destroy(
	struct rtsp_server *server);


struct rtsp_client *rtsp_client_new(
	const char *user_agent,
	struct pomp_loop *loop);


int rtsp_client_destroy(
	struct rtsp_client *client);


int rtsp_client_connect(
	struct rtsp_client *client,
	const char *url);


int rtsp_client_disconnect(
	struct rtsp_client *client,
	unsigned int timeout_ms);


int rtsp_client_options(
	struct rtsp_client *client,
	unsigned int timeout_ms);


int rtsp_client_describe(
	struct rtsp_client *client,
	char **session_description,
	unsigned int timeout_ms);


int rtsp_client_setup(
	struct rtsp_client *client,
	const char *resource_url,
	int client_stream_port,
	int client_control_port,
	int *server_stream_port,
	int *server_control_port,
	unsigned int timeout_ms);


int rtsp_client_play(
	struct rtsp_client *client,
	unsigned int timeout_ms);


int rtsp_client_teardown(
	struct rtsp_client *client,
	unsigned int timeout_ms);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_LIBRTSP_H_ */
