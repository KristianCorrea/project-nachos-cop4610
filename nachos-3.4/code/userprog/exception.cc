// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"
#include "addrspace.h"
#include "pcb.h"

//----------------------------------------------------------------------
// Project 2 helpers (Group 19)
//----------------------------------------------------------------------

// pid (SpaceId) of the running process, or -1 if it has none
static int
CurrentPid()
{
    if (currentThread->space == NULL || currentThread->space->pcb == NULL)
        return -1;
    return currentThread->space->pcb->getID();
}

// Move the PC past the syscall instruction (4 bytes). Without this the
// user program would make the same system call forever.
static void
IncrementPC()
{
    int pc = machine->ReadRegister(PCReg);
    machine->WriteRegister(PrevPCReg, pc);
    machine->WriteRegister(PCReg, pc + 4);
    machine->WriteRegister(NextPCReg, pc + 8);
}

// Copy a NULL-terminated string from user memory (virtual address
// "virtAddr") into "buf" (at most maxLen bytes incl. the NULL).
// Returns FALSE if the address is bad or the string is too long.
// Needed by Exec to get the file name.
static bool
ReadUserString(int virtAddr, char *buf, int maxLen)
{
    AddrSpace *space = currentThread->space;
    for (int i = 0; i < maxLen; i++) {
        int physAddr;
        if (!space->Translate(virtAddr + i, &physAddr))
            return FALSE;
        buf[i] = machine->mainMemory[physAddr];
        if (buf[i] == '\0')
            return TRUE;
    }
    return FALSE;
}

// End the current process: save the exit status for Join, wake anyone
// joining on it, orphan its children, free its memory and finish the
// kernel thread. Shared by Exit, Kill (on yourself) and bad exceptions.
static void
ExitProcess(int status)
{
    PCB *pcb = currentThread->space->pcb;
    int pid = pcb->getID();

    printf("Process %d exits with %d\n", pid, status);

    processManager->lock->Acquire();
    pcb->exitStatus = status;
    pcb->hasExited = TRUE;
    processManager->orphanChildren(pcb);
    pcb->exitCond->Broadcast(processManager->lock);
    if (pcb->parent == NULL)            // no one can ever Join us
        processManager->clearPID(pid);  // (also deletes the PCB)
    // else: stay as a "zombie" PCB so the parent's Join can read the
    // status. Freed when the parent joins us or exits.
    processManager->lock->Release();

    delete currentThread->space;        // gives pages back
    currentThread->space = NULL;
    currentThread->Finish();
}

//----------------------------------------------------------------------
// System calls. Each returns the value to put in r2.
// TODO markers are the parts still to be split up in the group.
//----------------------------------------------------------------------

static void
SysYield()
{
    currentThread->Yield();
}

static void
SysExit(int status)
{
    ExitProcess(status);
}

// First thing a forked process runs (in its new kernel thread).
// Loads the child's registers and page table into the machine,
// then starts running user code at the forked function.
static void
ForkedProcessStart(int unused)
{
    currentThread->RestoreUserState();
    currentThread->space->RestoreState();
    machine->Run();                     // never returns
    ASSERT(FALSE);
}

static int
SysFork(int funcAddr)
{
    AddrSpace *parentSpace = currentThread->space;
    PCB *parentPCB = parentSpace->pcb;
    int physAddr;

    // The function to run must be a real address in our program.
    if (!parentSpace->Translate(funcAddr, &physAddr))
        return -1;

    // 1. Get a pid for the child.
    processManager->lock->Acquire();
    int pid = processManager->getPID();
    processManager->lock->Release();
    if (pid == -1)
        return -1;                      // process table is full

    // 2. Copy the parent's address space (fails if memory is full).
    AddrSpace *childSpace = new AddrSpace(parentSpace);
    if (!childSpace->isValid()) {
        delete childSpace;
        processManager->lock->Acquire();
        processManager->clearPID(pid);
        processManager->lock->Release();
        return -1;
    }

    // 3. New kernel thread + PCB for the child.
    Thread *childThread = new Thread("forked process");
    childThread->space = childSpace;
    PCB *childPCB = new PCB(pid, childThread, parentPCB);
    childSpace->pcb = childPCB;
    processManager->lock->Acquire();
    processManager->addPCB(childPCB);
    processManager->lock->Release();

    // 4. Child's registers = parent's registers, except it starts at
    //    funcAddr. Its return address (r31) stays the parent's, so when
    //    func returns, the child goes back to the code right after the
    //    Fork() call. Trick: change the machine registers, save them
    //    into the child thread, then put the parent's back.
    int saved[NumTotalRegs];
    for (int i = 0; i < NumTotalRegs; i++)
        saved[i] = machine->ReadRegister(i);

    machine->WriteRegister(PCReg, funcAddr);
    machine->WriteRegister(NextPCReg, funcAddr + 4);
    machine->WriteRegister(PrevPCReg, funcAddr);
    childThread->SaveUserState();

    for (int i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, saved[i]);

    // Required print
    printf("Process %d Fork: start at address 0x%x with %d pages memory\n",
           parentPCB->getID(), funcAddr, childSpace->getNumPages());

    // 5. Put the child on the ready list. It starts in ForkedProcessStart.
    childThread->Fork(ForkedProcessStart, 0);

    return pid;                         // parent gets the child's pid
}

#define MAX_FILENAME 128

// Exec replaces the current process's program with a new one loaded
// from a file. Same process, same pid, new code and memory.
// Returns -1 on failure (the old program keeps running).
// Returns 0 on success: the new program is set up to start at its
// beginning, so the caller must NOT touch r2 or the PC afterwards.
static int
SysExec(int nameAddr)
{
    char name[MAX_FILENAME];

    // 1. Copy the file name out of user memory.
    if (!ReadUserString(nameAddr, name, MAX_FILENAME))
        return -1;

    // Required print
    printf("Exec Program: %d loading %s\n", CurrentPid(), name);

    // 2. Open the file and check it's a real program that will fit.
    //    The old program's pages get freed first, so count them as free.
    OpenFile *executable = fileSystem->Open(name);
    if (executable == NULL)
        return -1;

    AddrSpace *oldSpace = currentThread->space;
    int needed = AddrSpace::PagesNeeded(executable);
    int available = memoryManager->numFreePages() + (int) oldSpace->getNumPages();
    if (needed == -1 || needed > available) {
        delete executable;
        return -1;
    }

    // 3. Swap the old program out for the new one. Keep the same PCB,
    //    so the pid, parent and anyone Joining on us stay the same.
    PCB *pcb = oldSpace->pcb;
    delete oldSpace;                    // frees the old pages
    AddrSpace *newSpace = new AddrSpace(executable);
    delete executable;
    newSpace->pcb = pcb;
    currentThread->space = newSpace;

    if (!newSpace->isValid())           // old program is gone, can't
        ExitProcess(-1);                // go back to it, so end process

    // 4. Fresh registers (PC = 0) and the new page table.
    newSpace->InitRegisters();
    newSpace->RestoreState();
    return 0;
}

static int
SysJoin(int pid)
{
    // TODO (Join): must be our child; Wait on its exitCond until
    // hasExited, return exitStatus, then free its PCB. -1 on error.
    return -1;
}

static int
SysKill(int pid)
{
    // TODO (Kill): print "Process [pid] killed process [killed-pid]"
    // or "Process [pid] cannot kill process [killed-pid]: doesn't exist".
    // If pid is ourselves, ExitProcess. Otherwise remove the thread
    // from the ready list, free its memory and PCB. 0 ok, -1 bad pid.
    return -1;
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int arg1 = machine->ReadRegister(4);

    if (which != SyscallException) {
        // A bad user program must never crash Nachos: kill the process.
        printf("Unexpected user mode exception %d %d in process %d\n",
               which, type, CurrentPid());
        ExitProcess(-1);
        return;
    }

    switch (type) {
      case SC_Halt:
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
        break;

      case SC_Exit:
        printf("System Call: %d invoked Exit\n", CurrentPid());
        SysExit(arg1);                  // never returns
        break;

      case SC_Yield:
        printf("System Call: %d invoked Yield\n", CurrentPid());
        IncrementPC();                  // before yielding, so we resume
        SysYield();                     // at the next instruction
        return;

      case SC_Fork:
        printf("System Call: %d invoked Fork\n", CurrentPid());
        machine->WriteRegister(2, SysFork(arg1));
        break;

      case SC_Exec:
        printf("System Call: %d invoked Exec\n", CurrentPid());
        if (SysExec(arg1) == 0)
            return;                     // new program starts at PC 0:
                                        // don't change r2 or the PC
        machine->WriteRegister(2, -1);  // failed: old program continues
        break;

      case SC_Join:
        printf("System Call: %d invoked Join\n", CurrentPid());
        machine->WriteRegister(2, SysJoin(arg1));
        break;

      case SC_Kill:
        printf("System Call: %d invoked Kill\n", CurrentPid());
        machine->WriteRegister(2, SysKill(arg1));
        break;

      default:
        printf("Unexpected system call %d in process %d\n", type, CurrentPid());
        machine->WriteRegister(2, -1);  // fail the call, don't crash
        break;
    }

    IncrementPC();
}
