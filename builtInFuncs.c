#define _GNU_SOURCE //used to include getline() in stdio.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

//built in function implementations
int sh_cd(char **args, int status)
{
    if (args[1] == NULL)
    {
        if (chdir(getenv("HOME")) != 0)
        {
            perror("sh");
        }
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror(args[1]);
        }
    }

    return 1;
}

int sh_status(char **args, int status)
{
    if (WIFEXITED(status)) //on exit, get exit status and store it in exitStatus to be used by internal commands
    {
        printf("exit value %d\n", WEXITSTATUS(status));
    }

    if (WIFSIGNALED(status)) //on signal, get terminating signal and store it in exitStatus to be used by internal commands
    {
        printf("terminated by signal %d\n", WTERMSIG(status));
    }  

    return 1;
}

int sh_exit(char** args, int* status)
{
    return 0;
}

