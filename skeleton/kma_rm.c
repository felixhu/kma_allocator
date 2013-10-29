/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
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
#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

typedef struct
{
 int size;
 void* pageHeader;
 void* prevBlock;
 void* nextBlock;
} BLOCK_T;

typedef struct
{
 kma_page_t* page;
 BLOCK_T* freeList;
 int totalPages;
 int allocatedBlocks;
} HEADER_T;

/************Global Variables*********************************************/

static kma_page_t* gpages;

/************Function Prototypes******************************************/

//requests new page, initializes header
static void MakeNewPage();
//adds given block to free list
static void AddBlock(BLOCK_T* block, kma_size_t size);
//removes given block from free list
static void RemoveBlock(BLOCK_T* block);
//find block with enough space to allocate and return it
static BLOCK_T* FindFreeSpace(kma_size_t size);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* kma_malloc(kma_size_t size)
{
   //if size is greater than a page, ignore request
   if ((size + sizeof(void*)) > PAGESIZE)
      return NULL;
   //if there are no allocated pages, make a new one
   if (gpages == NULL)
      MakeNewPage();
   //find space for block, increase block count, and return block address
   BLOCK_T* block = FindFreeSpace(size);
   HEADER_T* blockHeader = block->pageHeader;
   blockHeader->allocatedBlocks++;
   return block;
}

void kma_free(void* ptr, kma_size_t size)
{
   //add block location back into free list with given size
   AddBlock(ptr, size);
   //decrease allocated block counter
   ((HEADER_T*)((BLOCK_T*)ptr)->pageHeader)->allocatedBlocks--;
   HEADER_T* firstHeader = gpages->ptr;
   //get number of pages requested
   int noPages = firstHeader->totalPages;
   int i;
   //cycle through pages starting from the most recently requested one
   for (i = noPages; i >= 0; i--)
   {
      HEADER_T* page = (((HEADER_T*)((int)firstHeader + (i * PAGESIZE))));
      //if page has nothing allocated to it, free it and continue looking for more
      if (page->allocatedBlocks == 0)
      {
         //remove all associated free blocks from free list
         BLOCK_T* current = firstHeader->freeList;
         while (current != NULL)
         {
            BLOCK_T* temp = current;
            if (current->pageHeader == page)
            {
               RemoveBlock(current);
            }
            current = temp->nextBlock;
         }
         //if removing the last page, set entry pointer to null and stop
         if (page == firstHeader)
         {
            gpages = NULL;
            free_page(page->page);
            return;
         }
         //otherwise decrease page count and free
         firstHeader->totalPages--;
         free_page(page->page);
      }
      //if page has blocks allocated to it, no need to look further
      else
      {
         return;
      }
   }
}

void MakeNewPage()
{
   //get new page
   kma_page_t* newPage = get_page();
   //if this is the first one, set pointer to list
   if (gpages == NULL)
   {
      gpages = newPage;
   }
   //point page to itself so header can access it
   *((kma_page_t**)newPage->ptr) = newPage;
   HEADER_T* pageHeader = newPage->ptr;
   //set starting address of free list to space after header
   pageHeader->freeList = (BLOCK_T*)((int)pageHeader + sizeof(HEADER_T));
   //add resulting space to free list
   AddBlock(pageHeader->freeList, PAGESIZE - sizeof(HEADER_T));
   //initialize counters
   pageHeader->totalPages = 0;
   pageHeader->allocatedBlocks = 0;
}       

void AddBlock(BLOCK_T* block, kma_size_t size)
{
   HEADER_T* frontHeader = gpages->ptr;
   BLOCK_T* current = frontHeader->freeList;
   //find which page the given block is on by taking truncated result
   int pageIndex = ((int)block - (int)frontHeader) / PAGESIZE;
   //set block information
   block->pageHeader = (HEADER_T*)((int)frontHeader + (pageIndex * PAGESIZE));
   block->size = size;
   block->prevBlock = NULL;
   //adding first block  
   if (current == block)
   {
      block->nextBlock = NULL;
   }
   //if block address comes before front of list, insert at front
   else if (current > block)
   {
      frontHeader->freeList->prevBlock = block;
      block->nextBlock = frontHeader->freeList;
      frontHeader->freeList = block;
   }
   //otherwise insert inside list where needed
   else
   {
      while (current->nextBlock != NULL)
      {
         if (current > block)
            break;
         current = current->nextBlock;
      }
      BLOCK_T* temp = current->nextBlock;
      current->nextBlock = block;
      block->nextBlock = temp;
      block->prevBlock = current;
      if (temp != NULL)
      {
         temp->prevBlock = block;
      }
   }        
}

void RemoveBlock(BLOCK_T* block)
{
   //if removing the only block in the list, set everything to null
   if (block->nextBlock == NULL && block->prevBlock == NULL)
   {
      ((HEADER_T*)gpages->ptr)->freeList = NULL;
      gpages = NULL;
   }
   //if removing the header, set front to next block in line
   else if (block->prevBlock == NULL)
   {
      HEADER_T* mainpage = gpages->ptr;
      ((BLOCK_T*)block->nextBlock)->prevBlock = NULL;
      mainpage->freeList = block->nextBlock;
   }
   //if removing the end, disconnect end of previous block
   else if (block->nextBlock == NULL)
   {
      ((BLOCK_T*)block->prevBlock)->nextBlock = NULL;
   }
   //otherwise bypass given block by setting pointers on both sides
   else
   {
      ((BLOCK_T*)block->nextBlock)->prevBlock = block->prevBlock;
      ((BLOCK_T*)block->prevBlock)->nextBlock = block->nextBlock;
   }
}

BLOCK_T* FindFreeSpace(kma_size_t size)
{
   //size must be at least size of block struct
   if (size < sizeof(BLOCK_T))
   {
      size = sizeof(BLOCK_T);
   }
   //iterate through all blocks to try and find free space
   BLOCK_T* current = ((HEADER_T*)gpages->ptr)->freeList;
   while (current != NULL)
   {
      int currentSize = current->size;
      //block size is greater than size needed
      if (currentSize >= size)
      {
         //if perfect fit or less than minimum size left, take block
         if (currentSize == size || (currentSize - size) < sizeof(BLOCK_T))
         {
            RemoveBlock(current);
            return current;
         }
         //otherwise block needs to be split first
         AddBlock((BLOCK_T*)((int)current + size), currentSize - size);
         RemoveBlock(current);
         return current;
      }
   current = current->nextBlock;
   }
   //if no block with enough free space exists, make another page and return its free list
   MakeNewPage();
   ((HEADER_T*)gpages->ptr)->totalPages++;
   //repeat search
   return FindFreeSpace(size);
}

#endif // KMA_RM