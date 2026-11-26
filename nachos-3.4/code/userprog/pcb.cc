// pcb.cc
//	Project 2, Group 19

#include "copyright.h"
#include "system.h"
#include "synch.h"
#include "pcb.h"

PCB::PCB(int pid, Thread *thread, PCB *parentPCB)
{
    processID = pid;
    processThread = thread;
    parent = parentPCB;
    exitStatus = 0;
    hasExited = FALSE;
    killed = FALSE;
    pendingSpace = NULL;
    exitCond = new Condition("PCB exit condition");
}

PCB::~PCB()
{
    delete exitCond;
}
