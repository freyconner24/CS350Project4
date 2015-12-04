/* passportofficeClerks.c
 *      Passport office as a user program to run all the clerks
 */

#include "syscall.h"
#include "setup.h"

int main() {
    int i;
    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength;

    PrintString("Setting up locks, conditions and monitor variables...", 53); PrintNum(i); PrintNl();
    setup();
    PrintString("Initializing all monitors...", 28); PrintNum(i); PrintNl();
    initializeEverything();
    PrintString("Clerk instantiation...", 22); PrintNl();
    for(clerkType = 0; clerkType < 4; ++clerkType) {
        if(clerkType == 0) {
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                Exec("../test/applicationClerk");
            }
        } else if(clerkType == 1) {
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                Exec("../test/pictureClerk");
            }
        } else if(clerkType == 2) {
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                Exec("../test/passportClerk");
            }
        } else { /* i == 3 */
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                Exec("../test/cashier");
            }
        }
    }
}
