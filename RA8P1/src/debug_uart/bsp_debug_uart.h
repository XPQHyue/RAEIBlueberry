/* bsp_debug_uart.h - UART0 调试串口驱动头文件 */

#ifndef DEBUG_UART_BSP_DEBUG_UART_H_
#define DEBUG_UART_BSP_DEBUG_UART_H_

#include "hal_data.h"
#include "stdio.h"

void Debug_UART0_Init(void);

bool UART_RxAvailable(void);
char UART_GetChar(void);

extern volatile bool uart_send_complete_flag;

#endif
