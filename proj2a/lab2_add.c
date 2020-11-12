#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <sched.h>
#include <errno.h>

#define BILLION 1E9

typedef struct {
	long long *counterPtr;
	int iterations;
} myarg_t;

int opt_yield = 0;
void add(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield)
		sched_yield();
	*pointer = sum;
}

void* performAdds(void* arg) {
	for (int i = 0; i < ((myarg_t*)arg)->iterations; i++) {
		add(((myarg_t*)arg)->counterPtr, 1);
		add(((myarg_t*)arg)->counterPtr, -1);
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	static int threadsFlag;
	static int iterFlag;
	int numThreads = 1;
	int numIters = 1;
	long long counter = 0;

	static struct option long_options[] = {
		{"yield", no_argument, &opt_yield, 1},
		{"threads", optional_argument, &threadsFlag, 2},
		{"iterations", optional_argument, &iterFlag, 3},
		{0, 0, 0, 0}
	};

	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		if (c != 0) {
			fprintf(stderr, "error: unrecognized argument; correct usage: --threads=# --iterations=#\n");
			exit(1);
		}
		if (option_index == 1 && optarg)
			numThreads = atoi(optarg);
		if (option_index == 2 && optarg)
			numIters = atoi(optarg);
	}

	if (!threadsFlag) {
		fprintf(stderr, "missing --threads=# argument\n");
		exit(1);
	}
	if (!iterFlag) {
		fprintf(stderr, "missing --itersFlag=# argument\n");
		exit(1);
	}

	struct timespec startTime;
	if (clock_gettime(CLOCK_REALTIME, &startTime) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}

	pthread_t threads[numThreads];
	myarg_t args = { &counter, numIters };
	for (int i = 0; i < numThreads; i++) {
		if (pthread_create(&threads[i], NULL, performAdds, &args) != 0) {
			fprintf(stderr, "pthread_create() error");
			exit(2);
		}
	}

	for (int i = 0; i < numThreads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "pthread_join() error");
			exit(2);
		}
	}

	struct timespec endTime;
	if (clock_gettime(CLOCK_REALTIME, &endTime) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}

	long numOps = 2 * numThreads * numIters;
	long numSecs = endTime.tv_sec - startTime.tv_sec;
	long numNanos = endTime.tv_nsec - startTime.tv_nsec;
	long totalNanos = numSecs * BILLION + numNanos;
	long avgTimePerOp = totalNanos / numOps;

	if (opt_yield == 1)
		fprintf(stdout, "add-yield-none,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, counter);
	else
		fprintf(stdout, "add-none,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, counter);
}
