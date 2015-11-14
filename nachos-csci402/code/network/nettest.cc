// nettest.cc
//	Test out message delivery between two "Nachos" machines,
//	using the Post Office to coordinate delivery.
//
//	Two caveats:
//	  1. Two copies of Nachos must be running, with machine ID's 0 and 1:
//		./nachos -m 0 -o 1 &
//		./nachos -m 1 -o 0 &
//
//	  2. You need an implementation of condition variables,
//	     which is *not* provided as part of the baseline threads
//	     implementation.  The Post Office won't work without
//	     a correct implementation of condition variables.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"
#include <sstream>
#include <string>

// Test out message delivery, by doing the following:
//	1. send a message to the machine with ID "farAddr", at mail box #0
//	2. wait for the other machine's message to arrive (in our mailbox #0)
//	3. send an acknowledgment for the other machine's message
//	4. wait for an acknowledgement from the other machine to our
//	    original message

#define MAX_MON_COUNT 50

void
MailTest(int farAddr)
{
    PacketHeader outPktHdr, inPktHdr;
    MailHeader outMailHdr, inMailHdr;
    char *data = "Hello there!";
    char *ack = "Got it!";
    char buffer[MaxMailSize];

    // construct packet, mail header for original message
    // To: destination machine, mailbox 0
    // From: our machine, reply to: mailbox 1
    outPktHdr.to = farAddr;
    outMailHdr.to = 0;
    outMailHdr.from = 1;
    outMailHdr.length = strlen(data) + 1;

    // Send the first message
    bool success = postOffice->Send(outPktHdr, outMailHdr, data);

    if ( !success ) {
        printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
        interrupt->Halt();
    }

    // Wait for the first message from the other machine
    postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
    printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
    fflush(stdout);

    // Send acknowledgement to the other machine (using "reply to" mailbox
    // in the message that just arrived
    outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;
    outMailHdr.length = strlen(ack) + 1;
    success = postOffice->Send(outPktHdr, outMailHdr, ack);

    if ( !success ) {
        printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
        interrupt->Halt();
    }

    // Wait for the ack from the other machine to the first message we sent.
    postOffice->Receive(1, &inPktHdr, &inMailHdr, buffer);
    printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
    fflush(stdout);

    // Then we're done!
    interrupt->Halt();
}

// ++++++++++++++++++++++++++++ Declarations ++++++++++++++++++++++++++++

// abstract concept.  Even thought there are no actual threads, it makes it easier
// to port from the synch.cc file
struct ServerThread{
  int machineId;
  int mailboxNum;
};

// check if ServerThread is "null".  again, this is an abstract concept
// in order to accommodate a direct mapping from the synch.cc file
bool threadIsNull(ServerThread thread) {
    if(thread.machineId != -1) {
        return FALSE;
    }

    if(thread.mailboxNum != -1) {
        return FALSE;
    }

    return TRUE;
}

// operator overload for == on ServerThread
// since we can't compare structs easily
bool operator==(const ServerThread& t1, const ServerThread& t2) {
    if(t1.machineId != t2.machineId) {
        return false;
    }

    if(t1.mailboxNum != t2.mailboxNum) {
        return false;
    }

    return true;
}

// lock implementation for server
struct ServerLock {
    bool deleteFlag;
    bool isDeleted;

    enum LockStatus {FREE, BUSY};
    LockStatus lockStatus;
    char* name;
    MailBox* waitQueue;
    int queueSize;
    ServerThread lockOwner;
};

// operator overload for == on ServerLock
// since we can't compare structs easily
bool operator==(const ServerLock& l1, const ServerLock& l2) {
    if(l1.deleteFlag != l2.deleteFlag) {
        return false;
    }

    if(l1.isDeleted != l2.isDeleted) {
        return false;
    }

    if(l1.lockStatus != l2.lockStatus) {
        return false;
    }

    if(l1.waitQueue != l2.waitQueue) {
        return false;
    }

    if(!(l1.lockOwner == l2.lockOwner)) {
        return false;
    }

    return true;
}

// check if ServerLock is "null".  again, this is an abstract concept
// in order to accommodate a direct mapping from the synch.cc file
bool lockIsNull(ServerLock lock) {
    if(lock.deleteFlag != FALSE) {
        return FALSE;
    }

    if(lock.isDeleted != FALSE) {
        return FALSE;
    }

    if(lock.lockStatus != lock.FREE) {
        return FALSE;
    }

    if(lock.waitQueue != NULL) {
        return FALSE;
    }

    if(!threadIsNull(lock.lockOwner)) {
        return FALSE;
    }

    return TRUE;
}

// set ServerLock to "null".  again, this is an abstract concept
// in order to accommodate a direct mapping from the synch.cc file
void setLockToNull(ServerLock& lock) {
    lock.deleteFlag = FALSE;
    lock.isDeleted = FALSE;

    lock.lockStatus = lock.FREE;
    lock.waitQueue = NULL;
    lock.lockOwner.machineId = -1;
    lock.lockOwner.mailboxNum = -1;
}

// monitor implementation on the server
struct ServerMon {
    bool deleteFlag;
    bool isDeleted;
};

// condition implementation on the server
struct ServerCond {
    bool deleteFlag;
    bool isDeleted;

    char* name;
    int waitingLockIndex;
    List *waitQueue;
};

// arrays of all of the monitor variables
ServerLock serverLocks[MAX_MON_COUNT];
ServerMon serverMons[MAX_MON_COUNT];
ServerCond serverConds[MAX_MON_COUNT];

// server counts of the locks, mons, and conds
int serverLockCount = 0;
int serverMonCount = 0;
int serverCondCount = 0;

// ++++++++++++++++++++++++++++ Validation ++++++++++++++++++++++++++++

// make sure that we were handed a valid lock
bool validateLockIndex(int lockIndex) {
    if (lockIndex < 0 || lockIndex >= serverLockCount){ // check if index is in valid range
      DEBUG('l',"    Lock::Lock number %d invalid, thread can't acquire-----------------------\n", lockIndex);
      return false;
    }
    if (serverLocks[lockIndex].isDeleted == TRUE){ // check if lock is deleted
  		DEBUG('l',"    Lock::Lock number %d already destroyed, thread can't acquire-----------------------\n", lockIndex);
  		return false;
  	}
    return true;
}

// make sure that we were handed a valid monitor
bool validateMonitorIndex(int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= serverMonCount){ // check if index is in valid range
      DEBUG('l',"    Mon::Mon number %d invalid\n", monitorIndex);
      return false;
    }
    if (serverMons[monitorIndex].isDeleted == TRUE){ // check if lock is deleted
      DEBUG('l',"    Mon::Mon number %d already destroyed\n", monitorIndex);
      return false;
    }
    return true;
}

// make sure that we were handed a valid condition
bool validateConditionIndex(int conditionIndex) {
    if (conditionIndex < 0 || conditionIndex >= serverCondCount){ // check if index is in valid range
      DEBUG('l',"    Cond::Cond number %d invalid\n", conditionIndex);
      return false;
    }
    if (serverConds[conditionIndex].isDeleted == TRUE){ // check if lock is deleted
      DEBUG('l',"    Cond::Cond number %d already destroyed\n", conditionIndex);
      return false;
    }
    return true;
}

// +++++++++++++++++ UTILITY SERVER METHODS +++++++++++++++++

// abstract method to send message to the client from the server
void sendMessageToClient(char* data, PacketHeader &pktHdr, MailHeader &mailHdr) {
    pktHdr.to = pktHdr.from;
    int clientMailbox = mailHdr.to;
    mailHdr.to = mailHdr.from;
    mailHdr.from = clientMailbox;
    mailHdr.length = strlen(data) + 1;

    bool success = postOffice->Send(pktHdr, mailHdr, data);

    if ( !success ) {
        printf("Release::The first postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
        interrupt->Halt();
    }
}

// abstract method to send message to the client from the server
void sendCreateEntityMessage(stringstream &ss, PacketHeader &pktHdr, MailHeader &mailHdr) {
    const char* tempChar = ss.str().c_str();
    cout << "tempChar: " << ss.str() << endl;
    char replyBuffer[MaxMailSize];
    for(unsigned int i = 0; i < strlen(tempChar); ++i) {
        replyBuffer[i] = tempChar[i];
    }

    replyBuffer[strlen(tempChar)] = '\0';

    //Send a reply (maybe)
    pktHdr.to = pktHdr.from;
    int clientMailbox = mailHdr.to;
    mailHdr.to = mailHdr.from;
    mailHdr.from = clientMailbox;
    mailHdr.length = strlen(tempChar) + 1;
    bool success = postOffice->Send(pktHdr, mailHdr, replyBuffer);

    if ( !success ) {
        printf("The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
        interrupt->Halt();
    }
}

// ++++++++++++++++++++++++++++ Locks ++++++++++++++++++++++++++++

// create lock server call
int CreateLock_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr) {
    serverLocks[serverLockCount].deleteFlag = FALSE;
    serverLocks[serverLockCount].isDeleted = FALSE;
    serverLocks[serverLockCount].lockStatus = serverLocks[serverLockCount].FREE;
    serverLocks[serverLockCount].name = name;
    serverLocks[serverLockCount].lockOwner.machineId = pktHdr.from;
    serverLocks[serverLockCount].lockOwner.mailboxNum = mailHdr.from;

    int currentLockIndex = serverLockCount;
    ++serverLockCount;

    return currentLockIndex;
}

// acquire lock server call
void Acquire_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    cout << lockIndex << " " << validateLockIndex(lockIndex) <<endl;
    if(!validateLockIndex(lockIndex)) {
        sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
        return;
    }

    ServerThread serverCurrentThread;
    serverCurrentThread.machineId = pktHdr.from; // this is essentailly the server machineId
    serverCurrentThread.mailboxNum = mailHdr.from; // this is the mailbox that the mail came from since it's equal to client mailbox

    if(serverLocks[lockIndex].lockStatus == serverLocks[lockIndex].BUSY && serverCurrentThread == serverLocks[lockIndex].lockOwner) //current thread is lock owner
    {
        sendMessageToClient("Lock is yours. Done nothing.", pktHdr, mailHdr);
        return;
    }

    if(serverLocks[lockIndex].lockStatus == serverLocks[lockIndex].FREE) //lock is available
    {
        //I can have the lock
        cout << "***********************" << endl;
        serverLocks[lockIndex].lockStatus = serverLocks[lockIndex].BUSY; //make state BUSY
        serverLocks[lockIndex].lockOwner = serverCurrentThread; //make myself the owner
        serverLocks[lockIndex].lockOwner.machineId;
        serverLocks[lockIndex].lockOwner.mailboxNum;
        sendMessageToClient("You got the lock!", pktHdr, mailHdr);
        return;
    }
    else //lock is busy
    {
      pktHdr.to = pktHdr.from;
      int temp = mailHdr.to;
      mailHdr.to = mailHdr.from;
        mailHdr.from = temp;

        serverLocks[lockIndex].waitQueue->Put(pktHdr, mailHdr, "You got the lock!"); //Put current thread on the lock’s waitQueue
        ++serverLocks[lockIndex].queueSize;
    }
}

// create release server call
void Release_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if(!validateLockIndex(lockIndex)) {
      cout << lockIndex << endl;
      sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
      return;
    }

    ServerThread serverCurrentThread;
    serverCurrentThread.machineId = pktHdr.from; // this is essentailly the server machineId
    serverCurrentThread.mailboxNum = mailHdr.from; // this is the mailbox that the mail came from since it's equal to client mailbox

    if(!(serverCurrentThread == serverLocks[lockIndex].lockOwner)) //current thread is not lock owner
    {
        sendMessageToClient("No permission to release!", pktHdr, mailHdr);
        return;
    }

    if (serverLocks[lockIndex].lockStatus = serverLocks[lockIndex].FREE ){
      sendMessageToClient("Lock is free, nothing is done.", pktHdr, mailHdr);
      return;
    }

    pktHdr.to = serverLocks[lockIndex].lockOwner.machineId;
    mailHdr.to = serverLocks[lockIndex].lockOwner.mailboxNum;

    if (serverLocks[lockIndex].deleteFlag == TRUE){
      serverLocks[lockIndex].isDeleted = TRUE;
      delete serverLocks[lockIndex].waitQueue;
      delete serverLocks[lockIndex].name;
      sendMessageToClient("Realesed, the lock is also deleted.", pktHdr, mailHdr);
      return;
    }
    else if(serverLocks[lockIndex].queueSize > 0) //lock waitQueue is not empty
    {
      sendMessageToClient("Released. Another thread took it.", pktHdr, mailHdr);
        char data[MaxMailSize];
        serverLocks[lockIndex].waitQueue->Get(&pktHdr,&mailHdr, data);
        --(serverLocks[lockIndex].queueSize);
        serverLocks[lockIndex].lockOwner.machineId = pktHdr.to; //unset ownership
        serverLocks[lockIndex].lockOwner.mailboxNum = mailHdr.to; //unset ownership
        postOffice->Send(pktHdr, mailHdr, data);
    }
    else
    {
        serverLocks[lockIndex].lockStatus = serverLocks[lockIndex].FREE; //make lock available
        serverLocks[lockIndex].lockOwner.machineId = -1; //unset ownership
        serverLocks[lockIndex].lockOwner.mailboxNum = -1; //unset ownership
        sendMessageToClient("You released the lock!", pktHdr, mailHdr);
    }
}

// destroy lock server call
void DestroyLock_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if(!validateLockIndex(lockIndex)) {
      sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
        return;
    }

    ServerThread serverCurrentThread;
    serverCurrentThread.machineId = pktHdr.from; // this is essentailly the server machineId
    serverCurrentThread.mailboxNum = mailHdr.from; // this is the mailbox that the mail came from since it's equal to client mailbox

    if(!(serverCurrentThread == serverLocks[lockIndex].lockOwner)) //current thread is not lock owner
    {
      pktHdr.to = serverLocks[lockIndex].lockOwner.machineId;
      mailHdr.to = serverLocks[lockIndex].lockOwner.mailboxNum;
      sendMessageToClient("Lock not yours! Can't Destroy.", pktHdr, mailHdr);
      return;
    }

    if(serverLocks[lockIndex].lockStatus == serverLocks[lockIndex].BUSY) //lock waitQueue is not empty
    {
      serverLocks[lockIndex].deleteFlag = TRUE;
      PacketHeader pktHdr1;
      MailHeader mailHdr1;
      pktHdr1.to = serverLocks[lockIndex].lockOwner.machineId;
      mailHdr1.to = serverLocks[lockIndex].lockOwner.mailboxNum;
      sendMessageToClient("Lock is in use will be destroyed later.", pktHdr, mailHdr);
    }
    else
    {
        serverLocks[lockIndex].lockStatus = serverLocks[lockIndex].FREE; //make lock available
        serverLocks[lockIndex].lockOwner.machineId = -1; //unset ownership
        serverLocks[lockIndex].lockOwner.mailboxNum = -1; //unset ownership
        serverLocks[lockIndex].isDeleted = TRUE; //unset ownership
        sendMessageToClient("You destroyed the lock!", pktHdr, mailHdr);
    }
}

// ++++++++++++++++++++++++++++ MVs ++++++++++++++++++++++++++++

// create monitor server call
int CreateMonitor_server(char* name, int appendNum) {
    int currentMonIndex = 0;
    return currentMonIndex;
}

// get monitor server call
void GetMonitor_server(int monitorIndex) {
    if(!validateMonitorIndex(monitorIndex)) {
        return;
    }
}

// set monitor server call
void SetMonitor_server(int monitorIndex) {
    if(!validateMonitorIndex(monitorIndex)) {
        return;
    }
}

// destroy monitor server call
void DestroyMonitor_server(int monitorIndex) {
    if(!validateMonitorIndex(monitorIndex)) {
        return;
    }
}

// ++++++++++++++++++++++++++++ CVs ++++++++++++++++++++++++++++

// create condition server call
int CreateCondition_server(char* name, int appendNum) {
    serverConds[serverCondCount].deleteFlag = FALSE;
    serverConds[serverCondCount].isDeleted = FALSE;
    serverConds[serverCondCount].name = name;
    serverConds[serverCondCount].waitingLockIndex = -1;

    int currentCondIndex = serverCondCount;
    ++serverCondCount;

    return currentCondIndex;
}

// wait condition server call
void Wait_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if(!validateLockIndex(lockIndex)) {
        return;
    }
    if(!validateConditionIndex(conditionIndex)) {
        return;
    }

    ServerThread serverCurrentThread;
    serverCurrentThread.machineId = 0;
    serverCurrentThread.mailboxNum = mailHdr.from;

    ServerLock conditionLock = serverLocks[lockIndex];
    ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);

    if(lockIsNull(conditionLock))
    {
        return;
    }
    if(lockIsNull(*waitingLock))
    {
        //no one waiting
        waitingLock = &conditionLock;
    }
    if(!(*waitingLock == conditionLock))
    {
        return;
    }
    //OK to wait
    serverConds[conditionIndex].waitQueue->Append(&serverCurrentThread);//Hung: add myself to Condition Variable waitQueue
    Release_server(lockIndex, pktHdr, mailHdr);
    Acquire_server(lockIndex, pktHdr, mailHdr);
}

// signal condition server call
void Signal_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if(!validateLockIndex(lockIndex)) {
        return;
    }
    if(!validateConditionIndex(conditionIndex)) {
        return;
    }

    ServerLock conditionLock = serverLocks[lockIndex];
    ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);


    if(serverConds[conditionIndex].waitQueue->IsEmpty()) //no thread waiting
    {
        return;
    }

    if(!(*waitingLock == conditionLock))
    {
        return;
    }

    //Wake up one waiting thread
    ServerThread thread = *(ServerThread*) (serverConds[conditionIndex].waitQueue->Remove()); //remove 1 waiting thread

    serverLocks[lockIndex].lockOwner = thread;
    Acquire_server(lockIndex, pktHdr, mailHdr);

    if(serverConds[conditionIndex].waitQueue->IsEmpty()) //waitQueue is empty
    {
        setLockToNull(*waitingLock);
    }
}

// broadcast condition server call
void Broadcast_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if(!validateLockIndex(lockIndex)) {
        return;
    }
    if(!validateConditionIndex(conditionIndex)) {
        return;
    }

    ServerLock conditionLock = serverLocks[lockIndex];
    ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);

    if(lockIsNull(conditionLock))
    {
        return;
    }

    if(!(*waitingLock == conditionLock))
    {
        return;
    }

    while(!serverConds[conditionIndex].waitQueue->IsEmpty()) //waitQueue is not empty
    {
        Signal_server(lockIndex, conditionIndex, pktHdr, mailHdr);
    }
}

// destroy condition server call
void DestroyCondition_server(int conditionIndex) {
    if(!validateConditionIndex(conditionIndex)) {
        return;
    }
}

// +++++++++++++++++ ENCODINGS +++++++++++++++++++

// CreateLock:       "L C name"
// Acquire:          "L A 32"
// Release:          "L R 2"
// DestroyLock:      "L D 21"

// CreateMonitor:    "M C name"
// GetMonitor:       "M G 32"
// SetMonitor:       "M S 2"
// DestroyMonitor:   "M D 21"

// CreateCondition:  "C C name"
// Wait:             "C W 32 2"
// Signal:           "C S 2 46"
// Broadcast:        "C B 21 36"
// DestroyCondition: "C D 21"


// Server polling and sending messages
void Server() {
    cout << "Server()" << endl;
    char sysCode1, sysCode2;
    for (int i = 0; i <MAX_MON_COUNT; ++i){
      serverLocks[i].waitQueue = new MailBox();
    }
    PacketHeader pktHdr; // Pkt is hardware level // just need to know the machine->Id at command line
    MailHeader mailHdr; // Mail
    char buffer[MaxMailSize];
    stringstream ss;

    char name[60];

    while(true) {
        //Recieve the message
        ss.str("");
        ss.clear();
        cout << "Recieve()" << endl;
        postOffice->Receive(0, &pktHdr, &mailHdr, buffer);
        printf("Got \"%s\" from %d, box %d\n",buffer,pktHdr.from,mailHdr.from);
        fflush(stdout);
        //Parse the message
        int entityId = -1;
        int entityIndex1 = -1;
        int entityIndex2 = -1;
        ss << buffer;
        cout << "Server::Buffer in server: " << buffer << endl;
        ss >> sysCode1 >> sysCode2;
        cout << "Server::sysCode1 >> sysCode2: " << sysCode1 << " " << sysCode2 << endl;
        if(sysCode2 == 'C') {
            cout << "Server::got to name: ";
            ss >> name;
            cout << name << endl;
        } else {
            cout << "fuck: ";
            ss >> entityIndex1;
        }
        cout << "Server::got past if block" << endl;
        switch(sysCode1) {
            case 'L': // lock server calls
                switch(sysCode2) {
                    case 'C': // create lock
                        cout << "Got to CreateLock_server" << endl;
                        ss.str("");
                        ss.clear();
                        entityId = CreateLock_server(name, serverLockCount, pktHdr, mailHdr);
                        ss << entityId;
                        cout << "CreateLock_server::entityId: " << entityId << endl;
                        //Process the message
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'A': // acquire lock
                        // only send reply when they can Acquire
                        cout << "Got to Acquire_server" << endl;
                        Acquire_server(entityIndex1, pktHdr, mailHdr);
                        ss.str("");
                        ss.clear();
                        ss << "Acquire_server";
                    break;
                    case 'R': // release lock
                        Release_server(entityIndex1, pktHdr, mailHdr);
                        ss.str("");
                        ss.clear();
                        ss << "Release_server";
                    break;
                    case 'D': // destroy lock
                        DestroyLock_server(entityIndex1, pktHdr, mailHdr);
                        ss.str("");
                        ss.clear();
                        ss << "DestroyLock_server";
                    break;
                }
            break;
            case 'M': // monitor server calls
                switch(sysCode2) {
                    case 'C': // create monitor
                        ss.str("");
                        ss.clear();
                        entityId = CreateMonitor_server(name, serverMonCount);
                        ss << entityId;
                        cout << "CreateMonitor_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'G': // get monitor
                        GetMonitor_server(entityIndex1);
                        ss.str("");
                        ss.clear();
                        ss << "GetMonitor_server";
                    break;
                    case 'S': // set monitor
                        SetMonitor_server(entityIndex1);
                        ss.str("");
                        ss.clear();
                        ss << "SetMonitor_server";
                    break;
                    case 'D': // destroy monitor
                        DestroyMonitor_server(entityIndex1);
                        ss.str("");
                        ss.clear();
                        ss << "DestroyMonitor_server";
                    break;
                }
            break;
            case 'C': // condition server calls
                switch(sysCode2) {
                    case 'C': // create condition
                        ss.str("");
                        ss.clear();
                        entityId = CreateCondition_server(name, serverCondCount);
                        ss << entityId;
                        cout << "CreateCondition_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'W': // condition wait
                        ss >> entityIndex2;
                        Wait_server(entityIndex1, entityIndex2, pktHdr, mailHdr); //lock then CV
                        ss.str("");
                        ss.clear();
                        ss << "Wait_server";
                    break;
                    case 'S': // condition signal
                        ss >> entityIndex2;
                        Signal_server(entityIndex1, entityIndex2, pktHdr, mailHdr); //lock then CV
                        ss.str("");
                        ss.clear();
                        ss << "Signal_server";
                    break;
                    case 'B': // create broadcast
                        ss >> entityIndex2;
                        Broadcast_server(entityIndex1, entityIndex2, pktHdr, mailHdr); //lock then CV
                        ss.str("");
                        ss.clear();
                        ss << "Broadcast_server";
                    break;
                    case 'D': // destroy condition
                        DestroyCondition_server(entityIndex2);
                        ss.str("");
                        ss.clear();
                        ss << "DestroyCondition_server";
                    break;
                }
            break;
        }
    }
}
