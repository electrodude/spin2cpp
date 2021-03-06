#include <errno.h>
#undef errno

static int errno;

int _geterror() { return errno; }
int _seterror(int num) { if (num) { errno = num; return -1; } else return 0; }

int *_geterrnoptr() { return &errno; }
