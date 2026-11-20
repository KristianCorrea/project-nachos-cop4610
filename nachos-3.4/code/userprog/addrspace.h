// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "translate.h"

class PCB;

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"
    AddrSpace(AddrSpace *parentSpace);	// Fork: make an exact copy of
					// another address space
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch 

    bool isValid() { return valid; }	// FALSE if construction failed
					// (bad file or not enough memory)
    unsigned int getNumPages() { return numPages; }

    // Virtual -> physical address. Returns FALSE if virtAddr is invalid.
    bool Translate(int virtAddr, int *physAddr);

    // Copy "size" bytes at "fileAddr" in "file" into this address space
    // starting at virtual address "virtAddr". Returns bytes copied.
    int ReadFile(int virtAddr, OpenFile *file, int size, int fileAddr);

    PCB *pcb;				// process that owns this space

  private:
    TranslationEntry *pageTable;	// Assume linear page table translation
					// for now!
    unsigned int numPages;		// Number of pages in the virtual 
					// address space
    bool valid;

    bool allocatePages();		// get numPages physical pages from
					// the memory manager (all or none)
};

#endif // ADDRSPACE_H
