#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "comm.h"
#include "util.h"

/* -------------------------Main function for the client ----------------------*/
void main(int argc, char * argv[]) {

	int pipe_user_reading_from_server[2], pipe_user_writing_to_server[2];
	// You will need to get user name as a parameter, argv[1].
	// printf("working\n");
	if(connect_to_server("STH", argv[1], pipe_user_reading_from_server, pipe_user_writing_to_server) == -1) {
		// printf("failing\n");
		exit(-1);
	}

	/* -------------- YOUR CODE STARTS HERE -----------------------------------*/


	// poll pipe retrieved and print it to sdiout

	// Poll stdin (input from the terminal) and send it to server (child process) via pipe

	print_prompt(argv[1]);

	char b[MAX_MSG];
	memset(b, '\0', sizeof(b));
	fcntl(pipe_user_reading_from_server[0], F_SETFL, O_NONBLOCK);
	fcntl(pipe_user_writing_to_server[0], F_SETFL, O_NONBLOCK);
	fcntl(0, F_SETFL, O_NONBLOCK);
	close(pipe_user_reading_from_server[1]);
	close(pipe_user_writing_to_server[0]);

	while (1) {
		memset(b, '\0', sizeof(b));
		int n = read(pipe_user_reading_from_server[0], b, MAX_MSG);

		if(n > 0) {
			if (strcmp(b, "User seems to be dead") == 0) {
				printf("%s\n", b);
				exit(0);
			}
			printf("\n%s \n", b);
			print_prompt(argv[1]);
			memset(b, '\0', sizeof(b));
		} else if (n == 0) {
			printf("The server process seems dead\n");
			close(pipe_user_writing_to_server[1]);
			close(pipe_user_reading_from_server[0]);
			exit(0);
		}

		int nbytes = read(0, b, MAX_MSG);
		if(nbytes > 0) {
			write(pipe_user_writing_to_server[1], b, sizeof(b));
			print_prompt(argv[1]);
			memset(b, '\0', sizeof(b));
		} else if (nbytes == 0) {
			close(pipe_user_writing_to_server[1]);
			close(pipe_user_reading_from_server[0]);
			exit(0);
		}
		usleep(50);
	}
	/* -------------- YOUR CODE ENDS HERE -----------------------------------*/
}

/*--------------------------End of main for the client --------------------------*/
