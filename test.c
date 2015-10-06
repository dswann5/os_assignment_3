#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

int main(int argc, char * argv[]) 
{
  // Test sleeptable
  int pid;
  int parent = getpid();

//  printf(1, "Nice %d returned %d\n", 1, nice(1));
  pid = fork();
  //printf(1, "Nice %d returned %d\n", 3, nice(3));
  //printf(1, "Nice %d returned %d\n", -1, nice(-1));
      if (pid == 0) {
        // Child Process
        // Kill parent
        printf(1, "Child process\n");
        printf(1, "%d\n", kill(parent));
        //kill(parent);
      } else if (pid > 0) {
        // Parent Process
        wait();
        printf(1, "This should not have printed\n");
      } else {
        // Error, failed to fork()
      }

//  int i;
//  for (i = 0; i<1000;i++) {
//  }
  exit(); 
}