/* condServer_t2.c
 *	Simple program to test the file handling system calls
 */

#include "syscall.h"

int lock1 = 0;
int lock2 = 1;
int theLockThatDoesntExist;
int theCondThatDoesntExist;
int i = 0;
int cond1 = 0;
int cond2 = 1;
int condToBeDestroyed = 2;

int main() {
	lock1 = CreateLock("Lock1", 5, 0);
	cond1 = CreateCondition("Condition1", 10, 0);
  Write("Broadcasting cond1 with lock1, should be successful\n", 52, ConsoleOutput);
  Acquire(lock1);
  Broadcast(lock1,cond1);
  Release(lock1);
  /*Wait(lock1, cond1);*/

  Write("Finshing condServer_t3\n", 23, ConsoleOutput);
  Exit(0);
}
