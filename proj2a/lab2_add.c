#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

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
	static int yieldFlag;
	int numThreads;
	int numIters;
	long long counter = 0;

	static struct option long_options[] = {
		{"threads", optional_argument, &threadsFlag, 1},
		{"iterations", optional_argument, &iterFlag, 2},
		{"yield", no_argument, &opt_yield, 3},
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
		if (option_index == 0) {
			if (optarg)
				numThreads = atoi(optarg);
			else
				numThreads = 1;
		}
		if (option_index == 1) {
			if (optarg)
				numIters = atoi(optarg);
			else
				numIters = 1;
		}
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
	
	int numOps = 2 * numThreads * numIters;
	int numSecs = endTime.tv_sec - startTime.tv_sec;
	int numNanos = endTime.tv_nsec - startTime.tv_nsec;
	int totalNanos = numSecs * (int) pow(10, 9) + numNanos;
	int avgTimePerOp = (int) ((double) totalNanos / numOps);

	if (opt_yield == 3)
		fprintf(stdout, "add-yield,%d,%d,%d,%d,%d,%lld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, counter);
	else if (opt_yield == 0)
		fprintf(stdout, "add-yield-none,%d,%d,%d,%d,%d,%lld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, counter);
	else
		fprintf(stdout, "add-none,%d,%d,%d,%d,%d,%lld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, counter);
//	fprintf(stdout, "numThreads = %d\n", numThreads);
//	fprintf(stdout, "numIters = %d\n", numIters);
//	fprintf(stdout, "seconds: %ld, nanoseconds: %ld\n", ts.tv_sec, ts.tv_nsec);
}
