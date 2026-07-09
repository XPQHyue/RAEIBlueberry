/* generated configuration header file - do not edit */
#ifndef BSP_PIN_CFG_H_
#define BSP_PIN_CFG_H_
#include "r_ioport.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#define LED_GREE (BSP_IO_PORT_01_PIN_10)
#define AIN0 (BSP_IO_PORT_03_PIN_08)
#define BIN0 (BSP_IO_PORT_03_PIN_12)
#define BIN1 (BSP_IO_PORT_05_PIN_11)
#define AIN1 (BSP_IO_PORT_05_PIN_12)

extern const ioport_cfg_t g_bsp_pin_cfg; /* R7KA8P1KFLCAC.pincfg */

void BSP_PinConfigSecurityInit();

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER
#endif /* BSP_PIN_CFG_H_ */
