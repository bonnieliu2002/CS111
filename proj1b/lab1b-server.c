// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

/********************
 *      SERVER      *    
 ********************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <zlib.h>

int sockfd;
z_stream out_stream;
z_stream in_stream;
static int portFlag;
static int shellFlag;
static int compressFlag;

void exitProcedure() {
	if (compressFlag) {
		deflateEnd(&out_stream);
		inflateEnd(&in_stream);
	}
}

int main(int argc, char* argv[]) {
	char* port;
	char* program;
	static struct option long_options[] = {
		{"port", required_argument, &portFlag, 1},
		{"shell", required_argument, &shellFlag, 2},
		{"compress", no_argument, &compressFlag, 3},
		{0, 0, 0, 0}
	};
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		if (c != 0) {
			fprintf(stderr, "must include --port=port/number\n");
			exit(1);
		}
		if (option_index == 0)
			port = optarg;
		else if (option_index == 1)
			program = optarg;
	}
	atexit(exitProcedure);
	if (portFlag != 1) {
		fprintf(stderr, "error: port argument needed; correct usage: --port=portnumber\n");
		exit(1);
	}
	if (shellFlag != 2) {
		fprintf(stderr, "error: shell argument needed; correct usage: --shell=program\n");
		exit(1);
	}
	if (compressFlag) {
		// We have to compress (a.k.a deflate) when sending through the socket
		out_stream.zalloc = Z_NULL;
		out_stream.zfree = Z_NULL;
		out_stream.opaque = Z_NULL;
		if (deflateInit(&out_stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
			fprintf(stderr, "Unable to create compression stream!\n");
			exit(1);
		}
		// We have to decompress (a.k.a inflate) when we receive messages from the socket
		in_stream.zalloc = Z_NULL;
		in_stream.zfree = Z_NULL;
		in_stream.opaque = Z_NULL;
		if (inflateInit(&in_stream) != Z_OK) {
			fprintf(stderr, "Unable to create compression stream!\n");
			exit(1);
		}
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Error while creating socket!\n");
		exit(1);
	}
	struct sockaddr_in address;
	int address_length = sizeof(address);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY; // maps IP address to 127.0.0.1
	address.sin_port = htons(atoi(port));
	
	// bind() assigns address to socket
	if (bind(sockfd, (struct sockaddr*) &address, address_length) < 0) {
		fprintf(stderr, "Error while binding socket!\n");
		exit(1);
	}
	
	// listen() waits for incoming connections
	// on most systems, maximum allowed number of connections in incoming queue is 5
	// that's why we put 5 as our backlog value
	if (listen(sockfd, 5) < 0) {
		fprintf(stderr, "Error while listening!\n");
		exit(1);
	}

	int new_socket = accept(sockfd, (struct sockaddr*) &address, (socklen_t*) &address_length);
	if (new_socket < 0) {
		fprintf(stderr, "Error accepting socket!\n");
		exit(1);
	}

	// read ASCII input from new_socket
	// make buffer size bigger in case system is running slowly and multiple characters accumulate
	const int BUFFER_SIZE = 2048;
	char* buffer = (char*) malloc(BUFFER_SIZE);
	char* carriageReturn = (char*) malloc(2);
	carriageReturn[0] = '\r';
	carriageReturn[1] = '\n';
	int pipeP2C[2];
	int pipeC2P[2];
	if (pipe(pipeP2C) == -1 || pipe(pipeC2P) == -1) {
		perror("pipe() error");
		exit(1);
	}
	int cpid = fork();
	if (cpid < 0) {
		perror("fork() error");
		exit(1);
	}
	else if (cpid == 0) { // child process
		close(pipeP2C[1]); // close unused write end
		close(pipeC2P[0]); // close unused read end
		// file redirection
		if (dup2(pipeP2C[0], 0) < 0) {
			perror("dup2() error");
			exit(1);
		}
		if (dup2(pipeC2P[1], 1) < 0) {
			perror("dup2() error");
			exit(1);
		}
		if (execl(program, "sh", (char*) NULL) == -1) {
			perror("execl() error");
			exit(1);
		}
	}
	else { // parent process
		close(pipeP2C[0]); // close unused read end
		close(pipeC2P[1]); // close unused write end
		struct pollfd fds[2];
		fds[0].fd = new_socket; // read from socket
		fds[1].fd = pipeC2P[0]; // pipe that returns output from shell
		fds[0].events = fds[1].events = POLLIN | POLLHUP | POLLERR;
		while (1) {
			int npoll = poll(fds, 2, -1);
			if (npoll == -1) {
				perror("poll() error");
				exit(1);
			}
			else if (npoll == 0) {
				fprintf(stderr, "timed out");
				exit(1);
			}
			if (fds[0].revents & POLLIN) {
				int nread = read(new_socket, buffer, BUFFER_SIZE);
				if (nread < 0) {
					close(pipeP2C[1]);
					perror("read() error");
					exit(1);
				}
				for (int i = 0; i < nread; i++) {
					if (buffer[i] == 0x0D || buffer[i] == 0x0A) {
						// only write line feed to shell
						if (write(pipeP2C[1], carriageReturn + 1, 1) < 0) {
							perror("write() error");
							exit(1);
						}
					}
					// 0x04 is end of file (can be invoked by ctrl-D)
					else if (buffer[i] == 0x04) {
						close(pipeP2C[1]);
					}
					else if (buffer[i] == 0x03) {
						if (kill(cpid, SIGINT) == -1) {
							perror("kill() error");
							exit(1);
						}
					}
					else {
						if (write(pipeP2C[1], &buffer[i], 1) < 0) {
							perror("write() error");
							exit(1);
						}
					}	
				}
			}
			// there is input from the shell waiting to be processed
			if (fds[1].revents & POLLIN) {
				int nread = read(pipeC2P[0], buffer, BUFFER_SIZE);
				if (nread < 0) {
					perror("read() error");
					exit(1);
				}
				for (int i = 0; i < nread; i++) {
					if (buffer[i] == 0x0A || buffer[i] == 0x0D) {
						if (send(new_socket, carriageReturn + 1, 1, 0) < 0) {
							perror("write() error");
							exit(1);
						}
					}
					else {
						if (send(new_socket, &buffer[i], 1, 0) < 0) {
							perror("send() error");
							exit(1);
						}
					}
				}
			}
			// POLLERR means shell returned error condition
			// POLLHUP means a hangup has occurred, and we can no longer write to a file descriptor
			if (fds[1].revents & (POLLERR | POLLHUP)) {
				int status;
				if (waitpid(cpid, &status, 0) == -1) {
					perror("waitpid() error");
					exit(1);
				}
				fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", 0xff & status, status / 256);
				if (shutdown(new_socket, SHUT_RDWR) != 0) {
					perror("shudown() error");
					exit(1);
				}
				exit(0);
			}
		}
	}
	exit(0);

}
