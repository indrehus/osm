#include "proc/write.h"
#include "lib/libc.h"
#include "drivers/device.h"
#include "drivers/gcd.h"
#include "kernel/assert.h"

int syscall_write(int filehandle, const void *buffer, int length) {
  device_t *dev;
  gcd_t *gcd;
  int len;

  switch (filehandle) {
  case 2:
    return -1;
  default:
    break;
  }

  /* Find system console (first tty) */
  dev = device_get(YAMS_TYPECODE_TTY, 0);
  KERNEL_ASSERT(dev != NULL);

  gcd = (gcd_t *)dev->generic_device;
  KERNEL_ASSERT(gcd != NULL);

  len = gcd->write(gcd, buffer, length);
  KERNEL_ASSERT(len >= 0);

  return len;
}
