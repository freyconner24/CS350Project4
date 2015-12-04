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
    PrintString("Initializing all monitors...", 28); PrintNum(i); PrintNl();

    initializeEverything();
    PrintString("Clerk instantiation....", 19); PrintNl();
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
    for(i = 0; i < customerCount; i++){
      PrintString("+++++Customer created with number: ", 35); PrintNum(i); PrintNl();
        Exec("../test/customer");
    }

    for(i = 0; i < senatorCount; i++){
      PrintString("+++++Senator Created\n", 21); PrintNum(i + 50); PrintNl();
        Exec("../test/senator");
    }
    Exec("../test/manager");



    /*Exit(0);*/
}
