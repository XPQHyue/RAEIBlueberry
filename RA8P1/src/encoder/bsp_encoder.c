/**
 * @file    bsp_encoder.c
 * @brief   AB相正交编码器驱动实现 — 基于 encoder_t 实例
 *
 * 设计思想:
 *
 *   ① 硬件卸载 — GPT Event Counting 自动完成正交解码 + 方向判决 + 四倍频计数
 *      CPU 只需周期调用 R_GPT_StatusGet() 读 counter 寄存器
 *
 *   ② 差分读取 + 符号编码 — 软件差分 (current - previous), 不依赖硬件清零;
 *      (int32_t) 将32位无符号差值转为有符号, 增量大小和方向打包在一个 int32_t 里
 *
 *   ③ 四倍频提升分辨率 — 8种 AB 相组合条件全勾 = 硬件四倍频
 *
 *   ④ 补码溢出无需显式处理 — RA8P1 GPT 是32位计数器, 两次读取间增量远小于 2^31,
 *      (int32_t) 转换在正常采样周期下不影响正确性
 *
 *   ⑤ 统一接口隔离实现 — 上层只需调用 Encoder_Read(&enc) 获取增量
 *
 * FSP 硬件配置 (e2 studio Stacks 标签页):
 *   Encoder A: g_timer2, Mode=Periodic, Period=0x100000000, Unit=Raw Counts
 *   Count Up Source:   GTIOCA↑B0 | GTIOCA↓B1 | GTIOCB↑A1 | GTIOCB↓A0
 *   Count Down Source: GTIOCA↑B1 | GTIOCA↓B0 | GTIOCB↑A0 | GTIOCB↓A1
 *   引脚: GTIOC2A=PD06, GTIOC2B=P712
 */

#include "bsp_encoder.h"

/* ====== 全局编码器实例 ====== */

encoder_t g_encoderA = {
    .p_timer_ctrl = &g_timer2_ctrl,
    .p_timer_cfg  = &g_timer2_cfg,
    .prev_counter = 0,
    .direction    = 0,
    .opened       = false,
};

encoder_t g_encoderB = {
    .p_timer_ctrl = &g_timer3_ctrl,
    .p_timer_cfg  = &g_timer3_cfg,
    .prev_counter = 0,
    .direction    = 0,
    .opened       = false,
};

/* ====== 实现 ====== */

void Encoder_Init(encoder_t *p_enc)
{
    fsp_err_t err;

    /* 打开 GPT — 配置结构体在 hal_data.c 中由 FSP 工具生成 */
    err = R_GPT_Open(p_enc->p_timer_ctrl, p_enc->p_timer_cfg);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    /* 清零计数器 (确保从 0 开始计数) */
    R_GPT_CounterSet(p_enc->p_timer_ctrl, 0);

    /* 启动事件计数 — 此后 GTIOCxA/GTIOCxB 上的正交边沿自动驱动 CNT 增减 */
    err = R_GPT_Start(p_enc->p_timer_ctrl);
    if (FSP_SUCCESS != err)
    {
        return;
    }

    p_enc->opened = true;
}

int32_t Encoder_Read(encoder_t *p_enc)
{
    if (!p_enc->opened)
    {
        return 0;
    }

    timer_status_t status;

    /* 读取当前计数器值 */
    fsp_err_t err = R_GPT_StatusGet(p_enc->p_timer_ctrl, &status);
    if (FSP_SUCCESS != err)
    {
        return 0;
    }

    /*
     * 软件差分: delta = current - previous
     *
     * 32 位无符号减法自动处理所有情况:
     *
     *   正常正向:  prev=100,    curr=105    → 105-100=5      → (int32_t)=+5 正转
     *   正常反向:  prev=100,    curr=95     → 0xFFFFFFFB     → (int32_t)=-5 反转
     *   32位回绕:  prev=0xFFFFFFF0, curr=0x0A → 0x0000001A → (int32_t)=26 ✓
     *
     *   前提: 两次读数间增量绝对值 < 2^31 (2147483647)。
     *   以 13 线 × 4 倍频 = 52 CPR, 10ms 采样周期:
     *     最大可测转速 ≈ 2.47×10^8 RPM, 远超过任何实际电机, 恒成立。
     */
    uint32_t current = status.counter;
    int32_t  delta   = (int32_t)(current - p_enc->prev_counter);
    p_enc->prev_counter = current;

    /* 应用编码器极性 (AB相序反接时翻转符号, 确保正转→正增量) */
    delta *= (int32_t)ENCODER_POLARITY;

    /* 死区滤波: |delta| < ENCODER_DEADBAND → 视为信号线噪声, 返回 0 */
    if (delta > -(int32_t)ENCODER_DEADBAND && delta < (int32_t)ENCODER_DEADBAND)
    {
        p_enc->direction = 0;  /* 无有效增量 → 方向归零, 显示 STOP */
        return 0;
    }

    /* 记录方向: delta 的符号即方向 */
    p_enc->direction = (delta > 0) ? 1 : -1;

    return delta;
}

float Encoder_SpeedCalcRPM(int32_t delta_pulses, uint32_t interval_ms)
{
    float dp    = (float)delta_pulses;
    float cpr   = (float)ENCODER_CPR;
    float dt_ms = (float)interval_ms;

    return (dp / cpr) * (60000.0f / dt_ms);
}

const char *Encoder_GetDirection(encoder_t *p_enc)
{
    if (!p_enc->opened)
    {
        return "STOP";
    }

    if (p_enc->direction > 0)
    {
        return "UP";
    }
    else if (p_enc->direction < 0)
    {
        return "DOWN";
    }
    else
    {
        return "STOP";
    }
}

uint32_t Encoder_GetRawCount(encoder_t *p_enc)
{
    if (!p_enc->opened)
    {
        return 0;
    }

    timer_status_t status;
    fsp_err_t err = R_GPT_StatusGet(p_enc->p_timer_ctrl, &status);
    if (FSP_SUCCESS != err)
    {
        return 0;
    }

    /* 不清零, 返回绝对计数值 */
    return status.counter;
}
