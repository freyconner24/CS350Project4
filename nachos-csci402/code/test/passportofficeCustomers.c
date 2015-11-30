/* passportoffice.c
 *      Passport office as a user program
 */

#include "syscall.h"
#include "setup.h"

int main() {
    int i;
    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength;
    PrintString("Setting up locks, conditions and monitor variables: ", 52); PrintNum(i); PrintNl();
    setup();
    PrintString("+++++Customer instantiation", 27); PrintNl();

    for(i = 0; i < customerCount; i++){
      PrintString("+++++Customer created with number: ", 35); PrintNum(i); PrintNl();
        Exec("../test/customer");
    }





    /*Exit(0);*/
}
