/* passportofficeManager.c
 *      Passport office as a user program that starts the manager
 */

#include "syscall.h"
#include "setup.h"

int main() {
    int i;
    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength;
    PrintString("Setting up locks, conditions and monitor variables...", 53); PrintNum(i); PrintNl();
    setup();
    Exec("../test/manager");
}
