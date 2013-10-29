/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_BUD
#define __KMA_IMPL__
#define BLOCKBITS 4
#define LEVELS 9

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct {
	unsigned short level;
	void* prev;
	void* head;
	kma_page_t* page;
	} block_t;
	
typedef struct {
	block_t* first;
	} empty_l;
	
typedef struct {
	empty_l empty[LEVELS+1];
	int used;
	} header_t;
	
/************Global Variables*********************************************/
static kma_page_t* start;

/************Function Prototypes******************************************/
unsigned short level_of_size(kma_size_t size);
void* find_block(unsigned short level);
void init();
void free_block(void* ptr);
void remove_block_from_empty(block_t* b, empty_l* list);
void* get_buddy(void *b, unsigned short level);
void* new_page();
void* coalesce(block_t* b, block_t* buddy);
void add_block_to_empty(block_t* b, unsigned short level);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
void* kma_malloc(kma_size_t size) {
	if (start == NULL) init();
	unsigned short level = level_of_size(size + sizeof(block_t));
	if (level > LEVELS) return NULL;
	return find_block(level);
}

void kma_free(void* ptr, kma_size_t size) {
	free_block(ptr);
	header_t* header = (header_t*)((void*)start->ptr + sizeof(block_t));
	if (header->used == 0) {
		free_page(start);
		start = NULL;
	}
}

void* find_block(unsigned short level) {
	header_t* header = (header_t*)((void*)start->ptr+sizeof(block_t));
	empty_l* empty_blocks = &header->empty[level];

	header->used++;
	if (empty_blocks->first == NULL) {	//no empty block on current block
		if (level < LEVELS) {	//level is not maximum level
			//add split block to empty list, return its buddy
			block_t* ret = (block_t*)((void*)find_block(level+1)-sizeof(block_t));
			uintptr_t offset = 0x1<<(level+4);
			block_t* split = (block_t*)((void*)ret + offset);
			
			add_block_to_empty(split, level);
			
			ret->head = (void*)empty_blocks;
			ret->level = level;
			return ((void*)ret + sizeof(block_t));
		} else {
			return new_page();
		}
	}
	//there is an empty block on this level
	block_t* ret = empty_blocks->first;
	empty_blocks->first = ret->head;
	ret->head = (void*)empty_blocks;
	ret->level = level;
	if(empty_blocks->first != NULL) {
		empty_blocks->first->prev = NULL;
	}
	return ((void*)ret + sizeof(block_t));
}

void* new_page() {
	header_t* header = (header_t*)((void*)start->ptr + sizeof(block_t));
	empty_l* empty_blocks = &header->empty[LEVELS];
	
	kma_page_t* p = get_page();
	*((kma_page_t**)p->ptr) = p;
	block_t* b = (block_t*)p->ptr;
	b->page = p;
	b->head = (void*)empty_blocks;
	b->level = LEVELS;
	return (void*)b + sizeof(block_t);
}

void free_block(void* ptr) {
	header_t* header = (header_t*)((void*)start->ptr + sizeof(block_t));
	header->used--;

	block_t* b = (block_t*)((void*)ptr - sizeof(block_t));
	empty_l* head_l = (empty_l*)b->head;
	unsigned short level = b->level;

	block_t* buddy = (block_t*) get_buddy((void*)b, level);
	if (level == LEVELS) {
		free_page((kma_page_t*)b->page);
		return;
	}
	if ((buddy->head==b->head)|(level+1 > LEVELS)|(level!=buddy->level)) {
		//if buddy is in use
		add_block_to_empty(b, level);
	} else {
		remove_block_from_empty(buddy, head_l);
		//recusive call to see if coalesced block has free buddy
		free_block(coalesce(b, buddy));
	}
}

void* coalesce(block_t* b, block_t* buddy) {
	header_t* header = (header_t*)((void*)start->ptr + sizeof(block_t));
	unsigned short level = b->level;
	
	if (buddy < b) {
		block_t* temp = b;
		b = buddy;
		buddy = temp;
	}
	b->head = &header->empty[level + 1];
	b->level++;
	return (void*)b + sizeof(block_t);
}

void* get_buddy(void *b, unsigned short level) {
	uintptr_t mask = 1 << (level + BLOCKBITS);
	uintptr_t address = (uintptr_t)b;
	return (void*)(address ^ mask);
}

void add_block_to_empty(block_t* b, unsigned short level) {
	header_t* header = (header_t*)((void*)start->ptr+sizeof(block_t));
	empty_l* empty_blocks = &header->empty[level];
	
	if (empty_blocks->first != NULL)
		empty_blocks->first->prev = b;
	b->head = (void*)empty_blocks->first;
	b->prev = NULL;
	b->level = level;
	empty_blocks->first = b;
}

void remove_block_from_empty(block_t* b, empty_l* list) {
	block_t* next = (block_t*)b->head;
	block_t* prev = (block_t*)b->prev;
	if (list->first == b) {
		list->first = next;
	}
	if (next != NULL){
		next->prev = prev;
	}
	if (prev != NULL){
		prev->head = next;
	}
}

void init() {
	start = get_page();
	
	*((kma_page_t**)start->ptr) = start;
	header_t *header;
	header = (header_t*)((void*)start->ptr+sizeof(block_t));
	
	header->used = 0;
	int i; for (i=1; i<LEVELS+1; i++) header->empty[i].first = NULL;
	
	block_t* b = start->ptr;
	b->head = NULL;
	b->prev = NULL;
	header->empty[LEVELS].first = b;
	kma_malloc(sizeof(header_t));
	
	header->used = 0; //resets used because only header was alloc'd
}

unsigned short level_of_size(kma_size_t size) {
	unsigned short i = 1;
	while (size > (1 << (i + BLOCKBITS))) i = i+1;
	return i;
}

#endif // KMA_BUD