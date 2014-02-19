#ifndef SWO_H_
#define SWO_H_

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

void swo_init();
void swo_deinit();
void swo_printf(char * format, ...);
void swo_wait();

#endif /* SWO_H_ */
