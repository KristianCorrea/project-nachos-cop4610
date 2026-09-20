// threadtest.cc
//	Simple test case for the threads assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"

#ifdef HW1_SEMAPHORES
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

void
SimpleThread(int which)
{
    int num, val;

    for (num = 0; num < 5; num++) {
#ifdef HW1_SEMAPHORES
        SharedVariableSem->P();
#endif
        val = SharedVariable;
        printf("*** thread %d sees value %d\n", which, val);
        currentThread->Yield();
        SharedVariable = val+1;
#ifdef HW1_SEMAPHORES
        SharedVariableSem->V();
#endif
        currentThread->Yield();
    }
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

    for (int i = 1; i <= n; i++) {
        char *name = new char[32];
        sprintf(name, "forked thread %d", i);
        Thread *t = new Thread(name);
        t->Fork(SimpleThread, i);
    }

    SimpleThread(0);
}
