/*
* Project: Process Injection Exploitation and Prevention / Detection Techniques in Linux
* Summary: This is the victim software that the attacker will misuse.
*/

#include <stdio.h>
#include <unistd.h>

int main()
{
    // Declarations
    int counter = 0;

    puts("** CSI-5500-OS Project-Process Injection Exploitation & Prevention/Detection Techniques in Linux **\n");
    printf("My process id is %d.\n", getpid());

    while(1)
    {
        counter++;
    }

    return 0;
}
