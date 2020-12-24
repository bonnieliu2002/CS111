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


void handler() {
	fprintf(stderr, "segmentation fault caught\n");
	exit(2);
}


void* doStuff(void* arg) {
	if (syncType && syncType[0] == 'm') {
		pthread_mutex_lock(&mLock);
	}
	else if (syncType && syncType[0] == 's') {
		while (__sync_lock_test_and_set(&sLock, 1));
	}
	// SortedList_insert
	for (long i = 0; i < *((long*) arg); i++) {
		SortedList_insert(head, &elements[i]);
	}
	// SortedList_length
	if (SortedList_length(head) < 0) {
		fprintf(stderr, "SortedList_length cannot be negative\n");
		exit(2);
	}
	// SortedList_delete
	for (long i = 0; i < *((long*) arg); i++) {
		SortedListElement_t* toDelete = SortedList_lookup(head, elements[i].key);
		if (!toDelete) {
			fprintf(stderr, "element not found\n");
			exit(2);
		}
		if (SortedList_delete(toDelete)) {
			fprintf(stderr, "corrupted list; problem deleting element\n");
			exit(2);
		}
	}
	if (syncType && syncType[0] == 'm') {
		pthread_mutex_unlock(&mLock);
	}
	else if (syncType && syncType[0] == 's') {
		__sync_lock_release(&sLock);
	}
	return NULL;
}


int main(int argc, char* argv[]) {
	static int threadsFlag;
	static int iterFlag;
	static int yieldFlag;
	static int syncFlag;
	int numThreads = 1;
	int numIters = 1;
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
	head[0].key = NULL;
	head[0].prev = head[0].next = head;

	// initialize the elements to add to the list
	long numElements = numThreads * numIters;
	elements = (SortedListElement_t*) malloc(numElements * sizeof(SortedListElement_t));
	if (elements == NULL) {
		fprintf(stderr, "Memory not allocated.\n");
		exit(2);
	}

	// instead of using all possible characters, I chose to use the ones between 33 and 126 (inclusive)
	// this helped me see the results more clearly when I was debugging	
	time_t t;
	srand((unsigned) time(&t));
	for (long i = 0; i < numElements; i++) {
		int x = rand() % 94 + 33;
		char* randKey = malloc(sizeof(char));
		randKey[0] = (char) x;
		elements[i].key = randKey;
	}

	struct timespec startTime;
	if (clock_gettime(CLOCK_REALTIME, &startTime) == -1) {
		perror("clock_gettime() error");
		exit(1);
	}

	pthread_t threads[numThreads];
	for (int i = 0; i < numThreads; i++) {
		if (pthread_create(&threads[i], NULL, doStuff, &numElements) != 0) {
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

	assert(SortedList_length(head) == 0);
	
	long numOps = 3 * numThreads * numIters;
	long numSecs = endTime.tv_sec - startTime.tv_sec;
	long numNanos = endTime.tv_nsec - startTime.tv_nsec;
	long totalNanos = numSecs * BILLION + numNanos;
	long avgTimePerOp = totalNanos / numOps;

	fprintf(stdout, "list-");
	if (!yields)
		fprintf(stdout, "none-");
	else
		fprintf(stdout, "%s-", yields);
	if (!syncType)
		fprintf(stdout, "none,");
	else
		fprintf(stdout, "%s,", syncType);
	fprintf(stdout, "%d,%d,1,%ld,%ld,%ld\n", numThreads, numIters, numOps, totalNanos, avgTimePerOp);

	if (syncType && syncType[0] == 'm')
                pthread_mutex_destroy(&mLock);

	// free dynamically allocated memory
	for (int i = 0; i < numElements; i++) {
		free((char*) elements[i].key);
	}
	free(elements);
	SortedListElement_t* cur = head->next;
	free(head);
	while (cur != head) {
		SortedListElement_t* temp = cur;
		cur = cur->next;
		free(temp);
	}

}
