// elevator.cc
// Solution to the Nachos elevator simulation (Exercise 4).
//
// The elevator serves up to MAX_CAPACITY people at once, picking up
// and dropping off passengers as it passes their floors.

#include "copyright.h"
#include "system.h"

#ifdef HW1_ELEVATOR

#include "synch.h"

#define MAX_CAPACITY 5

struct PersonThread {
    int id;
    int atFloor;
    int toFloor;
};

struct ElevatorThread {
    int numFloors;
    int currentFloor;
    int numPeopleIn;
};

struct PersonRequest {
    int id;
    int toFloor;
    Semaphore *boarded;
    Semaphore *arrived;
};

static ElevatorThread elevatorInfo;
static Lock *elevatorLock;
static Condition *elevatorCond;
static List **waitingAtFloor;
static PersonRequest *riders[MAX_CAPACITY];
static int nextPersonId = 0;

//----------------------------------------------------------------------
// FindNearestDemandFloor
//  Returns the closest floor (by distance from currentFloor) where
//  someone is waiting to board (only if there is free capacity), or a
//  rider wants to get off. Returns -1 if there is no demand anywhere.
//
//  When the car is full, waiting queues must not count as demand at the
//  current floor — otherwise we spin forever servicing a full car that
//  cannot board anyone and has no drop-offs here.
//----------------------------------------------------------------------

static int
FindNearestDemandFloor()
{
    int best = -1, bestDist = 1000000;
    bool hasRoom = (elevatorInfo.numPeopleIn < MAX_CAPACITY);

    for (int f = 1; f <= elevatorInfo.numFloors; f++) {
        bool demand = false;

        for (int i = 0; i < MAX_CAPACITY; i++) {
            if (riders[i] != NULL && riders[i]->toFloor == f) {
                demand = true;
                break;
            }
        }
        if (!demand && hasRoom && !waitingAtFloor[f]->IsEmpty())
            demand = true;

        if (demand) {
            int dist = (f > elevatorInfo.currentFloor)
                         ? (f - elevatorInfo.currentFloor)
                         : (elevatorInfo.currentFloor - f);
            if (dist < bestDist) {
                bestDist = dist;
                best = f;
            }
        }
    }
    return best;
}

//----------------------------------------------------------------------
// ServiceCurrentFloor
//  Drop off any riders whose destination is the current floor, then
//  board waiting passengers (up to capacity).
//----------------------------------------------------------------------

static void
ServiceCurrentFloor()
{
    int floor = elevatorInfo.currentFloor;

    for (int i = 0; i < MAX_CAPACITY; i++) {
        if (riders[i] != NULL && riders[i]->toFloor == floor) {
            printf("Person %d got out of the elevator.\n", riders[i]->id);
            riders[i]->arrived->V();
            riders[i] = NULL;
            elevatorInfo.numPeopleIn--;
        }
    }

    while (elevatorInfo.numPeopleIn < MAX_CAPACITY &&
           !waitingAtFloor[floor]->IsEmpty()) {
        PersonRequest *pr = (PersonRequest *)waitingAtFloor[floor]->Remove();

        for (int i = 0; i < MAX_CAPACITY; i++) {
            if (riders[i] == NULL) {
                riders[i] = pr;
                break;
            }
        }
        elevatorInfo.numPeopleIn++;
        printf("Person %d got into the elevator.\n", pr->id);
        pr->boarded->V();
    }
}

//----------------------------------------------------------------------
// ElevatorThreadFunc
//  The elevator's main loop: wait for demand, move one floor at a
//  time toward the nearest requested floor, servicing every floor
//  it stops at along the way.
//----------------------------------------------------------------------

static void
ElevatorThreadFunc(int unused)
{
    elevatorLock->Acquire();

    printf("Elevator arrives on floor %d.\n", elevatorInfo.currentFloor);
    ServiceCurrentFloor();

    while (true) {
        int target = FindNearestDemandFloor();
        while (target == -1) {
            elevatorCond->Wait(elevatorLock);
            target = FindNearestDemandFloor();
        }

        if (target == elevatorInfo.currentFloor) {
            ServiceCurrentFloor();
            continue;
        }

        int step = (target > elevatorInfo.currentFloor) ? 1 : -1;

        elevatorLock->Release();
        for (volatile int i = 0; i < 50; i++)
            ;                       // 50 ticks to move one floor
        currentThread->Yield();
        elevatorLock->Acquire();

        elevatorInfo.currentFloor += step;
        printf("Elevator arrives on floor %d.\n", elevatorInfo.currentFloor);
        ServiceCurrentFloor();
    }
}

//----------------------------------------------------------------------
// PersonThreadFunc
//  A student/faculty member: request the elevator, wait to board,
//  ride to the destination floor, then finish.
//----------------------------------------------------------------------

static void
PersonThreadFunc(int arg)
{
    PersonThread *p = (PersonThread *)arg;

    printf("Person %d wants to go to floor %d from floor %d.\n",
           p->id, p->toFloor, p->atFloor);

    PersonRequest *req = new PersonRequest;
    req->id = p->id;
    req->toFloor = p->toFloor;
    req->boarded = new Semaphore("boarded", 0);
    req->arrived = new Semaphore("arrived", 0);

    elevatorLock->Acquire();
    waitingAtFloor[p->atFloor]->Append((void *)req);
    elevatorCond->Signal(elevatorLock);
    elevatorLock->Release();

    req->boarded->P();
    req->arrived->P();

    delete req->boarded;
    delete req->arrived;
    delete req;
    delete p;
}

//----------------------------------------------------------------------
// Elevator
//  Starts the elevator thread serving "numFloors" floors.
//----------------------------------------------------------------------

void
Elevator(int numFloors)
{
    elevatorLock = new Lock("elevatorLock");
    elevatorCond = new Condition("elevatorCond");

    elevatorInfo.numFloors = numFloors;
    elevatorInfo.currentFloor = 1;
    elevatorInfo.numPeopleIn = 0;

    waitingAtFloor = new List*[numFloors + 1];
    for (int f = 1; f <= numFloors; f++)
        waitingAtFloor[f] = new List;

    for (int i = 0; i < MAX_CAPACITY; i++)
        riders[i] = NULL;

    Thread *t = new Thread("elevator thread");
    t->Fork(ElevatorThreadFunc, 0);
}

//----------------------------------------------------------------------
// ArrivingGoingFromTo
//  Creates a new person/faculty thread that requests the elevator
//  from "atFloor" to "toFloor".
//----------------------------------------------------------------------

void
ArrivingGoingFromTo(int atFloor, int toFloor)
{
    PersonThread *p = new PersonThread;
    p->id = nextPersonId++;
    p->atFloor = atFloor;
    p->toFloor = toFloor;

    char *name = new char[32];
    sprintf(name, "person %d", p->id);

    Thread *t = new Thread(name);
    t->Fork(PersonThreadFunc, (int)p);
}

#endif // HW1_ELEVATOR
