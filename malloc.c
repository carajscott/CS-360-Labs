/* Author: Cara Scott
 * Date: October 28, 2018
 * Class: CS 360
 * Purpose: basically my own malloc call
 * http://web.eecs.utk.edu/~huangj/cs360/360/labs/lab6/lab.html
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/types.h>
#include <utime.h>
#include <errno.h>

#include "fields.h"
#include "dllist.h"
#include "jrb.h"
#include "jval.h"
#include "malloc.h"

typedef struct info {
	unsigned int size; //size of the node
	struct info *next; //pointer to the next node
} *Info;

//Global Variable
Info malloc_head = NULL;

/* --- FUNCTIONS --- */


//This function gives the proper memory space requested
void *malloc(unsigned int size) {
	if ((int) size < 1) {
		return NULL;
	}

	Info free_info;
	Info tmp_fi;
	int multiplier = 1;

	//Pad the given size
	int num = size % 8;
	if (num != 0) {
		int padding = 8 - num;
		size += padding;
	}	
	size += sizeof(struct info); //16

	//check if malloc_head is NULL, aka we have not allocated any memory
	if (malloc_head == NULL) {
		//set free_info to the top memory space in the fresh, new memory
		free_info = (Info) sbrk(8192);
		if (free_info == sbrk(0)) {
			fprintf(stderr, "(1) Used up all of the heap memory, exiting\n");
			exit(1);
		}

		free_info->size = 8192;
		free_info->next = NULL;
		malloc_head = free_info;

		//check that the size isn't over the free space
		if ((size / free_info->size) >= 1) {
			//the given free space is not big enough so check how many 8192 blocks we need
			multiplier = size / 8192;
			if ((multiplier >= 1) && ((size % 8192) != 0)) {
				multiplier++;
			}
			free_info = (Info) sbrk(8192 * multiplier);
			if (free_info == sbrk(0)) {
				fprintf(stderr, "(2) Used up all of the heap memory, exiting\n");
				exit(1);
			}
			free_info->next = (Info) ((char *)free_info + size);
			free_info->next->size = (8192 * multiplier) - size;
			free_info->next->next = NULL;
			return (char *)free_info + 16;
		}
		//the first 8192 is big enough
		else {
			malloc_head = (Info) ((char *)free_info + size);
			malloc_head->size = 8192 - size;
			malloc_head->next = NULL;
			free_info->size = size;
			free_info->next = malloc_head;
			return (char *)free_info + 16;
		}
	}
	else {//if this is not the first call
		//check to see if the first chunk of code is big enough to fit the request
		if (malloc_head->size >= size) {
			free_info = malloc_head;
			//if the first chunk also happens to be the last
			if(malloc_head->next == NULL) {
				malloc_head = (Info) ((char *)malloc_head + size);
				malloc_head->size = free_info->size - size;
				malloc_head->next = NULL;
				free_info->size = size;
			}
			else {
				//the chunk size is exactly what we need so we don't need to carve anything off
				if (malloc_head->size == size) {
					malloc_head = malloc_head->next;
				}
				//the size is too big
				else {
					malloc_head = (Info) ((char *) free_info + size);
					malloc_head->size = free_info->size - size;
					malloc_head->next = free_info->next;
					free_info->size = size;
				}
			}
			return (char *)free_info + 16;
		}
		//go down the chain till you find one that does fit
		else {
			free_info = malloc_head->next;
			tmp_fi = malloc_head;
			if (free_info != NULL) {
				//go until there is a node big enough
				while (free_info->size < size) {
					free_info = free_info->next;
					tmp_fi = tmp_fi->next;

					if (free_info == NULL) {
						break;
					}
				}
			}
			if (free_info == NULL) {
				//none of the given free spaces are big enough so check how many 8192 blocks we need
				multiplier = size / 8192;
				if ((multiplier >= 1) && ((size % 8192) != 0)) {
					multiplier++;
				}
				else if (multiplier == 0) {
					multiplier = 1;
				}
				free_info = (Info) sbrk(8192 * multiplier);
				if (free_info == sbrk(0)) {
					fprintf(stderr, "(4) Used up all of the heap memory, exiting\n");
					exit(1);
				}
				free_info->next = (Info) ((char *)free_info + size);
				free_info->next->size = (8192 * multiplier) - size;
				free_info->next->next = NULL;
				return (char *)free_info + 16;
			}
			else { //there is a space big enough
				//check if we need to carve off memory or just return the entire node 
				int num = (free_info->size) - size;
				if (num < 24) { //return the entire node
					tmp_fi->next = free_info->next;
					return (char *)free_info + 16;
				}
				else if (num >= 24) { //carve off memory
					Info tmp_fi_jr = free_info;
					tmp_fi_jr = (Info) ((char *)tmp_fi_jr + size);
					tmp_fi_jr->size = num;
					tmp_fi_jr->next = free_info->next;
					tmp_fi->next = tmp_fi_jr;
					free_info->size = size;
					return (char *)free_info + 16;
				}
			}
		}
	}
}

// In other words, calloc() calls malloc() and then bzero() (read the man page). 
void *calloc(unsigned int nmemb, unsigned int size) {
	void *pointer = malloc(nmemb * size);
	bzero(pointer, (nmemb * size));
	
	return pointer;
}

//void *realloc(void *ptr, unsigned int size); 3 lines of code (check that it's not NULL)
// realloc() will always reallocate -- it will call malloc(), bcopy(), and then free().
void *realloc(void *ptr, unsigned int size) {
	void *pointer = malloc(size);
	bcopy(ptr, pointer, size);
	free(ptr);

	return pointer;
}

//This function frees up the memory sent to it by putting that memory block at the front of the list of free memory.
void free(void *ptr) {
	if (ptr == NULL) {
		return;
	}

	Info tmp_fi = malloc_head;

	malloc_head = (Info) ((char *)ptr - 16);

	malloc_head->next = tmp_fi;
}
