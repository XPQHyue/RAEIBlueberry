/*
 * hal_entry.c — PID 闭环速度控制 (定时器中断驱动)
 *
 * 架构:
 *   g_timer4 ISR (5ms / 200Hz) → 编码器 + PID + 电机更新
 *   主循环 (~10ms)            → 串口命令 + 按键 + 上报
 *
 * 串口命令:
 *   Txxxx   正转目标 RPM  (T300 = 300 RPM)
 *   T-xxxx  反转目标 RPM  (T-200 = 反转 200 RPM)
 *   B       刹车 + 退出闭环
 *   C       滑行 + 退出闭环
 */

#include "hal_data.h"
#include "bsp_head.h"


/* ---- PID 参数 (volatile 方便调试时用调试器修改) ---- */
PID_KpidGain MotorA_Gain;
PID_State  MotorA_State;
volatile float MotorA_Kp      = 3.10f;
volatile float MotorA_Ki      = 1.61f;
volatile float MotorA_Kd      = 1.00f;
volatile float MotorA_OutMax  = 1000.0f;
volatile float MotorA_OutMin  = -1000.0f;

PID_KpidGain MotorB_Gain;
PID_State  MotorB_State;
volatile float MotorB_Kp      = 3.10f;
volatile float MotorB_Ki      = 1.61f;
volatile float MotorB_Kd      = 1.00f;
volatile float MotorB_OutMax  = 1000.0f;
volatile float MotorB_OutMin  = -1000.0f;

#define PWM_DEADBAND   30   /* PID 输出 |out|<30 → 停机 */
#define ISR_PERIOD_MS  5U   /* g_timer4 中断周期 */
#define PID_PERIOD_MS  10U  /* PID 迭代周期 (2×ISR, 保证编码器分辨率) */
#define PID_DECIMATION 2U   /* PID_PERIOD / ISR_PERIOD */

extern volatile bool KEY_MD_press;

/* ---- ISR ↔ 主循环共享数据 (ISR 写入, 主循环读取并清零) ---- */
static volatile int32_t g_enc_accA = 0;
static volatile int32_t g_enc_accB = 0;


void hal_entry(void)
{
    R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);

    Key_IRQ_Init();
    Debug_UART0_Init();
    Motor_Init(&g_motorA);
    Motor_Init(&g_motorB);
    Encoder_Init(&g_encoderA);
    Encoder_Init(&g_encoderB);
    Cmd_Init();

    PID_Init(&MotorA_State, 0, MotorA_OutMax, MotorA_OutMin, 0, 200);
    MotorA_Gain.Kp = MotorA_Kp;
    MotorA_Gain.Ki = MotorA_Ki;
    MotorA_Gain.Kd = MotorA_Kd;

    PID_Init(&MotorB_State, 0, MotorB_OutMax, MotorB_OutMin, 0, 200);
    MotorB_Gain.Kp = MotorB_Kp;
    MotorB_Gain.Ki = MotorB_Ki;
    MotorB_Gain.Kd = MotorB_Kd;

    /* time_delta=1.0 匹配 PID_PERIOD (10ms), 与原 tuning 一致 */
    MotorA_State.time_delta = 1.0f;
    MotorB_State.time_delta = 1.0f;

    printf("UART0 Ready | T=目标RPM B=刹车 C=滑行\r\n");

    /* POEG 急停保护 — 必须在 GPT 之前 Open, 确保 PWM 启动时 POEG 已就绪 */
    fsp_err_t poeg_err = R_POEG_Open(&g_poeg1_ctrl, &g_poeg1_cfg);
    if (FSP_SUCCESS != poeg_err)
    {
        printf("POEG1 Open FAILED (err=%d)\r\n", (int)poeg_err);
    }
    else
    {
        /* 检查 GTETRGB 引脚是否正常 (应为 HIGH: 按钮闭合, 未急停) */
        bsp_io_level_t gt_level;
        R_IOPORT_PinRead(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, &gt_level);
        printf("POEG1 OK | GTETRGB(PA07)=%s\r\n",
               (gt_level == BSP_IO_LEVEL_HIGH) ? "HIGH" : "LOW(!)");
    }

    /* 启动 5ms 定时器中断 — PID 在 ISR 内执行 */
    R_GPT_Open(&g_timer4_ctrl, &g_timer4_cfg);
    R_GPT_Start(&g_timer4_ctrl);

    /* ====== 主循环: 仅串口命令 + 按键 + 上报 ====== */
    uint32_t report_tick = 0;

    while (1)
    {
        /* ---- 按键 (仅开环有效, 闭环时 PID 接管电机 B) ---- */
//        if (KEY_MD_press && !Cmd_IsClosedLoop())
//        {
//            KEY_MD_press = false;
//            static bool b_run = false;
//            b_run = !b_run;
//            if (b_run)
//                Motor_SetDirSpeed(&g_motorB, MOTOR_DIR_FORWARD, 500);
//            else
//                Motor_Stop(&g_motorB, MOTOR_DIR_BRAKE);
//        }

        /* ---- 串口命令轮询 ---- */
        Cmd_Poll();

        /* ---- 每秒上报 (主循环 ~10ms × 100 = ~1s) ---- */
        report_tick++;
        if (report_tick >= 100)
        {
            report_tick = 0;

            /* 原子读取 ISR 累加的编码器增量 */
            int32_t accA = g_enc_accA;
            int32_t accB = g_enc_accB;
            g_enc_accA = 0;
            g_enc_accB = 0;

            float rpmA = Encoder_SpeedCalcRPM(accA, 1000);
            float rpmB = Encoder_SpeedCalcRPM(accB, 1000);

            printf("%.2f,%.2f\r\n", (double)rpmA, (double)rpmB);
        }

        R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
    }
}


/**
 * @brief 5ms 定时器中断回调 — 编码器读取 + PID 闭环 + 电机更新
 *
 * 精确等间隔执行, 消除主循环 SoftwareDelay 的时序抖动。
 * 所有 PID 状态和方向缓存均为 static, 仅在本 ISR 内访问, 无线程竞争。
 */
void g_timer4_callback(timer_callback_args_t *p_args)
{
    if (TIMER_EVENT_CYCLE_END != p_args->event)
        return;

    /* ========== LED 心跳 (保留原有调试功能) ========== */
    {
        static uint32_t LedTickCnt = 0;
        bsp_io_level_t  level;

        LedTickCnt++;
        if (LedTickCnt > 200 * 1)
        {
            LedTickCnt = 0;
            R_IOPORT_PinRead(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_10, &level);
            if (level == BSP_IO_LEVEL_HIGH)
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_10, BSP_IO_LEVEL_LOW);
            else
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_10, BSP_IO_LEVEL_HIGH);
        }
    }

    /* ========== 编码器读取 + 累加 ========== */
    int32_t deltaA = Encoder_Read(&g_encoderA);
    int32_t deltaB = Encoder_Read(&g_encoderB);
    g_enc_accA += deltaA;
    g_enc_accB += deltaB;

    /* PID 降采样累加器: 每 2 个 ISR tick (10ms) 迭代一次 PID,
       保证编码器分辨率与原来一致 (52CPR, 10ms → ~115 RPM/count) */
    static int32_t s_pid_deltaA = 0;
    static int32_t s_pid_deltaB = 0;
    static uint8_t s_pid_tick   = 0;

    s_pid_deltaA += deltaA;
    s_pid_deltaB += deltaB;
    s_pid_tick++;

    if (s_pid_tick < PID_DECIMATION)
        return;  /* 本轮不执行 PID, 等下一个 ISR tick */

    /* 以下每 10ms 执行一次 */

    int32_t pid_deltaA = s_pid_deltaA;
    int32_t pid_deltaB = s_pid_deltaB;
    s_pid_deltaA = 0;
    s_pid_deltaB = 0;
    s_pid_tick   = 0;

    /* ========== PID 闭环控制 (10ms 周期) ========== */
    static bool       s_was_closed  = false;
    static float      s_prev_target = 0.0f;
    static motor_dir_t last_dir     = MOTOR_DIR_BRAKE;
    static uint16_t    last_duty    = 0;
    static motor_dir_t last_dirB    = MOTOR_DIR_BRAKE;
    static uint16_t    last_dutyB   = 0;

    bool is_closed = Cmd_IsClosedLoop();

    /* 闭环下降沿: 主循环设 s_closed_loop=false, ISR 独家执行 Motor_Stop */
    if (!is_closed && s_was_closed)
    {
        motor_dir_t mode = Cmd_GetStopMode();
        Motor_Stop(&g_motorA, mode);
        Motor_Stop(&g_motorB, mode);
    }

    /* 闭环上升沿: T0/B/C 后缓存失效, 强制下次走 Motor_SetDirSpeed */
    if (is_closed && !s_was_closed)
    {
        last_dir  = MOTOR_DIR_COAST;
        last_dirB = MOTOR_DIR_COAST;
    }
    s_was_closed = is_closed;

    if (is_closed)
    {
        float target = Cmd_GetTargetRPM();

        /* 目标换向 → 清零积分: 旧方向积分对反向是阻力 */
        if ((s_prev_target > 0.0f && target < 0.0f) ||
            (s_prev_target < 0.0f && target > 0.0f))
        {
            MotorA_State.integral = 0.0f;
            MotorB_State.integral = 0.0f;
        }
        s_prev_target = target;

        /* ========== 电机 A ========== */
        {
            MotorA_State.target = target;
            MotorA_State.actual = Encoder_SpeedCalcRPM(pid_deltaA, PID_PERIOD_MS);
            PID_Iterate(MotorA_Gain, &MotorA_State);

            float      outA  = MotorA_State.output;
            motor_dir_t dirA;
            uint16_t    dutyA;

            if (outA > PWM_DEADBAND)          { dirA = MOTOR_DIR_FORWARD; dutyA = (uint16_t)outA; }
            else if (outA < -PWM_DEADBAND)    { dirA = MOTOR_DIR_REVERSE; dutyA = (uint16_t)(-outA); }
            else                              { dirA = MOTOR_DIR_BRAKE;   dutyA = 0; }

            if (dirA != last_dir)             { Motor_SetDirSpeed(&g_motorA, dirA, dutyA); }
            else if (dutyA != last_duty)      { Motor_SetSpeed(&g_motorA, dutyA); }
            last_dir  = dirA;
            last_duty = dutyA;
        }

        /* ========== 电机 B ========== */
        {
            MotorB_State.target = target;
            MotorB_State.actual = Encoder_SpeedCalcRPM(pid_deltaB, PID_PERIOD_MS);
            PID_Iterate(MotorB_Gain, &MotorB_State);

            float      outB  = MotorB_State.output;
            motor_dir_t dirB;
            uint16_t    dutyB;

            if (outB > PWM_DEADBAND)          { dirB = MOTOR_DIR_FORWARD; dutyB = (uint16_t)outB; }
            else if (outB < -PWM_DEADBAND)    { dirB = MOTOR_DIR_REVERSE; dutyB = (uint16_t)(-outB); }
            else                              { dirB = MOTOR_DIR_BRAKE;   dutyB = 0; }

            if (dirB != last_dirB)            { Motor_SetDirSpeed(&g_motorB, dirB, dutyB); }
            else if (dutyB != last_dutyB)     { Motor_SetSpeed(&g_motorB, dutyB); }
            last_dirB  = dirB;
            last_dutyB = dutyB;
        }
    }
}


/* ========================================================================
 * POEG 急停保护
 * ======================================================================== */

static volatile bool g_poeg_triggered = false;  /* 主循环可查询急停状态 */


/**
 * @brief POEG 关断回调 — 硬件已关断 PWM, 此处仅记录事件
 *
 * 不要在此回调中调用 R_POEG_Reset! 故障未消除时 Reset 会立即再次触发。
 */
void poeg_callback(poeg_callback_args_t *p_args)
{
    (void)p_args;  /* 回调参数保留, 未来扩展故障类型判断 */

    /* 防止重复进入 (必要时可禁用 POEG 中断) */
    if (g_poeg_triggered) return;

    g_poeg_triggered = true;

    printf("\r\n!!! POEG EMERGENCY STOP !!!\r\n");

    /* 关断后设电机方向为刹车 (PWM 已被 POEG 拉低, 此处配合 H 桥刹车) */
    Motor_Stop(&g_motorA, MOTOR_DIR_BRAKE);
    Motor_Stop(&g_motorB, MOTOR_DIR_BRAKE);

    /* 通知命令模块退出闭环 (下次主循环 Cmd_Poll 不再生效) */
    /* s_closed_loop 已在 bsp_cmd.c 中, 这里通过设置目标为 0 间接通知 */
}


/**
 * @brief 软件急停 — 可在任意位置调用, 效果与 GTETRG 硬件触发相同
 */
void Emergency_Stop(void)
{
    R_POEG_OutputDisable(&g_poeg1_ctrl);
    /* poeg_callback 会被自动调用, 无需重复处理 */
}


/**
 * @brief 急停恢复 — 确认故障消除后调用, 恢复 PWM 输出
 * @return true=恢复成功, false=故障仍存在
 */
bool Emergency_Recover(void)
{
    /* 检查 GTETRG 引脚是否已恢复高电平 (按钮已松开) */
    bsp_io_level_t level;
    R_IOPORT_PinRead(&g_ioport_ctrl, BSP_IO_PORT_00_PIN_07, &level);  /* PA07 = GTETRGB */
    if (level != BSP_IO_LEVEL_HIGH)
    {
        return false;  /* 按钮仍按下 / 线路仍断开 */
    }

    /* 复位 POEG → PWM 输出恢复 */
    fsp_err_t err = R_POEG_Reset(&g_poeg1_ctrl);
    if (FSP_SUCCESS != err) return false;

    g_poeg_triggered = false;
    return true;
}


/**
 * @brief 查询是否处于急停状态
 */
bool Emergency_IsTripped(void)
{
    return g_poeg_triggered;
}



// TODO: 舵机控制预留
