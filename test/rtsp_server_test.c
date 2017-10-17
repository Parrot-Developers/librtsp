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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#define ULOG_TAG rtsp_server_test
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_server_test);

#include <librtsp.h>
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


int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS, err;
	int port = 0;
	struct rtsp_server *server = NULL;

	stopping = 0;
	loop = NULL;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	port = atoi(argv[1]);
	printf("Starting server on port %d\n", port);

	loop = pomp_loop_new();
	if (!loop) {
		ULOGE("pomp_loop_new() failed");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	server = rtsp_server_new(port, loop);
	if (!server) {
		ULOGE("rtsp_server_new() failed");
		ret = EXIT_FAILURE;
		goto cleanup;
	}

	signal(SIGINT, sighandler);
	printf("Server listening\n");

	while (!stopping)
		pomp_loop_wait_and_process(loop, -1);

	printf("Server stopped\n");

cleanup:
	if (server) {
		err = rtsp_server_destroy(server);
		if (err) {
			ULOGE("rtsp_server_destroy() failed");
			ret = EXIT_FAILURE;
		}
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
