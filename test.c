// Daniel Swann and Jerald Liu
// Operating Systems Fall 2015
// Assignment 3
// Test file, please ignore

#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

int main(int argc, char * argv[]) 
{
  // Test sleeptable
  int pid;
  int parent = getpid();
  int i;
  for (i=0;i<1;i++) {
    printf(1, "Nice %d returned %d\n", 1, nice(2));
    pid = fork();
    printf(1, "Nice %d returned %d\n", 3, nice(-4));
    printf(1, "Nice %d returned %d\n", -1, nice(2));
    if (pid == 0) {
      // Child Process
      // Kill parent
      printf(1, "Child process\n");
      printf(1, "Kill returned %d\n", kill(parent));
      kill(parent);
    } else if (pid > 0) {
      // Parent Process
      wait();
    } else {
      // Error, failed to fork()
      printf(1, "Error: fork failure");
    }

  }  
  exit(); 
}
