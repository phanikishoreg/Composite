#ifndef LLPRINT_H
#define LLPRINT_H

#include <stdio.h>
#include <string.h>

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
  	  va_list arg_ptr;
  	  int ret, len = 128;

  	  va_start(arg_ptr, fmt);
  	  ret = vsnprintf(s, len, fmt, arg_ptr);
  	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

int
prints(char *s)
{
	int len = strlen(s);

	printc("%u:", cos_thdid());
	cos_llprint(s, len);

	return len;
}

#endif /* LLPRINT_H */
