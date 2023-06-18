/*
* Developer: Purnima Naik
* Summary: Program to implement a custom shell and scheduler.
* The shell will be supporting following commands:
* 1. c #: Create # child processes
* 2. l: Display the child process information
* 3. q #: Set the quantum time to be # secs
* 4. b #: Set the burst time to be # secs for FCFS
* 5. t rr: Set the scheduling algorithm to be round robin
* 6. t fcfs: Set the scheduling algorithm to be first come first serve
* 7. k #: Terminate the process, having the process number #
* 8. r #: Resume the process, having the process number #
* 9. r all: Resume to run all the processes in ready state
* 10. s all: Suspend all the processes
* 11. x or X: Exit the shell & all child processes
* 12. clear: Clear the console
* 13. help: Displays list of commands supported by the shell
* The scheduler will be running processes using the following algorithms:
* 1) FCFS - In this algorithm, the process that got created first will complete its execution and then only it will move to the next process.
* For simulation purpose, have also given the provision to enter the burst time. So that every process, will run for the given burst time and will then get terminated.
* If the user does not provide a burst time, only the first non-terminated process will run; to run the subsequent process, the 'k #' command must be used to terminate the current one,
* and the control c command must be used to pause its execution.
* 2) Round Robin - Each process in this algorithm will execute one at a time for the specified quantum time.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#define DELIMITERS " \t\r\n\a"
#define BUFFER_SIZE 200
#define MAXIMUM_NUMBER_OF_CHILD_PROCESSES 25 /* Only upto 25 child processes can be created in this shell.*/
#define MAXIMUM_ROUND_ROBIN_QUANTUM_TIME_IN_SECS 180 /* Maximum round robin quantum time in secs */
#define MINIMUM_FCFS_BURST_TIME_IN_SECS 10 /* Minumum fcfs burst time in secs */

/*
* Enum Process States
* Ready - Process is scheduled for execution
* Running - Process is running
* Suspended - Control-c, 's-all', and SIGSTOP suspends a process
* Terminated - 'k #' terminates a process
*/
enum processStates
{
    Ready = 1,
    Running = 2,
    Suspended = 3,
    Terminated = 4
};

/*
* Struct processTable
* processNumber - Unique Identifier
* processId - Child process id
* state - Process state
* processAdded - Tracker used to fill this struct with data
*/
struct processTable
{
    int processNumber;
	int processId;
	enum processStates state;
	int processAdded;
};

/* Declarations - Variables, Methods */
static struct processTable childProcesses[MAXIMUM_NUMBER_OF_CHILD_PROCESSES];
char schedulingAlgo[10];
int roundRobinQuantumTimeInSecs;
int createChildProcesses(char **cmdArguments);
int setQuantumTime(char **cmdArguments);
int setSchedulingAlgorithm(char **cmdArguments);
int clearConsole(char **cmdArguments);
int terminateChildProcess(char **cmdArguments);
int displayChildProcessInformation(char **cmdArguments);
int resumeChildProcesses(char **cmdArguments);
int suspendAll(char **cmdArguments);
int supportedCommandsMessage();
int forkParentProcess(int processCount);
char* getProcessState(int processStateId);
int exitApplication(char **cmdArguments);
int creationOfChildProcessAllowed();
int setBurstTimeForFCFS(char **cmdArguments);
int isProcessRunning();
void runChildProcessUsingSchedulingAlgo(int processNumber);
int runFirstProcessUsingSchedulingAlgorithm();
int runChildProcess(int processNumber);
void killProcess(int procNo);
int childProcessCountTracker = 0;
int fcfsBurstTimeInSecs = 0;
int terminatedChildProcess = 0;
int runningOneProcess = 0;

char *shellCommands[] = {"c", "l", "q", "b", "t", "k", "r", "s", "x", "X", "clear", "help"};
int (*supportedShellCommands[]) (char **) = { &createChildProcesses, &displayChildProcessInformation,
&setQuantumTime, &setBurstTimeForFCFS, &setSchedulingAlgorithm, &terminateChildProcess, &resumeChildProcesses, &suspendAll,
&exitApplication, &exitApplication, &clearConsole, &supportedCommandsMessage};

/*
* Summary - This method will exit the shell & all child processes, when the user
* will enter 'x' or 'X' command
* Param 1 - cmdArguments - command line arguments
* Answer to Question 1 (x or X)
*/
int exitApplication(char **cmdArguments)
{
    // Declarations
    int processInfoCounter, childStatus;

    if(cmdArguments[1] == NULL)
    {
        for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
        {
            // A process will be killed, if its status is not terminated.
            if(childProcesses[processInfoCounter].state != Terminated)
            {
                kill(childProcesses[processInfoCounter].processId, SIGKILL);
                // Make the parent wait for a successful child termination.
                while(wait(&childStatus) > 0)
                {
                    // 9 is 'signal - kill'
                    if(childStatus == 9)
                    {
                        break;
                    }
                }
            }
        }
        exit(0);
    }
    puts("The command you entered to exit this shell appears to be incorrect.\n"
    "Please enter x or X.\n");
}

/*
* Summary - This method will populate the 'childProcesses' struct
* Param 1 - processId - the child process id
* Param 2 - state - the process state
*/
void addChildProcess(int processId, enum processStates state)
{
    // Declarations
    int counter = 0;

    while(counter < MAXIMUM_NUMBER_OF_CHILD_PROCESSES)
    {
        if(childProcesses[counter].processAdded == 0)
        {
            childProcesses[counter].processNumber = counter;
            childProcesses[counter].processId = processId;
            childProcesses[counter].state = state;
            childProcesses[counter].processAdded = 1;
            break;
        }
        counter++;
    }
}

/*
* Summary - To respond to a control-c command, this method will be used.
* Steps:
* 1. Check if any child process is running.
* 2. Suspend the executing child process using SIGSTOP signal.
* 3. Update the status of child as 'Suspended' in the childProcesses struct.
* 4. Control-c should suspend everything, so cancel the future alarm
* 5. Return control to the parent.
* Answer to Question 3 (Control-C Handler)
*/
void controlCHandler()
{
    // Declarations
    int counter = 0, loopCounter = 0;

    for(counter = 0; counter < childProcessCountTracker; counter++)
    {
        if(childProcesses[counter].state == Running)
        {
            kill(childProcesses[counter].processId, SIGSTOP);

            childProcesses[counter].state = Suspended;

            printf("\033[0;35m");
            printf("\n\nChild Process %d -> Running -> Suspended\n", childProcesses[counter].processId);
            printf("\033[0m");

            alarm(0);

            break;
        }
    }

    fflush(stdout);
}

/*
* Summary - This method is invoked when a process's quantum time or burst time has elapsed.
* It will first find the cuurent running process and will stop it in case of RR and will terminate it in case of FCFS.
* Then it will start the next process using the configured scheduling algorithm.
*/
void alarmHandler()
{
    // Declarations
    int counter = 0, innerLoopCounter = 0, executingProcessNo = -1, nextProcessNo = -1;
    bool foundExecutingProcess = false, foundNextProcess = false;

    for(counter = 0; counter < childProcessCountTracker; counter++)
    {
        // Step 1 - Find the running process.
        // The running process might also have been killed by the 'k #' command.
        // If killed, the status of the process would be terminated,
        // and the 'terminatedChildProcess' variable will have the process id of the running process that was terminated
        if(childProcesses[counter].state == Running || terminatedChildProcess == childProcesses[counter].processId)
        {
            executingProcessNo = counter;
            foundExecutingProcess = true;
            continue;
        }

        // Step 2 - Once executing process is found, we need to find the next process to be executed
        if(foundExecutingProcess)
        {
            if(childProcesses[counter].state == Suspended)
            {
                nextProcessNo = counter;
                foundNextProcess = true;
                break;
            }
        }
    }

    // Step 3 - If next process is not found, then scan the childProcesses struct from 0 to executingProcessNo to find the next process.
    if(!foundNextProcess)
    {
        int loopCounter;
        for(loopCounter = 0; loopCounter < executingProcessNo; loopCounter++)
        {
            if(childProcesses[loopCounter].state == Suspended)
            {
                nextProcessNo = loopCounter;
                foundNextProcess = true;
                break;
            }
        }
    }

    // Step 4 - Even after scanning the entire childProcesses struct, if the next child process to execute is not found,
    // it means there is only 1 process for execution (rest might be terminated), and that process is already running.
    // So, nextProNo = executingProNo
    if(!foundNextProcess)
    {
        // Step 5 - There is only process that is running.
        // The following steps will be executed, only if the executing process was not terminated by the k # command.
        // If RR, then continue running the process, dont stop it.
        // If FCFS, the burst time of this process has elapsed, so kill the process.
        if(childProcesses[executingProcessNo].state != Terminated)
        {
            if(strcmp(schedulingAlgo, "rr") == 0)
            {
                nextProcessNo = executingProcessNo;
                alarm(roundRobinQuantumTimeInSecs);
            }
            if(strcmp(schedulingAlgo, "fcfs") == 0)
            {
                killProcess(executingProcessNo);
            }
        }
    }
    else
    {
        // Step 6 - We have found the executing process and the next process.
        // If the executing process is not terminated, then stop in RR or kill it in FCFS.
        // Run the next process.
        printf("\033[0;31m");
        printf("\nAn alarm has been triggered, proceed with the next process.\n");
        printf("\033[0m");

        if(childProcesses[executingProcessNo].state != Terminated)
        {
            if(strcmp(schedulingAlgo, "rr") == 0)
            {
                kill(childProcesses[executingProcessNo].processId, SIGSTOP);
                childProcesses[executingProcessNo].state = Suspended;
                printf("\033[0;32m");
                printf("Child %d -> Running -> Suspended\n\n", childProcesses[executingProcessNo].processId);
                printf("\033[0m");
            }

            if(strcmp(schedulingAlgo, "fcfs") == 0)
            {
                killProcess(executingProcessNo);
            }
        }

        // Run the next process
        runChildProcessUsingSchedulingAlgo(nextProcessNo);
    }

    // Reset data, and do a return
    terminatedChildProcess = 0;
    return;
}

/*
* Summary - If burst time is set in FCFS, this method will be called to kill a process.
* Param 1 - procNo - the process identifier
*/
void killProcess(int procNo)
{
    // Declarations
    int childStatus = 0;

    kill(childProcesses[procNo].processId, SIGKILL);
    while(wait(&childStatus) > 0)
    {
        // SIGKILL is 9
        if(childStatus == 9)
        {
            break;
        }
    }

    childProcesses[procNo].state = Terminated;
    printf("\033[0;32m");
    printf("Child %d -> Running -> Terminated\n\n", childProcesses[procNo].processId);
    printf("\033[0m");
}

/*
* Summary - This method will suspend non-terminated child processes using SIGSTOP signal.
* The childProcesses struct will also get updated with SUSPENDED process state.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to Question 11 (s all)
*/
int suspendAll(char **cmdArguments)
{
    // Declarations
    int processInfoCounter;
    bool isAnyProcessSuspended = false;

    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        if (strcmp(cmdArguments[1], "all") == 0)
        {
            // Cancel the future alarm
            alarm(0);

            for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
            {
                if(childProcesses[processInfoCounter].state != Terminated)
                {
                    isAnyProcessSuspended = true;
                    childProcesses[processInfoCounter].state = Suspended;
                    kill(childProcesses[processInfoCounter].processId, SIGSTOP);
                }
            }

            if(!isAnyProcessSuspended)
            {
                puts("There is nothing to suspend because all of the processes have either been terminated or not created at all.\n");
            }
            else
            {
                puts("The child processes are moved to a suspended state.\n");
            }
            return 1;
        }
    }
    puts("The command you entered to suspend all child processes appears to be incorrect.\n"
    "If needed, use the 'help' command to see the command list.\n");
    return 1;
}

/*
* Summary - This method will be used to terminate a child process by using SIGKILL command.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to Question 8 (k #)
*/
int terminateChildProcess(char **cmdArguments)
{
    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        // Declarations
        int argDataLength = strlen(cmdArguments[1]);
        int argCount, processInfoCounter, childStatus;
        bool processFound = false, isProcessRunning = false;

        for(argCount = 0; argCount < argDataLength; argCount ++)
        {
            if(isdigit(cmdArguments[1][argCount]) == 0)
            {
                puts("The entered process number appears to be incorrect.\n"
                "Please use the 'l' command to display child processes information.\n");
                return 1;
            }
        }

        int processNumber = atoi(cmdArguments[1]);

        for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
        {
            if(childProcesses[processInfoCounter].processNumber == processNumber)
            {
                processFound = true;
                // If the process is not terminated, then terminate it.
                if(childProcesses[processInfoCounter].state != Terminated)
                {
                    if(childProcesses[processInfoCounter].state == Running)
                    {
                        isProcessRunning = true;
                    }

                    childProcesses[processInfoCounter].state = Terminated;
                    kill(childProcesses[processInfoCounter].processId, SIGKILL);

                    while(wait(&childStatus) > 0)
                    {
                        if(childStatus == 9)
                        {
                            printf("The process with number %d has been terminated.\n", processNumber);
                            break;
                        }
                    }

                    // If the terminated process was a running process,
                    // then for rr or fcfs, the alarm should be called immediately to run the next process.
                    // runningOneProcess will indicate if all processes are running (r all) or just 1 process (r #)
                    // runningOneProcess is 1, then it means the process is running because of r # command, and it has nothing
                    // to do with FCFS or RR
                    if(runningOneProcess == 0 && isProcessRunning)
                    {
                        terminatedChildProcess = childProcesses[processInfoCounter].processId;
                        // The need for immediate calling arises from the fact that, for instance if the quantum time is
                        // set to 50 secs, the next process will start after that amount of time.
                        ualarm(1, 0);
                    }
                }
                else
                {
                    printf("Failed to terminate the process.\nThe process is already terminated.\n");
                }

                return 1;
            }
        }

        if(!processFound)
        {
            puts("The entered process number appears to be incorrect.\n"
            "Please use the 'l' command to display child processes information.\n");

            return 1;
        }
    }

    puts("The command you entered appears to be incorrect.\n"
    "If needed, use the 'help' command to see the command list.\n");

    return 1;
}

/*
* Summary - This method will be used to run all child processes by using the configured scheduling algorithm.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to Question 9 (r #) & 10 (r all)
*/
int resumeChildProcesses(char **cmdArguments)
{
    int childProcessNumber;
    bool resumeAllChildProcesses = false, isInputValueADigit = true, isProcessRunning = false;

    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        // Declarations
        int argDataLength = strlen(cmdArguments[1]);
        int argCount;

        // Step 1 - Validations
        for(argCount = 0; argCount < argDataLength; argCount ++)
        {
            if(isdigit(cmdArguments[1][argCount]) == 0)
            {
                isInputValueADigit = false;
                break;
            }
        }

        if(strcmp(cmdArguments[1], "all") == 0)
        {
            resumeAllChildProcesses = true;
        }

        if(isInputValueADigit == false && resumeAllChildProcesses == false)
        {
            goto PrintError;
        }

        if(childProcessCountTracker == 0)
        {
            printf("There aren't any child processes that have been created to resume their execution.\n");
            return 1;
        }

        int loopCounter;
        bool isProcessAlive = false;

        for(loopCounter = 0; loopCounter < childProcessCountTracker; loopCounter++)
        {
            if(childProcesses[loopCounter].state != Terminated)
            {
                isProcessAlive = true;

                if(childProcesses[loopCounter].state == Running)
                {
                    isProcessRunning =  true;
                    break;
                }
            }
        }

        if(!isProcessAlive)
        {
            printf("There is no process to run because all processes have been terminated.\n");
            return 1;
        }

        if(isProcessRunning)
        {
            printf("There is an ongoing process execution. Please stop that first.\n");
            return 1;
        }

        // Reset data
        terminatedChildProcess = 0;

        // Step 2 - Run a single process, as the user has entered r # command.
        // This has nothing to do with the scheduling algorithm.
        if(!resumeAllChildProcesses)
        {
            childProcessNumber = atoi(cmdArguments[1]);
            int runChildProcessStatus = runChildProcess(childProcessNumber);
            return runChildProcessStatus;
        }
        // Step 3 - Run all processes by using the scheduling algorithm
        else
        {
            if(strcmp(schedulingAlgo, "") == 0)
            {
                printf("Please set the scheduling algorithm first in order to resume the process execution.\n");
                return 1;
            }

            if(strcmp(schedulingAlgo, "rr") == 0)
            {
                if(roundRobinQuantumTimeInSecs <= 0)
                {
                    printf("Please set the quantum time for executing processes utilizing round robin scheduling.\n");
                    return 1;
                }
            }

            int resumeStatus = runFirstProcessUsingSchedulingAlgorithm();
            return resumeStatus;
        }
    }

    PrintError:puts("The entered value appears to be incorrect.\n"
    "Please enter the process number or 'all' to resume the child process execution.\n"
    "If needed, use the 'help' command to see the command list.\n");

    return 1;
}

/*
* Summary - This method will be called for running the first non-terminated process using the configured scheduling algorithm.
* It will run the first process, and the rest will be executed by the alarm handler.
* Returns 1, after a flawless execution
*/
int runFirstProcessUsingSchedulingAlgorithm()
{
    // Declarations
    int processInfoCounter;

    for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
    {
        if(childProcesses[processInfoCounter].state != Terminated)
        {
            // runningOneProcess is 0, as 'r all' command is executing
            runningOneProcess = 0;

            if(strcmp(schedulingAlgo, "rr") == 0)
            {
                printf("\033[0;34m");
                printf("***Running all processes using Round Robin***\n\n");
                printf("\033[0m");

            }

            if(strcmp(schedulingAlgo, "fcfs") == 0)
            {
                printf("\033[0;34m");
                printf("***Running all processes using FCFS***\n\n");
                printf("\033[0m");

            }

            runChildProcessUsingSchedulingAlgo(processInfoCounter);
            return 1;
        }
    }

    return 1;
}

/*
* Summary - This method will run the process for the entered process number (r # command).
* It will just run the process by using SIGCONT. It will not use any scheduling algorithm.
* Param 1 - childProcessNumber - Unique identifier for the process
* Returns 1, after a flawless execution
*/
int runChildProcess(int processNumber)
{
    // Declarations
    int processInfoCounter;
    bool processFound = false;

    // Validations
    for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
    {
        if(childProcesses[processInfoCounter].processNumber == processNumber)
        {
            if(childProcesses[processInfoCounter].state == Terminated)
            {
                printf("The child process %d having process number %d is already terminated.\n",
                childProcesses[processInfoCounter].processId, processNumber);

                return 1;
            }

            processFound = true;
            break;
        }
    }

    if(!processFound)
    {
        puts("The entered process number appears to be incorrect.\n"
        "Please use the 'l' command to display child processes information.\n");

        return 1;
    }

    // runningOneProcess is 1, as 'r #' command is entered by the user
    runningOneProcess = 1;

    printf("\033[0;35m");

    printf("Child %d -> Suspended -> Ready\n", childProcesses[processNumber].processId);
    childProcesses[processNumber].state = Ready;
    printf("Child %d -> Ready -> Running\n", childProcesses[processNumber].processId);
    childProcesses[processNumber].state = Running;

    kill(childProcesses[processNumber].processId, SIGCONT);

    printf("\033[0m");

    return 1;
}

/*
* Summary - This method will run child process using the scheduling algorithm.
* The execution of child process will be started using SIGCONT.
* Param 1 - processNumber - The unique identifier of the child process.
*/
void runChildProcessUsingSchedulingAlgo(int processNumber)
{
    printf("\033[0;35m");

    printf("Child %d -> Suspended -> Ready\n", childProcesses[processNumber].processId);
    childProcesses[processNumber].state = Ready;
    printf("Child %d -> Ready -> Running\n", childProcesses[processNumber].processId);
    childProcesses[processNumber].state = Running;

    // If RR, then apply the quantum time using alarm
    if(strcmp(schedulingAlgo, "rr") == 0)
    {
        alarm(roundRobinQuantumTimeInSecs);
    }
    // If FCFS, if burst time is provided, then apply the burst time using alarm
    if(strcmp(schedulingAlgo, "fcfs") == 0)
    {
        if(fcfsBurstTimeInSecs > 0)
        {
            alarm(fcfsBurstTimeInSecs);
        }
    }

    // Start the process
    kill(childProcesses[processNumber].processId, SIGCONT);
    printf("\033[0m");
}

/*
* Summary - This method will get process state name by state id.
* Param 1 - processStateId - process state id
* Returns - Process state name
*/
char* getProcessState(int processStateId)
{
    switch(processStateId)
    {
        case 1: return "Ready";
        case 2: return "Running";
        case 3: return "Suspended";
        case 4: return "Terminated";
    }
}

/*
* Summary - This method will display the child processes information.
* Process Number, Child Process Id, Parent Process Id, Process State
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to Question 4 (l: to list the current user processes in the system including it PID, process number, and state)
*/
int displayChildProcessInformation(char **cmdArguments)
{
    // Declarations
    int processInfoCounter, childStatus;

    if(cmdArguments[1] == NULL)
    {
        if(childProcessCountTracker == 0)
        {
            printf("There aren't any child processes that have been created to show their information.\n");
        }
        for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
        {
            printf("\033[0;36m");
            printf("Process %d, PID %d, PPID %d, State %s\n", childProcesses[processInfoCounter].processNumber,
            childProcesses[processInfoCounter].processId, getpid(), getProcessState(childProcesses[processInfoCounter].state));
            printf("\033[0m");
        }
        return 1;
    }

    puts("The command you entered appears to be incorrect.\n"
    "If needed, use the 'help' command to see the command list.\n");

    return 1;
}

/*
* Summary - This method will create child processes.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to question 2 (c #)
*/
int createChildProcesses(char **cmdArguments)
{
    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        // Declarations
        int argDataLength = strlen(cmdArguments[1]);
        int argCount, forkStatus;

        for(argCount = 0; argCount < argDataLength; argCount ++)
        {
            if(isdigit(cmdArguments[1][argCount]) == 0)
            {
                goto PrintError;
            }
        }

        int childProcessCount = atoi(cmdArguments[1]);

        if(childProcessCount > 0 && childProcessCount <= MAXIMUM_NUMBER_OF_CHILD_PROCESSES)
        {
            forkStatus = forkParentProcess(childProcessCount);
            return forkStatus;
        }
    }

    // The user has entered an invalid value for the no. of child processes to be generated. Display the error and proceed the execution.
    PrintError: printf("The entered value for the no. of child processes to be generated appears to be incorrect.\nPlease set the value to atleast 1 and no longer than %d.\nIf needed, use the 'help' command to see the command list.\n", MAXIMUM_NUMBER_OF_CHILD_PROCESSES);

    return 1;
}

/*
* Summary - This method will fork the parent process for creating the entered number of child processes.
* Param 1 - childProcessCount - Number of child processes to be created.
* 1. Run a for loop for childProcessCount times, and create a child process in every iteration.
* 2. Load './process' program using execvp in the newly created child process.
* 3. Add the new child process in the childProcesses struct by calling addChildProcess method.
* Returns 1, after a flawless execution
*/
int forkParentProcess(int childProcessCount)
{
    // Declarations
    int processCount, childProcessId, procNo;
    enum processStates state;
    char *processName = "./proce";

    for(processCount = 0; processCount < childProcessCount; processCount ++)
    {
        int processCreationAllowed = creationOfChildProcessAllowed();
        if(!processCreationAllowed)
        {
            printf("Maximum number of child processes created. Only %d processes can be created with this shell.\n",
            MAXIMUM_NUMBER_OF_CHILD_PROCESSES);
            break;
        }

        childProcessId = fork();

        // Child process code
        if(childProcessId == 0)
        {
            char *arguments[] = {processName, NULL};
            execvp(processName, arguments);
        }
        // Parent process code
        else if(childProcessId > 0)
        {
            state = Suspended;
            addChildProcess(childProcessId, state);
            printf("Child %d of Parent %d has been created.\n", childProcessId, getpid());
        }
        // Fork failure
        else
        {
            // The creation of child process has failed. So return 0 to stop the execution of the program.
            printf("The creation of child process failed.\n");
            return 0;
        }
    }

    return 1;
}

/*
* Summary - This method will check, if the shell can create new child processes.
* This shell is designed to create only MAXIMUM_NUMBER_OF_CHILD_PROCESSES processes.
* Returns 1, after a flawless execution
*/
int creationOfChildProcessAllowed()
{
    // Decalrations
    int totalChildProcesses = 0;
    totalChildProcesses = childProcessCountTracker + 1;

    if(totalChildProcesses > MAXIMUM_NUMBER_OF_CHILD_PROCESSES)
    {
        return 0;
    }

    childProcessCountTracker = totalChildProcesses;

    return 1;
}

/*
* Summary - This method will clear the console.
* Returns 1, after a flawless execution
*/
int clearConsole(char **cmdArguments)
{
    if(cmdArguments[1] == NULL)
    {
        system("clear");
        return 1;
    }

    puts("The entered command appears to be incorrect.\n"
    "Please use the 'help' command to see the command list.\n");

    return 1;
}

/*
* Summary - This method will be used to set the quantum time that will be used by RR & RRN scheduling algorithms.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to question 5 (q #)
*/
int setQuantumTime(char **cmdArguments)
{
    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        // Declarations
        int argDataLength = strlen(cmdArguments[1]);
        int argCount, loopCounter;

        for(argCount = 0; argCount < argDataLength; argCount ++)
        {
            if(isdigit(cmdArguments[1][argCount]) == 0)
            {
                goto PrintError;
            }
        }

        if(isProcessRunning() == 0)
        {
            printf("There is an ongoing process execution. Please stop that first and then set the quantum time.\n");
            return 1;
        }

        int timeInSecs = atoi(cmdArguments[1]);

        if(timeInSecs > 0 && timeInSecs <= MAXIMUM_ROUND_ROBIN_QUANTUM_TIME_IN_SECS)
        {
            roundRobinQuantumTimeInSecs = timeInSecs;
            printf("The value of quantum time is set to %d secs.\n", roundRobinQuantumTimeInSecs);
            return 1;
        }
    }

    // Invalid quantum time is entered by the user. Display the error and proceed the execution.
    PrintError:printf("The entered quantum time appears to be incorrect.\nPlease set the quantum time to atleast 1 sec and no longer than %d secs.\nIf needed, use the 'help' command to see the command list.\n", MAXIMUM_ROUND_ROBIN_QUANTUM_TIME_IN_SECS);

    return 1;
}

/*
* Summary - This method will be used to set the burst time that will be used by FCFS scheduling algorithm.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
*/
int setBurstTimeForFCFS(char **cmdArguments)
{
    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        // Declarations
        int argDataLength = strlen(cmdArguments[1]);
        int argCount, loopCounter;

        for(argCount = 0; argCount < argDataLength; argCount ++)
        {
            if(isdigit(cmdArguments[1][argCount]) == 0)
            {
                goto PrintError;
            }
        }

        if(isProcessRunning() == 0)
        {
            printf("There is an ongoing process execution. Please stop that first and then set the burst time.\n");
            return 1;
        }

        int timeInSecs = atoi(cmdArguments[1]);

        if(timeInSecs >= MINIMUM_FCFS_BURST_TIME_IN_SECS)
        {
            fcfsBurstTimeInSecs = timeInSecs;
            printf("The value of burst time is set to %d secs.\n", fcfsBurstTimeInSecs);
            return 1;
        }
    }

    // Invalid burst time is entered by the user. Display the error and proceed the execution.
    PrintError:printf("The entered burst time appears to be incorrect.\nPlease set the burst time to atleast %d secs.\nIf needed, use the 'help' command to see the command list.\n", MINIMUM_FCFS_BURST_TIME_IN_SECS);

    return 1;
}

/*
* Summary - This method will be used to set the scheduling algorithm.
* 1.FCFS - First Come First Serve
* 2.RR - Round Robin
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
* Answer to question 6 & 7 (t rr, t fcfs)
*/
int setSchedulingAlgorithm(char **cmdArguments)
{
    // Declarations
    char *schedulingAlgorithms[] = {"fcfs", "rr"};
    int algoLength = sizeof(schedulingAlgorithms) / sizeof(char*);
    int countOfAlgo;

    if(cmdArguments[1] != NULL && cmdArguments[2] == NULL)
    {
        for(countOfAlgo = 0; countOfAlgo < algoLength; countOfAlgo++)
        {
            if (strcmp(cmdArguments[1], schedulingAlgorithms[countOfAlgo]) == 0)
            {
                if(isProcessRunning() == 0)
                {
                    printf("Cant set a new algorithm, as the process is already running.\n");
                    return 1;
                }

                // Reset the data
                terminatedChildProcess = 0;

                strcpy(schedulingAlgo, cmdArguments[1]);
                printf("The algorithm for process scheduling is set to %s.\n", schedulingAlgo);
                return 1;
            }
        }
    }

    // Invalid name of the algorithm is entered by the user. Display list of available algorithms & commands and proceed the execution.
    puts("The algorithm you entered appears to be incorrect.\n"
    "Round robin and fcfs are currently supported.\n"
    "If needed, use the 'help' command to see the command list.\n");

    return 1;
}

/*
* Summary - This method will check if any process is running in the background.
* Returns 0, if process is running, else 1.
*/
int isProcessRunning()
{
    int processInfoCounter;

    for(processInfoCounter = 0; processInfoCounter < childProcessCountTracker; processInfoCounter++)
    {
        if(childProcesses[processInfoCounter].state == Running)
        {
            return 0;
        }
    }

    return 1;
}

/*
* Summary - This method will help the user to understand the commands that are supported by this shell.
* Param 1 - cmdArguments - command line arguments
* Returns 1, after a flawless execution
*/
int supportedCommandsMessage(char **cmdArguments)
{
    if(cmdArguments[1] == NULL)
    {
        puts("List of available commands:"
        "\n>c #: Create # child processes."
        "\n>l: Display the child process information."
        "\n>q #: Set the quantum time to be # secs."
        "\n>b #: Set the burst time to be # secs for FCFS."
        "\n>t rr: Set the scheduling algorithm to be round robin."
        "\n>t fcfs: Set the scheduling algorithm to be first come first serve."
        "\n>k #: Terminate the process, having the process number #."
        "\n>r #: Resume the process, having the process number #."
        "\n>r all: Resume to run all the processes in ready / suspended state."
        "\n>s all: Suspend all the processes."
		"\n>x or X: Exit the shell & all child processes."
        "\n>clear: Clear the console.");
    }
    else
    {
        puts("The entered command seems to be incorrect.\n"
        "Please type only 'help' to display list of available commands.\n");
    }

    return 1;
}

/*
* Summary - This method will be used to execute the command entered by the user.
* Param 1 - cmdArguments - Command line arguments
* Returns 1, after a flawless execution
*/
int executeCommand(char **cmdArguments)
{
    // Declarations
    int cmdCounter;
    int countOfSupportedCommands = sizeof(shellCommands) / sizeof(char*);

    // No arguments are provided. Proceed the execution by returning 1
    if(cmdArguments[0] == NULL)
    {
        return 1;
    }

    for(cmdCounter = 0; cmdCounter < countOfSupportedCommands; cmdCounter++)
    {
        if (strcmp(cmdArguments[0], shellCommands[cmdCounter]) == 0)
        {
            return (*supportedShellCommands[cmdCounter])(cmdArguments);
        }
    }

    // Invalid argument is entered by the user. Display list of available commands and proceed the execution.
    puts("The command you entered appears to be incorrect.\n"
    "Please use the 'help' command to see the command list.\n");

    return 1;
}

/*
* Summary - This method will break the input command into list of tokens using the delimeter constant DELIMITERS.
* Param 1 - inputLine - The line that is entered by the user.
* Returns list of tokens.
*/
char** extractArguments(char *inputLine)
{
    // Declarations
    int argumentsBufferSize = (BUFFER_SIZE / 4);
    int argPosition = 0;
    char *argument;
    char **arguments = malloc(sizeof(char*) * argumentsBufferSize);

    if(!arguments)
    {
        fprintf(stderr, "An error occurred while allocating memory to an arguments buffer.\n");
        exit(1);
    }

    argument = strtok(inputLine, DELIMITERS);

    while(argument != NULL)
    {
        arguments[argPosition] = argument;

        argPosition++;

        // Reallocate the buffer, if the arguments data exceeds the buffer.
        if(argPosition >= argumentsBufferSize)
        {
            argumentsBufferSize = (argumentsBufferSize * 2);
            arguments = realloc(arguments, argumentsBufferSize * sizeof(char*));
            if (!arguments)
            {
                fprintf(stderr, "An error occurred while reallocating memory to an arguments buffer.\n");
                exit(1);
            }
        }

        argument = strtok(NULL, DELIMITERS);
    }

    arguments[argPosition] = NULL;

    return arguments;
}

/*
* Summary - This method will capture the command and will process it.
* 1. Take input from the user
* 2. Extract the arguments from the given input
* 3. Execute the requested command
*/
void captureAndProcessInput(void)
{
    char *inputLine;
    char **cmdArguments;
    size_t inputLength = 0;
    int cmdExecutionStatus = 1;
    char buf[100];

    while(cmdExecutionStatus)
    {
        printf("\033[0;33m");
        printf("shell 5500>>> ");
        printf("\033[0m");

        printf("\033[0;36m");
        inputLine = fgets(buf, sizeof(buf), stdin);
        printf("\033[0m");

        if(inputLine != NULL)
        {
            cmdArguments = extractArguments(inputLine);
            cmdExecutionStatus = executeCommand(cmdArguments);
            free(cmdArguments);
        }
        else
        {
            printf("\n");
        }
    }
}

int main()
{
    // Register the control-c signal handler
    struct sigaction controlCSignalAction =
    {
        .sa_handler = controlCHandler,
        .sa_flags = 0,
        .sa_mask = 0
    };
    sigaction(SIGINT, &controlCSignalAction, NULL);

    // Register the alarm signal handler
    struct sigaction alarmSignalAction =
    {
        .sa_handler = alarmHandler,
        .sa_flags = 0,
        .sa_mask = 0
    };
    sigaction(SIGALRM, &alarmSignalAction, NULL);

    captureAndProcessInput();

    exit(0);
}

