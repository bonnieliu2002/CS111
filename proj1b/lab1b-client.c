// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

/*****************
 *     CLIENT    *
 *****************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <arpa/inet.h>

struct termios initMode;
int sockfd;

void restoreNormal() {
	close(sockfd);
	if (tcsetattr(0, TCSANOW, &initMode) != 0) {
		perror("tcsetattr() error");
		exit(1);
	}
}

int main(int argc, char* argv[]) {
	// use getopt_long to process argument
	static int portFlag;
	static int logFlag;
	static int compressFlag;
	char* port;
	char* file;
	static struct option long_options[] = {
		{"port", required_argument, &portFlag, 1},
		{"log", required_argument, &logFlag, 2},
		{"compress", no_argument, &compressFlag, 3},
		{0, 0, 0, 0}
	};
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		if (c != 0) {
			fprintf(stderr, "must include --port=portnumber\ncan also include --log=filename or --compress\n");
			exit(1);
		}
		if (option_index == 0)
			port = optarg;
		else if (option_index == 1)
			file = optarg;
	}
	if (portFlag != 1) {
		fprintf(stderr, "error: port argument needed; correct usage: --port=portnumber\n");
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Error while creating socket!\n");
		exit(1);
	}
	struct sockaddr_in server_address;
	int address_length = sizeof(server_address);
	memset(&server_address, '0', address_length);
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(atoi(port));
	if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
		fprintf(stderr, "Invalid address!\n");
		exit(1);
	}
	if (connect(sockfd, (struct sockaddr*) &server_address, address_length) < 0) {
		fprintf(stderr, "Error while connecting!\n");
		exit(1);
	}
	
	if (tcgetattr(0, &initMode) != 0) {
		perror("tcgetattr() error");
		exit(1);
	}

	struct termios copyOfInit = initMode;
	copyOfInit.c_iflag = ISTRIP;
	copyOfInit.c_oflag = 0;
	copyOfInit.c_lflag = 0;
	if (tcsetattr(0, TCSANOW, &copyOfInit) != 0) {
		perror("tcsetsttr() error");
		exit(1);
	}
	atexit(restoreNormal);

	const int BUFFER_SIZE = 2048;
	char* buffer = (char*) malloc(BUFFER_SIZE);
	char* carriageReturn = (char*) malloc(2);
	carriageReturn[0] = '\r';
	carriageReturn[1] = '\n';
	struct pollfd fds[2];
	fds[0].fd = 0; // keyboard input
	fds[1].fd = sockfd; // input from the socket
	fds[0].events = fds[1].events = POLLIN | POLLHUP | POLLERR;
	while (1) {
		int npoll = poll(fds, 2, -1);
		if (npoll == -1) {
			perror("poll() error");
			exit(1);
		}
		if (npoll == 0) {
			fprintf(stderr, "timed out");
			exit(1);
		}
		if (fds[0].revents & POLLIN) {
			int nread = read(0, buffer, BUFFER_SIZE);
			if (nread < 0) {
				perror("read() error");
				exit(1);
			}
			for (int i = 0; i < nread; i++) {
				int nsend;
				int nwrite;
				if (buffer[i] == '\r' || buffer[i] == '\n') {
					nsend = send(sockfd, carriageReturn, 2, 0);
					nwrite = write(1, carriageReturn, 2);
				}
				else {
					nsend = send(sockfd, &buffer[i], 1, 0);
					if (buffer[i] == 0x03) {
						printf("^C\r\n");
					}
					else if (buffer[i] == 0x04) {
						printf("^D\r\n");
					}
					else
						nwrite = write(1, &buffer[i], 1);
				}
				if (nsend < 0) {
					perror("send() error");
					exit(1);
				}
				if (nwrite < 0) {
					perror("write() error");
					exit(1);
				}
			}
		}
		if (fds[1].revents & POLLIN) {
			int nread = read(sockfd, buffer, BUFFER_SIZE);
			// 0 indicates EOF --> socket is closed
			if (nread == 0) {
				exit(0);
			}
			else if (nread < 0) {
				perror("read() error");
				exit(1);
			}
			for (int i = 0; i < nread; i++) {
				int nwrite;
				if (buffer[i] == '\r' || buffer[i] == '\n') {
					nwrite = write(1, carriageReturn, 2);
				}
				else {
					nwrite = write(1, &buffer[i], 1);
				}
				if (nwrite < 0) {
					perror("write() error");
					exit(1);
				}
			}
		}
		if (fds[1].revents & POLLERR) {
			exit(1);
		}
		if (fds[1].revents & POLLHUP) {
			exit(0);
		}
	}
	exit(0);
}
