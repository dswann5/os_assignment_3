#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

int main(int argc, char * argv[]) 
{
  /*
  // Test sleeptable
  int pid;
  int parent = getpid();

  printf(1, "Nice %d returned %d\n", 1, nice(1));
  pid = fork();
  printf(1, "Nice %d returned %d\n", 3, nice(3));
  printf(1, "Nice %d returned %d\n", -1, nice(-1));

  int i;
  for (i = 0; i<1000;i++) {
      if (pid == 0) {
        // Child Process
        // Kill parent
        kill(parent);
        kill(parent);
      } else if (pid > 0) {
        // Parent Process
        wait();
        printf(1, "This should not have printed\n");
      } else {
        // Error, failed to fork()
      }
  }
  //printf(1, "%d\n", nice(1));
  exit(); 
  */
  /*
  int top_prior_ran = 1;
  int num = top_prior_ran;
  int curr_priority = HIGHEST_PRIORITY;
  int count = 0;
  for (;;) {
    printf(1, "Attempting to run priority: %d, top_prior_ran: %d\n", curr_priority, top_prior_ran);
    if (num%2==0) {
      num/=2;
      curr_priority++;
    } else {
      curr_priority = HIGHEST_PRIORITY;
      top_prior_ran++;
      if (top_prior_ran > pow(2,NPRIORITIES-1)) {
        top_prior_ran = 1;
      }
      num = top_prior_ran;
    }
  }
  */

  exit();
}
