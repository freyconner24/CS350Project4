#include "syscall.h"
#include "setup.h"

int prevTotalBoolCount = 0;
int currentTotalBoolCount = 0;
void Manager() {
    int totalLineCount, i, waitTime, senatorLineNum, clrkLineCnt, clrkBribeLineCnt;
    SetMonitor(allCustomersAreDone, 0, 0);
    do {
        Acquire(outsideLock);
        totalLineCount = 0;
        for(i = 0; i < clerkCount; ++i) {
            clrkLineCnt = GetMonitor(clerkLineCount, i);
            clrkBribeLineCnt = GetMonitor(clerkBribeLineCount, i);
            totalLineCount = totalLineCount + clrkLineCnt + clrkBribeLineCnt;
            senatorLineNum = GetMonitor(senatorLineCount, 0);
            if(totalLineCount > 0 || senatorLineNum > 0 ) {
                wakeUpClerks();
                break;
            }
        }
        printMoney();
        Release(outsideLock);
        waitTime = 100;
        for(i = 0; i < waitTime; ++i) {
            Yield();
        }
    } while(!customersAreAllDone());
    SetMonitor(allCustomersAreDone, 0, 1);

    printMoney();
    wakeUpClerks();

}


/*Manager functions*/
/*This function wakes up ALL clerks, so they check their lines, if noone is in their line, they'll go back on break*/
void wakeUpClerks() {
    int i;
    for(i = 0; i < clerkCount; ++i) {
        if(GetMonitor(clerkStates, i) == ONBREAK) {
            PrintString("Manager has woken up a ", 23); PrintString(clerkTypes[i], clerkTypesLengths[i]); PrintString("_", 1); PrintNum(i); PrintNl();
            Acquire(breakLock[i]);
            Signal(breakLock[i],breakCV[i]);
            Release(breakLock[i]);
            PrintString(clerkTypes[i], clerkTypesLengths[i]); PrintString("_", 1); PrintNum(i); PrintString(" is coming off break\n", 21);
        }
    }
}

/*Function to print money owned by clerks*/
void printMoney() {
    int totalMoney = 0;
    int applicationMoney = 0;
    int pictureMoney = 0;
    int passportMoney = 0;
    int cashierMoney = 0;
    int i, tempMon;
    for(i = 0; i < clerkCount; ++i) {
        tempMon = GetMonitor(clerkMoney, i);
        if (i < clerkArray[0]){ /*ApplicationClerk index*/
            applicationMoney += tempMon;
        } else if (i < clerkArray[0] + clerkArray[1]){ /*PictureClerk index*/
            pictureMoney += tempMon;
        } else if (i < clerkArray[0] + clerkArray[1] + clerkArray[2]){ /*PassportClerk index*/
            passportMoney += tempMon;
        } else if (i < clerkArray[0] + clerkArray[1] + clerkArray[2] + clerkArray[3]){ /*Cashier index*/
            cashierMoney += tempMon;
        }
        totalMoney += tempMon;
    }
    applicationMoney = applicationMoney * 100;
    pictureMoney = pictureMoney * 100;
    passportMoney = passportMoney * 100;
    cashierMoney = cashierMoney * 100;
    totalMoney = totalMoney * 100;

    PrintString("Manager has counted a total of ", 31); PrintNum(applicationMoney); PrintString(" for ApplicationClerks\n", 23);
    PrintString("Manager has counted a total of ", 31); PrintNum(pictureMoney); PrintString(" for PictureClerks\n", 19);
    PrintString("Manager has counted a total of ", 31); PrintNum(passportMoney); PrintString(" for PassportClerks\n", 20);
    PrintString("Manager has counted a total of ", 31); PrintNum(cashierMoney); PrintString(" for Cashiers\n", 14);
    PrintString("Manager has counted a total of ", 31); PrintNum(totalMoney); PrintString(" for passport office\n", 21);
}


int customersAreAllDone() {
    int boolCount = 0, i, temp, appMon, likeMon, certMon, doneMon;
    for(i = 0; i < customerCount + senatorCount; ++i) {
        temp = GetMonitor(isDone, i);
        boolCount += temp;
    }
    prevTotalBoolCount = currentTotalBoolCount;
    currentTotalBoolCount = 0;
    for(i = 0; i < customerCount; ++i) {
        appMon = GetMonitor(applicationIsFiled, i);
        likeMon = GetMonitor(likesPicture, i);
        certMon = GetMonitor(hasCertification, i);
        doneMon = GetMonitor(isDone, i);
      /*  currentTotalBoolCount = currentTotalBoolCount + appMon + likeMon + certMon + doneMon;*/

        currentTotalBoolCount = currentTotalBoolCount + doneMon;
    }
    PrintString("Manager is counting finished customers ", 39); PrintNum(currentTotalBoolCount); PrintNl();

    if(prevTotalBoolCount == currentTotalBoolCount) {
        wakeUpClerks();
    }

    if(boolCount == customerCount + senatorCount) {
        return true;
    }
    return false;
}

int main(){

  setup();
  Manager();
  Exit(0);
}
