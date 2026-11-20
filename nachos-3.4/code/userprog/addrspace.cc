// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int size;

    pcb = NULL;
    pageTable = NULL;
    numPages = 0;
    valid = FALSE;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    if (noffH.noffMagic != NOFFMAGIC) {
        DEBUG('a', "Not a NOFF executable\n");
        return;                         // valid stays FALSE
    }

    // Required print: segment sizes of the program being loaded
    printf("Loaded Program: %d code | %d data | %d bss\n",
           noffH.code.size, noffH.initData.size, noffH.uninitData.size);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);

    // Get real physical pages from the memory manager (all or nothing).
    // If memory is full, valid stays FALSE so Fork/Exec can return -1.
    if (!allocatePages()) {
        DEBUG('a', "Not enough free memory for %d pages\n", numPages);
        return;
    }

    // Zero every page we own (uninitialized data + stack must start at 0).
    for (unsigned int i = 0; i < numPages; i++)
        bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
              PageSize);

// then, copy in the code and data segments into memory, page by page
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        ReadFile(noffH.code.virtualAddr, executable,
                 noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        ReadFile(noffH.initData.virtualAddr, executable,
                 noffH.initData.size, noffH.initData.inFileAddr);
    }

    valid = TRUE;
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace(AddrSpace *parentSpace)
// 	Used by Fork. Makes a new address space the same size as the
//	parent's and copies every page of the parent's memory into it.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(AddrSpace *parentSpace)
{
    pcb = NULL;
    pageTable = NULL;
    valid = FALSE;
    numPages = parentSpace->numPages;

    if (!allocatePages()) {
        DEBUG('a', "Not enough free memory to copy %d pages\n", numPages);
        return;
    }

    for (unsigned int i = 0; i < numPages; i++) {
        pageTable[i].readOnly = parentSpace->pageTable[i].readOnly;
        bcopy(&(machine->mainMemory[parentSpace->pageTable[i].physicalPage * PageSize]),
              &(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
              PageSize);
    }

    valid = TRUE;
}

//----------------------------------------------------------------------
// AddrSpace::allocatePages
// 	Builds the page table using pages from the memory manager.
//	Checks there is enough free memory first; if an allocation still
//	fails, gives back every page already taken. All or nothing.
//----------------------------------------------------------------------

bool
AddrSpace::allocatePages()
{
    if (numPages == 0 || (int)numPages > memoryManager->numFreePages())
        return FALSE;

    pageTable = new TranslationEntry[numPages];
    for (unsigned int i = 0; i < numPages; i++) {
        int phys = memoryManager->getPage();
        if (phys == -1) {               // lost a race: undo and fail
            for (unsigned int j = 0; j < i; j++)
                memoryManager->clearPage(pageTable[j].physicalPage);
            delete [] pageTable;
            pageTable = NULL;
            return FALSE;
        }
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = phys;
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
    return TRUE;
}

//----------------------------------------------------------------------
// AddrSpace::Translate
// 	Virtual address -> physical address using this space's page table.
//	Returns FALSE (and leaves *physAddr alone) if virtAddr is out of range.
//----------------------------------------------------------------------

bool
AddrSpace::Translate(int virtAddr, int *physAddr)
{
    if (virtAddr < 0 || pageTable == NULL)
        return FALSE;
    unsigned int vpn = (unsigned) virtAddr / PageSize;
    unsigned int offset = (unsigned) virtAddr % PageSize;
    if (vpn >= numPages || !pageTable[vpn].valid)
        return FALSE;
    *physAddr = pageTable[vpn].physicalPage * PageSize + offset;
    return TRUE;
}

//----------------------------------------------------------------------
// AddrSpace::ReadFile
// 	Load "size" bytes from "file" (starting at "fileAddr") into this
//	address space at "virtAddr". Pages may not be contiguous in
//	physical memory, so copy one page-sized chunk at a time.
//----------------------------------------------------------------------

int
AddrSpace::ReadFile(int virtAddr, OpenFile *file, int size, int fileAddr)
{
    char *buffer = new char[size];
    int bytesRead = file->ReadAt(buffer, size, fileAddr);
    int copied = 0;

    while (copied < bytesRead) {
        int physAddr;
        if (!Translate(virtAddr + copied, &physAddr))
            break;
        int offset = (virtAddr + copied) % PageSize;
        int chunk = PageSize - offset;          // room left on this page
        if (chunk > bytesRead - copied)
            chunk = bytesRead - copied;
        bcopy(buffer + copied, &(machine->mainMemory[physAddr]), chunk);
        copied += chunk;
    }

    delete [] buffer;
    return copied;
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Give every physical page back to the memory manager.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    if (pageTable != NULL) {
        for (unsigned int i = 0; i < numPages; i++)
            memoryManager->clearPage(pageTable[i].physicalPage);
        delete [] pageTable;
    }
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
