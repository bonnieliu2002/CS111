// NAME: Bonnie Liu
// EMAIL: bonnieliu2002@g.ucla.edu
// ID: 005300989

#include "SortedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

int opt_yield = 0;

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	// empty list
	if (list->next == list) {
		if (opt_yield & INSERT_YIELD)
			sched_yield();
		list->next = list->prev = element;
		element->prev = element->next = list;
		return;
	}
	// nonempty list
	SortedListElement_t* before = list;
	SortedListElement_t* cur = list->next;
	while (cur != list && strcmp(cur->key, element->key) <= 0) {
		before = cur;
		cur = cur->next;
	}
	if (opt_yield & INSERT_YIELD)
		sched_yield();
	before->next = cur->prev = element;
	element->next = cur;
	element->prev = before;
}

int SortedList_delete(SortedListElement_t *element) {
	if (element->next->prev != element || element->prev->next != element) {
//		fprintf(stdout, "We want to delete the element with key: %c\n", element->key[0]);
		return 1;
	}
	if (opt_yield & DELETE_YIELD)
		sched_yield();
	element->prev->next = element->next;
	element->next->prev = element->prev;
	element->next = NULL;
	element->prev = NULL;
	return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
	if (opt_yield & LOOKUP_YIELD)
		sched_yield();
	SortedListElement_t* cur = list->next;
//	fprintf(stdout, "We want to look for element with key: %c\n", key[0]);
	while (cur != list) {
		if (strcmp(cur->key, key) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

int SortedList_length(SortedList_t *list) {
	int count = 0;
	if (opt_yield & LOOKUP_YIELD)
		sched_yield();
	SortedList_t* cur = list->next;
	while (cur != list) {
		cur = cur->next;
		count++;
	}
	return count;
}
