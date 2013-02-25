/*
 * Userland exec test
 */

#include "tests/lib.h"
#include "lib/libc.h"

static const char prog[] = "[fyams]hw"; /* The program to start. */

int main(void)
{
  uint32_t child;
  int ret;
  //kprintf("Starting program %s\n", prog);
  child = syscall_exec(prog);
  //kprintf("Now joining child %d\n", child);
  ret = (char)syscall_join(child);
  ret = ret;
  //kprintf("Child joined with status: %d\n", ret);
  syscall_halt();
  return 0;
}
