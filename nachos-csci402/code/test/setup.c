#include "syscall.h"
#include "setup.h"

#define CLERK_NUMBER 20
#define CUSTOMER_NUMBER 60
#define CLERK_TYPES 4
#define SENATOR_NUMBER 10
#define false 0
#define true 1

/*
void ApplicationClerk();
void PictureClerk();
void PassportClerk();
void Cashier();
void Customer();
void Senator();
void Manager();
*/

/*Setup variables*/
int testChosen = 1; /* CL: indicate test number (1-7) or full program (0)*/
int clerkCount = 0;  /* CL: number of total clerks of the 4 types that can be modified*/
int customerCount = 0; /* CL: number of customers that can be modified*/
int senatorCount = 1; /* CL: number of senators that can be modified*/


/*Locks and conditions*/
int clerkSenatorLineCV;
int clerkLineLock;
int outsideLineCV;
int outsideLock;
int senatorLock;
int senatorLineCV;

int clerkLock[CLERK_NUMBER];
int clerkCV[CLERK_NUMBER];
int clerkLineCV[CLERK_NUMBER];
int clerkBribeLineCV[CLERK_NUMBER];
int breakLock[CLERK_NUMBER];
int breakCV[CLERK_NUMBER];
int clerkSenatorCV[CLERK_NUMBER];
int clerkSenatorCVLock[CLERK_NUMBER];

/*Monitors of size 1*/
int allCustomersAreDone;
int senatorLineCount; /* CL: number of senators in a line at any given time*/
int senatorDone;
int outsideLineCount;

/*Monitors of CLERK_NUMBER size*/
int clerkLineCount;
int clerkBribeLineCount;
int clerkStates;
int clerkMoney;
int customerData;
int clerkTypesLengths[CLERK_NUMBER];

/*Monitor for customerAttributes*/
int SSN;
int likesPicture;
int applicationIsFiled;
int hasCertification;
int isDone;
int clerkMessedUp;
int money;

int serverClerkLock;
int clerkOwner;
int serverCustomerLock;

int clerkArray[CLERK_TYPES];

CustomerAttribute customerAttributes[CUSTOMER_NUMBER];  /*CL: customer attributes, accessed by custNumber*/

char clerkTypesStatic[CLERK_TYPES][30] = { "ApplicationClerks : ", "PictureClerks     : ", "PassportClerks    : ", "Cashiers          : " };
char clerkTypes[CLERK_NUMBER][30];
char* my_strcpy(char s1[], const char s2[], int len) {
    int i = 0;
    for(i = 0; i < len - 1; ++i) {
        s1[i] = s2[i];
    }
    s1[len] = '\0';
    return (s1);
}



/*
void clerkFactory(int countOfEachClerkType[]) {
    int tempClerkCount = 0, i = 0;
    for(i = 0; i < CLERK_TYPES; ++i) {
        if(testChosen == 0) {
            do {
                PrintString(clerkTypesStatic[i], 20);
                tempClerkCount = 4;
                if(tempClerkCount <= 0 || tempClerkCount > 5) {
                    PrintString("    The number of clerks must be between 1 and 5 inclusive!\n", 60);
                }
            } while(tempClerkCount <= 0 || tempClerkCount > 5);
            clerkCount += tempClerkCount;
            clerkArray[i] = tempClerkCount;
        } else {
            clerkArray[i] = countOfEachClerkType[i];
        }
    }

    PrintString("Number of ApplicationClerks = ", 30); PrintNum(clerkArray[0]); PrintNl();
    PrintString("Number of PictureClerks = ", 26); PrintNum(clerkArray[1]); PrintNl();
    PrintString("Number of PassportClerks = ", 27); PrintNum(clerkArray[2]); PrintNl();
    PrintString("Number of CashiersClerks = ", 27); PrintNum(clerkArray[3]); PrintNl();
    PrintString("Number of Senators = ", 21); PrintNum(senatorCount); PrintNl();
}
*/

void createClerkLocksConditionsMonitors() {
    int i;
    char name[70];
    char intPart[6];
    int cpyLen;

    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength;
    /*Conditions and locks*/
    serverClerkLock = CreateLock("SeClLo", 5, 0);
    serverCustomerLock = CreateLock("SeCuLo", 6, 0);
    clerkSenatorLineCV = CreateCondition("ClSeLiCV", 8, 0);
    clerkLineLock = CreateLock("ClLiLo", 6, 0);
    outsideLineCV = CreateCondition("OuLiCV", 6, 0);
    outsideLock = CreateLock("OuLo", 4, 0);
    senatorLock = CreateLock("SeLo", 4, 0);
    senatorLineCV = CreateCondition("SeLiCV", 6, 0);

  /*  PrintString("Setup clerkLineLock: ", 21); PrintNum(clerkLineLock); PrintNl();
    PrintString("Setup ClerkSenatorLineCV: ", 26); PrintNum(clerkSenatorLineCV); PrintNl();
    PrintString("Setup outsideLineCV: ", 21); PrintNum(outsideLineCV); PrintNl();
    PrintString("Setup outsideLock: ", 19); PrintNum(outsideLock); PrintNl();
    PrintString("Setup senatorLock: ", 19); PrintNum(senatorLock); PrintNl();
    PrintString("Setup senatorLineCV: ", 21); PrintNum(senatorLineCV); PrintNl();
*/
/*Monitor variables*/
    /*Single monitors*/
    allCustomersAreDone = CreateMonitor("AllCuRDone",10, 1);
    senatorLineCount = CreateMonitor("SeLiCo",6, 1);
    senatorDone = CreateMonitor("SenDone",7, 1);
    outsideLineCount = CreateMonitor("OuLiCnt",7, 1);


    /*Monitors of clerkCount size*/
    clerkLineCount = CreateMonitor("CleLiCnt",8, clerkCount);
    clerkBribeLineCount = CreateMonitor("ClBrLiCnt",9, clerkCount);
    clerkStates = CreateMonitor("clerkState",10, clerkCount);
    clerkMoney = CreateMonitor("clerkMoney",10, clerkCount);
    customerData = CreateMonitor("customData",10, clerkCount);
    /*clerkTypesLengths = CreateMonitor("clerkTypesLengths",17, clerkCount);*/

    clerkOwner = CreateMonitor("ClerkOwner", 10, clerkCount);
    /*
    CreateMonitor("clerkTypes",10, clerkCount);
    extern struct CustomerAttribute customerAttributes[CUSTOMER_NUMBER];
    */
/*Monitor for customerAttributes*/
    PrintString("Before custAttr: ", 17); PrintNum(customerCount + senatorCount);
    SSN = CreateMonitor("SSNMon", 6, customerCount + senatorCount);
    likesPicture = CreateMonitor("likePicMon", 10, customerCount + senatorCount);
    applicationIsFiled = CreateMonitor("appFileMon", 10, customerCount + senatorCount);
    hasCertification = CreateMonitor("hasCertMon", 10, customerCount + senatorCount);
    isDone = CreateMonitor("isDoneMon", 9, customerCount + senatorCount);
    clerkMessedUp = CreateMonitor("MessUpMon", 9, customerCount + senatorCount);
    money = CreateMonitor("moneyMon", 8, customerCount + senatorCount);
    for(i = 0; i < clerkCount; ++i) {
        if(i / 10 == 0) {
            cpyLen = 1;
        } else {
            cpyLen = 2;
        }
        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "ClerkLock_");
        strcat(name, intPart);

        clerkLock[i] = CreateLock(name, my_strlen(name), i);
        /*PrintString("Setup clerkLock[i]: ", 20); PrintNum(clerkLock[i]); PrintNl();*/

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "CleCV_");
        strcat(name, intPart);

        clerkCV[i] = CreateCondition(name, my_strlen(name), i);
        /*PrintString("Setup clerkCV[i]: ", 18); PrintNum(clerkCV[i]); PrintNl();*/

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "ClLiCV_");
        strcat(name, intPart);

        clerkLineCV[i] = CreateCondition(name, my_strlen(name), i);

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "CBrLCV_");
        strcat(name, intPart);

        clerkBribeLineCV[i] = CreateCondition(name, my_strlen(name), i);

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "BrkLo_");
        strcat(name, intPart);

        breakLock[i] = CreateLock(name, my_strlen(name), i);

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "BrkCV_");
        strcat(name, intPart);

        breakCV[i] = CreateCondition(name, my_strlen(name), i);

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "ClSeCV_");
        strcat(name, intPart);

        clerkSenatorCV[i] = CreateCondition(name, my_strlen(name), i);

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "ClSeCVLo");
        strcat(name, intPart);

        clerkSenatorCVLock[i] = CreateLock(name, my_strlen(name), i);



    }

    for(clerkType = 0; clerkType < 4; ++clerkType) {
        if(clerkType == 0) {
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                clerkTypeLength = 16;
                my_strcpy(clerkTypes[clerkNumber], "ApplicationClerk", clerkTypeLength + 1); /* plus one for null character */
                clerkTypesLengths[clerkNumber] = clerkTypeLength;
                PrintString("ApplicationClerk setting clerktype: ", 36 ); PrintNum(clerkNumber); PrintNl();
                ++clerkNumber;
            }
        } else if(clerkType == 1) {
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                clerkTypeLength = 12;
                my_strcpy(clerkTypes[clerkNumber], "PictureClerk", clerkTypeLength + 1);
                clerkTypesLengths[clerkNumber] = clerkTypeLength;

                PrintString("PictureClerk setting clerktype: ", 32 ); PrintNum(clerkNumber); PrintNl();
                ++clerkNumber;
            }
        } else if(clerkType == 2) {
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                clerkTypeLength = 13;
                my_strcpy(clerkTypes[clerkNumber], "PassportClerk", clerkTypeLength + 1);
                clerkTypesLengths[clerkNumber] = clerkTypeLength;

                PrintString("PassportClerk setting clerktype: ", 33 ); PrintNum(clerkNumber); PrintNl();
                ++clerkNumber;
            }
        } else { /* i == 3 */
            for(j = 0; j < clerkArray[clerkType]; ++j) {
                clerkTypeLength = 7;
                my_strcpy(clerkTypes[clerkNumber], "Cashier", clerkTypeLength + 1);
                clerkTypesLengths[clerkNumber] = clerkTypeLength;
                PrintString("Cashier setting clerktype: ", 27 ); PrintNum(clerkNumber); PrintNl();
                ++clerkNumber;
            }
        }
    }
}

void createTestVariables() {
    createClerkLocksConditionsMonitors();
}

void PassportOfficeTests() {
    int countOfEachClerkType[CLERK_TYPES] = {0,0,0,0};

    /*PrintString("Starting Part 2\n", 16);*/
    testChosen = 1; /* technically cin >> */
    PrintNum(testChosen); PrintNl();

    if(testChosen == 1) {
        PrintString("Starting Test 1\n", 16); /*Customers always take the shortest line, but no 2 customers ever choose the same shortest line at the same time*/
        customerCount = 3;
        clerkCount = 4;
        senatorCount = 3;
        clerkArray[0] = 1; clerkArray[1] = 1; clerkArray[2] = 1; clerkArray[3] = 1; /*TODO: replace countofeachclerktype with clerkarray with other tests*/

        createTestVariables();
    } else if(testChosen == 2) {
        PrintString("Starting Test 2\n", 16); /*Managers only read one from one Clerk's total money received, at a time*/
        customerCount = 5;
        clerkCount = 4;
        senatorCount = 0;
        countOfEachClerkType[0] = 1; countOfEachClerkType[1] = 1; countOfEachClerkType[2] = 1; countOfEachClerkType[3] = 1;

        createTestVariables();
    } else if(testChosen == 3) {
        PrintString("Starting Test 3\n", 16); /*Customers do not leave until they are given their passport by the Cashier.
                                     The Cashier does not start on another customer until they know that the last Customer has left their area*/
        customerCount = 5;
        clerkCount = 4;
        senatorCount = 3;
        countOfEachClerkType[0] = 1; countOfEachClerkType[1] = 1; countOfEachClerkType[2] = 1; countOfEachClerkType[3] = 1;

        createTestVariables();
    } else if(testChosen == 4) {
        PrintString("Starting Test 4\n", 16); /*Clerks go on break when they have no one waiting in their line*/
        customerCount = 5;
        clerkCount = 4;
        senatorCount = 0;
        countOfEachClerkType[0] = 1; countOfEachClerkType[1] = 1; countOfEachClerkType[2] = 1; countOfEachClerkType[3] = 1;

        createTestVariables();
    } else if(testChosen == 5) {
        PrintString("Starting Test 5\n", 16); /*Managers get Clerks off their break when lines get too long*/
        customerCount = 7;
        clerkCount = 4;
        senatorCount = 0;
        countOfEachClerkType[0] = 1; countOfEachClerkType[1] = 1; countOfEachClerkType[2] = 1; countOfEachClerkType[3] = 1;

        createTestVariables(countOfEachClerkType);
    } else if(testChosen == 6) {
        PrintString("Starting Test 6\n", 16); /*Total sales never suffers from a race condition*/
        customerCount = 25;
        clerkCount = 4;
        senatorCount = 0;
        countOfEachClerkType[0] = 1; countOfEachClerkType[1] = 1; countOfEachClerkType[2] = 1; countOfEachClerkType[3] = 1;

        createTestVariables(countOfEachClerkType);
    } else if(testChosen == 7) {
        PrintString("Starting Test 7\n", 16); /*Total sales never suffers from a race condition*/
        customerCount = 7;
        clerkCount = 4;
        senatorCount = 1;
        countOfEachClerkType[0] = 1; countOfEachClerkType[1] = 1; countOfEachClerkType[2] = 1; countOfEachClerkType[3] = 1;

        createTestVariables(countOfEachClerkType);
    } else if(testChosen == 0) {
        do {
            PrintString("Number of customers: ", 21);
            customerCount = 10; /* technically cin >> */
            if(customerCount <= 0 || customerCount > 50) {
                PrintString("    The number of customers must be between 1 and 50 inclusive!\n", 64);
            }
        } while(customerCount <= 0 || customerCount > 50);

        do {
            PrintString("Number of Senators: ", 20);
            senatorCount = 1; /* technically cin >> */
            if(senatorCount < 0 || senatorCount > 10) {
                PrintString("    The number of senators must be between 1 and 10 inclusive!\n", 63);
            }
        } while(senatorCount < 0 || senatorCount > 10);

        createTestVariables(countOfEachClerkType);
    }
}

void initializeEverything(){
  int i;
  int moneyArray[4] = {100, 600, 1100, 1600};
  int randomIndex;
  SetMonitor(allCustomersAreDone, 0, 0);
  SetMonitor(senatorLineCount, 0, 0);
  SetMonitor(senatorDone, 0, 0);
  SetMonitor(outsideLineCount, 0, 0);

  for(i = 0; i < clerkCount ; i++){
    /*Initialize clerckcount size monitors*/
    SetMonitor(clerkLineCount, i, 0);
    SetMonitor(clerkBribeLineCount, i, 0);
    SetMonitor(clerkStates, i, 0);
    SetMonitor(clerkMoney, i, 0);
    SetMonitor(customerData, i, 0);
    /*SetMonitor(clerkTypesLengths, i, 0);*/
    SetMonitor(clerkOwner, i, 1);
  }
  for(i = 0; i < customerCount + senatorCount; i++){

            SetMonitor(SSN, i, 0);
            SetMonitor(likesPicture, i, 0);
            SetMonitor(applicationIsFiled, i, 0);
            SetMonitor(hasCertification, i, 0);
            SetMonitor(isDone, i , 0);
            SetMonitor(clerkMessedUp, i, 0);
            randomIndex = Rand(4, 0); /*0 to 3*/
            SetMonitor(money, i, moneyArray[randomIndex]);
  }
}

int my_strlen(char* str){
        int i = 0;
        while(str[i] != '\0'){
          i++;
        }
        return i;
}

void my_strcpy2(char dest[], char source[]) {
        int i = 0;
        while (source[i] != '\0') {
                dest[i] = source[i];
                i++;
        }
        dest[i] = '\0';
}

void strcat(char dest[], char source[]) {
        int i = 0;
        int len = my_strlen(dest);
        while (source[i] != '\0') {
                dest[len + i] = source[i];
                i++;
        }
        dest[len + i] = '\0';
}

//Online implementation
void itoa(int n, char s[])
{
     int i, sign;

     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
}

/* reverse:  reverse string s in place */
void reverse(char s[])
{
     int i, j;
     char c;

     for (i = 0, j = my_strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }

 void clerArray(char string[], int length) {
         int i = 0;
         while (i < length) {
                 string[i++] = '\0';
         }
 }

int setup() {
    PassportOfficeTests();
    return 0;
}
