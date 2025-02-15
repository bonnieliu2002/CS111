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
#include <zlib.h>

struct termios initMode;
int sockfd;

static int portFlag;
static int logFlag;
static int compressFlag;
FILE* log_file;
z_stream out_stream; // compress from keyboard to shell
z_stream in_stream; // decompress from shell to display

void exitProcedure() {
	if (logFlag)
		fclose(log_file);
	if (compressFlag) {
		inflateEnd(&in_stream);
		deflateEnd(&out_stream);
	}
	close(sockfd);
	if (tcsetattr(0, TCSANOW, &initMode) != 0) {
		perror("tcsetattr() error");
		exit(1);
	}
}

int main(int argc, char* argv[]) {
	// use getopt_long to process argument
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
	if (logFlag) {
		log_file = fopen(file, "w");
	}
	if (compressFlag) {
		// We want to deflate out_stream so that it compresses before sending it over to the server
		out_stream.zalloc = Z_NULL;
		out_stream.zfree = Z_NULL;
		out_stream.opaque = Z_NULL;
		if (deflateInit(&out_stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
			fprintf(stderr, "Unable to create compression stream!\n");
			exit(1);
		}
		// We want to inflate in_stream so that it decompresses when we receive it from the server
		in_stream.zalloc = Z_NULL;
		in_stream.zfree = Z_NULL;
		in_stream.opaque = Z_NULL;
		if (inflateInit(&in_stream) != Z_OK) {
			fprintf(stderr, "Unable to create decompression stream!\n");
			exit(1);
		}

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

//	fprintf(stderr, "Connected successfully!");

	struct termios copyOfInit = initMode;
	copyOfInit.c_iflag = ISTRIP;
	copyOfInit.c_oflag = 0;
	copyOfInit.c_lflag = 0;
	if (tcsetattr(0, TCSANOW, &copyOfInit) != 0) {
		perror("tcsetsttr() error");
		exit(1);
	}
	atexit(exitProcedure);

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
			int nwrite;
			// write to stdout so users can see what they type
			for (int i = 0; i < nread; i++) {
				if (buffer[i] == '\r' || buffer[i] == '\n') {
					nwrite = write(1, carriageReturn, 2);
				}
				else if (buffer[i] == 0x03) {
					printf("^C\r\n");
				}
				else if (buffer[i] == 0x04) {
					printf("^D\r\n");
				}
				else {
					nwrite = write(1, &buffer[i], 1);
				}
			}
			if (nwrite < 0) {
				perror("write() error");
				exit(1);
			}
			int nsend;
			if (compressFlag) {
				char out[BUFFER_SIZE];
				out_stream.avail_in = nread;
				out_stream.next_in = (unsigned char*) buffer;
				out_stream.avail_out = BUFFER_SIZE;
				out_stream.next_out = (unsigned char*) out;
				do {
					deflate(&out_stream, Z_SYNC_FLUSH);
				} while (out_stream.avail_in > 0);
				nsend = send(sockfd, out, BUFFER_SIZE - out_stream.avail_out, 0);
				if (logFlag) {
					fprintf(log_file, "SENT %d bytes: %s\n", BUFFER_SIZE - out_stream.avail_out, out);
				}
			}
			else {
				nsend = send(sockfd, buffer, nread, 0);
				if (logFlag) {
					fprintf(log_file, "SENT %d bytes: ", nread);
					for (int i = 0; i < nread; i++) {
						fprintf(log_file, "%.1s", buffer + i);
					}
					fprintf(log_file, "\n");
				}
			}
			if (nsend < 0) {
				perror("send() error");
				exit(1);
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
			if (logFlag) {
				fprintf(log_file, "RECEIVED %d bytes: ", nread);
				for (int i = 0;  i < nread; i++) {
					fprintf(log_file, "%.1s", buffer + i);
				}
				fprintf(log_file, "\n");
			}
			if (compressFlag) {
				char out[BUFFER_SIZE];
				in_stream.avail_in = nread;
				in_stream.next_in = (unsigned char*) buffer;
				in_stream.avail_out = BUFFER_SIZE;
				in_stream.next_out = (unsigned char*) out;
				do {
					inflate(&in_stream, Z_SYNC_FLUSH);
				} while (in_stream.avail_in > 0);
				for (int i = 0; i < BUFFER_SIZE - (int) in_stream.avail_out; i++) {
					if (out[i] == '\r' || out[i] == '\n') {
						write(1, carriageReturn, 2);
					}
					else {
						write(1, &out[i], 1);
					}
				}
			}
			else {
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
