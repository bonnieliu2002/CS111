// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

/******************
 *      TLS       *
 ******************/

#include <signal.h>
#include <mraa/gpio.h>
#include <mraa/aio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <poll.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// provided in TA slides
const int B = 4275;
const int R0 = 100000;
const int BUFFER_SIZE = 2048;

sig_atomic_t volatile runFlag = 1;
int shouldReport = 1;
int sockfd;
SSL* sslClient;

void printTime(FILE* fp);

void doWhenInterrupted(int sig) {
	if (sig == SIGINT)
		runFlag = 0;
}

void printCommand(FILE* fp, char* command) {
	if (fp) {
		fprintf(fp, "%s\n", command);
	}
}

float getTemperature(int sensorInput, char scale) {
	float R = 1023.0 / sensorInput - 1.0;
	R *= R0;
	float cTemperature = 1.0/(log10(R/R0)/B+1/298.15)-273.15;
	if (scale == 'C')
		return cTemperature;
	return cTemperature * 1.8 + 32;
}

void sample(mraa_aio_context* sensor, FILE* fp, char scale) {
	if (shouldReport) {
		uint16_t value = mraa_aio_read(*sensor);
		char buf[50];
		time_t rawtime;
		struct tm* info;
		time(&rawtime);
		info = localtime(&rawtime);
		if (sprintf(buf, "%02d:%02d:%02d %.1f\n", info->tm_hour, info->tm_min, info->tm_sec, getTemperature(value, scale)) < 0) {
			fprintf(stderr, "Error using sprintf\n");
			exit(2);
		}
		SSL_write(sslClient, buf, strlen(buf));
		if (fp)
			fprintf(fp, "%s", buf);
	}
}

// returns 0 if command and option match; 1 otherwise
int myStrcmp(char* command, char* option) {
	size_t i = 0;
	for (i = 0; i < strlen(option); i++) {
		if (command[i] != option[i])
			return 1;
	}
	return 0;
}

int main(int argc, char* argv[]) {

	static int periodFlag;
	static int scaleFlag;
	static int logFlag;
	static int idFlag;
	static int hostFlag;

	int period = 1;
	char scale = 'F';
	char* fileName = NULL;
	FILE* fp = NULL;
	char* hostName = NULL;
	char* id = NULL;
	int portNum = -1;

	static struct option long_options[] = {
		{"period", required_argument, &periodFlag, 1},
		{"scale", required_argument, &scaleFlag, 1},
		{"log", required_argument, &logFlag, 1},
		{"id", required_argument, &idFlag, 1},
		{"host", required_argument, &hostFlag, 1},
		{0, 0, 0, 0}
	};

	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		// no more input to process
		if (c == -1)
			break;
		// unrecognized argument
		if (c != 0) {
			fprintf(stderr, "error: unrecognized argument; correct usage is --period=# --scale=# --log=#\n");
			exit(1);
		}
		// flag: --period=
		if (option_index == 0)
			period = atoi(optarg);
		// flag: --scale=
		else if (option_index == 1)
			scale = optarg[0];
		// flag: --log=
		else if (option_index == 2)
			fileName = optarg;
		// flag: --id=
		else if (option_index == 3) {
			// id must have 9 digits
			if (strlen(optarg) != 9) {
				fprintf(stderr, "error: id must be a 9-digit number\n");
				exit(1);
			}
			id = optarg;
		}
		// flag: --host=
		else if (option_index == 4) {
			hostName = optarg;
		}
	}
	// process port number (non-switch parameter)
	if (optind < argc) {
		portNum = atoi(argv[optind]);
		if (portNum < 0) {
			fprintf(stderr, "error: port number cannot be negative\n");
			exit(1);
		}
	}
	else {
		fprintf(stderr, "error: port number must be provided\n");
		exit(1);
	}

	// check if valid time period between samples
	if (period <= 0) {
		fprintf(stderr, "time between samples cannot be zero or negative\n");
		exit(1);
	}
	// check if scale is valid
	if (scale != 'F' && scale != 'C') {
		fprintf(stderr, "scale should either be F or C\n");
		exit(1);
	}
	// check if log, id, and host options are provided
	if (!logFlag || !idFlag || !hostFlag) {
		fprintf(stderr, "error: correct usage is ./lab4c_tcp --id=# --host=nameoraddress --log=filename\n");
		exit(1);
	}
	// create logfile
	fp = fopen(fileName, "w");
	if (fp == NULL) {
		fprintf(stderr, "problem creating file\n");
		exit(1);
	}
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Error while creating socket!\n");
		exit(1);
	}

	struct hostent* h;
	h = gethostbyname(hostName);
	if (h == NULL) {
		perror("error getting host by name");
		exit(1);
	}
	struct sockaddr_in server_address;
	memset(&server_address, '0', sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(portNum);
	if (inet_pton(AF_INET, "131.179.192.136", &server_address.sin_addr) <= 0) {
		fprintf(stderr, "Invalid IP address!\n");
		exit(1);
	}
	if (connect(sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
		fprintf(stderr, "Error while connecting!\n");
		exit(1);
	}

	// SSL basic setup
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	// SSL initialization
	SSL_CTX* newContext = SSL_CTX_new(TLSv1_client_method());
	if (newContext == NULL) {
		fprintf(stderr, "Unable to get SSL context\n");
		exit(2);
	}
	sslClient = SSL_new(newContext);
	if (sslClient == NULL) {
		fprintf(stderr, "Unable to complete SSL setup\n");
		exit(2);
	}

	// SSL connection
	if (SSL_set_fd(sslClient, sockfd) == 0) {
		fprintf(stderr, "Error connecting to SSL: SSL_set_fd() error\n");
		exit(2);
	}
	if (SSL_connect(sslClient) != 1) {
		fprintf(stderr, "SSL connection rejected\n");
		exit(2);
	}

	mraa_aio_context sensor;
	mraa_gpio_context button;
	sensor = mraa_aio_init(1);
	button = mraa_gpio_init(60);

	signal(SIGINT, doWhenInterrupted);

	char idString[14];
	sprintf(idString, "ID=%s\n", id);
	SSL_write(sslClient, idString, 13);
	fprintf(fp, "ID=%s\n", id);

	struct pollfd fds;
	fds.fd = sockfd;
	fds.events = POLLIN;
	char command[BUFFER_SIZE];

	while (runFlag) {
		int npoll = poll(&fds, 1, 0);
		if (npoll == -1) {
			perror("poll() error\n");
			exit(1);
		}
		if (fds.revents & POLLIN) {
			int nread = SSL_read(sslClient, command, BUFFER_SIZE);
			if (nread <= 0) {
				perror("SSL_read error");
				exit(2);
			}
			char* token;
			token = strtok(command, "\n");
			while (token != NULL && nread > 0) {
				printf("%s\n", token);
				printCommand(fp, token);
				if (myStrcmp(token, "SCALE=F") == 0) {
					scale = 'F';
				}
				else if (myStrcmp(token, "SCALE=C") == 0) {
					scale = 'C';
				}
				else if (myStrcmp(token, "STOP") == 0) {
					shouldReport = 0;
				}
				else if (myStrcmp(token, "START") == 0) {
					shouldReport = 1;
				}
				else if (myStrcmp(token, "OFF") == 0) {
					time_t rawtime;
					struct tm* info;
					time(&rawtime);
					info = localtime(&rawtime);
					sprintf(token, "%02d:%02d:%02d SHUTDOWN\n", info->tm_hour, info->tm_min, info->tm_sec);
					SSL_write(sslClient, token, strlen(token));
					fprintf(fp, "%s", token);
					SSL_shutdown(sslClient);
					SSL_free(sslClient);
					if (fp)
						fclose(fp);
					mraa_aio_close(sensor);
					mraa_gpio_close(button);
					exit(0);
				}
				else if (strstr(token, "PERIOD=") == token) {
					int newPeriod = 0;
					char* cur = token + 7;
					while (isdigit(*cur)) {
						newPeriod *= 10;
						newPeriod += *cur - '0';
						cur++;
					}
					if (newPeriod <= 0) {
						fprintf(stderr, "time between samples cannot be 0 or negative\n");
						exit(1);
					}
					period = newPeriod;
				}
				else if (strstr(token, "LOG") != token) {
					fprintf(stderr, "invalid command\n");
					fprintf(stderr, "valid commands are: SCALE=F, SCALE=C, PERIOD=seconds, STOP, START, LOG line of text, OFF\n");
					fprintf(stderr, "Your command was %s\n", token);
					exit(1);
				}
				nread -= strlen(token);
				token = memset(token, '\0', strlen(token));
				token = strtok(NULL, "\n");
			}
			int j = 0;
			for (j = 0; j < BUFFER_SIZE; j++) {
				command[j] = '\0';
			}
		}
		sample(&sensor, fp, scale);
		usleep(period * 1000000);
	}
	
	SSL_shutdown(sslClient);
	SSL_free(sslClient);

	mraa_aio_close(sensor);
	mraa_gpio_close(button);

	if (fp)
		fclose(fp);

	return 0;
}
