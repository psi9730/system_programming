#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  int status;
  int p[2];
  if (pipe(p) == -1) {
    perror("Cannot open pipe");
    return 1;
  }

  // fork
  pid_t pid = fork();
  if (pid == 0) {
    // parent
    char *child_argv[]={
        argv[1],
        NULL,
        NULL
    };
    // redirect write end of pipe to STDOUT
    if (dup2(p[1], STDOUT_FILENO) == -1) {
      perror("Cannot redirect stdout");
      return 1;
    }
    // close unused read end
    close(p[0]);
    execv(child_argv[0],child_argv);
    perror("Cannot execute child.");
  } else if (pid > 0) {
    int i, res;
    FILE *pipe_read;
    FILE *fp;
    sleep(1);
    // convert file descriptor into file stream
    pipe_read = fdopen(p[0], "r");
    // close unused write end of pipe
    close(p[1]);
    char temp[40]={NULL};
    // read numbers and print as hex until pipe closed
    fread(temp, sizeof(char),40, pipe_read);
    fp= fopen(argv[2],"w");
    if(fp!=NULL){
	fflush(stdout);
        fwrite(temp, sizeof(char), sizeof(temp), fp);
    }
  }

  return 0;
}
