#include "tests/lib.h"
#include "proc/syscall.h"

int main(void)
{
  char buffer[64];
  char startString[] = "Please write 10 characters: \n";
  char endString[] = "\nTest complete.\n";

  syscall_write(FILEHANDLE_STDIN, startString, 30);

  syscall_read(FILEHANDLE_STDIN, buffer, 10);

  syscall_write(FILEHANDLE_STDIN, buffer, 10);

  syscall_write(FILEHANDLE_STDIN, endString, 16);

  syscall_halt();
  
  return 0;
}
