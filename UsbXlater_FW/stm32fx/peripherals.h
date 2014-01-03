#ifndef PERIPHERALS_H_
#define PERIPHERALS_H_

#if defined(COMPILE_FOR_STM32F4)
#include <stm32f4/stm32f4xx_crc.h>
#include <stm32f4/stm32f4xx_dbgmcu.h>
#include <stm32f4/stm32f4xx_flash.h>
#include <stm32f4/stm32f4xx_gpio.h>
#include <stm32f4/stm32f4xx_rcc.h>
#include <stm32f4/stm32f4xx_syscfg.h>
#include <stm32f4/stm32f4xx_tim.h>
#include <stm32f4/stm32f4xx_usart.h>
#elif defined(COMPILE_FOR_STM32F2)
#include <stm32f2/stm32f2xx_crc.h>
#include <stm32f2/stm32f2xx_dbgmcu.h>
#include <stm32f2/stm32f2xx_flash.h>
#include <stm32f2/stm32f2xx_gpio.h>
#include <stm32f2/stm32f2xx_rcc.h>
#include <stm32f2/stm32f2xx_syscfg.h>
#include <stm32f2/stm32f2xx_tim.h>
#include <stm32f2/stm32f2xx_usart.h>
#endif

#endif