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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define ULOG_TAG rtsp_client_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_client_test);

#include <librtsp.h>
#include <libsdp.h>
#include <libpomp.h>


static int stopping;
struct pomp_loop *loop;


static void sighandler(int signum)
{
	printf("Stopping...\n");
	ULOGI("Stopping...");
	stopping = 1;
	if (loop)
		pomp_loop_wakeup(loop);
	signal(SIGINT, SIG_DFL);
}


static void *loop_thread_func(void *user)
{
	while (!stopping)
		pomp_loop_wait_and_process(loop, -1);
	return NULL;
}


int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS, err, i;
	char *url = NULL, *media_url = NULL;
	struct rtsp_client *client = NULL;
	pthread_t loop_thread;
	int loop_thread_init = 0;
	int server_stream_port = 0, server_control_port = 0;
	char *sdp_str = NULL;
	struct sdp_session *sdp = NULL;

	stopping = 0;
	loop = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <url>\n", argv[0]);
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	media_url = url = argv[1];
	printf("Starting client on URL %s\n", url);

	loop = pomp_loop_new();
	if (!loop) {
		ULOGE("pomp_loop_new() failed");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	client = rtsp_client_new(NULL, loop);
	if (!client) {
		ULOGE("rtsp_client_new() failed");
		ret = EXIT_FAILURE;
		goto cleanup;
	}
	printf("Client is created\n");

	err = pthread_create(&loop_thread, NULL, loop_thread_func, NULL);
	if (err != 0) {
		ULOGE("pthread_join() failed (%d)", err);
		ret = EXIT_FAILURE;
		goto cleanup;
	}
	loop_thread_init = 1;

	signal(SIGINT, sighandler);
	printf("Client is running\n");

	if (!stopping) {
		err = rtsp_client_connect(client, url);
		if (err) {
			ULOGE("rtsp_client_options() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
	}

	if (!stopping) {
		err = rtsp_client_options(client, 2000);
		if (err) {
			ULOGE("rtsp_client_options() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
	}

	if (!stopping) {
		err = rtsp_client_describe(client, &sdp_str, 2000);
		if (err) {
			ULOGE("rtsp_client_describe() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		} else if (!sdp_str) {
			ULOGE("no session description");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
	}

	if ((!stopping) && (sdp_str)) {
		sdp = sdp_description_read(sdp_str);
		if (!sdp) {
			ULOGE("sdp_description_read() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		} else {
			struct sdp_media *media = NULL;
			list_walk_entry_forward(&sdp->medias, media, node) {
				if ((media->type == SDP_MEDIA_TYPE_VIDEO) &&
					(media->control_url)) {
					media_url = media->control_url;
					ULOGI("media url: %s", media_url);
					break;
				}
			}
		}
	}

	if (!stopping) {
		err = rtsp_client_setup(client, media_url, 55004, 55005,
			&server_stream_port, &server_control_port, 2000);
		if (err) {
			ULOGE("rtsp_client_setup() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		} else
			ULOGI("server stream port=%d, server control port=%d",
				server_stream_port, server_control_port);
	}
	printf("Client is set up\n");

	if (!stopping) {
		err = rtsp_client_play(client, 2000);
		if (err) {
			ULOGE("rtsp_client_play() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
	}
	printf("Client is playing\n");

	for (i = 0; (i < 5) && (!stopping); i++)
		sleep(1);

	if (!stopping) {
		err = rtsp_client_teardown(client, 2000);
		if (err) {
			ULOGE("rtsp_client_teardown() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
	}

	printf("Client is stopped\n");

cleanup:
	if (client) {
		err = rtsp_client_disconnect(client, 2000);
		if (err) {
			ULOGE("rtsp_client_options() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
		printf("Client is disconnected\n");
	}

	stopping = 1;
	if (loop)
		pomp_loop_wakeup(loop);

	if (loop_thread_init) {
		err = pthread_join(loop_thread, NULL);
		if (err != 0)
			ULOGE("pthread_join() failed (%d)", err);
	}

	if (client) {
		do {
			err = rtsp_client_destroy(client);
			if ((err != 0) && (err != -EBUSY)) {
				ULOGE("rtsp_client_destroy() failed");
				ret = EXIT_FAILURE;
			}
			usleep(1000);
		} while (err == -EBUSY);
		printf("Client is destroyed\n");
	}

	if (loop) {
		err = pomp_loop_destroy(loop);
		if (err) {
			ULOGE("pomp_loop_destroy() failed");
			ret = EXIT_FAILURE;
		}
	}

	exit(ret);
}
