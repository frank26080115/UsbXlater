#ifndef DBG_WDG_H_
#define DBG_WDG_H_

#include <stm32fx/stm32fxxx.h>
#define DBGWDG_TIMx TIM4

void dbgwdg_init(void);
void dbgwdg_stop(void);

static inline void dbgwdg_feed(void);
static inline void dbgwdg_feed(void) {
	DBGWDG_TIMx->CNT = 1;
}

#endif
