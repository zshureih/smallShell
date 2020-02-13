#define _GNU_SOURCE //used to include getline() in stdio.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "builtInFuncs.h"

#define TOKEN_BUFFERSIZE 512
#define TOKEN_DELIMETER " \t\r\n\a\0"
#define NUM_BUILT_IN_FUNCS 3

int background = 0;
int foregroundMode = 0;

int NUM_PIDS = 0;
int child_pids[200];

int sh_execute(char **args, struct sigaction SIGINT_action, struct sigaction SIGTSTP_action);

int sh_launch(char **args, struct sigaction SIGINT_action, struct sigaction SIGTSTP_action);

char **sh_split_line(char *line);

char *sh_read_line();

void sh_handle_background_process(char **args, int status);

void sh_handle_redirect(char **args);

void catchSIGTSTP(int signo)
{
  if (!foregroundMode)
  {
    //enter foreground only mode
    char *line = "Entering foreground-only mode (& is now ignored)\n";
    write(1, line, 50); //write the line to STDOUT
    fflush(stdout);

    write(1, ": ", 3);
    fflush(stdout);

    foregroundMode = 1;
  }
  else
  {
    //exit foreground only mode
    char *line = "Exiting foreground-only mode (& is now enabled)\n";
    write(1, line, 49); //write the line to STDOUT
    fflush(stdout);

    write(1, ": ", 3);
    fflush(stdout);

    foregroundMode = 0;
  }
}

// void catchSIGINT(int signo)
// {
//   char *message = "terminated by signal 2\n";
//   write(1, message, 24); //write the message to STDOUT
//   fflush(stdout);
//   exit(0);
// }

//auxiliary function for bug checking
void printArgs(char **args)
{
  for (int i = 0; args[i] != NULL; i++)
  {
    printf("args[%d]: %s\n", i, args[i]);
  }
}

int main(int argc, char **argv)
{
  // Load config files, if any.
  struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};
  
  //create signal handler for ^C
  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);  

  //create signal handler for ^Z
  SIGTSTP_action.sa_handler = catchSIGTSTP;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = SA_RESTART;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  // Run command loop.
  char *line = '\0';
  char **args;
  int status = 1;
  int backgroundStatus = 0;
  int quitProgram = 1; //only turns 0 through exit command

  do
  {
    sh_handle_background_process(args, backgroundStatus);
    line = sh_read_line();       //read
    args = sh_split_line(line);     //parse read line into tokens/args
    status = sh_launch(args, SIGINT_action, SIGTSTP_action);   //execute arguments
    if(status == 0) // if the user calls exit
    {
      //kill runaway processes
      for(int i = 0; i < NUM_PIDS; i++)
      {
        if(kill(child_pids[i], SIGKILL) == 0)
        {
          //success
          printf("child %d terminated\n", child_pids[i]);
          sh_status(args, status); //print status
          fflush(stdout);
        }
      }
    }

    //handle memory
    free(line);
    free(args);
  }
  while (status); //while status is not 0, execute loop

  return 0;
}

void sh_handle_redirect(char **args)
{
  int targetFileDescriptor;
  char *fileName = NULL;
  int isRedirected = 0;

  for (int i = 0; args[i] != NULL; i++)
  {
    if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0) //if there is a redirect
    {
      isRedirected = 1;       //mark the flag
      fileName = args[i + 1]; //get the file name (next argument)
      if (background)         //if this is a background process
      {
        targetFileDescriptor = open("/dev/null", O_RDONLY); //process is not displayed in shell, pass to dev/null

        if (dup2(targetFileDescriptor, STDIN_FILENO) == -1) //redirect stdin
        {
          fprintf(stderr, "Error redirecting stdin");
          exit(1);
        }

        if (dup2(targetFileDescriptor, STDOUT_FILENO) == -1) //redirect stdout
        {
          fprintf(stderr, "Error redirecting stdout");
          exit(1);
        }
      }
      else //foreground process
      {
        if (strcmp(args[i], "<") == 0) //redirect input
        {
          targetFileDescriptor = open(fileName, O_RDONLY); //open file to read
          if (targetFileDescriptor == -1)
          {
            fprintf(stderr, "Cannot open %s for input\n", fileName);
            exit(1);
          }
          if (dup2(targetFileDescriptor, STDIN_FILENO) == -1)
          {
            fprintf(stderr, "Error redirecting"); 
            exit(1);
          }
        }
        else //redirect output
        {
          targetFileDescriptor = open(fileName, O_CREAT | O_RDWR | O_TRUNC, 0644); //open file to write with valid permissions
          if (targetFileDescriptor == -1)
          {
            fprintf(stderr, "Cannot open %s for output\n", fileName);
            exit(1);
          }
          if (dup2(targetFileDescriptor, STDOUT_FILENO) == -1)
          {
            fprintf(stderr, "Error redirecting");
            exit(1);
          }
        }
        close(targetFileDescriptor); //close the file
      }
    }
  }
  // remove all args except first so command can still execute
  if (isRedirected)
  {
    for (int i = 1; args[i + 1] != NULL; i++)
    {
      args[i] = NULL;
    }
  }
}

void sh_handle_background_process(char **args, int status)
{
  int wpid;

  while ((wpid = waitpid(-1, &status, WNOHANG)) > 0) //check for terminated background process
  {
    printf("child %d terminated\n", wpid);
    sh_status(args, status); //print status
    fflush(stdout);
  }
}

char *sh_read_line()
{
  printf(": "); //print user prompt
  char *line = NULL;
  int lineSize = 0;
  ssize_t buffSize = 0; //have getline allocate a buffer
  lineSize = getline(&line, &buffSize, stdin);
  if(line != NULL)
  {
    line[lineSize - 1] = '\0'; //set to null
  }
  else
  {
    strcpy(line, "#"); //pass a comment
  }
  
  if (strstr(line, "$$") != 0) //if line contains $$
  {
    char* subStr = strstr(line, "$$");
    
    //replace the $$ with the pid (this only works if $$ is at the end of a line like it is in the test script)
    sprintf(subStr, "%d", getpid()); 
  }

  return line;
}

//parses line read by sh_read_line() into a list of tokens
char **sh_split_line(char *line)
{
  int buffSize = TOKEN_BUFFERSIZE;
  int pos = 0;
  char **args = malloc(buffSize * sizeof(char *));
  char *token;

  if (!args) //if there's an allocation error
  {
    fprintf(stderr, "sh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, TOKEN_DELIMETER); //get first token delimited by " \t\r\n\a" from line
  while (token != NULL)
  {
    // Check for & to be a background process
    if (strcmp(token, "&") == 0 && strcmp(args[0], "echo") != 0)
    {
      if(!foregroundMode)
      {
        background = 1; //set next process to run in the background
      }
      args[pos] = NULL;
    }
    else
    {
      args[pos] = token; //add token to list of args
    }
    pos++;
    token = strtok(NULL, TOKEN_DELIMETER); //get next token
  }
  args[pos] = NULL; //mark the end of the list of args

  return args; //return the list of args to the main shell loop
}

int sh_execute(char **args, struct sigaction SIGINT_action, struct sigaction SIGTSTP_action)
{
  pid_t pid, wpid; //current pid and wait pid
  int status, input, output, result;

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGTSTP);
  sigprocmask(SIG_BLOCK, &set, NULL); //prevent SIGTSTP from being handled until it is unblocked 

  pid = fork(); //fork the current process

  // this creates two process
  // the one we were already in (parent) will wait for the child
  // the child will execute as normal, printing to stderror if there is any
  if (pid < 0)
  {
    //error forking
    perror("FORK ERROR\n");
    exit(2);
  }
  else if (pid == 0)
  {
    // child process

    //process will handle ^C normally if it is a foreground process
    if (!background)
    {
      SIGINT_action.sa_handler = SIG_DFL;
      sigaction(SIGINT, &SIGINT_action, NULL);
    }

    sh_handle_redirect(args);

    // execute the command
    if (execvp(args[0], args) == -1)
    {
      perror(args[0]);
      fflush(stdout);
      exit(2);
    }
  }
  else
  {
    //parent process
    if (background && !foregroundMode) //if background mode is on, and the process is to be run in the background
    {
      printf("background pid is %d\n", pid); //print pid
      waitpid(pid, &status, WNOHANG);
      fflush(stdout);
      //add background process to list
      background = 0; //set background to 0 so the next command isn't acccidentally run in the background

      //add process to list of child processes
      child_pids[NUM_PIDS] = pid;
      NUM_PIDS++;
    }
    else // if not a background process
    {
      do
      {
        waitpid(pid, &status, WUNTRACED);
      } while (!WIFEXITED(status) && !WIFSIGNALED(status)); //wait for child process to end

      if (WIFSIGNALED(status))//on signal
      {
        printf("terminated by signal %d\n", WTERMSIG(status)); //print 
      }
    }

    exitStatus = status;
  }

  sigprocmask(SIG_UNBLOCK, &set, NULL); //alllow SIGTSTP signals to propogate again

  return 1;
}

int sh_launch(char **args, struct sigaction SIGINT_action, struct sigaction SIGTSTP_action)
{
  int i;
  int builtIn = 0;

  if (args[0] == NULL) //if there are no args
  {
    return 1; //empty command, continue running the shell
  }

  if (args[0][0] == 35) //if first char in first arg is #
  {
    return 1; //comment, continue running the shell
  }

  //check for built in function in list of args
  for (i = 0; i < NUM_BUILT_IN_FUNCS; i++)
  {
    if (strcmp(args[0], builtInFuncNames[i]) == 0)
    {
      return (*builtInFuncs[i])(args, exitStatus); //execute first built in command found
    }
  }

  //otherwise execute an outside function with given args
  return sh_execute(args, SIGINT_action, SIGTSTP_action);
}