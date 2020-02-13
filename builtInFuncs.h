int sh_status(char** args, int status);

int sh_exit(char** args, int status);

int sh_cd(char** args, int status);

char *builtInFuncNames[] =
{
    "cd",
    "status",
    "exit"
};

int (*builtInFuncs[])(char **, int) =
{
    &sh_cd,
    &sh_status,
    &sh_exit
};

int exitStatus = 0;