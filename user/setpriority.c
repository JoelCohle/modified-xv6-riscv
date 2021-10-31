#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int priority, pid;
  if(argc < 3){
    fprintf(2,"Invalid number of arguments\n");
    exit(1);
  }

  priority = atoi(argv[1]);
  pid = atoi(argv[2]);

  if (priority > 100 || priority < 0){
    fprintf(2,"Entered priority is invalide(0-100)!\n");
    exit(1);
  }
  set_priority(priority, pid);
  exit(1);
}