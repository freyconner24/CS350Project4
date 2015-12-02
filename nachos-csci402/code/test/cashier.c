#include "syscall.h"
#include "setup.h"



void Cashier() {
    int myLine = -1;
    int numYields, clerkMessedUp, i;
    char personName[50], isCustomer = 1;

    Acquire(serverClerkLock);
    for(i = clerkArray[0] + clerkArray[1] + clerkArray[2]; i < clerkArray[0] + clerkArray[1] + clerkArray[2] + clerkArray[3]; ++i) {
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
      PrintString("Cashier_Allocation worked ", 26); PrintNum(myLine); PrintNl();
    }

    while(GetMonitor(allCustomersAreDone, 0) == 0) {
        int custNumber = chooseCustomerFromLine(myLine, "Cashier_", 8);
        if(custNumber >= customerCount) {
            isCustomer = 0;
        } else {
            isCustomer = 1;
        }
        hasSignaledString(isCustomer, "Cashier_", 8, myLine, custNumber);
        Yield();
        givenSSNString(isCustomer, custNumber, "Cashier_", 8, myLine);
        Yield();
        recievedSSNString(isCustomer, "Cashier_", 8, myLine, custNumber);

        if(GetMonitor(hasCertification, custNumber)) {
            PrintString("Cashier_", 8); PrintNum(myLine); PrintString(" has verified that ", 19);
            PrintCust(isCustomer); PrintNum(custNumber); PrintString("has been certified by a PassportClerk\n", 38);
            SetMonitor(money, custNumber, GetMonitor(money, custNumber) - 100);
            PrintString("Cashier_", 8); PrintNum(myLine); PrintString(" has received the $100 from ", 28);
            PrintCust(isCustomer); PrintNum(custNumber); PrintString("after certification\n", 20);
            PrintCust(isCustomer); PrintNum(custNumber); PrintString(" has given ", 11); PrintString("Cashier_", 8); PrintNum(myLine); PrintString(" $100\n", 6);

            SetMonitor(clerkMoney, myLine, GetMonitor(clerkMoney, myLine) + 100);
            SetMonitor(clerkStates, myLine, BUSY);
            numYields = Rand(80, 20);
            /* CL: yields after processing money*/
            for(i = 0; i < numYields; ++i) {
                Yield();
            }
            clerkMessedUp = Rand(100, 0);
            if(custNumber > 49){
                clerkMessedUp = 100;
            }
            if(clerkMessedUp <= 5 && isCustomer) { /* Send to back of line*/
                PrintString("Cashier_", 8); PrintNum(myLine); PrintString(": Messed up for ", 16);
                PrintCust(isCustomer); PrintNum(custNumber);
                PrintString(". Sending customer to back of line.\n", 36);
                SetMonitor(clerkMessedUp, custNumber, 1);
            } else {
                PrintString("Cashier_", 8); PrintNum(myLine); PrintString(" has provided ", 14);
                PrintCust(isCustomer); PrintNum(custNumber); PrintString(" their completed passport\n", 26);
                Yield();
                PrintString("Cashier_", 8); PrintNum(myLine); PrintString(" has recorded that ", 19);
                PrintCust(isCustomer); PrintNum(custNumber); PrintString(" has been given their completed passport\n", 41);
                SetMonitor(clerkMessedUp, custNumber, 0);
                SetMonitor(isDone, custNumber, 1);
            }
        }
        clerkSignalsNextCustomer(myLine);
    }
}

/*Clerk functions*/

int chooseCustomerFromLine(int myLine, char* clerkName, int clerkNameLength) {
    int testFlag = false;
    int clerkBribeLineCnt, clerkLineCnt, senatorLineCnt, senatorDoneMon, clerkState;
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
              SetMonitor(clerkStates, myLine, AVAILABLE);
              Release(clerkSenatorCVLock[myLine]);
            }else if(senatorLineCnt > 0 && senatorDoneMon == 1){
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
  Cashier();
  Exit(0);
}
