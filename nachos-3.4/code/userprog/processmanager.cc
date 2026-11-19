// processmanager.cc
//	Project 2, Group 19

#include "copyright.h"
#include "system.h"
#include "synch.h"
#include "pcb.h"
#include "processmanager.h"

ProcessManager::ProcessManager()
{
    pids = new BitMap(MAX_PROCESSES);
    pids->Mark(0);                   // reserve 0 so pids start at 1
    for (int i = 0; i < MAX_PROCESSES; i++)
        table[i] = NULL;
    lock = new Lock("ProcessManager lock");
}

ProcessManager::~ProcessManager()
{
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (table[i] != NULL)
            delete table[i];
    delete pids;
    delete lock;
}

int
ProcessManager::getPID()
{
    return pids->Find();             // -1 when the table is full
}

void
ProcessManager::clearPID(int pid)
{
    if (pid <= 0 || pid >= MAX_PROCESSES)
        return;
    if (table[pid] != NULL) {
        delete table[pid];
        table[pid] = NULL;
    }
    if (pids->Test(pid))
        pids->Clear(pid);
}

void
ProcessManager::addPCB(PCB *pcb)
{
    int pid = pcb->getID();
    if (pid > 0 && pid < MAX_PROCESSES)
        table[pid] = pcb;
}

PCB *
ProcessManager::getPCB(int pid)
{
    if (pid <= 0 || pid >= MAX_PROCESSES)
        return NULL;
    return table[pid];
}

void
ProcessManager::orphanChildren(PCB *parentPCB)
{
    for (int i = 1; i < MAX_PROCESSES; i++) {
        PCB *child = table[i];
        if (child != NULL && child->parent == parentPCB) {
            child->parent = NULL;
            if (child->hasExited)    // zombie: no one can Join it now
                clearPID(i);
        }
    }
}
