#include "syscall.h"

int mon1 = 0;
int mon2 = 1;
int mon3 = 1;
int value1 = 0;
int value2 = 1;
int lock1 = 0;
int lock2 = 1;
int cond1 = 0;
int cond2 = 1;
int i = 0;

int main(){
	mon1 = CreateMonitor("Monitor1", 8, 5);
	mon2 = CreateMonitor("Monitor2", 8, 10);
	mon3 = CreateMonitor("Monitor3", 8, 10);
	lock1 = CreateLock("Lock1", 5, 0);
	lock2 = CreateLock("Lock2", 5, 0);
	cond1 = CreateCondition("Cond1", 5, 0);
	cond2 = CreateCondition("Cond2", 5, 0);
	PrintString("Acquiring and Signaling.\n", 25);
	Acquire(lock1);
  Signal(lock1, cond1);
	Release(lock1);

	Exit(1);
}
