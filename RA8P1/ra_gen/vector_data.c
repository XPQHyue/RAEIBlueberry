/* generated vector source file - do not edit */
#include "bsp_api.h"
/* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
#if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = r_icu_isr, /* ICU IRQ29 (External pin interrupt 29) */
            [1] = sci_b_uart_rxi_isr, /* SCI0 RXI (Receive data full) */
            [2] = sci_b_uart_txi_isr, /* SCI0 TXI (Transmit data empty) */
            [3] = sci_b_uart_tei_isr, /* SCI0 TEI (Transmit end) */
            [4] = sci_b_uart_eri_isr, /* SCI0 ERI (Receive error) */
            [5] = iic_master_rxi_isr, /* IIC2 RXI (Receive data full) */
            [6] = iic_master_txi_isr, /* IIC2 TXI (Transmit data empty) */
            [7] = iic_master_tei_isr, /* IIC2 TEI (Transmit end) */
            [8] = iic_master_eri_isr, /* IIC2 ERI (Transfer error) */
            [9] = gpt_counter_overflow_isr, /* GPT4 COUNTER OVERFLOW (Overflow) */
            [10] = poeg_event_isr, /* POEG1 EVENT (Port Output disable 1 interrupt) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_ICU_IRQ29,GROUP0), /* ICU IRQ29 (External pin interrupt 29) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_SCI0_RXI,GROUP1), /* SCI0 RXI (Receive data full) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_SCI0_TXI,GROUP2), /* SCI0 TXI (Transmit data empty) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_SCI0_TEI,GROUP3), /* SCI0 TEI (Transmit end) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_SCI0_ERI,GROUP4), /* SCI0 ERI (Receive error) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_IIC2_RXI,GROUP5), /* IIC2 RXI (Receive data full) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TXI,GROUP6), /* IIC2 TXI (Transmit data empty) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TEI,GROUP7), /* IIC2 TEI (Transmit end) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_IIC2_ERI,GROUP0), /* IIC2 ERI (Transfer error) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_GPT4_COUNTER_OVERFLOW,GROUP1), /* GPT4 COUNTER OVERFLOW (Overflow) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_POEG1_EVENT,GROUP2), /* POEG1 EVENT (Port Output disable 1 interrupt) */
        };
        #endif
        #endif
