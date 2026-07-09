/**
 * @file    bsp_encoder.h
 * @brief   AB相正交编码器驱动 — 基于 encoder_t 实例的硬件四倍频解码
 *
 * 硬件基础:   GPT Event Counting 模式, GTIOCxA/GTIOCxB
 * 核心机制:   8 种 AB 相边沿组合 → count_up/count_down → CNT 自动增减
 *
 * 当前硬件:
 *   Encoder A: GPT2, GTIOC2A=PD06, GTIOC2B=P712
 *   Encoder B: (预留 GPT3)
 *
 * 使用方式:
 *   1. Encoder_Init(&g_encoderA)            — 一次性初始化
 *   2. Encoder_Read(&g_encoderA)            — 差分读取 (读后自动更新 prev)
 *   3. Encoder_SpeedCalcRPM(delta, ms)      — M法测速, 在固定周期定时器中调用
 */

#ifndef __BSP_ENCODER_H
#define __BSP_ENCODER_H

#include "hal_data.h"
#include <stdint.h>
#include <stdbool.h>

/* ====== 编码器物理参数 (当前硬件下所有编码器型号相同) ====== */

#define ENCODER_PPR         13U     /**< 编码器物理线数 (每圈脉冲数) */
#define ENCODER_GEAR_RATIO  30U     /**< 电机减速比 (G513P30: 30:1) */
#define ENCODER_MULTIPLIER  4U      /**< 硬件四倍频系数 (固定) */
#define ENCODER_DEADBAND    5U      /**< 死区阈值 (counts/次), |delta|小于此值视为噪声返回0 */
#define ENCODER_POLARITY    (-1)    /**< 编码器极性: 1=正常, -1=AB相序反接时翻转符号 */
                                    /**< ⚠️ 诊断方法: 先用开环正转 → 看 RPM 符号 */
                                    /**<   正 RPM → POLARITY=1; 负 RPM → POLARITY=-1 (正反馈!) */
#define ENCODER_CPR          ((uint32_t)(ENCODER_PPR) * ENCODER_MULTIPLIER * ENCODER_GEAR_RATIO)
                                    /**< 输出轴每圈计数 = 13×4×30 = 1560 */

/* ====== 编码器实例结构体 ====== */

typedef struct {
    /* ---- 硬件配置 (Init 时注入) ---- */
    gpt_instance_ctrl_t  *p_timer_ctrl;   /**< GPT 控制块 (g_timer2_ctrl / ...) */
    timer_cfg_t const    *p_timer_cfg;    /**< GPT 配置                                */

    /* ---- 运行时状态 (由 API 维护) ---- */
    uint32_t              prev_counter;   /**< 上次数值 (软件差分)                    */
    int8_t                direction;      /**< 当前方向: +1=正转, -1=反转, 0=静止      */
    bool                  opened;         /**< 是否已初始化                            */
} encoder_t;

/* ====== 全局实例声明 ====== */

extern encoder_t g_encoderA;  /**< Encoder A: GPT2, PD06/P712 */
extern encoder_t g_encoderB;  /**< Encoder B: GPT3, P911/P912 */

/* ====== API ====== */

/**
 * @brief 编码器初始化 — Open GPT 并启动事件计数
 * @param p_enc  编码器实例指针
 * @note  调用前确保 FSP 已配置好 g_timer2 (count_up_source / count_down_source)
 */
void Encoder_Init(encoder_t *p_enc);

/**
 * @brief 差分读取编码器脉冲增量
 * @param p_enc  编码器实例指针
 * @return 有符号脉冲增量 (正=正转, 负=反转)
 * @note  32位补码: 正方向最大 +2147483647, 反向最大 -2147483648
 *        两次调用之间增量绝对值不应超过 2^31-1 (在合理采样周期下恒成立)
 */
int32_t Encoder_Read(encoder_t *p_enc);

/**
 * @brief M法计算转速 (静态工具函数, 不访问硬件)
 * @param delta_pulses  在 interval_ms 毫秒内累计的脉冲增量
 * @param interval_ms   累计时间间隔 (毫秒), 由调用方显式指定
 * @return 转速 RPM (浮点), 正=正转, 负=反转
 * @note  RPM = delta_pulses / CPR × (60000 / interval_ms)
 */
float Encoder_SpeedCalcRPM(int32_t delta_pulses, uint32_t interval_ms);

/**
 * @brief 获取当前计数方向 (调试用)
 * @param p_enc  编码器实例指针
 * @return "UP" / "DOWN" / "STOP"
 */
const char *Encoder_GetDirection(encoder_t *p_enc);

/**
 * @brief 获取编码器原始计数值 (不清零, 调试用)
 * @param p_enc  编码器实例指针
 * @return 32位无符号原始计数器值
 */
uint32_t Encoder_GetRawCount(encoder_t *p_enc);

#endif /* __BSP_ENCODER_H */
