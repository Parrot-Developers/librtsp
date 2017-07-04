/**
 * @file rtsp_client_test.c
 * @brief Real Time Streaming Protocol library - client test program
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define ULOG_TAG rtsp_client_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_client_test);

#include <librtsp.h>
#include <libpomp.h>


static int stopping = 0;
struct pomp_loop *loop = NULL;


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
	while(!stopping)
		pomp_loop_wait_and_process(loop, -1);
	return NULL;
}


int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS, err;
	char *url = NULL;
	struct rtsp_client *client = NULL;
	pthread_t loop_thread;
	int loop_thread_init = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <url>\n", argv[0]);
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	url = argv[1];
	printf("Starting client on URL %s\n", url);

	loop = pomp_loop_new();
	if (!loop) {
		ULOGE("pomp_loop_new() failed");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	client = rtsp_client_new(argv[1], loop);
	if (!client) {
		ULOGE("rtsp_client_new() failed");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	err = pthread_create(&loop_thread, NULL, loop_thread_func, NULL);
	if (err != 0) {
		ULOGE("pthread_join() failed (%d)", err);
		ret = EXIT_FAILURE;
		goto cleanup;
	}
	loop_thread_init = 1;

	signal(SIGINT, sighandler);
	printf("Client running\n");

	while (!stopping) {
		sleep(1);
		err = rtsp_client_send_request(client);
		if (err) {
			ULOGE("rtsp_client_send_request() failed");
			ret = EXIT_FAILURE;
			stopping = 1;
		}
	}

	printf("Client stopped\n");

cleanup:
	if (client) {
		err = rtsp_client_destroy(client);
		if (err) {
			ULOGE("rtsp_client_destroy() failed");
			ret = EXIT_FAILURE;
		}
	}

	stopping = 1;
	if (loop)
		pomp_loop_wakeup(loop);

	if (loop_thread_init) {
		err = pthread_join(loop_thread, NULL);
		if (err != 0)
			ULOGE("pthread_join() failed (%d)", err);
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
