/* passportoffice.c
 *      Passport office as a user program
 */

#include "syscall.h"
#include "setup.h"

int main() {
    int i;
    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength;
    PrintString("Setting up locks, conditions and monitor variables...", 53); PrintNum(i); PrintNl();
    setup();
    PrintString("Senator instantiation...", 24); PrintNl();
    /*Function to instantiate up to 8 senators*/
    for(i = 0; i < senatorCount; i++){
      PrintString("Senator created - ", 18); PrintNum(i + customerCount); PrintNl();
        Exec("../test/senator");
    }
}
