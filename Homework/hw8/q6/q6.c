// filename: 4.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  int status;
  char *hello = "Hello, System Programming!";
  printf("Input string to send to child : %s\n", hello);
  // open pipe
  int p[2];
  if (pipe(p) == -1) {
    perror("Cannot open pipe");
    return 1;
  }

  // fork
  pid_t pid = fork();
  if (pid > 0) {
    // parent
    
    // redirect write end of pipe to STDOUT
    printf("Parent pid = %d: sending to child %d : %s\n", getpid(), pid, hello);
    if (dup2(p[1], STDOUT_FILENO) == -1) {
      perror("Cannot redirect stdout");
      return 1;
    }
    // close unused read end
    close(p[0]);
    printf("%s",hello);
    sleep(3);
  } else if (pid == 0) {
    int i, res;
    FILE *pipe_read;
    pid_t parentPID=getppid();
    // convert file descriptor into file stream
    pipe_read = fdopen(p[0], "r");
    // close unused write end of pipe
    close(p[1]);
    char *temp = malloc(40*sizeof(char));
    // read numbers and print as hex until pipe closed
    fread(temp, sizeof(char),40, pipe_read);
    printf("Child pid = %d, received from parent %d : %s\n",getpid(), parentPID, temp);
  }

  return 0;
}
