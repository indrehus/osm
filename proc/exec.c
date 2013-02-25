#include "proc/exec.h"
#include "proc/process.h"

int syscall_exec(const char*filename) {
  return (int) process_spawn(filename);
}
