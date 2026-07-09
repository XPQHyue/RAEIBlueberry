/**
 * @file    bsp_motor.h
 * @brief   电机控制模块 — 基于实例的参数化设计, 支持 N 个电机
 *
 * 每个电机由一个 motor_t 实例表示, 携带其硬件配置 (GPT 通道 + 方向引脚)
 * 和运行时状态。所有 API 接受 motor_t* 指针, 同一套代码驱动所有实例。
 *
 * H 桥真值表 (TB6612 / L298 / DRV8833 等通用逻辑):
 *   AINx=0, AINx=1 → 正转 (Forward)
 *   AINx=1, AINx=0 → 反转 (Reverse)
 *   AINx=1, AINx=1 → 刹车 (Brake, 电机两端短接)
 *   AINx=0, AINx=0 → 滑行 (Coast, 电机两端悬空)
 *
 * 当前硬件:
 *   Motor A: GPT5 GTIOC5B (P914) + AIN0(P308) + AIN1(P512)
 *   Motor B: GPT1 GTIOC1A (P105) + BIN0(P312) + BIN1(P511)
 */

#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H

#include "hal_data.h"
#include <stdint.h>
#include <stdbool.h>

/* ====== 方向枚举 (所有实例共用) ====== */

typedef enum {
    MOTOR_DIR_FORWARD,          /**< 正转: AINx=0, AINx=1 */
    MOTOR_DIR_REVERSE,          /**< 反转: AINx=1, AINx=0 */
    MOTOR_DIR_BRAKE,            /**< 刹车: AINx=1, AINx=1 (电机两端短接制动) */
    MOTOR_DIR_COAST,            /**< 滑行: AINx=0, AINx=0 (电机两端悬空) */
} motor_dir_t;

/* ====== 电机实例结构体 ====== */

typedef struct {
    /* ---- 硬件配置 (Init 时注入, 此后只读) ---- */
    gpt_instance_ctrl_t  *p_timer_ctrl;   /**< GPT 控制块 (g_timer5_ctrl / g_timer1_ctrl) */
    timer_cfg_t const    *p_timer_cfg;    /**< GPT 配置   (g_timer5_cfg  / g_timer1_cfg)  */
    gpt_io_pin_t          pwm_pin;        /**< PWM 输出引脚 (GTIOCA 或 GTIOCB)          */
    bsp_io_port_pin_t     ain0_pin;       /**< H 桥方向引脚 0                           */
    bsp_io_port_pin_t     ain1_pin;       /**< H 桥方向引脚 1                           */

    /* ---- 运行时状态 (由 API 维护) ---- */
    motor_dir_t           direction;      /**< 当前方向                                  */
    bool                  opened;         /**< 是否已初始化                              */
} motor_t;

/* ====== 全局实例声明 ====== */

extern motor_t g_motorA;  /**< Motor A: GPT5 GTIOCB + AIN0/AIN1 */
extern motor_t g_motorB;  /**< Motor B: GPT1 GTIOCA + BIN0/BIN1 */

/* ====== API ====== */

/**
 * @brief 电机初始化 — Open GPT 并启动 PWM, 默认状态: 刹车 + PWM=0%
 * @param p_motor  电机实例指针
 */
void Motor_Init(motor_t *p_motor);

/**
 * @brief 设置电机旋转方向 (不改变 PWM 占空比)
 * @param p_motor  电机实例指针
 * @param dir      方向: FORWARD / REVERSE / BRAKE / COAST
 */
void Motor_SetDirection(motor_t *p_motor, motor_dir_t dir);

/**
 * @brief 设置 PWM 占空比 (不改变方向)
 * @param p_motor  电机实例指针
 * @param duty     占空比 0~1000 (0.0%~100.0%), 超出自动限幅
 */
void Motor_SetSpeed(motor_t *p_motor, uint16_t duty);

/**
 * @brief 同时设置方向和速度 (原子操作, 避免中间状态)
 * @param p_motor  电机实例指针
 * @param dir      方向
 * @param duty     占空比 0~1000
 */
void Motor_SetDirSpeed(motor_t *p_motor, motor_dir_t dir, uint16_t duty);

/**
 * @brief 停止电机
 * @param p_motor   电机实例指针
 * @param stop_mode BRAKE (刹车制动) 或 COAST (惯性滑行)
 */
void Motor_Stop(motor_t *p_motor, motor_dir_t stop_mode);

/**
 * @brief 获取当前方向
 * @param p_motor  电机实例指针
 * @return 当前设置的方向枚举值
 */
motor_dir_t Motor_GetDirection(motor_t *p_motor);

/**
 * @brief 获取方向名称 (调试/日志用)
 * @param p_motor  电机实例指针
 * @return "FWD" / "REV" / "BRAKE" / "COAST"
 */
const char *Motor_GetDirName(motor_t *p_motor);

#endif /* __BSP_MOTOR_H */
