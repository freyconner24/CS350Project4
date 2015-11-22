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
#include <ctime>

// Test out message delivery, by doing the following:
//	1. send a message to the machine with ID "farAddr", at mail box #0
//	2. wait for the other machine's message to arrive (in our mailbox #0)
//	3. send an acknowledgment for the other machine's message
//	4. wait for an acknowledgement from the other machine to our
//	    original message

#define MAX_MON_COUNT 1000
#define MAX_ARR_COUNT 50
#define MAX_MAILBOX_COUNT 10
#define MAX_CLIENT_COUNT 10

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

struct ServerRequest{
	ServerRequest(char* msg){
		message = msg;
		isEmpty = false;
		serverRespondCount = 0;
	};
	ServerRequest(){
		isEmpty = true;
		serverRespondCount = 0;
	};
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

// arrays of all of the monitor variables
ServerLock serverLocks[MAX_MON_COUNT];
ServerMon serverMons[MAX_MON_COUNT];
ServerCond serverConds[MAX_MON_COUNT];
ServerRequest pending[MAX_CLIENT_COUNT][MAX_MAILBOX_COUNT];

// server counts of the locks, mons, and conds
int serverLockCount = 0;
int serverMonCount = 0;
int serverCondCount = 0;

// ++++++++++++++++++++++++++++ Validation ++++++++++++++++++++++++++++

int decodeMailbox(int value) {
    return value / 1000;
}

int decodeEntityIndex(int value) {
    return value % 1000;
}

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

bool validateArrayIndex(int arrayIndex) {
  // cout << "arrayindex " << arrayIndex << endl;
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
    cout << stringArr[stringIndex];
    cout << stringArr[stringIndex].length();
    mailHdr.length = stringArr[stringIndex].length();
    char buf[stringArr[stringIndex].length() + 1];
    for(unsigned int i = 0; i < stringArr[stringIndex].length(); ++i) {
      buf[i] = stringArr[stringIndex][i];
    }
    buf[stringArr[stringIndex].length()]= '\0';
    postOffice->Send(pktHdr, mailHdr, buf);
  }
  else
  {
      serverLocks[lockIndex].lockStatus = FREE; //make lock available
      serverLocks[lockIndex].lockOwner.machineId = -1; //unset ownership
      serverLocks[lockIndex].lockOwner.mailboxNum = -1; //unset ownership
      cout << "Lock released after \n";
  }
}

void redirectPktMailHeader(PacketHeader &pktHdr, MailHeader &mailHdr, int messageLength) {
    pktHdr.to = pktHdr.from;
    int mailbox = mailHdr.to;
    mailHdr.to = mailHdr.from;
    mailHdr.from = mailbox;
    mailHdr.length = messageLength + 1;
}

void redirectToOriginalClient(PacketHeader &pktHdr, MailHeader &mailHdr, int id, int mailbox, int messageLength){
	pktHdr.to = machineId;
    pktHdr.from = id;
    mailHdr.to = 0;
    mailHdr.from = mailbox;
    mailHdr.length = messageLength+1;
}

void appendMessageToEntityQueue(PacketHeader &pktHdr, MailHeader &mailHdr, char* data, int entityIndex, Entity e){
    List* waitQueue = NULL;
    string entityString = "";
    int isCond = 0; // this is technically a boolean but we want stringstream to int it instead
    switch(e) {
        case LOCK:
              entityString = "Lock";
              waitQueue = serverLocks[entityIndex].waitQueue;
            break;
        case CONDITION:
            entityString = "Condition";
            waitQueue = serverConds[entityIndex].waitQueue; //Put current thread on the lock’s waitQueue
            isCond = 1;
            break;
    }
    redirectPktMailHeader(pktHdr, mailHdr, strlen(data));
  stringstream ss;
  ss << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << ' ' << isCond;

  string *msg = new string();
  *msg = ss.str();

  cout << "Append to " << entityString << " waitQueue" << *msg << endl;
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

bool hasEntity(char* msg, bool isCreate, char* name, Entity e, bool containsEntity, int mailbox){
	// if it is a create request, we check if 
	cout << "--------- hasEntity::msg: " << msg << endl;
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

int encodeEntityIndex(int entityIndex){
	return (machineId*1000 + entityIndex);
}

// a function to check if the current server contains the lock/CV/MV
// returns true if current server contains the lock/CV/MV
bool checkCurrentServerContainEntity(bool isCreate, char* name, char* msg, int sysCode1, int sysCode2, int entityIndex1, int entityIndex2, PacketHeader &pktHdr, MailHeader &mailHdr){
    int index1 = decodeEntityIndex(entityIndex1);
    int index2 = decodeEntityIndex(entityIndex2);
    int mailbox1 = decodeMailbox(entityIndex1);
    int mailbox2 = decodeMailbox(entityIndex2);

    bool hasLock = validateEntityIndex(index1, LOCK);
    bool hasCond = validateEntityIndex(index1, CONDITION);
    bool hasMon = validateEntityIndex(index1, MONITOR);

    switch(sysCode1) {
        case 'L':
        	return hasEntity(msg, isCreate, name, LOCK, hasLock, mailbox1);
        break;
        case 'C':
        	// if(sysCode2 == 'W') {
       		// 	hasCond = validateEntityIndex(index2, CONDITION);
        	// 	return 
	        // 		(hasEntity(msg, isCreate, name, LOCK, hasLock, mailbox1)
	        // 		&& hasEntity(msg, isCreate, name, CONDITION, hasCond, mailbox2));
        	// } else {
	         	return hasEntity(msg, isCreate, name, CONDITION, hasCond, mailbox1);
        	// }
        break;
        case 'M':
            return hasEntity(msg, isCreate, name, MONITOR, hasMon, mailbox1);
        break;
    }
    return false;
}

// +++++++++++++++++ UTILITY SERVER METHODS +++++++++++++++++

// abstract method to send message to the client from the server
// again, we swap the header info and send the data
void sendMessageToClient(char* data, PacketHeader &pktHdr, MailHeader &mailHdr) {
    redirectPktMailHeader(pktHdr, mailHdr, strlen(data));
    cout << pktHdr.to << ' ' << pktHdr.from << ' ' << mailHdr.to << ' ' << mailHdr.from << ' ' << mailHdr.length << endl;
    cout << "dataaaaa " << data << "\nstrlen(data) " << strlen(data) << endl;
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

void checkOtherServers(char *tempString, PacketHeader &pktHdr, MailHeader &mailHdr){
	cout << "checking other servers\n";
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
    // receive from other servers
    /*for (int i = 0; i < serverCount-1; ++i){
        postOffice->Receive(0, &pktHdr, &mailHdr, buffer);
        ss << buffer;
        // string to store incoming message, send to other servers if necessary
        char replyBuffer[MaxMailSize];
        char* tempString = ss.str().c_str();
        strncpy(replyBuffer, tempString, strlen(tempString));

        ss >> response;

        if (response == 'Y'){
        	// we found the entity on a certain server
        	// send the message with response yes and let the server do the work
        	pktHdr.to = pktHdr.from;
        	mailHdr.to = 0;
        	mailHdr.from = 0;
        	postOffice->Send(replyBuffer, pktHdr, mailHdr);
        	cout << "--------------Server replies Y\n";
            return true;
        }
    }
    // entity not found, return false
    return false;*/
}

// ++++++++++++++++++++++++++++ Locks ++++++++++++++++++++++++++++

// create lock server call
int CreateLock_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr) {
	cout << "====== Server::Entered CreateLock_server: " << endl;
    if (serverLockCount < 0 ||serverLockCount >= MAX_LOCK_COUNT){
      cout << "======= INVALID LOCK" << endl;
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
    cout << "======== currentLockIndex " << currentLockIndex << endl;
    cout << "======encoded index " << encodeEntityIndex(currentLockIndex) << endl;
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

    if(serverCurrentThread == serverLocks[lockIndex].lockOwner) //current thread is lock owner
    {
        sendMessageToClient("Lock is yours. Done nothing.", pktHdr, mailHdr);
        return;
    }

    if(serverLocks[lockIndex].lockStatus == FREE) //lock is available
    {
        //I can have the lock
        //cout << "***********************" << endl;
        serverLocks[lockIndex].lockStatus = BUSY; //make state BUSY
        //serverLocks[lockIndex].lockOwner = serverCurrentThread; //make myself the owner
        serverLocks[lockIndex].lockOwner.machineId = pktHdr.from;
        serverLocks[lockIndex].lockOwner.mailboxNum = mailHdr.from;
        sendMessageToClient("You got the lock!", pktHdr, mailHdr); //send the message to the client
        return;
    }
    else //lock is busy
    {

      appendMessageToEntityQueue(pktHdr, mailHdr, "You got the lock!", lockIndex, LOCK); //put the message on the waitqueue so that it can be sent when someone releases the lock
    }
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
    if (!(serverCurrentThread == serverLocks[lockIndex].lockOwner)) //current thread is not lock owner
    {
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
    if(!serverLocks[lockIndex].waitQueue->IsEmpty()) //lock waitQueue is not empty
    {
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
    if(serverLocks[lockIndex].lockStatus == BUSY) //lock waitQueue is not empty
    {
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
int CreateMonitor_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr) {
  if (appendNum <= 0 || appendNum > 50){//sanity checks
    sendMessageToClient("Array invalid! Enter 1-50", pktHdr, mailHdr);
    return -1;
  }
  if (serverMonCount < 0 ||serverMonCount >= MAX_MON_COUNT){
    sendMessageToClient("Too many mons!", pktHdr, mailHdr);
    return -1;
  }
  serverMons[serverMonCount].deleteFlag = FALSE;
  serverMons[serverMonCount].isDeleted = FALSE;
  serverMons[serverMonCount].name = name;
  serverMons[serverMonCount].values = new int [appendNum];

  int currentMonIndex = serverMonCount;
  ++serverMonCount;
  //sendMessageToClient("Monitor created!", pktHdr, mailHdr);

  return encodeEntityIndex(currentMonIndex);
}

// get monitor server call
int GetMonitor_server(int monitorIndex, int arrayIndex,PacketHeader &pktHdr, MailHeader &mailHdr) {
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
    sendMessageToClient(&temp, pktHdr, mailHdr);
    return serverMons[monitorIndex].values[arrayIndex];
}

// set monitor server call
void SetMonitor_server(int monitorIndex, int arrayIndex, int value,PacketHeader &pktHdr, MailHeader &mailHdr) {
    // set the value and return the message
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
void DestroyMonitor_server(int monitorIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
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
int CreateCondition_server(char* name, int appendNum, PacketHeader &pktHdr, MailHeader &mailHdr) {
    if (serverCondCount < 0 ||serverCondCount >= MAX_COND_COUNT){
      sendMessageToClient("Too many conds!", pktHdr, mailHdr);
      return -1;
    }
    // initialize
    serverConds[serverCondCount].deleteFlag = FALSE;
    serverConds[serverCondCount].isDeleted = FALSE;
    serverConds[serverCondCount].name = name;
    serverConds[serverCondCount].waitingLockIndex = -1;
    serverConds[serverCondCount].hasWaitingLock == FALSE;
    int currentCondIndex = serverCondCount;
    ++serverCondCount;
    return encodeEntityIndex(currentCondIndex);
}

// wait condition server call
void Wait_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
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
void Signal_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
  int tempPktTo = pktHdr.from;
  int tempMailTo = mailHdr.from;
  int tempMailFrom = mailHdr.to;
  if(!validateEntityIndex(conditionIndex, CONDITION)) {
    sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
  }else if(serverConds[conditionIndex].waitQueue->IsEmpty()) //no thread waiting
  {
    sendMessageToClient("No thread waiting!", pktHdr, mailHdr);
  } else {
    // we remove the message from the queue, decode it and use it to acquire the lock after being signaled
    string* msg;
    stringstream ss;
    msg = (string*) (serverConds[conditionIndex].waitQueue->Remove());//Hung: add myself to Condition Variable waitQueue
    ss << *msg;
    ss >> pktHdr.from;
    ss >> mailHdr.from;
    ss >> mailHdr.to;
    //cout << "here?\n" << pktHdr.from << ' ' << mailHdr.from << ' ' << mailHdr.to << endl;
    Acquire_server(lockIndex, pktHdr, mailHdr);
    pktHdr.from = tempPktTo;
    mailHdr.from = tempMailTo;
    mailHdr.to = tempMailFrom;
    mailHdr.length = 9;
    //cout << "here?\n" << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << endl;

    sendMessageToClient("Signalled", pktHdr, mailHdr);
  }
  if(serverConds[conditionIndex].waitQueue->IsEmpty()){
    serverConds[conditionIndex].hasWaitingLock == FALSE; //reset if is empty
    serverConds[conditionIndex].waitingLockIndex == -1;
  }
}

// helper function to signal without postoffice functions
void Signal_without_send(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
  int tempPktTo = pktHdr.from;
  int tempMailTo = mailHdr.from;
  int tempMailFrom = mailHdr.to;
  if(!validateEntityIndex(conditionIndex, CONDITION)) {
    sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
  }else if(serverConds[conditionIndex].waitQueue->IsEmpty()) //no thread waiting
  {
    sendMessageToClient("No thread waiting!", pktHdr, mailHdr);
  } else {
    string* msg;
    stringstream ss;
    msg = (string*) (serverConds[conditionIndex].waitQueue->Remove());//Hung: add myself to Condition Variable waitQueue
    ss << *msg;
    ss >> pktHdr.from;
    ss >> mailHdr.from;
    ss >> mailHdr.to;
    //cout << "here?\n" << pktHdr.from << ' ' << mailHdr.from << ' ' << mailHdr.to << endl;
    Acquire_server(lockIndex, pktHdr, mailHdr);
    pktHdr.from = tempPktTo;
    mailHdr.from = tempMailTo;
    mailHdr.to = tempMailFrom;
    mailHdr.length = 9;
    //cout << "here?\n" << pktHdr.to << ' ' << mailHdr.to << ' ' << mailHdr.from << endl;

  }
  if(serverConds[conditionIndex].waitQueue->IsEmpty()){
    serverConds[conditionIndex].hasWaitingLock == FALSE;
    serverConds[conditionIndex].waitingLockIndex == -1;
  }
}


// broadcast condition server call
void Broadcast_server(int lockIndex, int conditionIndex, PacketHeader &pktHdr, MailHeader &mailHdr) {
  ServerThread thread;
  thread.machineId = pktHdr.from;
  thread.mailboxNum = mailHdr.from;
  ServerLock conditionLock = serverLocks[lockIndex];
  ServerLock *waitingLock = &(serverLocks[serverConds[conditionIndex].waitingLockIndex]);
  int tempPktTo = pktHdr.from;
  int tempMailTo = mailHdr.from;
  int tempMailFrom = mailHdr.to;
  char data[MaxMailSize];
  if(!validateEntityIndex(lockIndex, LOCK)) {
    sendMessageToClient("Invalid lock index!", pktHdr, mailHdr);
  }else if(!validateEntityIndex(conditionIndex, CONDITION)) {
    sendMessageToClient("Invalid cond index!", pktHdr, mailHdr);
  }else if(!(*waitingLock == conditionLock)) {
    sendMessageToClient("No permission to broadcast!", pktHdr, mailHdr);
  }else{
    string* msg;
    int stringIndex;
    stringstream ss;
    while(!serverConds[conditionIndex].waitQueue->IsEmpty()) //waitQueue is not empty
    {
      Signal_without_send(lockIndex, conditionIndex, pktHdr, mailHdr);
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
  }else {
    serverConds[conditionIndex].deleteFlag = TRUE;
    sendMessageToClient("Cond in use, destroy later.", pktHdr, mailHdr);
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
    while(true) {
        //Recieve the message
        ss.str("");
        ss.clear();
        // cout << "Recieve()" << endl;
        cout << "====== Receiving..." << endl;
        postOffice->Receive(0, &pktHdr, &mailHdr, buffer);
        cout << "====== got somehting!" << endl;
        //printf("Got \"%s\" from %d, box %d\n",buffer,pktHdr.from,mailHdr.from);
        fflush(stdout);
        //Parse the message
        serverSuccessCount = 0;
        int entityId = -1;
        int entityIndex1 = -1;
        int entityIndex2 = -1;
        int entityIndex3 = -1;
        // project4: init variables for server logic
        int id = -1, mailbox = -1, value = -1, stringIndex = -1;
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
        cout << "======= tempString: " << tempString << endl;
        // cout << "!!!!! client name " << name << endl;
        if (serverLockCount>0){
        //	        cout << "serverLocks.name " << serverLocks[serverLockCount-1].name << endl;

        }
        if(id == -1 || mailbox == -1) {
            printf("id == %d || mailbox == %d \n\n", id, mailbox);
            interrupt->Halt();
        }
        int decodedMailboxNum = decodeMailbox(value);

        // check if current server has what we need

        bool isCreate = false;
        if (sysCode2 == 'C')
        	isCreate = true;
	    bool currentServerContainsEntity = checkCurrentServerContainEntity(isCreate, name, tempString, sysCode1, sysCode2, entityIndex1, entityIndex2, pktHdr, mailHdr);
	    // cout << tempString << endl;

	    if (serverCount > 1){
	    	switch (response){
	        	case '0':
	        		cout << "======== check response 0" << endl;
	        		// request is sent from client
		        	// put into pending table
	        		// cout << "&&&&&&& RESPONSE IS 0\n";
		        	if (!currentServerContainsEntity){
		        		pending[id][mailbox].isEmpty = false;
			        	pending[id][mailbox].serverRespondCount = 0;
			        	pending[id][mailbox].message = new char[strlen(tempString)+1];
			        	strncpy(pending[id][mailbox].message, tempString, strlen(tempString));
			        	pending[id][mailbox].message[strlen(tempString)] = '\0';
		        		cout << "======= Sending to other servers" << endl;
		        		checkOtherServers(tempString, pktHdr, mailHdr);
		        		continue;
		        	}
		        	// cout << "&&&&&&& after checkservers\n";
	        	break;
	        	case 'C':
	        		cout << "======== check response C" << endl;
		        	// response is "Check" :: return Y or N
	        		mailHdr.to = 0;
	        		mailHdr.from = 0;
	        		cout << "======= in check::tempString: " << tempString << endl;
		            sendMessageToClient(tempString, pktHdr, mailHdr);

		            if (!currentServerContainsEntity) 
		            	continue;
	        	break;
	        	case 'Y':
	        		cout << "======== check response Y" << endl;
	        		++ (pending[id][mailbox].serverRespondCount);
	        		if (pending[id][mailbox].serverRespondCount == (serverCount-1)){
	        			cout << "+++++++ reached serverRespondCount\n";
        				pending[id][mailbox].isEmpty = true;
			        	pending[id][mailbox].serverRespondCount = 0;
			        	pending[id][mailbox].message[0] = '\0';
				    }
		        	// request is sent from another server and the server has what we need
		        	continue;
	        	break;
	        	case 'N':
	        		cout << "======== check response N" << endl;
	        		if (pending[id][mailbox].isEmpty) {
	        			cout << "***********Pending Request Table is empty!\n";
	            		interrupt->Halt();
	        		}

	        		++ (pending[id][mailbox].serverRespondCount);
	        		if (pending[id][mailbox].serverRespondCount == (serverCount-1)){
	        			cout << "+++++++ reached serverRespondCount\n";
	        			pending[id][mailbox].isEmpty = true;
			        	pending[id][mailbox].serverRespondCount = 0;
			        	pending[id][mailbox].message[0] = '\0';
	        			if (sysCode2 != 'C'){
			            	// not a create syscall and all other servers return NO
			            	// request failed
			                char* reqFailed = "Request Failed!";

			                pktHdr.from = id;
			                mailHdr.from = mailbox;
			                mailHdr.to = 0;
			                sendMessageToClient(reqFailed, pktHdr, mailHdr);
			                continue;
		        		}
	        		}else{
	        			continue;
	        		}
	        	break;
	        }
	    }

        

        // sanity check::at this point the machineId of server and request should match
        if(decodedMailboxNum != 0 && !serverAndMachineIdMatch(decodedMailboxNum)) {
            printf("res == %c || id == %d || mailbox == %d || value == %d || machineId == %d \n\n", response, id, mailbox, value, machineId);
            interrupt->Halt();
        }

            
        /* Project 3: ss >> sysCode1 >> sysCode2;
        if(sysCode2 == 'C') {
            ss >> name;
            cout << name << endl;
        } else {
            ss >> entityIndex1;
        }*/
        // big switch statement to determine syscalls
        redirectToOriginalClient(pktHdr, mailHdr, id, mailbox, strlen(tempString));
        entityIndex1 = decodeEntityIndex(entityIndex1);
        entityIndex2 = decodeEntityIndex(entityIndex2);
        entityIndex3 = decodeEntityIndex(entityIndex3);
        switch(sysCode1) {
            case 'L': // lock server calls
                switch(sysCode2) {
                    case 'C': // create lock
                        cout << "===== Outside function call Got to CreateLock_server" << endl;
                        ss.str("");
                        ss.clear();
                        entityId = CreateLock_server(name, serverLockCount, pktHdr, mailHdr);
                        ss << entityId;
                        //cout << "CreateLock_server::entityId: " << entityId << endl;
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
                        entityId = CreateMonitor_server(name, entityIndex1, pktHdr, mailHdr);
                        ss << entityId;
                        //cout << "CreateMonitor_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'G': // get monitor
                        ss.str("");
                        ss.clear();
                        entityId = GetMonitor_server(entityIndex1, entityIndex2,pktHdr, mailHdr);
                    break;
                    case 'S': // set monitor
                        SetMonitor_server(entityIndex1, entityIndex2,entityIndex3,pktHdr, mailHdr);
                        ss.str("");
                        ss.clear();
                        ss << "SetMonitor_server";
                    break;
                    case 'D': // destroy monitor
                        DestroyMonitor_server(entityIndex1, pktHdr, mailHdr);
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
                        entityId = CreateCondition_server(name, serverCondCount, pktHdr, mailHdr);
                        ss << entityId;
                        //cout << "CreateCondition_server::entityId: " << entityId << endl;
                        sendCreateEntityMessage(ss, pktHdr, mailHdr);
                    break;
                    case 'W': // condition wait
                        Wait_server(entityIndex1, entityIndex2, pktHdr, mailHdr); //lock then CV
                        ss.str("");
                        ss.clear();
                        ss << "Wait_server";
                    break;
                    case 'S': // condition signal
                        Signal_server(entityIndex1, entityIndex2, pktHdr, mailHdr); //lock then CV

                        ss.str("");
                        ss.clear();
                        ss << "Signal_server";
                    break;
                    case 'B': // create broadcast
                        Broadcast_server(entityIndex1, entityIndex2, pktHdr, mailHdr); //lock then CV
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
