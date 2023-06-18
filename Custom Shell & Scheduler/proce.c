/*
* Developer: Purnima Naik
* Summary: The loop iteration number and process id are printed by this program's endless loop.
*/
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(int argc, char* argv[])
{
    // Declarations
    int loopCount;

    // Ignore the signal SIGINT, as it will get propogated from parent to all its children.
    // The signal is handled in the parent, so it can be ignored here.
    signal(SIGINT, SIG_IGN);

    // As soon as the process gets created, stop its execution.
    kill(getpid(), SIGSTOP);

    // Print the loop iteration number and the process id
    while(1)
    {
        printf("\033[0m");
        printf("Process %d at iteration %d\n", getpid(), loopCount);
        sleep(1);
        loopCount ++;
    }

    return 0;
}


