/**
 * @file    bsp_gpt_pwm_output.c
 * @brief   GPT PWM 输出实现 — 参数化, 支持多定时器/多引脚
 *
 * 当前选用 PWM 输出引脚:
 *   GPT5 GTIOC5B (P914) — Motor A
 *   GPT1 GTIOC1A (P105) — Motor B
 */

#include "bsp_gpt_pwm_output.h"


void GPT_PWM_Init(void)
{
    /* 初始化 GPT 模块 */
    R_GPT_Open(&g_timer5_ctrl, &g_timer5_cfg);
    R_GPT_Open(&g_timer1_ctrl, &g_timer1_cfg);

    /* 启动 GPT 定时器 */
    R_GPT_Start(&g_timer5_ctrl);
    R_GPT_Start(&g_timer1_ctrl);

    /* 占空比初始化为 0 (安全) */
    GPT_PWM_SetDuty(&g_timer5_ctrl, GPT_IO_PIN_GTIOCB, 0);
    GPT_PWM_SetDuty(&g_timer1_ctrl, GPT_IO_PIN_GTIOCA, 0);
}


/**
 * @brief 设置 PWM 占空比 (参数化, 统一替代 SetDuty / SetDuty_B)
 * @param p_ctrl  GPT 控制块指针
 * @param pin     PWM 输出引脚
 * @param duty    占空比 0~1000
 */
void GPT_PWM_SetDuty(gpt_instance_ctrl_t *p_ctrl, gpt_io_pin_t pin, uint16_t duty)
{
    timer_info_t info;
    uint32_t current_period_counts;
    uint32_t duty_cycle_counts;

    if (duty > 1000)
    {
        duty = 1000;
    }

    /* 获得 GPT 信息 */
    R_GPT_InfoGet(p_ctrl, &info);

    /* 获得计时器一个周期需要的计数次数 */
    current_period_counts = info.period_counts;

    /* 根据占空比和一个周期的计数次数计算 GTCCR 寄存器的值 */
    duty_cycle_counts = (uint32_t)(((uint64_t) current_period_counts * duty) / 1000);

    /* 调用 FSP 库函数设置占空比 */
    R_GPT_DutyCycleSet(p_ctrl, duty_cycle_counts, pin);
}
