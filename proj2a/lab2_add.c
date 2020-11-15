// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#define BILLION 1E9

typedef struct {
	long long *counterPtr;
	int iterations;
} myarg_t;

int opt_yield = 0;
char* syncType = NULL;
pthread_mutex_t mLock;
int sLock = 0;

void add(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield)
		sched_yield();
	*pointer = sum;
}

// mutex synchronization
void addM(long long *pointer, long long value) {
	pthread_mutex_lock(&mLock);
	long long sum = *pointer + value;
	if (opt_yield)
		sched_yield();
	*pointer = sum;
	pthread_mutex_unlock(&mLock);
}

// spin-lock synchronization
void addS(long long *pointer, long long value) {
	while(__sync_lock_test_and_set(&sLock, 1));
	long long sum = *pointer + value;
	if (opt_yield)
		sched_yield();
	*pointer = sum;
	__sync_lock_release(&sLock);
}

// compare-and-swap synchronization
void addC(long long *pointer, long long value) {
	long long expected;
	do {
		expected = *pointer;
		if (opt_yield)
			sched_yield();
	} while (__sync_val_compare_and_swap(pointer, expected, expected + value) != expected);
}

void* performAdds(void* arg) {
	for (int i = 0; i < ((myarg_t*)arg)->iterations; i++) {
		if (!syncType) {
			add(((myarg_t*)arg)->counterPtr, 1);
			add(((myarg_t*)arg)->counterPtr, -1);
		}
		else if (syncType[0] == 'm') {
			addM(((myarg_t*)arg)->counterPtr, 1);
			addM(((myarg_t*)arg)->counterPtr, -1);
		}
		else if (syncType[0] == 's') {
			addS(((myarg_t*)arg)->counterPtr, 1);
			addS(((myarg_t*)arg)->counterPtr, -1);
		}
		else if (syncType[0] == 'c') {
			addC(((myarg_t*)arg)->counterPtr, 1);
			addC(((myarg_t*)arg)->counterPtr, -1);
		}
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	static int threadsFlag;
	static int iterFlag;
	static int syncFlag;
	int numThreads = 1;
	int numIters = 1;
	long long counter = 0;

	static struct option long_options[] = {
		{"yield", no_argument, &opt_yield, 1},
		{"threads", optional_argument, &threadsFlag, 2},
		{"iterations", optional_argument, &iterFlag, 3},
		{"sync",required_argument, &syncFlag, 4},
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
		if (option_index == 3) {
			syncType = optarg;
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
	if (syncType && syncType[0] == 'm') {
		int rc = pthread_mutex_init(&mLock, NULL);
		assert(rc == 0);
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

	fprintf(stdout, "add-");
	if (opt_yield == 1)
		fprintf(stdout, "yield-");
	if (!syncType)
		fprintf(stdout, "none");
	else if (syncType[0] == 'm')
		fprintf(stdout, "m");
	else if (syncType[0] == 's')
		fprintf(stdout, "s");
	else if (syncType[0] == 'c')
		fprintf(stdout, "c");
	fprintf(stdout, ",%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, counter);

	if (syncType && syncType[0] == 'm')
		pthread_mutex_destroy(&mLock);

}
