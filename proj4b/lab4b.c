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

const int B = 4275;
const int R0 = 100000;
const int BUFFER_SIZE = 2048;

sig_atomic_t volatile runFlag = 1;
int shouldReport = 1;

void printTime(FILE* fp);

void doWhenInterrupted(int sig) {
	if (sig == SIGINT)
		runFlag = 0;
}

void doWhenButtonPressed(void* fp) {
	printTime((FILE*) fp);
	printf("SHUTDOWN\n");
	if (fp)
		fprintf(fp, "SHUTDOWN\n");
	runFlag = 0;
}

void printTime(FILE* fp) {
	time_t rawtime;
	struct tm* info;
	time(&rawtime);
	info = localtime(&rawtime);
	printf("%d:%d:%d ", info->tm_hour, info->tm_min, info->tm_sec);
	if (fp)
		fprintf(fp, "%d:%d:%d ", info->tm_hour, info->tm_min, info->tm_sec);
}

void printCommand(FILE* fp, char* command) {
	if (fp) {
		fprintf(fp, "%s", command);
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
		printTime(fp);
		printf("%.1f\n", getTemperature(value, scale));
		if (fp)
			fprintf(fp, "%.1f\n", getTemperature(value, scale));
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

	int period = 1;
	char scale = 'F';
	char* fileName = NULL;
	FILE* fp = NULL;

	static struct option long_options[] = {
		{"period", required_argument, &periodFlag, 1},
		{"scale", required_argument, &scaleFlag, 1},
		{"log", required_argument, &logFlag, 1},
		{0, 0, 0, 0}
	};

	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		if (c != 0) {
			fprintf(stderr, "error: unrecognized argument; correct usage is --period=# --scale=# --log=#\n");
			exit(1);
		}
		if (option_index == 0)
			period = atoi(optarg);
		else if (option_index == 1)
			scale = optarg[0];
		else if (option_index == 2)
			fileName = optarg;
	}

	if (period <= 0) {
		fprintf(stderr, "time between samples cannot be zero or negative\n");
		exit(1);
	}
	if (scale != 'F' && scale != 'C') {
		fprintf(stderr, "scale should either be F or C\n");
		exit(1);
	}
	if (fileName) {
		fp = fopen(fileName, "w");
		if (fp == NULL) {
			fprintf(stderr, "problem creating file\n");
			exit(1);
		}
	}

	mraa_aio_context sensor;
	mraa_gpio_context button;
	sensor = mraa_aio_init(1);
	button = mraa_gpio_init(60);
	mraa_gpio_dir(button, MRAA_GPIO_IN);
	mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, &doWhenButtonPressed, (void*) fp);

	signal(SIGINT, doWhenInterrupted);

	struct pollfd fds;
	fds.fd = 0;
	fds.events = POLLIN;

	char command[BUFFER_SIZE];

	while (runFlag) {
		int npoll = poll(&fds, 1, 0);
		if (npoll == -1) {
			perror("poll() error\n");
			exit(1);
		}
		// if there is keyboard input waiting to be processed
		if (fds.revents & POLLIN) {
			fgets(command, BUFFER_SIZE, stdin);
			printCommand(fp, command);
			if (myStrcmp(command, "SCALE=F") == 0) {
				scale = 'F';
			}
			else if (myStrcmp(command, "SCALE=C") == 0) {
				scale = 'C';
			}
			else if (myStrcmp(command, "STOP") == 0) {
				shouldReport = 0;
			}
			else if (myStrcmp(command, "START") == 0) {
				shouldReport = 1;
			}
			else if (myStrcmp(command, "OFF") == 0) {
				doWhenButtonPressed(fp);
				exit(0);
			}
			else if (strstr(command, "PERIOD=") == command) {
				int newPeriod = 0;
				char* cur = command + 7;
				while (isdigit(*cur)) {
					newPeriod *= 10;
					newPeriod += *cur - '0';
					cur++;
				}
				if (newPeriod <= 0) {
					printf("time between samples cannot be 0 or negative\n");
					exit(1);
				}
				period = newPeriod;
			}
			else if (strstr(command, "LOG") != command) {
				fprintf(stderr, "invalid command\n");
				fprintf(stderr, "valid commands are: SCALE=F, SCALE=C, PERIOD=seconds, STOP, START, LOG line of text, OFF\n");
				fprintf(stderr, "Your command was %s\n", command);
				exit(1);
			}
		}
		sample(&sensor, fp, scale);
		usleep(period * 1000000);
	}
	
	mraa_aio_close(sensor);
	mraa_gpio_close(button);

	if (fp)
		fclose(fp);

	return 0;
}
