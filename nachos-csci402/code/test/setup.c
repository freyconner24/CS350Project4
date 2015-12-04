#include "syscall.h"
#include "setup.h"

#define CLERK_NUMBER 20
#define CUSTOMER_NUMBER 60
#define CLERK_TYPES 4
#define SENATOR_NUMBER 10
#define false 0
#define true 1

#define CUSTOMER_COUNT 16
#define CLERK_COUNT 8
#define SENATOR_COUNT 0
#define APPLICATIONCLERK_COUNT 2
#define PICTURECLERK_COUNT 2
#define PASSPORTCLERK_COUNT 2
#define CASHIER_COUNT 2

/*Above DEFINEs:
NUMBERS define size of arrays, COUNTS determine the number entities
NOTE: CLERKs have to add up to CLERK_COUNT
*/
/*Setup variables*/
int clerkCount = CLERK_COUNT;  /* CL: number of total clerks of the 4 types that can be modified*/
int customerCount = CUSTOMER_COUNT; /* CL: number of customers that can be modified*/
int senatorCount = SENATOR_COUNT; /* CL: number of senators that can be modified*/


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
/*Variables for server operation*/
int serverClerkLock;
int clerkOwner;
int serverCustomerLock;
int serverCustomInitLock;
int customerInitMon;
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

/*This function initializes all locks, conditions and monitors for all entities*/
void createClerkLocksConditionsMonitors() {
    int i;
    char name[70];
    char intPart[6];

    int clerkType = 0, j = 0;
    int clerkNumber = 0, clerkTypeLength;
    /*Conditions and locks*/
    serverClerkLock = CreateLock("SeClLo", 5, 0);
    serverCustomerLock = CreateLock("SeCuLo", 6, 0);
    serverCustomInitLock = CreateLock("SeCuInLo", 8, 0);
    clerkSenatorLineCV = CreateCondition("ClSeLiCV", 8, 0);
    clerkLineLock = CreateLock("ClLiLo", 6, 0);
    outsideLineCV = CreateCondition("OuLiCV", 6, 0);
    outsideLock = CreateLock("OuLo", 4, 0);
    senatorLock = CreateLock("SeLo", 4, 0);
    senatorLineCV = CreateCondition("SeLiCV", 6, 0);
/*Monitor variables*/
    /*Single monitors*/
    allCustomersAreDone = CreateMonitor("AllCuRDone",10, 1);
    senatorLineCount = CreateMonitor("SeLiCo",6, 1);
    senatorDone = CreateMonitor("SenDone",7, 1);
    outsideLineCount = CreateMonitor("OuLiCnt",7, 1);
    customerInitMon = CreateMonitor("CuIniMon", 8, 1);

    /*Monitors of clerkCount size*/
    clerkLineCount = CreateMonitor("CleLiCnt",8, clerkCount);
    clerkBribeLineCount = CreateMonitor("ClBrLiCnt",9, clerkCount);
    clerkStates = CreateMonitor("clerkState",10, clerkCount);
    clerkMoney = CreateMonitor("clerkMoney",10, clerkCount);
    customerData = CreateMonitor("customData",10, clerkCount);
    clerkOwner = CreateMonitor("ClerkOwner", 10, clerkCount);

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

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "ClerkLock_");
        strcat(name, intPart);
        clerkLock[i] = CreateLock(name, my_strlen(name), i);

        clerArray(name, 70);
        clerArray(intPart, 6);
        itoa(i, intPart);
        my_strcpy2(name, "CleCV_");
        strcat(name, intPart);
        clerkCV[i] = CreateCondition(name, my_strlen(name), i);

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


/*Setting up number of entities*/
void PassportOfficeTests() {

        PrintString("Starting variable setup\n", 23);
        customerCount = CUSTOMER_COUNT;
        clerkCount = CLERK_COUNT;
        senatorCount = SENATOR_COUNT;
        clerkArray[0] = APPLICATIONCLERK_COUNT; clerkArray[1] = PICTURECLERK_COUNT; clerkArray[2] = PASSPORTCLERK_COUNT; clerkArray[3] = CASHIER_COUNT;

        createClerkLocksConditionsMonitors();

}
/*Function to be called once to initialize all monitor variables*/
void initializeEverything(){
  int i;
  int moneyArray[4] = {100, 600, 1100, 1600};
  int randomIndex;
  SetMonitor(allCustomersAreDone, 0, 0);
  SetMonitor(senatorLineCount, 0, 0);
  SetMonitor(senatorDone, 0, 0);
  SetMonitor(outsideLineCount, 0, 0);
  SetMonitor(customerInitMon, 0, 0);
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

/*Online implementation of itoa*/
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

/* reverse used alongside with itoa*/
void reverse(char s[])
{
     int i, j;
     char c;
     for (i = 0, j = my_strlen(s)-1; i < j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }
/*Function to clear out memory for a char array*/
 void clerArray(char string[], int length) {
         int i = 0;
         while (i < length) {
                 string[i++] = '\0';
         }
 }

/*Function called by all entities to initialize locks, conditions and monitors*/
int setup() {
    PassportOfficeTests();
    return 0;
}
