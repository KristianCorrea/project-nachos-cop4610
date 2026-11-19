// processmanager.h
//	Hands out process IDs (SpaceIds) and keeps the global table of PCBs.
//	PID 0 is never handed out, so a SpaceId of 0 is always invalid.
//
//	Project 2, Group 19

#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include "bitmap.h"

#define MAX_PROCESSES 32

class PCB;
class Lock;

class ProcessManager {
  public:
    ProcessManager();
    ~ProcessManager();

    int getPID();                    // reserve a free pid, -1 if none
    void clearPID(int pid);          // release a pid (deletes its PCB)
    void addPCB(PCB *pcb);           // store a PCB under its pid
    PCB *getPCB(int pid);            // NULL if pid is invalid/unused

    // Called when a process ends: its children lose their parent, and
    // children that already exited (zombies nobody can join) are freed.
    void orphanChildren(PCB *parentPCB);

    Lock *lock;                      // protects the table; also the lock
                                     // used with each PCB's exitCond

  private:
    BitMap *pids;
    PCB *table[MAX_PROCESSES];
};

#endif // PROCESSMANAGER_H
