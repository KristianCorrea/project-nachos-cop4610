// memorymanager.h
//	Hands out physical pages of Nachos main memory to address spaces.
//	One bit per physical page: set = in use, clear = free.
//	All operations are protected by a Lock so they are atomic.
//
//	Project 2, Group 19

#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include "bitmap.h"

class Lock;

class MemoryManager {
  public:
    MemoryManager(int numTotalPages);
    ~MemoryManager();

    int getPage();              // allocate the first free page, -1 if none
    void clearPage(int pageId); // free a page
    int numFreePages();         // how many pages are still free

  private:
    BitMap *physPages;
    Lock *lock;
    int totalPages;
};

#endif // MEMORYMANAGER_H
