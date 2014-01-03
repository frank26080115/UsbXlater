#include <compile_options.h>

#if defined(COMPILE_FOR_STM32F4)
#include <stm32f4/startup_stm32f4xx.S>
#elif defined(COMPILE_FOR_STM32F2)
#include <stm32f2/startup_stm32f2xx.S>
#endif
