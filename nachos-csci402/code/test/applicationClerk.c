#include "syscall.h"
#include "setup.h"


void ApplicationClerk() {
  int myLine = -1;
  int i, numYields;
  char personName[50];
  int isCustomer = 1, custNumber;
  Acquire(serverClerkLock);
  for(i = 0; i < clerkArray[0]; ++i) {
      if (GetMonitor(clerkOwner, i) == 1 ){ /*ApplicationClerk index*/
          myLine = i;
          SetMonitor(clerkOwner, i, 0);
          break;
      }
  }
  Release(serverClerkLock);


    while(GetMonitor(allCustomersAreDone, 0) == 0) {
        custNumber = chooseCustomerFromLine(myLine, "ApplicationClerk_", 17);
        if(custNumber >= customerCount) {
            isCustomer = 0;
        } else {
            isCustomer = 1;
        }

        SetMonitor(clerkStates, myLine, BUSY);
        hasSignaledString(isCustomer, "ApplicationClerk_", 17, myLine, custNumber);
        Yield();
        givenSSNString(isCustomer, custNumber, "ApplicationClerk_", 17, myLine);
        Yield();
        recievedSSNString(isCustomer, "ApplicationClerk_", 17, myLine, custNumber);

/* CL: random time for applicationclerk to process data */
        numYields = Rand(80, 20);
        for(i = 0; i < numYields; ++i) {
            Yield();
        }
        SetMonitor(applicationIsFiled, custNumber, 1);
        PrintString("ApplicationClerk_", 17); PrintNum(myLine);
        PrintString(" has recorded a completed application for ", 42);
        PrintCust(isCustomer); PrintNum(custNumber); PrintNl();
        /*TODO: delete next line*/
        PrintNum(GetMonitor(applicationIsFiled, custNumber)); PrintString("test11", 6); PrintNl();

        clerkSignalsNextCustomer(myLine);
    }
}


/*Clerk functions*/

int chooseCustomerFromLine(int myLine, char* clerkName, int clerkNameLength) {
    int testFlag = false;
    int clerkBribeLineCnt, clerkLineCnt, senatorLineCnt, senatorDoneMon, clerkState, allCustDone;
    do {
        testFlag = false;
        /* TODO: -1 used to be NULL.  Hung needs to figure this out */
        senatorLineCnt = GetMonitor(senatorLineCount, 0);
        senatorDoneMon = GetMonitor(senatorDone, 0);
        if((senatorLineCnt > 0 ) || (senatorLineCnt > 0 && senatorDoneMon == 1)) {
            /* CL: chooses senator line first */
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is required by the senator\n", 29);/*HUNG LINE*/
            Acquire(clerkSenatorCVLock[myLine]);
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock acquired\n", 30);/*HUNG LINE*/
            Signal(clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock signal\n", 29);/*HUNG LINE*/
              /*Wait for senator here, if they need me*/
            Wait( clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock waited e\n", 30);/*HUNG LINE*/

            if(senatorLineCnt == 0){
              Acquire(clerkLineLock);
              PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" was unused by senator\n", 23);/*HUNG LINE*/
              SetMonitor(clerkStates, myLine, AVAILABLE);
              Release(clerkSenatorCVLock[myLine]);
            }else if(senatorLineCnt > 0 && senatorDoneMon == 1){
              SetMonitor(clerkStates, myLine, AVAILABLE);
              PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is being used by senator\n", 26);/*HUNG LINE*/

              Release(clerkSenatorCVLock[myLine]);
            }else{
              SetMonitor(clerkStates, myLine, BUSY);
              testFlag = true;
            }
            PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" clerkSenatorCVLock waited f\n", 30);/*HUNG LINE*/

        }else{
          PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is not required by the senator\n", 33);/*HUNG LINE*/
            Acquire(clerkLineLock);

              PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" acquired clerkLinelock\n", 24);/*HUNG LINE*/
            clerkBribeLineCnt = GetMonitor(clerkBribeLineCount, myLine);
            clerkLineCnt = GetMonitor(clerkLineCount, myLine);
            if(clerkBribeLineCnt > 0) {
                PrintString(clerkName, clerkNameLength); PrintNum(myLine); PrintString(" is servicing a customer from bribe line\n", 41);
                Signal(clerkLineLock, clerkBribeLineCV[myLine]);
                SetMonitor(clerkStates, myLine, BUSY); /*redundant setting*/
            } else if(clerkLineCnt > 0) {
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
                allCustDone = GetMonitor(allCustomersAreDone, 0);
                if(allCustDone == 1) {
                    Exit(0);
                }
            }
        }
        clerkState = GetMonitor(clerkStates, myLine);
    } while(clerkState!= BUSY);

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
    clerkState = GetMonitor(customerData, myLine);
    return clerkState;
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
  ApplicationClerk();
  Exit(0);
}
