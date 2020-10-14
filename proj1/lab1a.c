// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <poll.h>

int main(int argc, char* argv[]) {
	
	// use getopt_long to process argument
	static int shellFlag;
	char* program = NULL;
	static struct option long_options[] = {
		{"shell", required_argument, &shellFlag, 1},
		{0, 0, 0, 0}
	};

	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		if (c != 0) {
			printf("error: unrecognized argument; correct usage: --shell=program\n");
			exit(1);
		}
		if (option_index == 0)
			program = optarg;
	}

	struct termios initMode;
	struct termios copyOfInit;
	
	// store initial terminal modes in struct termios initMode
	if (tcgetattr(0, &initMode) != 0) {
		perror("tcgetattr() error");
		exit(1);
	}

	// make a copy of initMode and modify flags
	copyOfInit = initMode;
	copyOfInit.c_iflag = ISTRIP; // only lower 7 bits
	copyOfInit.c_oflag = 0; // don't process output flags
	copyOfInit.c_lflag = 0; // don't process local flags
	if (tcsetattr(0, TCSANOW, &copyOfInit) != 0) {
		perror("tcsetsttr() error");
		exit(1);
	}

	// read ASCII input from keyboard into buffer
	// make buffer size bigger in case system is running slowly and multiple characters accumulate
	const int BUFFER_SIZE = 256;
	char* buffer = (char*) malloc(BUFFER_SIZE);
	char* carriageReturn = (char*) malloc(2);
	carriageReturn[0] = '\r';
	carriageReturn[1] = '\n';

	if (shellFlag) {
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
			dup2(pipeP2C[0], 0);
			dup2(pipeC2P[1], 1);
			if (execl(program, "sh", (char*) NULL) == -1) {
				perror("execl() error");
				exit(1);
			}	
		}
		else { // parent process
			close(pipeP2C[0]); // close unused read end
			close(pipeC2P[1]); // close unused write end
			struct pollfd fds[2];
			fds[0].fd = 0; // keyboard (stdin)
			fds[1].fd = pipeC2P[0]; // pipe that returns output from shell
			fds[0].events = fds[1].events = POLLIN | POLLHUP | POLLERR; // wait for either input (POLLIN) or error (POLLHUP, POLLERR)
			while (1) {
/*				if (signal(SIGPIPE, s) == SIG_ERR) {
					perror("signal() error");
				}*/
				int npoll = poll(fds, 2, -1);
				if (npoll == -1) {
					perror("poll() error");
					break;
				}
				else if (npoll == 0) {
					printf("timed out");
					exit(1);
				}
				if (fds[0].revents & POLLIN) {
					int nread = read(0, buffer, BUFFER_SIZE);
					if (nread < 0) {
						perror("read() error");
						exit(1);
					}
					// loop through each read-in character, and print them one by one
					for (int i = 0; i < nread; i++) {
						// 0x0D is carriage return ('\r')
						// 0x0A is line feed ('\n')
						// if either one is encountered
						if (buffer[i] == 0x0D || buffer[i] == 0x0A) {
							// echo both to stdout
							int nwrite = write(1, carriageReturn, 2);
							if (nwrite < 0) {
								perror("write() error");
								exit(1);
							}
							// only write line feed to shell
							nwrite = write(pipeP2C[1], carriageReturn + 1, 1);
							if (nwrite < 0) {
								perror("write() error");
								exit(1);
							}
						}
						// 0x04 is end of file (can be invoked by ctrl-D)
						else if (buffer[i] == 0x04) {
							close(pipeP2C[1]);
							close(pipeP2C[0]);
						}
						else if (buffer[i] == 0x03) {
							printf("^C\n");
							if (kill(cpid, SIGINT) == -1) {
								perror("kill() error");
								exit(1);
							}
						}
						else {
							int nwrite = write(1, buffer, 1);
							if (nwrite < 0) {
								perror("write() error");
								exit(1);
							}
							nwrite = write(pipeP2C[1], buffer, 1);
							if (nwrite < 0) {
								perror("write() error");
								exit(1);
							}
						}
					}
				}
				if (fds[1].revents & POLLIN) {
					int nread = read(pipeC2P[0], buffer, BUFFER_SIZE);
					if (nread < 0) {
						perror("read() error");
						exit(1);
					}
					for (int i = 0; i < nread; i++) {
						if (buffer[i] == 0x0A || buffer[i] == 0x0D) {
							int nwrite = write(1, carriageReturn, 2);
							if (nwrite < 0) {
								perror("write() error");
								exit(1);
							}
						}
						else {
							int nwrite = write(1, &buffer[i], 1);
							if (nwrite < 0) {
								perror("write() error");
								exit(1);
							}
						}
					}
				}
				if (fds[1].revents & (POLLERR | POLLHUP)) {
						// reset terminal to initial modes
					 	if (tcsetattr(0, TCSANOW, &initMode) != 0) {
							perror("tcsetattr() error");
							exit(1);
						}
						int status;
						if (waitpid(cpid, &status, 0) == -1) {
							perror("waitpid() error");
							exit(1);
						}
						fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", 0xff & status, status / 256);
						exit(0);
				}
			}			
		}
	}
	// shell flag was NOT passed
	else {
		while (1) {
			int nread = read(0, buffer, BUFFER_SIZE);
			if (nread < 0) {
				perror("read() error");
				exit(1);
			}
			for (int i = 0; i < nread; i++) {
				if (buffer[i] == 0x0D || buffer[i] == 0x0A) {
					int nwrite = write(1, carriageReturn, 2);
					if (nwrite < 0) {
						perror("write() error");
						exit(1);
					}
				}
				else if (buffer[i] == 0x04) {
					if (tcsetattr(0, TCSANOW, &initMode) != 0) {
						perror("tcsetattr() error");
						exit(1);
					}
					exit(0);
				}
				else {
					int nwrite = write(1, buffer, 1);
					if (nwrite < 0) {
						perror("write() error");
						exit(1);
					}
				}
			}
		}	
	}

	// reset terminal to initial modes
	if (tcsetattr(0, TCSANOW, &initMode) != 0) {
		perror("tcsetattr() error");
		exit(1);
	}

	// exit successfully
	exit(0);
}
