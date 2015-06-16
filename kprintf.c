#include "monitor.h"
#include "kprintf.h"
#include "vsprintf.h"
#include <stdarg.h>

void kprintf (const char *fmt, ...)
{
	static char buf [1024];

 	va_list args;
 	int i;
 
 	va_start(args, fmt);
 	i = vsprintf(buf,fmt,args);
 	va_end(args);

 	buf[i] = '\0';
 	monitor_write (buf);
}

void sprintf (char *buf, const char *fmt, ...)
{

 	va_list args;
 	int i;
 
 	va_start(args, fmt);
 	i = vsprintf(buf,fmt,args);
 	va_end(args);

 	buf[i] = '\0';
}
