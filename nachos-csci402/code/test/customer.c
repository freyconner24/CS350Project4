#include "syscall.h"
#include "setup.h"

struct CustomerAttribute initCustAttr(int ssn) {
    int moneyArray[4] = {100, 600, 1100, 1600};
    int randomIndex = Rand(4, 0); /*0 to 3*/

    struct CustomerAttribute ca;
    ca.SSN = ssn;
    ca.applicationIsFiled = false;
    ca.likesPicture = false;
    ca.hasCertification = false;
    ca.isDone = false;
    ca.clerkMessedUp = false;

    ca.money = moneyArray[randomIndex];
    return ca;
}

void Customer() {
    int custNumber = -1;
    int yieldTime, i;
    int bribe = false; /*HUNG: flag to know whether the customer has paid the bribe, can't be arsed to think of a more elegant way of doing this*/
    int myLine = -1;
    int lineSize = 1000;
    int pickedApplication;
    int pickedPicture;
    int totalLineCount;
    int moneyArray[4] = {100, 600, 1100, 1600};
    int randomIndex;
    int ssnMon, appMon, picMon, certMon, doneMon, moneyMon, clerkLineCountMon, clerkBribeLineCountMon, stateMon;
    int testing, threadArgs, senatorLineCnt, clerkCash, custCash, clerkMessedUpMon;
    struct CustomerAttribute myCustAtt = initCustAttr(custNumber); /*Hung: Creating a CustomerAttribute for each new customer*/

    Acquire(serverCustomerLock);
    for(i = 0; i < customerCount; ++i) {
        ssnMon = GetMonitor(SSN, i);
        if (ssnMon == 0 ){ /*ApplicationClerk index*/
             custNumber = i;
             threadArgs = GetThreadArgs();
             SetMonitor(SSN, i, threadArgs + 1);
             PrintString("ThreadArgs for customer, custNumber: ", 25); PrintNum(threadArgs); PrintString(", ",2);PrintNum(custNumber);PrintNl();
             break;
         }
    }

     SetMonitor(likesPicture, custNumber, 0);
     SetMonitor(applicationIsFiled, custNumber, 0);
     SetMonitor(hasCertification, custNumber, 0);
     SetMonitor(isDone, custNumber , 0);
     SetMonitor(clerkMessedUp, custNumber, 0);
     randomIndex = Rand(4, 0); /*0 to 3*/
     SetMonitor(money, custNumber, moneyArray[randomIndex]);
     Release(serverCustomerLock);
     if(custNumber == -1){
       PrintString("Allocation didn't work\n", 24);
     }

     if(custNumber != -1){
       PrintString("Allocation worked customer, moneyarray ", 27); PrintNum(moneyArray[randomIndex]); PrintNl();
     }
    customerAttributes[custNumber] = myCustAtt;
    while(!GetMonitor(isDone, custNumber)) {     /*while(!customerAttributes[custNumber].isDone) {*/
        PrintString("-------Customer starting loop: myLine: ", 39); PrintNum(myLine); PrintNl();
        senatorLineCnt = GetMonitor(senatorLineCount, 0);
        if( senatorLineCnt> 0){

            PrintString("Customer acquiring outside: myLine ", 35); PrintNum(myLine); PrintNl();
            Acquire(outsideLock);
            Wait(outsideLock, outsideLineCV);
            Release(outsideLock);
            PrintString("Customer getting back inside: myLine ", 35); PrintNum(myLine); PrintNl();

        }

        PrintString("Customer acquiring clerkLineLock: myLine, clerkLineLock", 55); PrintNum(myLine); PrintNum(clerkLineLock); PrintNl();
        Acquire(clerkLineLock); /* CL: acquire lock so that only this customer can access and get into the lines*/
        PrintString("Customer acquired clerkLineLock: myLine ", 40); PrintNum(myLine); PrintNl();

        bribe = false;
        myLine = -1;
        lineSize = 1000;
        appMon = GetMonitor(applicationIsFiled, custNumber);
        picMon = GetMonitor(likesPicture, custNumber);
        certMon = GetMonitor(hasCertification, custNumber);
        doneMon = GetMonitor(isDone, custNumber);
        if(appMon == 0 && picMon == 0) { /* check conditions if application and picture are done*/
            pickedApplication = Rand(2, 0);
            if(pickedApplication == 1){
              pickedPicture = 0;
            }else{
              pickedPicture = 1;
            }
        } else {
            pickedApplication = 1;
            pickedPicture = 1;
        }
        for(i = 0; i < clerkCount; i++) {
            clerkLineCountMon = GetMonitor(clerkLineCount, i);
            clerkBribeLineCountMon = GetMonitor(clerkBribeLineCount, i);
            totalLineCount =  clerkLineCountMon + clerkBribeLineCountMon;
            /* CL: if else pairs for customer to choose clerk based on their attributes*/
            if(pickedApplication == 1 &&
                appMon == 0 &&
                certMon == 0 &&
                doneMon == 0 &&
                my_strcmp(clerkTypes[i], "ApplicationClerk", clerkTypesLengths[i])) {
                if(totalLineCount < lineSize) {
                    PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" choosing application clerk: ", 29); PrintNum(i); PrintNl();
                    myLine = i;
                    lineSize = totalLineCount;
                }
            } else if(pickedPicture == 1 &&
                      picMon == 0 &&
                      certMon == 0 &&
                      doneMon == 0 &&
                      my_strcmp(clerkTypes[i], "PictureClerk", clerkTypesLengths[i])) {
                if(totalLineCount < lineSize) {

                      PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" choosing picture clerk ", 24); PrintNum(i); PrintNl();
                    myLine = i;
                    lineSize = totalLineCount;
                }
            } else if(appMon == 1 &&
                      picMon == 1 &&
                      certMon == 0 &&
                      doneMon == 0 &&
                      my_strcmp(clerkTypes[i], "PassportClerk",clerkTypesLengths[i])) {
                if(totalLineCount < lineSize) {
                    PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" choosing passport clerk ", 25);  PrintNum(i); PrintNl();
                    myLine = i;
                    lineSize = totalLineCount;
                }
            } else if(appMon == 1 &&
                      picMon == 1 &&
                      certMon == 1 &&
                      doneMon == 0 &&
                      my_strcmp(clerkTypes[i], "Cashier", clerkTypesLengths[i])) {
                if(totalLineCount < lineSize) {
                      PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" choosing cashier clerk ", 24); PrintNum(i); PrintNl();
                    myLine = i;
                    lineSize = totalLineCount;
                }
            }
        }

        PrintString("Customer_", 9); PrintNum(custNumber);PrintString("------------- myLine: ", 22); PrintNum(myLine); PrintString(", ",2); PrintNum(appMon);PrintString(", ",2); PrintNum(picMon);PrintString(", ",2); PrintNum(certMon);PrintString(", ",2); PrintNum(doneMon); PrintNl();
        stateMon = GetMonitor(clerkStates, myLine);
        if(stateMon != AVAILABLE ) { /*clerkStates[myLine] == BUSY*/
            /*I must wait in line*/
            custCash = GetMonitor(money, custNumber);
            clerkCash = GetMonitor(clerkMoney, myLine);

            if(custCash > 100){

                clerkBribeLineCountMon = GetMonitor(clerkBribeLineCount, myLine);
                PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" has gotten in bribe line for ", 30);
                PrintString(clerkTypes[myLine], clerkTypesLengths[myLine]); PrintString("_", 1); PrintNum(myLine); PrintString(" of length ", 11); PrintNum(clerkBribeLineCountMon); PrintNl();
                /* CL: takes bribe money*/
                custCash = custCash - 5;
                SetMonitor(money, custNumber, custCash);
                clerkCash = clerkCash + 5;
                SetMonitor(clerkMoney, myLine, clerkCash);
                clerkBribeLineCountMon = clerkBribeLineCountMon + 1;
                SetMonitor(clerkBribeLineCount, myLine, clerkBribeLineCountMon);
                bribe = true;

                /*testing*/
                custCash = GetMonitor(money, custNumber);
                PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" has ", 5); PrintNum(custCash); PrintNl();
                clerkCash = GetMonitor(clerkMoney, myLine);
                PrintString(clerkTypes[myLine], clerkTypesLengths[myLine]); PrintString("_", 1); PrintNum(myLine); PrintString(" has ", 11); PrintNum(clerkCash); PrintNl();

                Wait(clerkLineLock, clerkBribeLineCV[myLine]);
            } else {
                clerkLineCountMon = GetMonitor(clerkLineCount, myLine);
                PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" has gotten in regular line for ", 32);
                PrintString(clerkTypes[myLine], clerkTypesLengths[myLine]); PrintString("_", 1); PrintNum(myLine); PrintString(" of length ", 11); PrintNum(clerkLineCountMon); PrintNl();
                clerkLineCountMon = clerkLineCountMon + 1;
                SetMonitor(clerkLineCount, myLine, clerkLineCountMon);
                Wait(clerkLineLock, clerkLineCV[myLine]);
            }
            /*redundant*/
            totalLineCount = 0;
            for(i = 0; i < clerkCount; ++i) {

                totalLineCount = totalLineCount + GetMonitor(clerkBribeLineCount, i) + GetMonitor(clerkLineCount, i);
            }

            if(bribe) {
                clerkBribeLineCountMon = GetMonitor(clerkBribeLineCount, myLine);
                clerkBribeLineCountMon = clerkBribeLineCountMon -1;
                SetMonitor(clerkBribeLineCount, myLine, clerkBribeLineCountMon);
            } else {
                clerkLineCountMon = GetMonitor(clerkLineCount, myLine);
                clerkLineCountMon = clerkLineCountMon -1;
                SetMonitor(clerkLineCount, myLine, clerkLineCountMon);
            }

        } else {
          stateMon = GetMonitor(clerkStates, myLine);
          PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" is first in line for clerk.\n", 30);
            PrintNum(stateMon);PrintNl();
            SetMonitor(clerkStates, myLine, BUSY);
        }
        PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" is trying to release clerkLineLock\n", 36);
        Release(clerkLineLock);
        PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" released clerkLineLock\n", 24);

        Acquire(clerkLock[myLine]);
        /*Give my data to my clerk*/
        SetMonitor(customerData, myLine, custNumber);
        PrintString("Test print 1\n", 13);

        Signal(clerkLock[myLine], clerkCV[myLine]);

        PrintString("Test print 2\n", 13);
        /*wait for clerk to do their job*/
        Wait(clerkLock[myLine], clerkCV[myLine]);
       /*Read my data*/
        Signal(clerkLock[myLine], clerkCV[myLine]);
        Release(clerkLock[myLine]);
        clerkMessedUpMon = GetMonitor(clerkMessedUp, custNumber);
        if(clerkMessedUpMon == 1) {
            PrintString("Customer_", 9); PrintNum(custNumber);PrintString(": clerk messed up.  Customer is going to the back of the line.: ", 61); PrintNum(clerkMessedUpMon);PrintNl();
            yieldTime = Rand(900, 100);
            for(i = 0; i < yieldTime; ++i) {
                Yield();
            }
            SetMonitor(clerkMessedUp, custNumber, 0);
        }
        testing = GetMonitor(applicationIsFiled, custNumber);
        PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" monitors applicationIsFiled: ", 30); PrintNum(testing); PrintNl();
        testing = GetMonitor(likesPicture, custNumber);
        PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" monitors likesPicture: ", 24);  PrintNum(testing); PrintNl();
        testing = GetMonitor(hasCertification, custNumber);
        PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" monitors hasCertification: ", 28);  PrintNum(testing); PrintNl();
        testing = GetMonitor(isDone, custNumber);
        PrintString("Customer_", 9); PrintNum(custNumber);PrintString(" monitors isDone: ", 18);  PrintNum(testing); PrintNl();
    }
    /* CL: CUSTOMER IS DONE! YAY!*/
    PrintString("Customer_", 9); PrintNum(custNumber); PrintString(" is leaving the Passport Office.\n", 33);


}


/*Customer functions*/



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
  Customer();
  Exit(0);
}
