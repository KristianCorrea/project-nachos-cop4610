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

    // Give the memory back FIRST, so a parent that wakes up from Join
    // can immediately use it (e.g. to Fork again). Set space to NULL
    // before deleting so a context switch never touches freed memory.
    AddrSpace *space = currentThread->space;
    currentThread->space = NULL;
    delete space;

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
// Attaches the child's memory to the thread, loads its registers and
// page table into the machine, then runs user code at the forked
// function. Interrupts are off during setup: if a timer switch happened
// before the registers were loaded, Nachos would save the CPU's current
// (parent's) registers over the child's prepared ones.
static void
ForkedProcessStart(int pcbAddr)
{
    PCB *pcb = (PCB *) pcbAddr;

    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    currentThread->space = pcb->pendingSpace;
    pcb->pendingSpace = NULL;
    currentThread->RestoreUserState();
    currentThread->space->RestoreState();
    (void) interrupt->SetLevel(oldLevel);

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

    // 3. New kernel thread + PCB for the child. The memory is parked in
    //    the PCB and attached when the child first runs (see
    //    ForkedProcessStart for why).
    Thread *childThread = new Thread("forked process");
    PCB *childPCB = new PCB(pid, childThread, parentPCB);
    childPCB->pendingSpace = childSpace;
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
    childThread->Fork(ForkedProcessStart, (int) childPCB);

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
    currentThread->space = NULL;        // no switch may touch it now
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

// Join waits until child "pid" has finished and returns its exit
// status. Only a parent can Join its own child. -1 on error.
static int
SysJoin(int pid)
{
    PCB *me = currentThread->space->pcb;

    processManager->lock->Acquire();
    PCB *child = processManager->getPCB(pid);
    if (child == NULL || child->parent != me) {
        processManager->lock->Release();
        return -1;                      // not a valid child of ours
    }

    // Sleep until the child exits. Exit broadcasts on exitCond.
    while (!child->hasExited)
        child->exitCond->Wait(processManager->lock);

    int status = child->exitStatus;
    processManager->clearPID(pid);      // done with the zombie PCB
    processManager->lock->Release();

    if (me->killed)                     // someone Killed us while we
        ExitProcess(-1);                // were waiting
    return status;
}

// Kill ends process "pid". 0 on success, -1 if pid doesn't exist.
static int
SysKill(int pid)
{
    int myPid = CurrentPid();

    processManager->lock->Acquire();
    PCB *target = processManager->getPCB(pid);
    if (target == NULL || target->hasExited) {
        processManager->lock->Release();
        printf("Process %d cannot kill process %d: doesn't exist\n",
               myPid, pid);
        return -1;
    }
    printf("Process %d killed process %d\n", myPid, pid);

    // Killing yourself is just Exit.
    if (pid == myPid) {
        processManager->lock->Release();
        ExitProcess(-1);                // never returns
    }

    Thread *victim = target->processThread;

    // If it's blocked (e.g. waiting in Join), we can't safely pull it
    // out of that wait. Mark it; it ends itself as soon as it wakes.
    if (victim->getStatus() == BLOCKED) {
        target->killed = TRUE;
        processManager->lock->Release();
        return 0;
    }

    // Otherwise it's on the ready list: end it right now, the same way
    // Exit would, but from the outside. Memory goes back first (see
    // ExitProcess for why).
    scheduler->RemoveThread(victim);
    AddrSpace *space = victim->space;
    if (space == NULL) {                // forked but never ran yet
        space = target->pendingSpace;
        target->pendingSpace = NULL;
    }
    victim->space = NULL;
    delete space;
    target->exitStatus = -1;
    target->hasExited = TRUE;
    processManager->orphanChildren(target);
    target->exitCond->Broadcast(processManager->lock);
    if (target->parent == NULL)         // nobody can Join it
        processManager->clearPID(pid);  // (also deletes the PCB)
    processManager->lock->Release();

    delete victim;                      // free its kernel thread
    return 0;
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
