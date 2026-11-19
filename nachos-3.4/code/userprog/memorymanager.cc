// memorymanager.cc
//	Physical page allocator for multiprogramming.
//
//	Project 2, Group 19

#include "copyright.h"
#include "system.h"
#include "synch.h"
#include "memorymanager.h"

MemoryManager::MemoryManager(int numTotalPages)
{
    totalPages = numTotalPages;
    physPages = new BitMap(numTotalPages);
    lock = new Lock("MemoryManager lock");
}

MemoryManager::~MemoryManager()
{
    delete physPages;
    delete lock;
}

// Returns the index of a free physical page and marks it used,
// or -1 if memory is full.
int
MemoryManager::getPage()
{
    lock->Acquire();
    int page = physPages->Find();   // Find() also marks the bit
    lock->Release();
    return page;
}

void
MemoryManager::clearPage(int pageId)
{
    if (pageId < 0 || pageId >= totalPages)
        return;
    lock->Acquire();
    if (physPages->Test(pageId))
        physPages->Clear(pageId);
    lock->Release();
}

int
MemoryManager::numFreePages()
{
    lock->Acquire();
    int n = physPages->NumClear();
    lock->Release();
    return n;
}
