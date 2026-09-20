// threadtest.cc
//	Simple test case for the threads assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"

#if defined(HW1_SEMAPHORES) || defined(HW1_LOCKS)
#include "synch.h"
#endif

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, reading/writing a shared variable and yielding
//	the CPU to another ready thread each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

int SharedVariable;

#ifdef HW1_SEMAPHORES
Semaphore *SharedVariableSem = new Semaphore("SharedVariableSem", 1);
#endif

#ifdef HW1_LOCKS
Lock *SharedVariableLock = new Lock("SharedVariableLock");
#endif

#if defined(HW1_SEMAPHORES) || defined(HW1_LOCKS)
Semaphore *BarrierMutex = new Semaphore("BarrierMutex", 1);
Semaphore *BarrierSem = new Semaphore("BarrierSem", 0);
int numThreadsTotal = 0;
int numArrived = 0;
#endif

void
SimpleThread(int which)
{
    int num, val;

    for (num = 0; num < 5; num++) {
#ifdef HW1_SEMAPHORES
        SharedVariableSem->P();
#endif
#ifdef HW1_LOCKS
        SharedVariableLock->Acquire();
#endif
        val = SharedVariable;
        printf("*** thread %d sees value %d\n", which, val);
        currentThread->Yield();
        SharedVariable = val+1;
#ifdef HW1_SEMAPHORES
        SharedVariableSem->V();
#endif
#ifdef HW1_LOCKS
        SharedVariableLock->Release();
#endif
        currentThread->Yield();
    }

#if defined(HW1_SEMAPHORES) || defined(HW1_LOCKS)
    // One-shot barrier: wait until every thread has finished the loop
    // so they all observe the same final SharedVariable value.
    BarrierMutex->P();
    numArrived++;
    if (numArrived == numThreadsTotal) {
        for (int i = 0; i < numThreadsTotal; i++)
            BarrierSem->V();
    }
    BarrierMutex->V();
    BarrierSem->P();
#endif

    val = SharedVariable;
    printf("Thread %d sees final value %d\n", which, val);
}

//----------------------------------------------------------------------
// ThreadTest
// 	Fork n threads, each running SimpleThread, plus have the
//	original (main) thread run SimpleThread(0) as well.
//----------------------------------------------------------------------

void
ThreadTest(int n)
{
    DEBUG('t', "Entering ThreadTest");

#if defined(HW1_SEMAPHORES) || defined(HW1_LOCKS)
    numThreadsTotal = n + 1;	// n forked threads + main
    numArrived = 0;
#endif

    for (int i = 1; i <= n; i++) {
        char *name = new char[32];
        sprintf(name, "forked thread %d", i);
        Thread *t = new Thread(name);
        t->Fork(SimpleThread, i);
    }

    SimpleThread(0);
}
