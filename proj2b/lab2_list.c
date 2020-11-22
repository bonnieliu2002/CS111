// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "SortedList.h"
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>
#include <string.h>

#define BILLION 1E9


SortedList_t* head;
SortedListElement_t* elements;
char* syncType;
pthread_mutex_t mLock;
int sLock = 0;
int numThreads = 1;
int numIters = 1;
int numElements;
long totalElapsed;

void handler() {
	fprintf(stderr, "segmentation fault caught\n");
	exit(2);
}


long calculateElapsedTime(struct timespec* start, struct timespec* end) {
	long numSecs = end->tv_sec - start->tv_sec;
	long numNanos = end->tv_nsec - start->tv_nsec;
	long totalNanos = numSecs * BILLION + numNanos;
	return totalNanos;
}


void* doStuff(void* arg) {
	struct timespec lockStart;
	struct timespec lockEnd;
	// SortedList_insert
	for (int i = *((int*) arg); i < numElements; i += numThreads) {
		if (clock_gettime(CLOCK_REALTIME, &lockStart) == -1) {
			perror("clock_gettime() error");
			exit(1);
		}
		if (syncType && syncType[0] == 'm')
			pthread_mutex_lock(&mLock);
		else if (syncType && syncType[0] == 's')
			while (__sync_lock_test_and_set(&sLock, 1));
		if (clock_gettime(CLOCK_REALTIME, &lockEnd) == -1) {
			perror("clock_gettime() error");
			exit(1);
		}
		totalElapsed += calculateElapsedTime(&lockStart, &lockEnd);
		SortedList_insert(head, &(elements[i]));
		if (syncType && syncType[0] == 'm')
			pthread_mutex_unlock(&mLock);
		else if (syncType && syncType[0] == 's')
			__sync_lock_release(&sLock);
	}

	// SortedList_length
	if (clock_gettime(CLOCK_REALTIME, &lockStart) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}
	if (syncType && syncType[0] == 'm')
		pthread_mutex_lock(&mLock);
	else if (syncType && syncType[0] == 's')
		while (__sync_lock_test_and_set(&sLock, 1));
	if (clock_gettime(CLOCK_REALTIME, &lockEnd) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}
	totalElapsed += calculateElapsedTime(&lockStart, &lockEnd);
	int size = SortedList_length(head);
	if (size < 0) {
		fprintf(stderr, "SortedList_length cannot be negative\n");
		exit(2);
	}
	if (syncType && syncType[0] == 'm')
		pthread_mutex_unlock(&mLock);
	else if (syncType && syncType[0] == 's')
		__sync_lock_release(&sLock);

	// SortedList_lookup and SortedList_delete
	for (int i = *((int*)arg); i < numElements; i += numThreads) {
		if (clock_gettime(CLOCK_REALTIME, &lockStart) == -1) {
			perror("clock_gettime() error");
			exit(1);
		}
		if (syncType && syncType[0] == 'm')
			pthread_mutex_lock(&mLock);
		else if (syncType && syncType[0] == 's')
			while (__sync_lock_test_and_set(&sLock, 1));
		if (clock_gettime(CLOCK_REALTIME, &lockEnd) == -1) {
			perror("clock_gettime() error");
			exit(1);
		}
		totalElapsed += calculateElapsedTime(&lockStart, &lockEnd);
		SortedListElement_t* toDelete = SortedList_lookup(head, elements[i].key);
		if (!toDelete) {
			fprintf(stderr, "element not found\n");
			exit(2);
		}
		if (SortedList_delete(toDelete)) {
			fprintf(stderr, "corrupted list; problem deleting element\n");
			exit(2);
		}
		if (syncType && syncType[0] == 'm')
			pthread_mutex_unlock(&mLock);
		else if (syncType && syncType[0] == 's')
			__sync_lock_release(&sLock);
	}
	return NULL;
}


int main(int argc, char* argv[]) {
	static int threadsFlag;
	static int iterFlag;
	static int yieldFlag;
	static int syncFlag;
	char* yields = NULL;
	syncType = NULL;
	opt_yield = 0;
	
	static struct option long_options[] = {
		{"threads", optional_argument, &threadsFlag, 1},
		{"iterations", optional_argument, &iterFlag, 2},
		{"yield", required_argument, &yieldFlag, 3},
		{"sync", required_argument, &syncFlag, 4},
		{0, 0, 0, 0}
	};

	while (1) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		if (c != 0) {
			fprintf(stderr, "error: unrecognized argument; correct usage is --threads=# --iterations=#\n");
			exit(1);
		}
		if (option_index == 0 && optarg)
			numThreads = atoi(optarg);
		else if (option_index == 1 && optarg)
			numIters = atoi(optarg);
		else if (option_index == 2)
			yields = optarg;
		else if (option_index == 3)
			syncType = optarg;
	}

	if (!threadsFlag) {
		fprintf(stderr, "missing --threads=# argument\n");
		exit(1);
	}
	if (!iterFlag) {
		fprintf(stderr, "missing --iterations=# argument\n");
		exit(1);
	}
	if (yieldFlag) {
		for (size_t i = 0; i < strlen(yields); i++) {
			if (yields[i] == 'i')
				opt_yield |= INSERT_YIELD;
			else if (yields[i] == 'd')
				opt_yield |= DELETE_YIELD;
			else if (yields[i] == 'l')
				opt_yield |= LOOKUP_YIELD;
			else {
				fprintf(stderr, "invalid argument for --yield; valid arguments are some combination of i, d, and l\n");
				exit(1);
			}
		}
	}
	if (syncType) {
	       	if (syncType[0] == 'm') {
			int rc = pthread_mutex_init(&mLock, NULL);
			assert(rc == 0);
		}
		else if (syncType[0] != 's') {
			fprintf(stderr, "invalid argument for --sync; valid arguments can be either m or s\n");
			exit(1);
		}
	}

	signal(SIGSEGV, handler);

	head = malloc(sizeof(SortedList_t));
	char lowestKey = 0;
	head[0].key = &lowestKey;
	head[0].prev = head[0].next = head;

	// initialize the elements to add to the list
	numElements = numThreads * numIters;
	elements = (SortedListElement_t*) malloc(numElements * sizeof(SortedListElement_t));
	if (elements == NULL) {
		fprintf(stderr, "Memory not allocated.\n");
		exit(2);
	}

	// instead of using all possible characters, I chose to use the ones between 33 and 126 (inclusive)
	// this helped me see the results more clearly when I was debugging	
	time_t t;
	char* randKeys = malloc(numElements * sizeof(char));
	srand((unsigned) time(&t));
	for (int i = 0; i < numElements; i++) {
		int x = rand() % 94 + 33;
		randKeys[i] = x;
		elements[i].key = &(randKeys[i]);
	}

	struct timespec startTime;
	if (clock_gettime(CLOCK_REALTIME, &startTime) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}

	pthread_t threads[numThreads];
	int threadIndex[numThreads];
	int i = 0;
	for (; i < numThreads; i++) {
		threadIndex[i] = i;
	}
	for (i = 0; i < numThreads; i++) {
		if (pthread_create(&threads[i], NULL, doStuff, &threadIndex[i]) != 0) {
			fprintf(stderr, "pthread_create() error");
			exit(2);
		}
	}
	totalElapsed = 0;
	for (i = 0; i < numThreads; i++) {
		if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "pthread_join() error");
			exit(2);
		}
	}
//	int numLockOps = numThreads * (2 * numIters + 1);
//	long avgWaitForLock = totalElapsed / numLockOps;

	struct timespec endTime;
	if (clock_gettime(CLOCK_REALTIME, &endTime) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}

//	assert(SortedList_length(head) == 0);
	
	long numOps = 3 * numThreads * numIters;
	long totalNanos = calculateElapsedTime(&startTime, &endTime);
	long avgTimePerOp = totalNanos / numOps;
	long avgWaitForLock = totalElapsed / numOps;

	fprintf(stdout, "list-");
	if (!yields)
		fprintf(stdout, "none-");
	else
		fprintf(stdout, "%s-", yields);
	if (!syncType)
		fprintf(stdout, "none,");
	else
		fprintf(stdout, "%s,", syncType);
	fprintf(stdout, "%d,%d,1,%ld,%ld,%ld,%ld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp, avgWaitForLock);

	if (syncType && syncType[0] == 'm')
                pthread_mutex_destroy(&mLock);

	// free dynamically allocated memory
	free(elements);
	free(randKeys);
	free(head);
}
