#include <propeller.h>
#include "test77.h"

char test77::dat[] = {
  0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x40, 0xf3, 0x04, 0xb5, 0x3f, 
};
int32_t *test77::Dummy(void)
{
  return (((int32_t *)&dat[0]));
}

