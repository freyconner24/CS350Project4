
#ifndef SETUP_H
#define SETUP_H

#include "syscall.h"


#define CLERK_NUMBER 20
#define CUSTOMER_NUMBER 60
#define CLERK_TYPES 4
#define SENATOR_NUMBER 10
#define false 0
#define true 1
/*Setup variables*/
extern int testChosen; /* CL: indicate test number (1-7) or full program (0)*/
extern int clerkCount;  /* CL: number of total clerks of the 4 types that can be modified*/
extern int customerCount; /* CL: number of customers that can be modified*/
extern int senatorCount; /* CL: number of senators that can be modified*/

/*Locks and conditions*/
extern int clerkSenatorLineCV;
extern int clerkLineLock;
extern int outsideLineCV;
extern int outsideLock;
extern int senatorLock;
extern int senatorLineCV;

extern int clerkLock[CLERK_NUMBER];
extern int clerkCV[CLERK_NUMBER];
extern int clerkLineCV[CLERK_NUMBER];
extern int clerkBribeLineCV[CLERK_NUMBER];
extern int breakLock[CLERK_NUMBER];
extern int breakCV[CLERK_NUMBER];
extern int clerkSenatorCV[CLERK_NUMBER];
extern int clerkSenatorCVLock[CLERK_NUMBER];

/*Server lock*/
extern int serverClerkLock;
extern int clerkOwner;
extern int serverCustomerLock;
extern int serverCustomInitLock;
/*Customer number initializion*/
extern int customerInitMon;
/*Monitors of size 1*/
extern int allCustomersAreDone;
extern int senatorLineCount; /* CL: number of senators in a line at any given time*/
extern int senatorDone;
extern int outsideLineCount;

/*Monitors of CLERK_NUMBER size*/
extern int clerkLineCount;
extern int clerkBribeLineCount;
extern int clerkStates;
extern int clerkMoney;
extern int customerData;
/*Monitor for customerAttributes*/
extern int SSN;
extern int likesPicture;
extern int applicationIsFiled;
extern int hasCertification;
extern int isDone;
extern int clerkMessedUp;
extern int money;

extern struct CustomerAttribute customerAttributes[CUSTOMER_NUMBER];
typedef struct CustomerAttribute {
    int SSN;
    int likesPicture;
    int applicationIsFiled;
    int hasCertification;
    int isDone;
    int clerkMessedUp;
    int money;
    int currentLine;
}CustomerAttribute;

extern int clerkTypesLengths[CLERK_NUMBER];
extern char clerkTypes[CLERK_NUMBER][30];
extern int clerkArray[CLERK_TYPES];
extern char clerkTypesStatic[CLERK_TYPES][30];
/*Helpers*/
typedef enum X {AVAILABLE, BUSY, ONBREAK} ClerkState; /* CL: enum for clerk's conditions*/
typedef void (*VoidFunctionPtr)(int arg);
extern int setup();
extern void createClerkLocksConditionsMonitors();
extern void initializeEverything();

#endif
