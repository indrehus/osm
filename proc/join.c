#include "proc/join.h"
#include "proc/process.h"

int syscall_join(int pid) {
  return process_join((process_id_t) pid);
}
