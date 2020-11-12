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
		list->next = list->prev = element;
		if (opt_yield & INSERT_YIELD)
			sched_yield();
		element->prev = element->next = list;
		return;
	}
	// nonempty list
	SortedListElement_t* before = list;
	SortedListElement_t* cur = list->next;
	while (cur != list && strcmp(cur->key, element->key) < 0) {
		before = cur;
		cur = cur->next;
	}
	before->next = cur->prev = element;
	element->next = cur;
	element->prev = before;
}

int SortedList_delete(SortedListElement_t *element) {
	if (opt_yield & DELETE_YIELD)
		sched_yield();
	if(element->next->prev != element)
		return 1;
	if(element->prev->next != element)
		return 1;
	element->prev->next = element->next;
	element->next->prev = element->prev;
	element->next = NULL;
	element->prev = NULL;
	return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
	SortedListElement_t* cur = list->next;
	while (cur != list) {
		if (strcmp(cur->key, key) == 0)
			return cur;
		if (opt_yield & LOOKUP_YIELD)
			sched_yield();
		cur = cur->next;
	}
	return NULL;
}

int SortedList_length(SortedList_t *list) {
	int count = 0;
	Sorted_List_t* cur = list->next;
	while (cur != list) {
		cur = cur->next;
		if (opt_yield & LOOKUP_YIELD)
			sched_yield();
		count++;
	}
	return count;
}
