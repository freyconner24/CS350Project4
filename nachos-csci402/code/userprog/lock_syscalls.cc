#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "custom_syscalls.h"
#include "synchlist.h"
#include "addrspace.h"
#include "network.h"
#include "post.h"
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h>

#define BUFFER_SIZE 32

// +++++++++++++++++++++++++ UTILITY +++++++++++++++++++++++++
// generic function that allows us to send any message to the server
void sendToServer(PacketHeader &pktHdr, MailHeader &mailHdr, char* serverCode, char name[], int entityIndex1, int entityIndex2, int entityIndex3, int mailBox) {
    mailHdr.to = 0;
    mailHdr.from = mailBox;
    // generating random server number
    int serverNum = rand() % (serverCount);
    // serverNum = (serverCount - 1);
    // cout << "_____________serverNum " << serverNum << endl;
    pktHdr.to = serverNum;

    stringstream ss;

	ss << '0' << ' ' << machineId << ' ' << mailBox << ' ' << serverCode[0] << ' ' << serverCode[2] << ' ' << entityIndex1 << ' ' << entityIndex2 << ' ' << entityIndex3 << ' ' << name;
	string str = ss.str();
	cout << "str: " << str << endl;
	char sendBuffer[64];

	for(unsigned int i = 0; i < str.size(); ++i) {
		sendBuffer[i] = str.at(i);
	}
	sendBuffer[str.size()] = '\0';
	mailHdr.length = str.size() + 1;
	// cout << "pktHdr.to: " << pktHdr.to << "mailHdr.to: " << mailHdr.to <<  "mailHdr.from: " << mailHdr.from << endl;
	// cout << "Client::sendBuffer: " << sendBuffer << endl;
    bool success = postOffice->Send(pktHdr, mailHdr, sendBuffer);

    if ( !success ) {
    	// cout << serverCode << "::";
		printf("Client::The postOffice Send failed. You must not have the other Nachos running. Terminating Nachos.\n");
		interrupt->Halt();
	}
}

// generic function that allows us to receive any message to the server
string getFromServer(PacketHeader &pktHdr, MailHeader &mailHdr, int mailBox) {
	char inBuffer[64];
    postOffice->Receive(mailBox, &pktHdr, &mailHdr, inBuffer);
    stringstream ss;
    ss << inBuffer;
    return ss.str();
}

// generic send and recieve paradigm for program
// this function takes the syscallCode, name of entity to be created, and the entity indexes
// if it is not create, pass name as ""
// if entity indexe(s) are needed, pass -1
string sendAndRecieveMessage(char* sysCode, char* name, int entityIndex1, int entityIndex2, int entityIndex3) {
	PacketHeader pktHdr;
	MailHeader mailHdr;
	sendToServer(pktHdr, mailHdr, sysCode, name, entityIndex1, entityIndex2, entityIndex3, currentThread->mailbox);

	return getFromServer(pktHdr, mailHdr, currentThread->mailbox);
}

// create lock syscall
int CreateLock_sys(int vaddr, int size, int appendNum) {
	char* name = new char[size + 1]; //allocate new char array
	name[size] = '\0'; //end the char array with a null character

	if (copyin(vaddr, size, name) == -1){
		DEBUG('l',"%s"," COPYIN FAILED\n");
		delete[] name;
		currentThread->space->locksLock->Release();
		return -1;
	}; //copy contents of the virtual addr (ReadRegister(4)) to the name

    string receivedString = sendAndRecieveMessage("L C ", name, -1, -1, -1);
    cout << "receivedString " << receivedString << endl;
    stringstream ss;
    ss << receivedString;

    int currentLockIndex = -1;
    ss >> currentLockIndex;
    // std::time_t result = std::time(NULL);
	   //  std::cout << std::asctime(std::localtime(&result))
	   //            << result << " seconds since the Epoch\n";

    if(currentLockIndex == -1) {
        cout << "Client::currentLockIndex == -1" << endl;
        interrupt->Halt();
    }
    cout << "Client::CreateLock:: " <<currentLockIndex<< endl;

	currentThread->space->lockCount += 1;
	return currentLockIndex;
}

// acquire lock syscall
void Acquire_sys(int lockIndex) {
	string receivedString = sendAndRecieveMessage("L A ", "", lockIndex, -1, -1);
    cout << "Acquire::receivedString: " << receivedString << endl;
}

// release lock syscall
void Release_sys(int lockIndex) {
	string receivedString = sendAndRecieveMessage("L R ", "", lockIndex, -1, -1);
    cout << "Release::receivedString: " << receivedString << endl;
}

void DestroyLock_sys(int lockIndex) {
	string receivedString = sendAndRecieveMessage("L D ", "", lockIndex, -1, -1);
    cout << "DestroyLock::receivedString: " << receivedString << endl;
}

// ++++++++++++++++++++ MONITORS ++++++++++++++++++++++++

// create monitor syscall
int CreateMonitor_sys(int vaddr, int size, int arraySize) {
	char* name = new char[size + 1]; //allocate new char array
	name[size] = '\0'; //end the char array with a null character

	if (copyin(vaddr, size, name) == -1){
		DEBUG('l',"%s"," COPYIN FAILED\n");
		delete[] name;
		currentThread->space->locksLock->Release();
		return -1;
	}; //copy contents of the virtual addr (ReadRegister(4)) to the name

    string receivedString = sendAndRecieveMessage("M C ", name, arraySize, -1, -1);
    stringstream ss;
    ss << receivedString;

    int currentMonIndex = -1;
    ss >> currentMonIndex;
    if(currentMonIndex == -1) {
         cout << "Client::currentMonIndex == -1" << endl;
        interrupt->Halt();
    }

	//currentThread->space->lockCount += 1;
	return currentMonIndex;
}

// get monitor syscall
int GetMonitor_sys(int monitorIndex, int arrayIndex) {
	string receivedString = sendAndRecieveMessage("M G ", "", monitorIndex, arrayIndex, -1);
    cout << "Client::GetMonitor::receivedString: "  <<  endl;
    cout << receivedString << endl;
    int value;
    stringstream ss;
    ss << receivedString;
    ss >> value;
    // cout << "Got Monitor value: " << value << endl;
    return value;
}

// set monitor syscall
void SetMonitor_sys(int monitorIndex, int arrayIndex, int value) {
	string receivedString = sendAndRecieveMessage("M S ", "", monitorIndex, arrayIndex, value);
  cout << "Client::SetMonitor::receivedString: " << receivedString <<  ' ' << monitorIndex << ' ' << arrayIndex << ' '<< value << endl;
}

// destroy monitor syscall
void DestroyMonitor_sys(int monitorIndex) {
	string receivedString = sendAndRecieveMessage("M D ", "", monitorIndex, -1, -1);
     cout << "Client::DestroyMonitor::receivedString: " << receivedString << endl;
}
