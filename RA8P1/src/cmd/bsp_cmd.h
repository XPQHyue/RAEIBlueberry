/**
 * @file    bsp_cmd.h
 * @brief   串口命令解析模块
 *
 * 支持命令:
 *   Txxxx / T-xxxx — 闭环目标 RPM
 *   B              — 刹车 + 退出闭环
 *   C              — 滑行 + 退出闭环
 */

#ifndef CMD_BSP_CMD_H_
#define CMD_BSP_CMD_H_

#include <stdint.h>
#include <stdbool.h>
#include "motor/bsp_motor.h"

void Cmd_Init(void);

/**
 * @brief 命令轮询 — 每个主循环周期调用一次
 */
void Cmd_Poll(void);

/**
 * @brief 获取闭环目标转速 (T 命令设定)
 * @return 目标 RPM, 正=正转, 负=反转
 */
float Cmd_GetTargetRPM(void);

/**
 * @brief 是否处于闭环模式
 */
bool Cmd_IsClosedLoop(void);

/**
 * @brief 获取停机模式 (B→BRAKE, C→COAST, T0→BRAKE)
 *
 * 供 ISR 在闭环下降沿调用 Motor_Stop 时使用,
 * 避免主循环与 ISR 同时操作电机硬件。
 */
motor_dir_t Cmd_GetStopMode(void);

#endif /* CMD_BSP_CMD_H_ */
