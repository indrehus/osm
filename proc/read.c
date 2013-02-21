#include "lib/libc.h"
#include "proc/read.h"
#include "drivers/device.h"
#include "drivers/gcd.h"
#include "kernel/assert.h"

int syscall_read(int filehandle, void *buffer, int length) {
  device_t *dev;
  gcd_t *gcd;
  int len = 0;
  int i;

  switch (filehandle){
  case 1:
    return -1;
  case 2:
    return -1;
  default:
    break;
  }
  

  /* Find system console (first tty) */
  dev = device_get(YAMS_TYPECODE_TTY, filehandle);
  KERNEL_ASSERT(dev != NULL);


  gcd = (gcd_t *)dev->generic_device;
  KERNEL_ASSERT(gcd != NULL);

  for(i = 0; i < length; i++) {
    len += gcd->read(gcd, buffer++, 1);
  }

  KERNEL_ASSERT(len >= 0);
  buffer = '\0';

  return len;
}
