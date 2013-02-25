#include "proc/exit.h"
#include "proc/process.h"

void syscall_exit(int retval) {
  process_finish(retval);
}
