#include "syscall.h"
#include "setup.h"



void Senator(){
    int custNumber = -1;
    int i, myLine, theLineCount, randomIndex;
    int ssnMon, appMon, picMon, certMon, doneMon, moneyMon, clerkLineCountMon;
    int testing, threadArgs, senatorLineCnt, clerkCash, custCash, clerkMessedUpMon;

    /*Function to help the senator find a unique identifier from the server*/
    Acquire(serverCustomerLock);
    for(i = customerCount; i < customerCount + senatorCount; ++i) {
        if (GetMonitor(SSN, i) == 0 ){
            custNumber = i;
            threadArgs = GetThreadArgs();
            SetMonitor(SSN, i, threadArgs + 1);
            break;
        }
    }

    /*Initialize senator monitors for this customer*/
    SetMonitor(likesPicture, custNumber, 0);
    SetMonitor(applicationIsFiled, custNumber, 0);
    SetMonitor(hasCertification, custNumber, 0);
    SetMonitor(isDone, custNumber , 0);
    SetMonitor(clerkMessedUp, custNumber, 0);
    randomIndex = Rand(4, 0); /*0 to 3*/
    SetMonitor(money, custNumber, 100);
    Release(serverCustomerLock);
    Acquire(senatorLock);
    theLineCount = GetMonitor(senatorLineCount, 0);

    if(theLineCount > 0){
        theLineCount = theLineCount + 1;
        SetMonitor(senatorLineCount, 0, theLineCount);
        Wait(senatorLock, senatorLineCV);
        SetMonitor(senatorDone, 0, 1); /*Set senatorDone to true*/
        for(i = 0; i < clerkCount; i++){
            Acquire(clerkLock[i]);
            Acquire(clerkSenatorCVLock[i]);
        }
        for(i = 0; i < clerkCount; i++){
            Signal(clerkLock[i], clerkCV[i]);
            Release(clerkLock[i]);
        }
        for(i = 0; i < clerkCount; i++){
            PrintString("Senator Waiting for clerk ", 18); PrintNum(i); PrintNl();
            Signal(clerkSenatorCVLock[i], clerkSenatorCV[i]);

            Wait(clerkSenatorCVLock[i], clerkSenatorCV[i]);
            PrintString("Getting confirmation from clerk ", 32); PrintNum(i); PrintNl();
        }
        SetMonitor(senatorDone, 0, 0); /*Set senatorDone to false*/
    }else{
        for(i = 0; i < clerkCount; i++){
            Acquire(clerkSenatorCVLock[i]);
        }
        theLineCount = theLineCount + 1;
        SetMonitor(senatorLineCount, 0, theLineCount);
        Release(senatorLock);
        SetMonitor(senatorDone, 0, 0); /*Set senatorDone to false*/

        for(i = 0; i < clerkCount; i++){
            PrintString("Waiting for clerk ", 18); PrintNum(i); PrintNl();
            Wait(clerkSenatorCVLock[i], clerkSenatorCV[i]);
            PrintString("Getting confirmation from clerk ", 32); PrintNum(i); PrintNl();
        }
    }

    myLine = 0;
    while(!GetMonitor(isDone, custNumber)) {
      appMon = GetMonitor(applicationIsFiled, custNumber);
      picMon = GetMonitor(likesPicture, custNumber);
      certMon = GetMonitor(hasCertification, custNumber);
      doneMon = GetMonitor(isDone, custNumber);

        for(i = 0; i < clerkCount; i++) {
          if(
              appMon == 0 &&
              certMon == 0 &&
              doneMon == 0 &&
                my_strcmp(clerkTypes[i], "ApplicationClerk", clerkTypesLengths[i])) {
                PrintString("    ", 4); PrintString("Senator_", 8); PrintNum(custNumber); PrintString("::: ApplicationClerk chosen\n", 28);
                myLine = i;
            } else if(/*customerAttributes[custNumber].applicationIsFiled &&*/
                      picMon == 0 &&
                      certMon == 0 &&
                      doneMon == 0 &&
                      my_strcmp(clerkTypes[i], "PictureClerk", clerkTypesLengths[i])) {
                PrintString("    ", 4); PrintString("Senator_", 8); PrintNum(custNumber); PrintString("::: PictureClerk chosen\n", 24);
                myLine = i;
            } else if(appMon == 1 &&
                      picMon == 1 &&
                      certMon == 0 &&
                      doneMon == 0 &&
                      my_strcmp(clerkTypes[i], "PassportClerk", clerkTypesLengths[i])) {
                PrintString("    ", 4); PrintString("Senator_", 8); PrintNum(custNumber); PrintString("::: PassportClerk chosen\n", 25);
                myLine = i;
            } else if(appMon == 1 &&
                      picMon == 1 &&
                     certMon == 1 &&
                      doneMon == 0 &&
                      my_strcmp(clerkTypes[i], "Cashier", clerkTypesLengths[i])) {
                PrintString("    ", 4); PrintString("Senator_", 8); PrintNum(custNumber); PrintString("::: Cashier chosen\n", 19);
                myLine = i;
            }
        }
        Signal(clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);
        PrintString("Senator_", 8); PrintNum(custNumber); PrintString("has gotten in regular line for ", 31);
        PrintString(clerkTypes[myLine], clerkTypesLengths[myLine]); PrintString("_", 1); PrintNum(myLine); PrintNl();
        Wait(clerkSenatorCVLock[myLine], clerkSenatorCV[myLine]);

        Release(clerkSenatorCVLock[myLine]);
        Acquire(clerkLock[myLine]);
        /*Give my data to my clerk*/
        SetMonitor(customerData, myLine, custNumber);
        Signal(clerkLock[myLine], clerkCV[myLine]);

        /*wait for clerk to do their job*/
        Wait(clerkLock[myLine], clerkCV[myLine]);
        /*Read my data*/
    }

    Acquire(senatorLock);
    senatorLineCnt =  GetMonitor(senatorLineCount, 0);
    senatorLineCnt = senatorLineCnt - 1;
    SetMonitor(senatorLineCount, 0,  senatorLineCnt);
    if(senatorLineCnt == 0){
        for(i = 0 ; i < clerkCount ; i++){
            /*Free up clerks that worked with me*/
            Signal(clerkLock[i],clerkCV[i]);
            /*Frees all locks I hold*/
            Release(clerkLock[i]);
            /*Free up clerks that I didn't work with*/
            Signal(clerkSenatorCVLock[i], clerkSenatorCV[i]);
            /*Frees up all clerkSenatorCVLocks*/
            Release(clerkSenatorCVLock[i]);
        }
        Acquire(outsideLock);
        Broadcast(outsideLock, outsideLineCV);
        Release(outsideLock);
        Release(senatorLock);
    }else{
        for(i = 0 ; i < clerkCount ; i++){
            /*Free up clerkLocks for other senator*/
            Release(clerkLock[i]);
            /*Frees up clerkSenatorCVLocks for other senator*/
            Release(clerkSenatorCVLock[i]);
        }
        Signal(senatorLock, senatorLineCV);
        Release(senatorLock);
    }
    PrintString("Senator_", 8); PrintNum(custNumber); PrintString(" is leaving the Passport Office.\n", 33);

}

/*Senator helper function to compary char arrays*/
int my_strcmp(char s1[], const char s2[], int len) {
    int i = 0;
    for(i = 0; i < len; ++i) {
        if(s1[i] == '\0') {
            return false;
        }

        if(s2[i] == '\0') {
            return false;
        }

        if(s1[i] != s2[i]) {
            return false;
        }
    }

    return true;
}


int main(){
  setup();
  Senator();
  Exit(0);
}
