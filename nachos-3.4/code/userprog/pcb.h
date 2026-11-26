// pcb.h
//	Process Control Block: everything the kernel tracks about one
//	user process.
//
//	Project 2, Group 19

#ifndef PCB_H
#define PCB_H

class Thread;
class Condition;
class AddrSpace;

class PCB {
  public:
    PCB(int pid, Thread *thread, PCB *parent);
    ~PCB();

    int getID() { return processID; }

    Thread *processThread;   // kernel thread running this process
    PCB *parent;             // NULL if no parent (or parent exited)
    int exitStatus;          // value passed to Exit(), returned by Join()
    bool hasExited;          // TRUE once the process called Exit / was killed
    bool killed;             // Kill() was called while it was blocked;
                             // it ends itself as soon as it wakes up
    AddrSpace *pendingSpace; // Fork: the child's memory, waiting to be
                             // attached to its thread when it first runs
    Condition *exitCond;     // Join() waits here, Exit() broadcasts
                             // (used with processManager's lock)

  private:
    int processID;           // the SpaceId
};

#endif // PCB_H
