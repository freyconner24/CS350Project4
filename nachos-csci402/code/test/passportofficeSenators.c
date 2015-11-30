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
    PrintString("+++++Senator instantiation", 24); PrintNl();


    for(i = 0; i < senatorCount; i++){
      PrintString("+++++Senator Created\n", 21); PrintNum(i + 50); PrintNl();
        Exec("../test/senator");
    }

    /*Exit(0);*/
}
