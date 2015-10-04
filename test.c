#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char * argv[]) 
{
  // Test sleeptable
  int pid;
  int parent = getpid();
  pid = fork();
  if (pid == 0) {
    // Child Process
    printf(1, "Child id: %d\n", pid);
    kill(parent);
  } else if (pid > 0) {
    // Parent Process
    printf(1, "Parent id: %d\n", pid);
    wait();
    printf(1, "Parent: Child done executing.\n");
  } else {
    // Error, failed to fork()
  }
  
  
  
  //printf(1, "%d\n", nice(1));
  exit(); 
}
