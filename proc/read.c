#include "lib/libc.h"
#include "proc/read.h"

int syscall_read(int filehandle, void *buffer, int length) {
  kprintf("test");
  return 0;
}
