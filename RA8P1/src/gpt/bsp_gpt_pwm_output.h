/**
 * @file    bsp_gpt_pwm_output.h
 * @brief   GPT PWM 输出 — 参数化占空比设置, 支持多定时器/多引脚
 *
 * 当前硬件:
 *   GPT5 GTIOC5B (P914) — Motor A PWM
 *   GPT1 GTIOC1A (P105) — Motor B PWM
 */

#ifndef __BSP_GPT_PWM_OUTPUT_H
#define __BSP_GPT_PWM_OUTPUT_H

#include "hal_data.h"

/** 一次性初始化所有 GPT PWM 通道 */
void GPT_PWM_Init(void);

/**
 * @brief 设置 PWM 占空比 (参数化 — 替代 SetDuty / SetDuty_B)
 * @param p_ctrl  GPT 控制块指针 (如 &g_timer5_ctrl)
 * @param pin     PWM 输出引脚 (GPT_IO_PIN_GTIOCA 或 GPT_IO_PIN_GTIOCB)
 * @param duty    占空比 0~1000 (0.0%~100.0%), 超出自动限幅
 */
void GPT_PWM_SetDuty(gpt_instance_ctrl_t *p_ctrl, gpt_io_pin_t pin, uint16_t duty);

#endif /* __BSP_GPT_PWM_OUTPUT_H */
