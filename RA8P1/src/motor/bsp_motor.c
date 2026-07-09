/**
 * @file    bsp_motor.c
 * @brief   电机控制模块实现 — 基于 motor_t 实例的参数化驱动
 *
 * H 桥真值表 (TB6612 / L298 / DRV8833 等通用逻辑):
 *   AINx=0, AINx=1 → 电机电流 A→B, 正转 (Forward)
 *   AINx=1, AINx=0 → 电机电流 B→A, 反转 (Reverse)
 *   AINx=1, AINx=1 → 电机两端短接到地, 刹车 (Brake)
 *   AINx=0, AINx=0 → 电机两端悬空, 滑行 (Coast / Free-run)
 *
 * 注意: 方向切换和占空比设置应该先设方向再调整占空比,
 *       避免在非预期方向上有大电流 PWM 输出。
 *
 * 设计原则:
 *   参数化隔离硬件变动 — motor_t 结构体携带硬件差异, API 逻辑完全复用。
 */

#include "bsp_motor.h"
#include "gpt/bsp_gpt_pwm_output.h"

/* ====== 全局电机实例 (硬件配置在此集中定义) ====== */

motor_t g_motorA = {
    .p_timer_ctrl = &g_timer5_ctrl,
    .p_timer_cfg  = &g_timer5_cfg,
    .pwm_pin      = GPT_IO_PIN_GTIOCB,
    .ain0_pin     = AIN0,                    /* BSP_IO_PORT_03_PIN_08 */
    .ain1_pin     = AIN1,                    /* BSP_IO_PORT_05_PIN_12 */
    .direction    = MOTOR_DIR_BRAKE,
    .opened       = false,
};

motor_t g_motorB = {
    .p_timer_ctrl = &g_timer1_ctrl,
    .p_timer_cfg  = &g_timer1_cfg,
    .pwm_pin      = GPT_IO_PIN_GTIOCA,
    .ain0_pin     = BIN0,                    /* BSP_IO_PORT_03_PIN_12 */
    .ain1_pin     = BIN1,                    /* BSP_IO_PORT_05_PIN_11 */
    .direction    = MOTOR_DIR_BRAKE,
    .opened       = false,
};

/* ====== 内部辅助 ====== */

/** 将方向枚举映射到 GPIO 电平并写入硬件 */
static void ApplyDirection(motor_t *p_motor, motor_dir_t dir)
{
    switch (dir)
    {
    case MOTOR_DIR_FORWARD:
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain0_pin, BSP_IO_LEVEL_LOW);   /* AINx=0 */
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain1_pin, BSP_IO_LEVEL_HIGH);  /* AINx=1 */
        break;

    case MOTOR_DIR_REVERSE:
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain0_pin, BSP_IO_LEVEL_HIGH);  /* AINx=1 */
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain1_pin, BSP_IO_LEVEL_LOW);   /* AINx=0 */
        break;

    case MOTOR_DIR_BRAKE:
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain0_pin, BSP_IO_LEVEL_HIGH);  /* AINx=1 */
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain1_pin, BSP_IO_LEVEL_HIGH);  /* AINx=1 */
        break;

    case MOTOR_DIR_COAST:
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain0_pin, BSP_IO_LEVEL_LOW);   /* AINx=0 */
        R_IOPORT_PinWrite(&g_ioport_ctrl, p_motor->ain1_pin, BSP_IO_LEVEL_LOW);   /* AINx=0 */
        break;
    }
}

/* ====== 公开 API ====== */

void Motor_Init(motor_t *p_motor)
{
    fsp_err_t err;

    /* 1. 打开 GPT PWM 通道 */
    err = R_GPT_Open(p_motor->p_timer_ctrl, p_motor->p_timer_cfg);
    if (FSP_SUCCESS != err) return;

    /* 2. 启动 GPT 计数 */
    err = R_GPT_Start(p_motor->p_timer_ctrl);
    if (FSP_SUCCESS != err) return;

    /* 3. PWM 占空比初始化为 0 */
    GPT_PWM_SetDuty(p_motor->p_timer_ctrl, p_motor->pwm_pin, 0);

    /* 4. 方向引脚设为刹车 (最安全的状态) */
    ApplyDirection(p_motor, MOTOR_DIR_BRAKE);
    p_motor->direction = MOTOR_DIR_BRAKE;

    p_motor->opened = true;
}

void Motor_SetDirection(motor_t *p_motor, motor_dir_t dir)
{
    if (!p_motor->opened) return;

    /*
     * 切换方向前先把 PWM 降到 0, 避免 H 桥换向时的大电流冲击。
     * 注意: 如果调用方要同时改方向+速度, 应使用 Motor_SetDirSpeed()
     *       以避免这里把速度清零后又重新设置的两步延迟。
     */
    GPT_PWM_SetDuty(p_motor->p_timer_ctrl, p_motor->pwm_pin, 0);

    ApplyDirection(p_motor, dir);
    p_motor->direction = dir;
}

void Motor_SetSpeed(motor_t *p_motor, uint16_t duty)
{
    if (!p_motor->opened) return;
    GPT_PWM_SetDuty(p_motor->p_timer_ctrl, p_motor->pwm_pin, duty);
}

void Motor_SetDirSpeed(motor_t *p_motor, motor_dir_t dir, uint16_t duty)
{
    if (!p_motor->opened) return;

    /*
     * 原子操作: 先将 PWM 归零 → 切换方向 → 设定新占空比。
     * 三个步骤在微秒级完成, 对电机运动几乎无感。
     */
    GPT_PWM_SetDuty(p_motor->p_timer_ctrl, p_motor->pwm_pin, 0);
    ApplyDirection(p_motor, dir);
    p_motor->direction = dir;
    GPT_PWM_SetDuty(p_motor->p_timer_ctrl, p_motor->pwm_pin, duty);
}

void Motor_Stop(motor_t *p_motor, motor_dir_t stop_mode)
{
    if (!p_motor->opened) return;

    /* 先断 PWM, 再切方向到刹车/滑行 */
    GPT_PWM_SetDuty(p_motor->p_timer_ctrl, p_motor->pwm_pin, 0);
    ApplyDirection(p_motor, stop_mode);
    p_motor->direction = stop_mode;
}

motor_dir_t Motor_GetDirection(motor_t *p_motor)
{
    return p_motor->direction;
}

const char *Motor_GetDirName(motor_t *p_motor)
{
    switch (p_motor->direction)
    {
    case MOTOR_DIR_FORWARD: return "FWD";
    case MOTOR_DIR_REVERSE: return "REV";
    case MOTOR_DIR_BRAKE:   return "BRAKE";
    case MOTOR_DIR_COAST:   return "COAST";
    default:                return "???";
    }
}
