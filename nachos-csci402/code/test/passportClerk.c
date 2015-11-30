#include "syscall.h"
#include "setup.h"


void PassportClerk() {
    int myLine = -1;
    int numYields, clerkMessedUp, i, isCustomer = 1;
    char personName[50];

    Acquire(serverClerkLock);
    for(i = clerkArray[0] + clerkArray[1]; i < clerkArray[0] + clerkArray[1] + clerkArray[2]; ++i) {
        if (GetMonitor(clerkOwner, i) == 1 ){ /*ApplicationClerk index*/
            myLine = i;
            SetMonitor(clerkOwner, i, 0);
            break;
        }
    }
    Release(serverClerkLock);
    if(myLine == -1){
      PrintString("Allocation didn't work\n", 24);
    }
    if(myLine != -1){
      PrintString("PassportClerk_Allocation worked ", 32); PrintNum(myLine); PrintNl();
    }

    while(GetMonitor(allCustomersAreDone, 0) == 0) {
        int custNumber = chooseCustomerFromLine(myLine, "PassportClerk_", 14);
        if(custNumber >= customerCount) {
            isCustomer = 0;
        } else {
            isCustomer = 1;
        }
        hasSignaledString(isCustomer, "PassportClerk_", 14, myLine, custNumber);
        Yield();
        givenSSNString(isCustomer, custNumber, "PassportClerk_", 14, myLine);
        Yield();
        recievedSSNString(isCustomer, "PassportClerk_", 14, myLine, custNumber);

        if(GetMonitor(likesPicture, custNumber) &&  GetMonitor(applicationIsFiled, custNumber)) {
            /* CL: only determine that customer has app and pic completed by the boolean */
            PrintString("PassportClerk_", 14); PrintNum(myLine); PrintString(" has determined that ", 21);
            PrintCust(isCustomer); PrintNum(custNumber); PrintString(" has both their application and picture completed\n",50);

            SetMonitor(clerkStates, myLine, BUSY);

            numYields = Rand(80, 20);

            clerkMessedUp = Rand(100, 0);
            if(custNumber > 49){
              clerkMessedUp = 100;
            }
            if(clerkMessedUp <= 5) { /* Send to back of line */
                PrintString("PassportClerk_", 14); PrintNum(myLine); PrintString(": Messed up for ", 16);
                PrintCust(isCustomer); PrintNum(custNumber); PrintString(". Sending customer to back of line.\n", 36);
                SetMonitor(clerkMessedUp, custNumber, 1);
            } else {
                PrintString("PassportClerk_", 14); PrintNum(myLine); PrintString(" has recorded ", 14);
                PrintCust(isCustomer); PrintNum(custNumber); PrintString(" passport documentation\n", 24);
                for(i = 0; i < numYields; ++i) {
                    Yield();
                }
                SetMonitor(clerkMessedUp, custNumber, 0);
                SetMonitor(hasCertification, custNumber, 1);
            }
        } else {
            PrintString("PassportClerk_", 14); PrintNum(myLine); PrintString(" has determined that ", 21);
            PrintCust(isCustomer); PrintNum(custNumber); PrintString(" does not have both their application and picture completed\n", 60);
        }
        clerkSignalsNextCustomer(myLine);
    }
}


/*Clerk functions*/

int chooseCustomerFromLine(int myLine, char* clerkName, int clerkNameLength) {
    int testFlag = false;
    do {
        testFlag = false;
        /* TODO: -1 used to be NULL.  Hung needs to figure this out */
        if((GetMonitor(senatorLineCount, 0) > 0 ) || ((GetMonitor(senatorLineCount, 0) > 0 && GetMonitor(senatorDone, 0)))) {
            /* CL: chooses senator line first */
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is required by the senator\n", 29);/*HUNG LINE*/
            Acquire(clerkSenatorCVLock[myLine]);
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock acquired\n", 30);/*HUNG LINE*/
            Signal(clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock signal\n", 29);/*HUNG LINE*/

            /*Wait for senator here, if they need me*/
            Wait( clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock waited e\n", 30);/*HUNG LINE*/

            if(GetMonitor(senatorLineCount, 0) == 0){
              Acquire(clerkLineLock);
              SetMonitor(clerkStates, myLine, AVAILABLE);
              Release(clerkSenatorCVLock[myLine]);
            }else if(GetMonitor(senatorLineCount, 0) > 0 && GetMonitor(senatorDone, 0)){
              SetMonitor(clerkStates, myLine, AVAILABLE);
              Release(clerkSenatorCVLock[myLine]);

            }else{
              SetMonitor(clerkStates, myLine, BUSY);
              testFlag = true;
            }
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock waited f\n", 30);/*HUNG LINE*/

        }else{
          PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is not required by the senator\n", 33);/*HUNG LINE*/
            Acquire(clerkLineLock);
            if(GetMonitor(clerkBribeLineCount, myLine) > 0) {
                PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is servicing a customer from bribe line\n", 41);
                Signal(clerkLineLock, clerkBribeLineCV[myLine]);
                SetMonitor(clerkStates, myLine, BUSY); /*redundant setting*/
            } else if(GetMonitor(clerkLineCount, myLine) > 0) {
                PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is servicing a customer from regular line\n", 43);
                Signal(clerkLineLock, clerkLineCV[myLine]);
                SetMonitor(clerkStates, myLine, BUSY); /*redundant setting*/
            }else{
                Acquire(breakLock[myLine]);
                PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is going on break\n", 19);
                SetMonitor(clerkStates, myLine, ONBREAK);
                Release(clerkLineLock);
                Wait(breakLock[myLine], breakCV[myLine]);
                /*PrintNum(breakLock[myLine]); PrintNl();*/
                PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is waking up from a break now.\n", 33); PrintNl();/*HUNG LINE*/
                SetMonitor(clerkStates, myLine, AVAILABLE);
                Release(breakLock[myLine]);
                /*if(allCustomersAreDone) {
                    Exit(0);
                }*/
            }
        }
    } while(GetMonitor(clerkStates, myLine) != BUSY);

    PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" Acquring clerkLock\n", 21);/*HUNG LINE*/

    Acquire(clerkLock[myLine]);
    PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" Clerklock acquired\n", 21);/*HUNG LINE*/

    Release(clerkLineLock);
    /*wait for customer*/
    if(testFlag){
      Signal(clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);
      Release(clerkSenatorCVLock[myLine]);
    }
    Wait(clerkLock[myLine], clerkCV[myLine]);
    /*Do my job -> customer waiting*/
    return GetMonitor(customerData, myLine);
}


void hasSignaledString(int isCustomer, char* threadName, int threadNameLength, int clerkNum, int custNumber) {
    PrintString(threadName, threadNameLength); PrintNum(clerkNum); PrintString(" has signalled ", 15);
    PrintCust(isCustomer); PrintNum(custNumber); PrintString(" to come to their counter. (", 28);
    PrintCust(isCustomer); PrintNum(custNumber); PrintString(")", 1); PrintNl();
}

void givenSSNString(int isCustomer, int custNumber, char* threadName, int threadNameLength, int clerkNum) {
    PrintCust(isCustomer); PrintNum(custNumber); PrintString(" has given SSN ", 15);
    PrintNum(custNumber); PrintString(" to ", 4); PrintString(threadName, threadNameLength); PrintNum(clerkNum); PrintNl();
}

void recievedSSNString(int isCustomer, char* threadName, int threadNameLength, int clerkNum, int custNumber) {
    PrintString(threadName, threadNameLength); PrintNum(clerkNum); PrintString(" has received SSN ", 18); PrintNum(custNumber);
    PrintString(" from ", 6); PrintCust(isCustomer); PrintNum(custNumber); PrintNl();
}

void clerkSignalsNextCustomer(int myLine) {
    Signal(clerkLock[myLine], clerkCV[myLine]);
    /* If there is a senator, here is where the clerk waits, after senator is done with them */
    Wait(clerkLock[myLine],clerkCV[myLine]);
    Release(clerkLock[myLine]);
}


void PrintCust(int isCustomer) {
    if(isCustomer) {
        PrintString("Customer_", 9);
    } else {
        PrintString("Senator_", 8);
    }
}


int main(){

    setup();
  PassportClerk();
  Exit(0);
}
