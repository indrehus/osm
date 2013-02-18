#include "proc/write.h"

int syscall_write(int filehandle, const void *buffer, int length) {
  kprintf("test");
}
