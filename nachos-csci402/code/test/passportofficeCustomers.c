/* passportoffice.c
 *      Passport office as a user program
 */

#include "syscall.h"
#include "setup.h"

int main() {
    int i;
    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength, custInitCnt;
    PrintString("Setting up locks, conditions and monitor variables...", 53); PrintNum(i); PrintNl();
    setup();
    PrintString("Customer instantiation...", 25); PrintNl();
    /*Function to find out how many customers still need to be instantiated*/
    Acquire(serverCustomInitLock);
    custInitCnt = GetMonitor(customerInitMon, 0);
    if(custInitCnt + 8 < customerCount){
      for(i = 0; i < 8; i++){
        PrintString("Customer created - ", 19); PrintNum(i + custInitCnt); PrintNl();
          Exec("../test/customer");
      }
      custInitCnt = custInitCnt + 8;
      SetMonitor(customerInitMon, 0, custInitCnt);
    }else{
      for(i = 0; i < customerCount - custInitCnt ; i++){
        PrintString("Customer created - ", 19); PrintNum(i + custInitCnt); PrintNl();
          Exec("../test/customer");
      }
      custInitCnt = customerCount;
      SetMonitor(customerInitMon, 0, custInitCnt);
    }
    Release(serverCustomInitLock);




    /*Exit(0);*/
}
