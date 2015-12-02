// nettest.cc
//  Test out message delivery between two "Nachos" machines,
//  using the Post Office to coordinate delivery.
//
//  Two caveats:
//    1. Two copies of Nachos must be running, with machine ID's 0 and 1:
//    ./nachos -m 0 -o 1 &
//    ./nachos -m 1 -o 0 &
//
//    2. You need an implementation of condition variables,
//       which is *not* provided as part of the baseline threads
//       implementation.  The Post Office won't work without
//       a correct implementation of condition variables.
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
#include <ctime>

// Test out message delivery, by doing the following:
//  1. send a message to the machine with ID "farAddr", at mail box #0
//  2. wait for the other machine's message to arrive (in our mailbox #0)
//  3. send an acknowledgment for the other machine's message
//  4. wait for an acknowledgement from the other machine to our
//      original message

#define MAX_MON_COUNT 1000
#define MAX_ARR_COUNT 50
#define MAX_MAILBOX_COUNT 10
#define MAX_CLIENT_COUNT 10

void releaseLockMoreThanOne_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox);
void AcquireMoreThanOne_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox);


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

// this is the server request object.
// essentially, this is used for server-to-server interactions
struct ServerRequest{
  ServerRequest(char* msg){
    gotYes = false;
    message = msg;
    isEmpty = false;
    serverRespondCount = 0;
  };
  ServerRequest(){
    gotYes = false;
    isEmpty = true;
    serverRespondCount = 0;
  };
  bool gotYes;
  bool isEmpty;
  int serverRespondCount;
  char* message;
};

string stringArr[10];

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
enum LockStatus {FREE, BUSY};

struct ServerLock {
    int num;
    bool deleteFlag;
    bool isDeleted;

    LockStatus lockStatus;
    char* name;
    List* waitQueue;
    int queueSize;
    ServerThread lockOwner;
};

// operator overload for == on ServerLock
// since we can't compare structs easily
bool operator==(const ServerLock& l1, const ServerLock& l2) {

    if(l1.num != l2.num) {
      cout << "NOT EQUAL\n";
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

    if(lock.lockStatus != FREE) {
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
    lock.name = "";
    lock.num = -1;
    lock.deleteFlag = FALSE;
    lock.isDeleted = FALSE;

    lock.lockStatus = FREE;
    lock.waitQueue = NULL;
    lock.lockOwner.machineId = -1;
    lock.lockOwner.mailboxNum = -1;
}

// condition implementation on the server
struct ServerCond {
    bool deleteFlag;
    bool isDeleted;

    char* name;
    int waitingLockIndex;
    List* waitQueue;
    int queueSize;
    bool hasWaitingLock;
};

// monitor implementation on the server
struct ServerMon {
    bool deleteFlag;
    bool isDeleted;
    int* values;
    char* name;
};

// a message struct that holds the pktHdr mailHdr and message

struct Msg{
    Msg(PacketHeader p, MailHeader m, char* d){
      pktHdr = p;
      mailHdr = m;
      data = d;
    };
    Msg(){};
    PacketHeader pktHdr;
    MailHeader mailHdr;
    char* data;
};

// this is the request made specifically for the Wait Server call since it needs to work with
// two entities (Lock and Condition).  This is made into a WaitRequest Table
struct WaitRequest{
    WaitRequest(){
        isEmpty = true;
        hasLock = false;
        hasCond = false;
        failed = false;
        lockServerOwner = -1;
        condServerOwner = -1;
        msg = NULL;
        lockRespondCount = -1;
        condRespondCount = -1;
    };
    bool isEmpty;
    bool failed;
    bool hasLock;
    bool hasCond;
    int lockServerOwner;
    int condServerOwner;
    char* msg;
    int lockRespondCount;
    int condRespondCount;
    ServerThread lockClientOwner;
};

// arrays of all of the monitor variables
ServerLock serverLocks[MAX_MON_COUNT];
ServerMon serverMons[MAX_MON_COUNT];
ServerCond serverConds[MAX_MON_COUNT];
ServerRequest pending[MAX_CLIENT_COUNT][MAX_MAILBOX_COUNT];
WaitRequest waitRequest[MAX_CLIENT_COUNT][MAX_MAILBOX_COUNT];

// server counts of the locks, mons, and conds
int serverLockCount = 0;
int serverMonCount = 0;
int serverCondCount = 0;
// ++++++++++++++++++++++++++++ Validation ++++++++++++++++++++++++++++

// this decodes the encoded mailbox value
int decodeMailbox(int value) {
    return value / 1000;
}

// this decodes the encoded entity index so as to be able to access the array
int decodeEntityIndex(int value) {
    return value % 1000;
}

// this checks that the current server and machineId match
bool serverAndMachineIdMatch(int mailboxNum) {
    return mailboxNum == machineId;
}


// make sure that we were handed a valid lock
enum Entity { LOCK, CONDITION, MONITOR };
// make sure that we were handed a valid lock
bool validateEntityIndex(int entityIndex, Entity e) {
  entityIndex = decodeEntityIndex(entityIndex);
    string entityType = "";
    int entityCount = -1;
    bool isDeleted = FALSE;

    switch(e) {
        case LOCK:
            entityType = "Lock";
            entityCount = serverLockCount;
            isDeleted = serverLocks[entityIndex].isDeleted;
            break;
        case CONDITION:
            entityType = "Condition";
            entityCount = serverCondCount;
            isDeleted = serverConds[entityIndex].isDeleted;
            break;
        case MONITOR:
            entityType = "Monitor";
            entityCount = serverMonCount;
            isDeleted = serverMons[entityIndex].isDeleted;
            break;
    }
    if (entityIndex < 0 || entityIndex >= entityCount){ // check if index is in valid range
      DEBUG('l',"    %s number %d invalid\n", entityIndex);
      return FALSE;
    }

    if (isDeleted){ // check if lock is deleted
      DEBUG('l',"    %s number %d already destroyed\n", entityIndex);
      return FALSE;
    }
    return TRUE;
}

// this makes sure that the index given is a valid array index
bool validateArrayIndex(int arrayIndex) {
    if (arrayIndex < 0 || arrayIndex >= MAX_ARR_COUNT){ // check if index is in valid range
      DEBUG('l',"    Mon::Array index %d invalid\n", arrayIndex);
      return false;
    }
    return true;
}

// a function to release the lock on the server side without actually sending any messages out to the postOffice
// identical to Release_server aside from messages.
void serverReleaseLock(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr){
  cout << "Releasing lock in server Release\n";
  if(!serverLocks[lockIndex].waitQueue->IsEmpty()) //lock waitQueue is not empty
  {
    string* msg;
    int stringIndex;
    stringstream ss;
    msg = (string*) serverLocks[lockIndex].waitQueue->Remove();
    ss << *msg;
    cout << *msg << endl;
    ss >> pktHdr.to;
    ss >> mailHdr.to;
    ss >> mailHdr.from;
    serverLocks[lockIndex].lockOwner.machineId = pktHdr.to; //unset ownership
    serverLocks[lockIndex].lockOwner.mailboxNum = mailHdr.to; //unset ownership

        char* temp = "You got the lock!";
        mailHdr.length = strlen(temp);
        postOffice->Send(pktHdr, mailHdr, temp);

    delete msg;
  }
  else
  {
      serverLocks[lockIndex].lockStatus = FREE; //make lock available
      serverLocks[lockIndex].lockOwner.machineId = -1; //unset ownership
      serverLocks[lockIndex].lockOwner.mailboxNum = -1; //unset ownership
      cout << "Lock released after \n";
  }
}

// this redirects the current packet and mail headers to the client in which the request was sent
void redirectPktMailHeader(PacketHeader &pktHdr, MailHeader &mailHdr, int messageLength) {
    pktHdr.to = pktHdr.from;
    int mailbox = mailHdr.to;
    mailHdr.to = mailHdr.from;
    mailHdr.from = mailbox;
    mailHdr.length = messageLength + 1;
}

// this redirects the message from the server to the original client that made the request
void redirectToOriginalClient(PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox, int messageLength){
  pktHdr.to = machineId;
    pktHdr.from = id;
    mailHdr.to = 0;
    mailHdr.from = mailbox;
    mailHdr.length = messageLength+1;
}

// this appends an encoded stringstream message to the appropriate waitQueue
void appendMessageToEntityQueue(PacketHeader &pktHdr, MailHeader &mailHdr, char* data, int entityIndex, Entity e){
    entityIndex = decodeEntityIndex(entityIndex);
    List* waitQueue = NULL;
    string entityString = "";
    int isCond = 0; // this is technically a boolean but we want stringstream to int it instead
    switch(e) {
        case LOCK:
              entityString = "Lock";
              waitQueue = serverLocks[entityIndex].waitQueue;
            cout << "  append to lock waitqueue\n";

            break;
        case CONDITION:
            entityString = "Condition";
            waitQueue = serverConds[entityIndex].waitQueue; //Put current thread on the lock’s waitQueue
            cout << "    entityIndex " << entityIndex << endl;
            isCond = 1;
            cout << "   append to cond waitqueue\n";
            break;
    }
    redirectPktMailHeader(pktHdr, mailHdr, strlen(data));
  stringstream ss;
  ss << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << ' ' << isCond;

  string *msg = new string();
  *msg = ss.str();

  cout << "Append to " << entityString << " waitQueue " << *msg << endl;
  waitQueue->Append(msg); //Put current thread on the lock’s waitQueue
}

/*bool sendMessageToAllOtherServers(string ssString, PacketHeader &pktHdr, MailHeader &mailHdr) {
    char replyBuffer[MaxMailSize];
    const char* ssStringC = ssString.c_str();
    for(unsigned int i = 0; i < strlen(ssStringC); ++i) {
        replyBuffer[i] = ssStringC[i];
    }

    replyBuffer[strlen(replyBuffer)] = '\0';

    for(int i = 0; i < serverCount; ++i) {
        if(i != machineId) {
            pktHdr.to = i;
            sendMessageToServer(replyBuffer, pktHdr, mailHdr);
        }
    }
}*/

// makes the the entity with the current name actually exists on the current server
bool checkEntityByName(char* name, Entity e){
  // check entity by name because it is a create request
  // we do not know if the lock/CV/MV is already there or not
    switch (e){
        case LOCK:
            for(int i = 0; i < serverLockCount; ++i){
                if (strcmp(name, serverLocks[i].name) == 0){
                    if (!serverLocks[i].isDeleted && !serverLocks[i].deleteFlag){
                        return true;
                    }
                }
            }
            break;
        case CONDITION:
            for(int i = 0; i < serverCondCount; ++i){
                if (strcmp(name, serverConds[i].name) == 0){
                    if (!serverConds[i].isDeleted && !serverConds[i].deleteFlag){
                        return true;
                    }
                }
            }
            break;
        case MONITOR:
            for(int i = 0; i < serverMonCount; ++i){
                if (strcmp(name, serverMons[i].name) == 0){
                    if (!serverMons[i].isDeleted && !serverMons[i].deleteFlag){
                        return true;
                    }
                }
            }
        break;
    }

    return false;
}

// this checks if the current server has the entity in question
bool hasEntity(char* msg, bool isCreate, char* name, Entity e, bool containsEntity, int mailbox){
  // we check if it is a create request from the client
  if (isCreate){
    cout << "isCreate" << endl;
    if (checkEntityByName(name, e)){
      msg[0] = 'Y';
      return true;
    }
    msg[0] = 'N';
    return false;
  }
    if (!serverAndMachineIdMatch(mailbox)) {
      msg[0] = 'N';
        return false;
    }
    if (containsEntity){
        msg[0] = 'Y';
        return true;
    } else {
        msg[0] = 'N';
        return false;
    }
}

// encodes the current entity index so that it can be passed through the servers
int encodeEntityIndex(int entityIndex){
  return (machineId*1000 + entityIndex);
}

// a function to check if the current server contains the lock/CV/MV
// returns true if current server contains the lock/CV/MV
bool checkCurrentServerContainEntity(bool isCreate, char* name, char* msg, int sysCode1, int entityIndex1){
    int index1 = decodeEntityIndex(entityIndex1);
    int mailbox1 = decodeMailbox(entityIndex1);

    bool hasLock = validateEntityIndex(index1, LOCK);
    bool hasCond = validateEntityIndex(index1, CONDITION);
    bool hasMon = validateEntityIndex(index1, MONITOR);

    switch(sysCode1) {
        case 'L':
          return hasEntity(msg, isCreate, name, LOCK, hasLock, mailbox1);
        break;
        case 'C':
          return hasEntity(msg, isCreate, name, CONDITION, hasCond, mailbox1);
        break;
        case 'M':
            return hasEntity(msg, isCreate, name, MONITOR, hasMon, mailbox1);
        break;
    }
    return false;
}

// deletes the current pending request in question
void deletePendingRequest(int id, int mailbox){
  pending[id][mailbox].isEmpty = true;
  pending[id][mailbox].gotYes = false;
  pending[id][mailbox].serverRespondCount = 0;
  pending[id][mailbox].message[0] = '\0';
}

// this sets the packet and mail headers to send to the correct clients and servers
void setClientandServer(PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    pktHdr.to = machineId;
    pktHdr.from = id;
    mailHdr.to = 0;
    mailHdr.from = mailbox;
}

// +++++++++++++++++ UTILITY SERVER METHODS ++++++++++++++++

// abstract method to send message to the client from the server
// again, we swap the header info and send the data
void sendMessageToClient(char* data, PacketHeader &pktHdr, MailHeader &mailHdr) {
    redirectPktMailHeader(pktHdr, mailHdr, strlen(data));
    printf( "      pktHdr.to: %d; pktHdr.from: %d; mailHdr.to: %d; mailHdr.from: %d; mailHdr.length: %d;\n", pktHdr.to, pktHdr.from, mailHdr.to, mailHdr.from, mailHdr.length);
    cout << "      data returned to client: " << data << "\nstrlen(data) " << strlen(data) << endl;
    bool success = postOffice->Send(pktHdr, mailHdr, data);
    if ( !success ) {
      // std::time_t result = std::time(NULL);
      // std::cout << std::asctime(std::localtime(&result))
      //           << result << " seconds since the Epoch\n";
        printf("The first postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
        interrupt->Halt();
    }
}

// abstract method to send message to the client from the server
// another helper function to encode entity index messages
void sendCreateEntityMessage(stringstream &ss, PacketHeader &pktHdr, MailHeader &mailHdr) {
  // cout << "breaking here?\n";
    const char* tempChar = ss.str().c_str();
    // cout << "tempChar: " << ss.str() << endl;
    char replyBuffer[MaxMailSize];
    for(unsigned int i = 0; i < strlen(tempChar); ++i) {
        replyBuffer[i] = tempChar[i];
    }

    replyBuffer[strlen(tempChar)] = '\0';

    //Send a reply (maybe)
    sendMessageToClient(replyBuffer, pktHdr, mailHdr);
}


// sends a message to all other servers excluding the current server
void checkOtherServers(char *tempString, PacketHeader &pktHdr, MailHeader &mailHdr){
  cout << "checking other servers...\n";
  // cout << "======tempString and length "<< tempString << " " << strlen(tempString) << endl;
  // response is set to CHECK
  tempString[0] = 'C';
  // server mailboxes are 0
  mailHdr.to = 0;
  mailHdr.from = 0;
  mailHdr.length = strlen(tempString) + 1;
  // send to other servers
  // cout << "====== servercount "<< serverCount << endl;
  // cout << "====== machineId "<< machineId << endl;
  for(int i = 0; i < serverCount; ++i) {
    // cout << "====== checking other servers::forloop: " << i << endl;
        if(i != machineId) {
          // cout << "====== sending to machineId: " << i << endl;
            pktHdr.to = i;
            pktHdr.from = machineId;
            // cout << pktHdr.to << ' ' << pktHdr.from << ' ' << mailHdr.to << ' ' << mailHdr.from << endl;
            postOffice->Send(pktHdr, mailHdr, tempString);
        }
    }
}

// ++++++++++++++++++++++++++++ Locks ++++++++++++++++++++++++++++

// create lock server call
int CreateLock_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr) {
  cout << "    CreateLock_server entered" << endl;
    if (serverLockCount < 0 ||serverLockCount >= MAX_LOCK_COUNT){
        cout << "      INVALID LOCK" << endl;
        sendMessageToClient("Too many locks!", pktHdr, mailHdr);
        return -1;
    }
    // if we have the lock with the name they want to create
    // we return the index

    bool deleteFlag;
    bool isDeleted;
    for (int i = 0; i < serverLockCount; ++i){
        deleteFlag = serverLocks[i].deleteFlag;
        isDeleted = serverLocks[i].isDeleted;
        if (strcmp(name, serverLocks[i].name) == 0 && !isDeleted){
        // cout << serverLocks[i].name << " returning 0?\n";
        // cout << "name == serverLocks[].name " << strcmp(name, serverLocks[i].name) << endl;
        return encodeEntityIndex(i);
        }
    }

    // initialize all the values
    serverLocks[serverLockCount].deleteFlag = FALSE;
    serverLocks[serverLockCount].isDeleted = FALSE;
    serverLocks[serverLockCount].lockStatus = FREE;

    serverLocks[serverLockCount].name = new char [strlen(name) + 1];
    strncpy(serverLocks[serverLockCount].name, name, strlen(name));
    serverLocks[serverLockCount].name[strlen(name)] = '\0';

    serverLocks[serverLockCount].lockOwner.machineId = -1;
    serverLocks[serverLockCount].lockOwner.mailboxNum = -1;

    int currentLockIndex = serverLockCount;
    serverLocks[serverLockCount].num = currentLockIndex;
    ++serverLockCount; //increment count for lock

    //sendMessageToClient("Lock created!", pktHdr, mailHdr);
    cout << "    currentLockIndex " << currentLockIndex << endl;
    cout << "    encoded index " << encodeEntityIndex(currentLockIndex) << endl;
    return encodeEntityIndex(currentLockIndex);
}

// acquire lock server call
void Acquire_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if(!validateEntityIndex(lockIndex, LOCK)) {
        sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
        return;
    }
    ServerThread serverCurrentThread;
    serverCurrentThread.machineId = pktHdr.from; // this is essentailly the server machineId
    serverCurrentThread.mailboxNum = mailHdr.from; // this is the mailbox that the mail came from since it's equal to client mailbox

    if(serverCurrentThread == serverLocks[lockIndex].lockOwner) { //current thread is lock owner
        sendMessageToClient("Lock is yours. Did nothing.", pktHdr, mailHdr);
        return;
    }

    if(serverLocks[lockIndex].lockStatus == FREE) {//lock is available
        //I can have the lock
        serverLocks[lockIndex].lockStatus = BUSY; //make state BUSY
        serverLocks[lockIndex].lockOwner.machineId = pktHdr.from;
        serverLocks[lockIndex].lockOwner.mailboxNum = mailHdr.from;
        sendMessageToClient("You got the lock!", pktHdr, mailHdr); //send the message to the client
        return;
    }
    else { //lock is busy
      appendMessageToEntityQueue(pktHdr, mailHdr, "You got the lock!", lockIndex, LOCK); //put the message on the waitqueue so that it can be sent when someone releases the lock
    }
}

// this is exactly like the Acqire call, but built in mind that there are multiple servers
void Acquire_RPC_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    int decodedLockIndex = decodeEntityIndex(lockIndex);
    int serverThatContainsLock = decodeMailbox(lockIndex); // yes, this is correct
    if(serverAndMachineIdMatch(serverThatContainsLock)) {
        Acquire_server(decodedLockIndex, pktHdr, mailHdr);
        return;
    }

    stringstream ss;
    ss << '0' << ' ' << pktHdr.from << ' ' << mailHdr.from << ' ' << 'L' << ' ' << 'A' << ' ' << lockIndex << ' ' << -1 << ' ' << -1 << ' ' << "noname";

    int bufferSize = ss.str().size();
    char* tempString = new char [bufferSize+1];
    strncpy(tempString, ss.str().c_str(), bufferSize);
    tempString[bufferSize] = '\0';

    pktHdr.to = serverThatContainsLock;
    pktHdr.from = machineId;
    mailHdr.to = 0;
    mailHdr.from = 0;
    postOffice->Send(pktHdr, mailHdr, tempString);
}

// create release server call
void Release_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
  ServerThread serverCurrentThread;
  serverCurrentThread.machineId = pktHdr.from; // this is essentailly the server machineId
  serverCurrentThread.mailboxNum = mailHdr.from; // this is the mailbox that the mail came from since it's equal to client mailbox
    if(!validateEntityIndex(lockIndex, LOCK)) { //sanity checks
        sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
        return;
    }
    if (!(serverCurrentThread == serverLocks[lockIndex].lockOwner)) {//current thread is not lock owner
        sendMessageToClient("No permission to release!", pktHdr, mailHdr);
        return;
    }

    pktHdr.to = serverLocks[lockIndex].lockOwner.machineId;
    mailHdr.to = serverLocks[lockIndex].lockOwner.mailboxNum;

    if (serverLocks[lockIndex].deleteFlag == TRUE){
        serverLocks[lockIndex].isDeleted = TRUE;
        delete serverLocks[lockIndex].waitQueue;
        delete serverLocks[lockIndex].name;
        sendMessageToClient("Released, the lock is also deleted.", pktHdr, mailHdr);
        return;
    }
    if(!serverLocks[lockIndex].waitQueue->IsEmpty()) {//lock waitQueue is not empty
        // compliated code to get messages from the waitqueue, decode it and send it out
        sendMessageToClient("Released. Another thread took it.", pktHdr, mailHdr);
        string* msg;
        stringstream ss;
        int stringIndex;
        msg = (string*) (serverLocks[lockIndex].waitQueue->Remove());
        --(serverLocks[lockIndex].queueSize);
        ss << *msg;
        ss >> pktHdr.to;
        ss >> mailHdr.to;
        ss >> mailHdr.from;
        ss >> stringIndex;
        serverLocks[lockIndex].lockOwner.machineId = pktHdr.to; //unset ownership
        serverLocks[lockIndex].lockOwner.mailboxNum = mailHdr.to; //unset ownership

        mailHdr.length = stringArr[stringIndex].length()+1;
        char buf[stringArr[stringIndex].length() + 1];
        for(unsigned int i = 0; i < stringArr[stringIndex].length(); ++i) {
            buf[i] = stringArr[stringIndex][i];
        } //copy the char*
        buf[stringArr[stringIndex].length()]= '\0';
        postOffice->Send(pktHdr, mailHdr, buf);
    } else {
        // queue is empty
        serverLocks[lockIndex].lockStatus = FREE; //make lock available
        serverLocks[lockIndex].lockOwner.machineId = -1; //unset ownership
        serverLocks[lockIndex].lockOwner.mailboxNum = -1; //unset ownership
        sendMessageToClient("You released the lock!", pktHdr, mailHdr);
    }
}

// destroy lock server call
void DestroyLock_server(int lockIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    ServerThread serverCurrentThread;
    serverCurrentThread.machineId = pktHdr.from; // this is essentailly the server machineId
    serverCurrentThread.mailboxNum = mailHdr.from; // this is the mailbox that the mail came from since it's equal to client mailbox

    if(!validateEntityIndex(lockIndex, LOCK)) {
      sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
        return;
    }
    if(serverLocks[lockIndex].lockStatus == BUSY){ //lock waitQueue is not empty
        serverLocks[lockIndex].deleteFlag = TRUE;
        pktHdr.to = serverLocks[lockIndex].lockOwner.machineId;
        mailHdr.from = mailHdr.to;
        mailHdr.to = serverLocks[lockIndex].lockOwner.mailboxNum;
        sendMessageToClient("Lock is in use will be destroyed later.", pktHdr, mailHdr);
    } else {
        serverLocks[lockIndex].lockStatus = FREE; //make lock available
        serverLocks[lockIndex].lockOwner.machineId = -1; //unset ownership
        serverLocks[lockIndex].lockOwner.mailboxNum = -1; //unset ownership
        serverLocks[lockIndex].isDeleted = TRUE; //unset ownership
        delete serverLocks[lockIndex].waitQueue;
        sendMessageToClient("You destroyed the lock!", pktHdr, mailHdr);
    }
}

// ++++++++++++++++++++++++++++ MVs ++++++++++++++++++++++++++++

// create monitor server call
int CreateMonitor_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    cout << "    CreateMonitor_server entered" << endl;
    if (appendNum <= 0 || appendNum > 50){//sanity checks
        cout << "      INVALID MON" << endl;
        sendMessageToClient("Array invalid! Enter 1-50", pktHdr, mailHdr);
        return -1;
    }
    if (serverMonCount < 0 || serverMonCount >= MAX_MON_COUNT){
        sendMessageToClient("Too many mons!", pktHdr, mailHdr);
        return -1;
    }

    bool deleteFlag;
    bool isDeleted;
    for (int i = 0; i < serverMonCount; ++i){
        deleteFlag = serverMons[i].deleteFlag;
        isDeleted = serverMons[i].isDeleted;
        if (strcmp(name, serverMons[i].name) == 0 && !isDeleted){
            // cout << serverMons[i].name << " returning 0?\n";
            // cout << "name == serverMons[].name " << strcmp(name, serverMons[i].name) << endl;
            return encodeEntityIndex(i);
        }
    }

    serverMons[serverMonCount].deleteFlag = FALSE;
    serverMons[serverMonCount].isDeleted = FALSE;

    serverMons[serverMonCount].name = new char [strlen(name) + 1];
    strncpy(serverMons[serverMonCount].name, name, strlen(name));
    serverMons[serverMonCount].name[strlen(name)] = '\0';

    serverMons[serverMonCount].values = new int [appendNum];

    int currentMonIndex = serverMonCount;
    ++serverMonCount;

    //sendMessageToClient("Monitor created!", pktHdr, mailHdr);
    cout << "    currentMonIndex " << currentMonIndex << endl;
    cout << "    encoded index " << encodeEntityIndex(currentMonIndex) << endl;
    return encodeEntityIndex(currentMonIndex);
}

// get monitor server call
int GetMonitor_server(int monitorIndex, int arrayIndex,PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    setClientandServer(pktHdr, mailHdr, id, mailbox);
    if(!validateEntityIndex(monitorIndex, MONITOR)) {
        sendMessageToClient("Invalid monitor index!", pktHdr, mailHdr);
        return -1;
    }
    if (!validateArrayIndex(arrayIndex)){
        sendMessageToClient("Invalid array index!", pktHdr, mailHdr);
        return -1;
    }
    // we return the required value for monitor
    char temp;
    stringstream ss;
    ss << serverMons[monitorIndex].values[arrayIndex];
    ss >> temp;
    cout << "*************" << serverMons[monitorIndex].values[arrayIndex] << endl;
    // sendMessageToClient(&temp, pktHdr, mailHdr);
    return serverMons[monitorIndex].values[arrayIndex];
}

// set monitor server call
void SetMonitor_server(int monitorIndex, int arrayIndex, int value,PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    // set the value and return the message
    setClientandServer(pktHdr, mailHdr, id, mailbox);
    if(!validateEntityIndex(monitorIndex, MONITOR)) {
        sendMessageToClient("Invalid monitor index!", pktHdr, mailHdr);
        return;
    }
    if (!validateArrayIndex(arrayIndex)){
        sendMessageToClient("Invalid array index!", pktHdr, mailHdr);
        return;
    }
    serverMons[monitorIndex].values[arrayIndex] = value;

    sendMessageToClient("Set monitor successfully!", pktHdr, mailHdr);

}

// destroy monitor server call
void DestroyMonitor_server(int monitorIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    setClientandServer(pktHdr, mailHdr, id, mailbox);
    if(!validateEntityIndex(monitorIndex, MONITOR)) {
        sendMessageToClient("Invalid monitor index!", pktHdr, mailHdr);
        return;
    }
    serverMons[serverMonCount].isDeleted = TRUE;
    delete serverMons[serverMonCount].values;
    sendMessageToClient("Deleted monitor successfully!", pktHdr, mailHdr);
    return;
}

// ++++++++++++++++++++++++++++ CVs ++++++++++++++++++++++++++++

// create condition server call
int CreateCondition_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    printf("    CreateCondition_server entered\n");
    if (serverCondCount < 0 ||serverCondCount >= MAX_COND_COUNT){
        cout << "      INVALID COND" << endl;
        sendMessageToClient("Too many conds!", pktHdr, mailHdr);
        return -1;
    }

    bool deleteFlag;
    bool isDeleted;
    for (int i = 0; i < serverCondCount; ++i){
      deleteFlag = serverConds[i].deleteFlag;
      isDeleted = serverConds[i].isDeleted;
      if (strcmp(name, serverConds[i].name) == 0 && !isDeleted){
        // cout << serverConds[i].name << " returning 0?\n";
        // cout << "name == serverConds[].name " << strcmp(name, serverConds[i].name) << endl;
        return encodeEntityIndex(i);
      }
    }

    // initialize
    serverConds[serverCondCount].deleteFlag = FALSE;
    serverConds[serverCondCount].isDeleted = FALSE;

    serverConds[serverCondCount].name = new char [strlen(name) + 1];
    strncpy(serverConds[serverCondCount].name, name, strlen(name));
    serverConds[serverCondCount].name[strlen(name)] = '\0';

    serverConds[serverCondCount].waitingLockIndex = -1;
    serverConds[serverCondCount].hasWaitingLock == FALSE;

    int currentCondIndex = serverCondCount;
    ++serverCondCount;
    cout << "    currentCondIndex " << currentCondIndex << endl;
    cout << "    encoded index " << encodeEntityIndex(currentCondIndex) << endl;
    return encodeEntityIndex(currentCondIndex);
}

// wait condition server call
void Wait_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    ServerThread thread;
    thread.machineId = id;
    thread.mailboxNum = mailbox;
    ServerLock conditionLock = serverLocks[lockIndex];
    ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);
    int tempPktTo =pktHdr.to;
    int tempMailTo =mailHdr.to;
    int tempMailFrom =mailHdr.from;
    pktHdr.to = machineId;
    pktHdr.from = id;
    mailHdr.to = 0;
    mailHdr.from = mailbox;
    if(!validateEntityIndex(lockIndex, LOCK)) { //long sanity checks with both locks and conditions
        sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
    }else if(!validateEntityIndex(conditionIndex, CONDITION)) {
        sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
    } else if (!(serverLocks[lockIndex].lockOwner == thread)){
        sendMessageToClient("Lock is not acquired!", pktHdr, mailHdr);
    }else if (serverConds[conditionIndex].deleteFlag){
        sendMessageToClient("Cond will be destroyed, can't wait!", pktHdr, mailHdr);
    }else if(!serverConds[conditionIndex].hasWaitingLock) {
        //no one waiting, set and wait directly
        waitingLock = &conditionLock;
        serverConds[conditionIndex].waitingLockIndex = lockIndex;
        serverConds[conditionIndex].hasWaitingLock = TRUE;
    }else if(!(*waitingLock == conditionLock)){
        sendMessageToClient("No permission to wait!", pktHdr, mailHdr);
        return;
    }
    // add the message so that we could pass it when we get signaled
    appendMessageToEntityQueue(pktHdr, mailHdr, "Finished Waiting!", conditionIndex, CONDITION);
    pktHdr.to = tempPktTo;
    mailHdr.to = tempMailTo;
    mailHdr.from = tempMailFrom;
    // use the saved headers to release the lock
    serverReleaseLock(lockIndex, pktHdr, mailHdr); // not the syscall, the helper function
}

// this server call JUST releases the lock.  The lock has already been validated at this stage and all that needs
// to happen is that the lock is released by the noted server.
void Just_Release(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    int decodedLockIndex = decodeEntityIndex(lockIndex);
    int decodedCondIndex = decodeEntityIndex(conditionIndex);

    cout << "Releasing lock in Just_Release\n";
    if(!serverLocks[decodedLockIndex].waitQueue->IsEmpty()) {//lock waitQueue is not empty
        string* msg;
        int stringIndex;
        stringstream ss;
        msg = (string*) serverLocks[decodedLockIndex].waitQueue->Remove();
        ss << *msg;
        cout << *msg << endl;
        ss >> pktHdr.to;
        ss >> mailHdr.to;
        ss >> mailHdr.from;
        serverLocks[decodedLockIndex].lockOwner.machineId = pktHdr.to; //unset ownership
        serverLocks[decodedLockIndex].lockOwner.mailboxNum = mailHdr.to; //unset ownership
        mailHdr.length = 8;
        postOffice->Send(pktHdr, mailHdr, "Acquired");
    } else {
        serverLocks[decodedLockIndex].lockStatus = FREE; //make lock available
        serverLocks[decodedLockIndex].lockOwner.machineId = -1; //unset ownership
        serverLocks[decodedLockIndex].lockOwner.mailboxNum = -1; //unset ownership
        cout << "Lock released in Just_Release \n";
    }
}

// tells the server in question to Wait without doing any checks because all of the entities in question
// have been validated already
void Just_Wait(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox, bool isOriginalServer){
    int ogPktFrom = pktHdr.from;
    int decodedCondIndex = decodeEntityIndex(conditionIndex);
    ServerThread thread;
    thread.machineId = id;
    thread.mailboxNum = mailbox;
    cout << "          lockIndex: " << lockIndex << endl;
    if (!serverConds[decodedCondIndex].hasWaitingLock){
            serverConds[decodedCondIndex].waitingLockIndex = lockIndex;
            serverConds[decodedCondIndex].hasWaitingLock = TRUE;
        }
        pktHdr.to = machineId;
        pktHdr.from = id;
        mailHdr.to = 0;
        mailHdr.from = mailbox;
        appendMessageToEntityQueue(pktHdr, mailHdr, "Finished Waiting!", conditionIndex, CONDITION);
        cout << "          after append to cond waitqueue\n";
        //cout << "          waitRequest[id][mailbox].lockServerOwner: " << waitRequest[id][mailbox].lockServerOwner << endl;
    if (isOriginalServer){

        releaseLockMoreThanOne_server(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    } else {
        stringstream ss;
        ss << 'Y' << ' ' << id << ' ' << mailbox << ' ' << 'C' << ' ' << 'R' << ' ' << lockIndex << ' ' << ' ' << conditionIndex << ' ' << -1 << ' ' << "noname";
        char* msg = new char[strlen(ss.str().c_str())];
        strncpy(msg, ss.str().c_str(), strlen(ss.str().c_str()));
        msg[strlen(ss.str().c_str())] = '\0';
        pktHdr.from = ogPktFrom;
        pktHdr.to = machineId;
        mailHdr.from = 0;
        mailHdr.to = 0;
        redirectPktMailHeader(pktHdr, mailHdr, strlen(msg));
        postOffice->Send(pktHdr, mailHdr, msg);
        cout << "[id]: " << id << " mailbox: " << mailbox << endl;
        // cout << "waitRequest[id][mailbox].condServerOwner: " << waitRequest[id][mailbox].condServerOwner << " MACHINEID: " << machineId << endl;
        // cout << "ERROR IN JUST WAIT, OWNER != MACHINEID\n";
        // interrupt->Halt();
    }
}

// tells the server in question to Acquire without doing any checks because all of the entities in question
// have been validated already
void Just_Acquire(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    //lockIndex has to be decoded so that we access the correct index instead of the index with server Id
    int decodedLockIndex = decodeEntityIndex(lockIndex);
    // headers will be redirected in sendmessagetoclient, so we have to treat it as 'received' on server from client

    pktHdr.to = machineId;
    pktHdr.from = id;
    mailHdr.to = 0;
    mailHdr.from = mailbox;
    if(serverLocks[decodedLockIndex].lockStatus == FREE) {//lock is available
        //I can have the lock
        serverLocks[decodedLockIndex].lockStatus = BUSY; //make state BUSY
        serverLocks[decodedLockIndex].lockOwner.machineId = id;
        serverLocks[decodedLockIndex].lockOwner.mailboxNum = mailbox;

        sendMessageToClient("You got the lock!", pktHdr, mailHdr); //send the message to the client
    } else { //lock is busy
        appendMessageToEntityQueue(pktHdr, mailHdr, "You got the lock!", lockIndex, LOCK); //put the message on the waitqueue so that it can be sent when someone releases the lock
    }
}

// tells the server in question to Signal without doing any checks because all of the entities in question
// have been validated already
void Just_Signal(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox, int ogClientId, int ogClientMailbox){
    int decodedCondIndex = decodeEntityIndex(conditionIndex);
    string* msg;
    stringstream ss;
    msg = (string*) (serverConds[decodedCondIndex].waitQueue->Remove());//Hung: add myself to Condition Variable waitQueue

    //cout << "here?\n" << pktHdr.from << ' ' << mailHdr.from << ' ' << mailHdr.to << endl;
    AcquireMoreThanOne_server(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    pktHdr.to = machineId;
    pktHdr.from = ogClientId;
    mailHdr.from = ogClientMailbox;
    mailHdr.to = 0;
    mailHdr.length = 9;
    //cout << "here?\n" << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << endl;

    sendMessageToClient("Signalled", pktHdr, mailHdr);
    if(serverConds[conditionIndex].waitQueue->IsEmpty()){
        serverConds[conditionIndex].hasWaitingLock = FALSE; //reset if is empty
        serverConds[conditionIndex].waitingLockIndex = -1;
    }
}

// abstract function for sending a message to the server that must execute a certain instruction
void actionOnRemoteServer(stringstream &ss, bool isLock, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    cout << "          sending out the remote command\n";
    int bufferSize = ss.str().size();
    char* tempLockString = new char [bufferSize + 1];
    strncpy(tempLockString, ss.str().c_str(), bufferSize);
    tempLockString[bufferSize] = '\0';
    if (isLock){
        pktHdr.to = waitRequest[id][mailbox].lockServerOwner;
        cout << "          waitRequest[id][mailbox].lockServerOwner: " << waitRequest[id][mailbox].lockServerOwner << endl;
    } else {
        pktHdr.to = waitRequest[id][mailbox].condServerOwner;
    }

    pktHdr.from = machineId;
    mailHdr.to = 0;
    mailHdr.from = 0;
    cout << "pktHdr.to: " << pktHdr.to << " pktHdr.from: " << pktHdr.from << " mailHdr.to: " << mailHdr.to << " mailHdr.from: " << mailHdr.from << endl;
    postOffice->Send(pktHdr, mailHdr, tempLockString);
}

// the current server tells the remote server to Release()
void releaseOnRemoteServer(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    stringstream ss;
    cout << "        releasing on remoteserver\n";
    cout << "        id: " << id << " mailbox: " << mailbox << endl;
    ss << 'R' << ' ' << id << ' ' << mailbox << ' ' << 'C' << ' ' << 'W' << ' ' << lockIndex << ' ' << conditionIndex << ' ' << -1 << ' ' << "noname";
    actionOnRemoteServer(ss, TRUE, pktHdr, mailHdr, id, mailbox);
}

// the current server tells the remote server to Wait()
void waitOnRemoteServer(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    stringstream ss;
    ss << 'W' << ' ' << id << ' ' << mailbox << ' ' << 'C' << ' ' << 'W' << ' ' << lockIndex << ' ' << conditionIndex << ' ' << -1 << ' ' << "noname";
    actionOnRemoteServer(ss, FALSE, pktHdr, mailHdr, id, mailbox);
}

// the current server tells the remote server to Acquire()
void acquireOnRemoteServer(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    stringstream ss;
    ss << 'A' << ' ' << id << ' ' << mailbox << ' ' << 'C' << ' ' << 'S' << ' ' << lockIndex << ' ' << conditionIndex << ' ' << -1 << ' ' << "noname";
    actionOnRemoteServer(ss, TRUE, pktHdr, mailHdr, id, mailbox);
}

// the current server tells the remote server to Signal()
void signalOnRemoteServer(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox, int ogClientId, int ogClientMailbox){
    stringstream ss;
    ss << 'S' << ' ' << id << ' ' << mailbox << ' ' << 'C' << ' ' << 'S' << ' ' << lockIndex << ' ' << conditionIndex << ' ' << -1 << ' ' << "noname" << ' ' << ogClientId << ' ' << ogClientMailbox;
    actionOnRemoteServer(ss, FALSE, pktHdr, mailHdr, id, mailbox);
}

// The Signal() server call rebuilt with more than one server in mind
void SignalMoreThanOne_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox, int ogClientId, int ogClientMailbox) {
    if (waitRequest[id][mailbox].condServerOwner == machineId){
        Just_Signal(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox, ogClientId, ogClientMailbox);
    } else {
        signalOnRemoteServer(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox, ogClientId, ogClientMailbox);
    }
}

// The Wait() server call rebuilt with more than one server in mind
void WaitMoreThanOne_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    cout << "    entered WaitMoreThanOne_server\n";
    cout << "    waitRequest[id][mailbox].lockServerOwner: " << waitRequest[id][mailbox].lockServerOwner << endl;
    printf( "    id: %d || mailbox: %d\n", id, mailbox);
    if (waitRequest[id][mailbox].condServerOwner == machineId){
        cout << "    original server has the condition\n";
        Just_Wait(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox, true);
    } else {
        cout << "    original server does not have the condition\n";
        waitOnRemoteServer(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    }
}

// The Acquire() server call rebuilt with more than one server in mind
void AcquireMoreThanOne_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    if (waitRequest[id][mailbox].lockServerOwner == machineId){
        Just_Acquire(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    } else {
        acquireOnRemoteServer(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    }
}

// The Release() server call rebuilt with more than one server in mind
void releaseLockMoreThanOne_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox){
    cout << "    got into releaseLockMoreThanOne_server\n";
    cout << "    waitRequest[id][mailbox].lockServerOwner: " << waitRequest[id][mailbox].lockServerOwner << endl;
    cout << "    machineId: " << machineId << endl;
    if (waitRequest[id][mailbox].lockServerOwner == machineId){
        cout << "      current server is lockowner, so just release\n";

        Just_Release(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    } else {
        cout << "      current server is not lockowner, so release on remote server\n";

        releaseOnRemoteServer(lockIndex, conditionIndex, pktHdr, mailHdr, id, mailbox);
    }
}

// wait on the lock without releasing the lock
void Wait_without_release(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
    ServerThread thread;
    thread.machineId = pktHdr.from;
    thread.mailboxNum = mailHdr.from;
    ServerLock conditionLock = serverLocks[lockIndex];
    ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);
    int tempPktTo =pktHdr.to;
    int tempMailTo =mailHdr.to;
    int tempMailFrom =mailHdr.from;
    if(!validateEntityIndex(lockIndex, LOCK)) { //long sanity checks with both locks and conditions
        sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
    }else if(!validateEntityIndex(conditionIndex, CONDITION)) {
        sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
    } else if (!(serverLocks[lockIndex].lockOwner == thread)){
        sendMessageToClient("Lock is not acquired!", pktHdr, mailHdr);
    }else if (serverConds[conditionIndex].deleteFlag){
        sendMessageToClient("Cond will be destroyed, can't wait!", pktHdr, mailHdr);
    }else if(!serverConds[conditionIndex].hasWaitingLock) {
        //no one waiting, set and wait directly
        waitingLock = &conditionLock;
        serverConds[conditionIndex].waitingLockIndex = lockIndex;
        serverConds[conditionIndex].hasWaitingLock = TRUE;
    }else if(!(*waitingLock == conditionLock)){
        sendMessageToClient("No permission to wait!", pktHdr, mailHdr);
        return;
    }
    // add the message so that we could pass it when we get signaled
    appendMessageToEntityQueue(pktHdr, mailHdr, "Finished Waiting!", conditionIndex, CONDITION);
    pktHdr.to = tempPktTo;
    mailHdr.to = tempMailTo;
    mailHdr.from = tempMailFrom;
    // use the saved headers to release the lock
    serverReleaseLock(lockIndex, pktHdr, mailHdr); // not the syscall, the helper function
}

// signal condition server call
void Signal_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    int tempPktTo = pktHdr.from;
    int tempMailTo = mailHdr.from;
    int tempMailFrom = mailHdr.to;
    printf("    Signal_server\n");
    cout << "    This is the conditionIndex: " << conditionIndex << endl;
    if(!validateEntityIndex(conditionIndex, CONDITION)) {
        sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
    } else if(serverConds[conditionIndex].waitQueue->IsEmpty()) {//no thread waiting
        sendMessageToClient("No thread waiting!", pktHdr, mailHdr);
    } else {
        // we remove the message from the queue, decode it and use it to acquire the lock after being signaled
        string* msg;
        stringstream ss;
        msg = (string*) (serverConds[conditionIndex].waitQueue->Remove());//Hung: add myself to Condition Variable waitQueue
        ss << *msg;
        ss >> pktHdr.from >> mailHdr.from >> mailHdr.to;
        //cout << "here?\n" << pktHdr.from << ' ' << mailHdr.from << ' ' << mailHdr.to << endl;
        Acquire_RPC_server(serverConds[conditionIndex].waitingLockIndex, pktHdr, mailHdr, id, mailbox);
        pktHdr.from = tempPktTo;
        mailHdr.from = tempMailTo;
        mailHdr.to = tempMailFrom;
        mailHdr.length = 9;
        //cout << "here?\n" << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << endl;

        sendMessageToClient("Signalled", pktHdr, mailHdr);
    }
    if(serverConds[conditionIndex].waitQueue->IsEmpty()){
        serverConds[conditionIndex].hasWaitingLock = FALSE; //reset if is empty
        serverConds[conditionIndex].waitingLockIndex = -1;
    }
}

// helper function to signal without postoffice functions
void Signal_without_send(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    int ogLockIndex = lockIndex;
    int ogCondIndex = conditionIndex;
    lockIndex = decodeEntityIndex(lockIndex);
    conditionIndex = decodeEntityIndex(conditionIndex);
    int tempPktTo = pktHdr.from;
    int tempMailTo = mailHdr.from;
    int tempMailFrom = mailHdr.to;
    printf("    Signal_server_without_send\n");
    cout << "    This is the conditionIndex: " << conditionIndex << endl;
    if(!validateEntityIndex(conditionIndex, CONDITION)) {
        sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
    } else if(serverConds[conditionIndex].waitQueue->IsEmpty()) {//no thread waiting
        sendMessageToClient("No thread waiting!", pktHdr, mailHdr);
    } else {
        // we remove the message from the queue, decode it and use it to acquire the lock after being signaled
        string* msg;
        stringstream ss;
        msg = (string*) (serverConds[conditionIndex].waitQueue->Remove());//Hung: add myself to Condition Variable waitQueue
        ss << *msg;
        ss >> pktHdr.from >> mailHdr.from >> mailHdr.to;
        //cout << "here?\n" << pktHdr.from << ' ' << mailHdr.from << ' ' << mailHdr.to << endl;
        Acquire_RPC_server(serverConds[conditionIndex].waitingLockIndex, pktHdr, mailHdr, id, mailbox);
        pktHdr.from = tempPktTo;
        mailHdr.from = tempMailTo;
        mailHdr.to = tempMailFrom;
        mailHdr.length = 9;
        //cout << "here?\n" << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << endl;

        // sendMessageToClient("Signalled", pktHdr, mailHdr);
    }
    if(serverConds[conditionIndex].waitQueue->IsEmpty()){
        serverConds[conditionIndex].hasWaitingLock = FALSE; //reset if is empty
        serverConds[conditionIndex].waitingLockIndex = -1;
    }
}


// broadcast condition server call
void Broadcast_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox) {
    int ogLockIndex = lockIndex;
    int ogCondIndex = conditionIndex;
    lockIndex = decodeEntityIndex(lockIndex);
    conditionIndex = decodeEntityIndex(conditionIndex);

    ServerThread thread;
    thread.machineId = pktHdr.from;
    thread.mailboxNum = mailHdr.from;
    ServerLock conditionLock = serverLocks[lockIndex];
    ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);
    int tempPktTo = pktHdr.from;
    int tempMailTo = mailHdr.from;
    int tempMailFrom = mailHdr.to;
    char data[MaxMailSize];
    /*if(!validateEntityIndex(lockIndex, LOCK)) {
        sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
    } else */
    if(!validateEntityIndex(conditionIndex, CONDITION)) {
        sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
    } else if(!(*waitingLock == conditionLock)) {
        sendMessageToClient("No permission to broadcast!", pktHdr, mailHdr);
    } else {
        string* msg;
        int stringIndex;
        stringstream ss;
        while(!serverConds[conditionIndex].waitQueue->IsEmpty()) {//waitQueue is not empty
          Signal_without_send(ogLockIndex, ogCondIndex, pktHdr, mailHdr, id, mailbox);
        }
        serverConds[conditionIndex].hasWaitingLock == FALSE;
        serverConds[conditionIndex].waitingLockIndex = -1;
    }
    pktHdr.from = tempPktTo;
    mailHdr.from = tempMailTo;
    mailHdr.to = tempMailFrom;
    mailHdr.length = 12;
    sendMessageToClient("Broadcasted!", pktHdr, mailHdr);
}

// destroy condition server call
void DestroyCondition_server(int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
  if(!validateEntityIndex(conditionIndex, CONDITION)) {
    sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
      return;
  }
  // can be destroyed
  if (serverConds[conditionIndex].waitQueue->IsEmpty()){
    serverConds[conditionIndex].isDeleted = TRUE;
    delete serverConds[conditionIndex].waitQueue;
    sendMessageToClient("Condition is destroyed.", pktHdr, mailHdr);
  } else {
    serverConds[conditionIndex].deleteFlag = TRUE;
    sendMessageToClient("Cond in use, destroy later.", pktHdr, mailHdr);
  }
}

// function that specifically looks for the lock on other servers
void lookForLockOnOtherServers(int id, int mailBox, char sysCode1, int entityIndex1, int entityIndex2, int entityIndex3, char* name, PacketHeader &pktHdr, MailHeader &mailHdr) {
    //We don't have the lock on current server
    cout << "      looking for lock on other servers..." << endl;
    stringstream ss_lock;
    ss_lock << 'C' << ' ' << id << ' ' << mailBox << ' ' << sysCode1 << ' ' << 'L' << ' ' << entityIndex1 << ' ' << entityIndex2 << ' ' << entityIndex3 << ' ' << name;
    char* tempLockString = new char [strlen(ss_lock.str().c_str())+1];
    strncpy(tempLockString, ss_lock.str().c_str(), strlen(ss_lock.str().c_str()));
    tempLockString[strlen(ss_lock.str().c_str())] = '\0';
    checkOtherServers(tempLockString, pktHdr, mailHdr);
}

// if the current server does not have the lock, look for it on another server
bool lookForLockLogic(bool currentServerHasLock, int id, int mailbox, char sysCode1, int entityIndex1, int entityIndex2, int entityIndex3, char* name, PacketHeader &pktHdr, MailHeader &mailHdr){
    if(currentServerHasLock) {
        cout << "      initial server has Lock" << endl;
        waitRequest[id][mailbox].hasLock = true;
        waitRequest[id][mailbox].lockRespondCount = serverCount-1;
        waitRequest[id][mailbox].lockServerOwner = machineId;
        printf( "      id: %d || mailbox: %d\n", id, mailbox);
        cout << "      waitRequest[id][mailbox].lockServerOwner: " << waitRequest[id][mailbox].lockServerOwner << endl;
        return true;
        // do work
    } else {
        cout << "      initial server doesn't have Lock" << endl;
        cout << "      = checkOtherServers..." << endl;
        lookForLockOnOtherServers(id, mailbox, sysCode1, entityIndex1, entityIndex2, entityIndex3, name, pktHdr, mailHdr);
        return false;
    }
}

// function that deletes the waitingRequest from the waitTable
void deleteWaitingRequest(int id, int mailbox){
    waitRequest[id][mailbox].isEmpty = true;
    waitRequest[id][mailbox].failed = false;
    waitRequest[id][mailbox].hasLock = false;
    waitRequest[id][mailbox].hasCond = false;
    waitRequest[id][mailbox].lockServerOwner = -1;
    waitRequest[id][mailbox].condServerOwner = -1;
    // waitRequest[id][mailbox].msg[0] = '\0';
    waitRequest[id][mailbox].lockRespondCount = -1;
    waitRequest[id][mailbox].condRespondCount = -1;
    waitRequest[id][mailbox].lockClientOwner.machineId = -1;
    waitRequest[id][mailbox].lockClientOwner.mailboxNum = -1;
}

// checks wether or not the waitingRequest fails and reports accordingly
void waitRequestFail(int id, int mailbox, PacketHeader &pktHdr, MailHeader &mailHdr){
    cout << "  got into waitRequestFail" << endl;
    char* reqFailed = "  waitRequestFail!";
    waitRequest[id][mailbox].failed = true;
    pktHdr.to = machineId;
    pktHdr.from = id;
    mailHdr.from = mailbox;
    mailHdr.to = 0;
    cout << "  before deleteWaitingRequest" << endl;
    deleteWaitingRequest(id,mailbox);
    cout << "  before sendMessageToClient" << endl;
    sendMessageToClient(reqFailed, pktHdr, mailHdr);
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
    cout << "Server() machineId: " << machineId << " mailbox: " << currentThread->mailbox << " serverCount: " << serverCount << endl;
    PacketHeader pktHdr; // Pkt is hardware level // just need to know the machine->Id at command line
    MailHeader mailHdr; // Mail
    /*
    for (int i = 0; i < 1000000; ++i){
      currentThread->Yield();
    }

    mailHdr.to = 0;
  mailHdr.from = 0;
  mailHdr.length = 5;

    char* testString = "test";
    for (int i = 0; i < serverCount; ++i){
      if(i != machineId) {
          cout << "====== sending to machineId: " << i << endl;
            pktHdr.to = i;
            pktHdr.from = machineId;
            testString[4] = (char) (i+'0');
            postOffice->Send(pktHdr, mailHdr, testString);
        }
    }*/

    stringArr[0] = "You got the lock!";
    stringArr[1] = "Finished Waiting!";

    char sysCode1, sysCode2;
    for (int i = 0; i <MAX_MON_COUNT; ++i){
        serverLocks[i].waitQueue = new List();
        serverConds[i].waitQueue = new List();
        serverConds[i].hasWaitingLock = FALSE;
        serverLocks[i].lockStatus = FREE;
    }

    char buffer[MaxMailSize];
    stringstream ss;

    char name[60];
    int serverSuccessCount = 0;
    int serverToSendTo = 0;
    int entityId = -1;
    int entityIndex1 = -1;
    int entityIndex2 = -1;
    int entityIndex3 = -1;
    int id = -1, mailbox = -1, value = -1, stringIndex = -1;
    int ogClientId = -1;
    int ogClientMailbox = -1;
    while(true) {
        //Recieve the message
        ss.str("");
        ss.clear();
        // cout << "Recieve()" << endl;
        cout << "====== Receiving..." << endl;
        postOffice->Receive(0, &pktHdr, &mailHdr, buffer);
        cout << "====== Got something!" << endl;
        //printf("Got \"%s\" from %d, box %d\n",buffer,pktHdr.from,mailHdr.from);
        fflush(stdout);
        //Parse the message
        serverSuccessCount = 0;
        // project4: init variables for server logic
        ogClientId = pktHdr.from;
        ogClientMailbox = mailHdr.from;
        char response;
        ss << buffer;
        // string to store incoming message, send to other servers if necessary
        char tempString[MaxMailSize];
        const char* tempChar = ss.str().c_str();
        for(unsigned int i = 0; i < strlen(tempChar); ++i) {
            tempString[i] = tempChar[i];
        }
        tempString[strlen(tempChar)] = '\0';

        ss >> response >> id >> mailbox >> sysCode1 >> sysCode2 >> entityIndex1 >> entityIndex2 >> entityIndex3 >> name;
        if (response == 'S'){
            ss >> ogClientId >> ogClientMailbox;
        }

        cout << "-- data received: " << tempString << endl;

        if(id == -1 || mailbox == -1) {
            printf("id == %d || mailbox == %d \n\n", id, mailbox);
            interrupt->Halt();
        }
        int decodedMailboxNum = decodeMailbox(value);

        bool isCreate = false;
        if (sysCode2 == 'C') {
            isCreate = true;
        }

        bool isWait = false;
        if (sysCode1 == 'C' && sysCode2 == 'W') {
            isWait = true;
        }

        bool isLookingForLock = false;
        if(sysCode2 == 'L') {
            isLookingForLock = true;
        }

        bool isSignal = false;
        if (sysCode1 == 'C' && sysCode2 == 'S') {
            isSignal = true;
        }

        bool isRelease = false;
        if (sysCode1 == 'C' && sysCode2 == 'R') {
            isRelease = true;
        }

        bool isBroadcast = false;
        if (sysCode1 == 'C' && sysCode2 == 'B') {
            isBroadcast = true;
        }

        bool currentServerHasEntity = false; // for 'regular'
        bool currentServerHasCond = false; // for wait only
        bool currentServerHasLock = false; // for wait only

        if(isWait || isSignal || isBroadcast) { // if it is Wait, we must check if we have the CV
            currentServerHasCond = checkCurrentServerContainEntity(isCreate, name, tempString, 'C', entityIndex2);
        } else {
            currentServerHasEntity = checkCurrentServerContainEntity(isCreate, name, tempString, sysCode1, entityIndex1);
        }

        if(serverCount > 1) {
            switch(response) {
                case '0':
                    cout << "---- Server got message from Client with '0' ----" << endl;
                    // request is sent from client
                    // put into pending table
                    if(isWait) {
                        waitRequest[id][mailbox].isEmpty = false;
                        waitRequest[id][mailbox].condRespondCount = 0;
                        waitRequest[id][mailbox].msg = new char [strlen(tempString) + 1];
                        strncpy(waitRequest[id][mailbox].msg, tempString, strlen(tempString));
                        waitRequest[id][mailbox].msg[strlen(tempString)] = '\0';

                        cout << "  isWait syscall" << endl;
                        if(currentServerHasCond) { // we have the CV
                            cout << "    initial server has Cond" << endl;
                            waitRequest[id][mailbox].hasCond = true;
                            waitRequest[id][mailbox].condRespondCount = serverCount-1;
                            waitRequest[id][mailbox].condServerOwner = machineId;

                            currentServerHasLock = checkCurrentServerContainEntity(isCreate, name, tempString, 'L', entityIndex1);
                            if (!lookForLockLogic(currentServerHasLock, id, mailbox, sysCode1, entityIndex1, entityIndex2, entityIndex3, name, pktHdr, mailHdr)){
                                continue;
                            }
                        } else { //We don't have the CV
                            cout << "    initial server doesn't have Cond" << endl;
                            //probably have to flag stuff in request table
                            cout << "    = checkOtherServers..." << endl;
                            checkOtherServers(tempString, pktHdr, mailHdr);
                            continue;
                        }
                    } else { // other than wait call
                        if (isSignal || isBroadcast){
                            currentServerHasEntity = currentServerHasCond;
                        }
                        if(!currentServerHasEntity) {
                            pending[id][mailbox].isEmpty = false;
                            pending[id][mailbox].serverRespondCount = 0;
                            pending[id][mailbox].message = new char[strlen(tempString)+1];
                            strncpy(pending[id][mailbox].message, tempString, strlen(tempString));
                            pending[id][mailbox].message[strlen(tempString)] = '\0';
                            cout << "  current server doesn't contain entity" << endl;
                            checkOtherServers(tempString, pktHdr, mailHdr);
                            continue;
                        }
                    }
                    break;
                case 'C':
                    cout << "---- Server got message from other Server with 'C' ----" << endl;
                    if(isWait || isSignal || isBroadcast) { // checking if we have CV that OG server didn't have
                        cout << "  looking for Cond from Wait" << endl;
                        if (currentServerHasCond){
                            cout << "    current server has Cond, reply with 'Y'" << endl;
                            // we have the cond, so reply Y
                            tempString[0] = 'Y';
                        }else{
                            cout << "    current server doesn't have Cond, reply with 'N'" << endl;
                            // we dont have the cond, so reply N
                            tempString[0] = 'N';
                        }
                        sendMessageToClient(tempString, pktHdr, mailHdr);
                        if (isWait || !currentServerHasCond){
                            continue;
                        }
                        // TODO: send message to server?
                    } else if(isLookingForLock) {
                        cout << "  looking for Lock from Wait" << endl;
                        // other servers telling us to check for a lock from wait
                        currentServerHasLock = checkCurrentServerContainEntity(isCreate, name, tempString, 'L', entityIndex1);
                        if(currentServerHasLock){
                            cout << "    current server has Lock, reply with 'Y'" << endl;
                            // we have the lock, so reply Y
                            tempString[0] = 'Y';
                            ServerThread thread;
                            thread.machineId = id;
                            thread.mailboxNum = mailbox;
                            if (!(serverLocks[decodeEntityIndex(entityIndex1)].lockOwner == thread)){
                                cout << "    lock is not acquired by request client\n";
                                tempString[0] = 'F';
                            }
                        } else {
                            cout << "    current server doesn't have Lock, reply with 'N'" << endl;
                            // we dont have the lock, so reply N
                            tempString[0] = 'N';
                        }
                        // send message back to parent server

                        sendMessageToClient(tempString, pktHdr, mailHdr);
                        continue;
                    } else {
                        mailHdr.to = 0;
                        mailHdr.from = 0;
                        cout << "  sending message to client: " << tempString << endl;
                        sendMessageToClient(tempString, pktHdr, mailHdr);

                        if (!currentServerHasEntity) {
                            continue;
                        }
                    }
                    break;
                case 'Y':
                    cout << "---- Server got message from other Server with 'Y' ----" << endl;
                    if (isRelease){
                        cout << "  handle 'Y' response for Cond::Release" << endl;
                        releaseLockMoreThanOne_server(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox);
                        continue;
                    }else if (isWait){
                        cout << "  handle 'Y' response for Cond" << endl;
                        waitRequest[id][mailbox].hasCond = true;
                        ++(waitRequest[id][mailbox].condRespondCount);
                        waitRequest[id][mailbox].condServerOwner = pktHdr.from;
                        currentServerHasLock = checkCurrentServerContainEntity(isCreate, name, tempString, 'L', entityIndex1);
                        lookForLockLogic(currentServerHasLock, id, mailbox, sysCode1, entityIndex1, entityIndex2, entityIndex3, name, pktHdr, mailHdr);
                        if (!currentServerHasLock){
                            continue;
                        }
                        // go to switch statement
                    } else if (isLookingForLock){
                        cout << "  handle 'Y' response for Wait::Lock" << endl;
                        waitRequest[id][mailbox].hasLock = true;
                        ++(waitRequest[id][mailbox].lockRespondCount);
                        waitRequest[id][mailbox].lockServerOwner = pktHdr.from;
                        sysCode2 = 'W';
                        // current server can do work now since the lock is found
                    }else{
                        pending[id][mailbox].gotYes = true;
                        ++ (pending[id][mailbox].serverRespondCount);
                        if (pending[id][mailbox].serverRespondCount == (serverCount-1)){
                            cout << "    searched through all servers.  Deleting pending request from table: ";
                            printf("pending[%d][%d]\n", id, mailbox);
                            deletePendingRequest(id, mailbox);
                        }
                        // request is sent from another server and the server has what we need
                        continue;
                    }
                    break;
                case 'N':
                    cout << "---- Server got message from other Server with 'N' ----" << endl;
                    if (!isLookingForLock && !isWait && pending[id][mailbox].isEmpty) {
                        cout << "  ***Pending Request Table is empty!\n";
                        interrupt->Halt();
                    }

                    if(isWait) {
                        if (waitRequest[id][mailbox].isEmpty) {
                            cout << "  ***Waiting Request Table is empty!\n";
                            interrupt->Halt();
                        }
                        cout << "  handle 'N' response for Cond (++serverCondRespondCount)" << endl;
                        ++(waitRequest[id][mailbox].condRespondCount);
                        if (waitRequest[id][mailbox].condRespondCount == (serverCount-1)){
                            cout << "    got all responses for Cond::serverCondRespondCount" << endl;
                            bool gotYesResponse = waitRequest[id][mailbox].hasCond;
                            if(gotYesResponse){
                                cout << "      we got at least one 'Y' for Cond, so we continue" << endl;
                                continue;
                            } else {
                                cout << "      Request failed because there is NO Cond" << endl;
                                // request for cond failed, so wait failed
                                waitRequestFail(id, mailbox, pktHdr, mailHdr);
                                continue;
                            }
                        } else {
                            continue;
                        }
                    } else if(isLookingForLock) {
                        cout << "  handle 'N' response for Lock  (++serverLockRespondCount)" << endl;
                        ++(waitRequest[id][mailbox].lockRespondCount);
                        if (waitRequest[id][mailbox].lockRespondCount == (serverCount-1)){
                            cout << "    got all responses for Lock::serverLockRespondCount" << endl;
                            //deleting request from table
                            bool gotYesResponse = waitRequest[id][mailbox].hasLock;
                            if(gotYesResponse){
                                cout << "      we got at least one 'Y' for Lock, so continue" << endl;

                            } else {
                                cout << "      Request failed because there is NO Lock" << endl;
                                // request for lock failed, so wait fails
                                waitRequestFail(id, mailbox, pktHdr, mailHdr);
                                continue;
                            }
                        } else {
                            continue;
                        }
                    } else {
                        ++(pending[id][mailbox].serverRespondCount);
                        if (pending[id][mailbox].serverRespondCount == (serverCount-1)){
                            cout << "  checked all servers and still got 'N'\n";
                            //deleting request from table
                            bool gotYesResponse = pending[id][mailbox].gotYes;
                            if(gotYesResponse){
                                deletePendingRequest(id, mailbox);
                                continue;
                            } else if ( sysCode2 != 'C'){
                                cout << "    Request failed!!" << endl;
                                // not a create syscall and all other servers return NO
                                // request failed
                                char* reqFailed = "Request Failed!";
                                pktHdr.to = machineId;
                                pktHdr.from = id;
                                mailHdr.from = mailbox;
                                mailHdr.to = 0;
                                sendMessageToClient(reqFailed, pktHdr, mailHdr);
                                deletePendingRequest(id, mailbox);
                                continue;
                            }
                        }else{
                            continue;
                        }
                    }
                break;
                case 'F':
                    cout << "---- Server got message from other Server with 'F' ----" << endl;
                    waitRequestFail(id, mailbox, pktHdr, mailHdr);
                break;
                case 'W':
                    // int lockIndex = decodeEntityIndex(entityIndex1);
                    // int condIndex = decodeEntityIndex(entityIndex2);
                    cout << "---- Server got message from other Server with 'W' ----" << endl;

                    Just_Wait(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox, false);
                    continue;
                break;
                case 'S':
                    cout << "---- Server got message from other Server with 'S' ----" << endl;


                    Just_Signal(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox, ogClientId, ogClientMailbox);
                    continue;
                break;
                case 'A':
                    cout << "---- Server got message from other Server with 'A' ----" << endl;

                    Just_Acquire(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox);
                    continue;
                break;
                case 'R':
                    cout << "---- Server got message from other Server with 'R' ----" << endl;

                    Just_Release(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox);
                    continue;
                break;

            }
        }

        // sanity check::at this point the machineId of server and request should match
        if(decodedMailboxNum != 0 && !serverAndMachineIdMatch(decodedMailboxNum)) {
            printf("res == %c || id == %d || mailbox == %d || value == %d || machineId == %d \n\n", response, id, mailbox, value, machineId);
            interrupt->Halt();
        }

        // big switch statement to determine syscalls
        redirectToOriginalClient(pktHdr, mailHdr, id, mailbox, strlen(tempString));
        int originalEntityIndex1 = entityIndex1;
        int originalEntityIndex2 = entityIndex2;
        entityIndex1 = decodeEntityIndex(entityIndex1);
        entityIndex2 = decodeEntityIndex(entityIndex2);
        entityIndex3 = decodeEntityIndex(entityIndex3);
        switch(sysCode1) {
            case 'L': // lock server calls
                switch(sysCode2) {
                    case 'C': // create lock
                        ss.str("");
                        ss.clear();
                        entityId = CreateLock_server(name, serverLockCount, pktHdr, mailHdr);
                        ss << entityId;
                        //cout << "CreateLock_server::entityId: " << entityId << endl;
                        //Process the message
                        pktHdr.to = machineId;
                        pktHdr.from = id;
                        mailHdr.to = 0;
                        mailHdr.from = mailbox;
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
                        entityId = CreateMonitor_server(name, entityIndex1, pktHdr, mailHdr, id, mailbox);
                        ss << entityId;
                        pktHdr.to = machineId;
                        pktHdr.from = id;
                        mailHdr.to = 0;
                        mailHdr.from = mailbox;
                        //cout << "CreateMonitor_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'G': // get monitor
                        ss.str("");
                        ss.clear();
                        entityId = GetMonitor_server(entityIndex1, entityIndex2,pktHdr, mailHdr, id, mailbox);
                        ss << entityId;
                        pktHdr.to = machineId;
                        pktHdr.from = id;
                        mailHdr.to = 0;
                        mailHdr.from = mailbox;
                        //cout << "CreateCondition_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'S': // set monitor
                        SetMonitor_server(entityIndex1, entityIndex2,entityIndex3,pktHdr, mailHdr, id, mailbox);
                        ss.str("");
                        ss.clear();
                        ss << "SetMonitor_server";
                    break;
                    case 'D': // destroy monitor
                        DestroyMonitor_server(entityIndex1, pktHdr, mailHdr, id, mailbox);
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
                        entityId = CreateCondition_server(name, serverCondCount, pktHdr, mailHdr, id, mailbox);
                        ss << entityId;
                        pktHdr.to = machineId;
                        pktHdr.from = id;
                        mailHdr.to = 0;
                        mailHdr.from = mailbox;
                        //cout << "CreateCondition_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'W': // condition wait
                        if (serverCount > 1){
                            WaitMoreThanOne_server(originalEntityIndex1, originalEntityIndex2, pktHdr, mailHdr, id, mailbox);
                        } else {
                            Wait_server(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox); //lock then CV
                        }
                        ss.str("");
                        ss.clear();
                        cout << "Wait_server\n";
                    break;
                    case 'S': // condition signal
                        //if (serverCount > 1){
                        // SignalMoreThanOne_server(originalEntityIndex1, originalEntityIndex2, pktHdr, mailHdr, id, mailbox);
                        //} else {
                        Signal_server(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox); //lock then CV
                        //}
                        ss.str("");
                        ss.clear();
                        ss << "Signal_server";
                    break;
                    case 'B': // create broadcast
                        Broadcast_server(entityIndex1, entityIndex2, pktHdr, mailHdr, id, mailbox); //lock then CV
                        ss.str("");
                        ss.clear();
                        ss << "Broadcast_server";
                    break;
                    case 'D': // destroy condition
                        DestroyCondition_server(entityIndex1, pktHdr, mailHdr);
                        ss.str("");
                        ss.clear();
                        ss << "DestroyCondition_server";
                    break;
                }
            break;
        }
    }
}
