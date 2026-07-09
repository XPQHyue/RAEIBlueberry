/**
 * @file    bsp_cmd.c
 * @brief   串口命令解析 — 纯闭环: T=目标RPM, B/C=停机
 */

#include "bsp_cmd.h"
#include "hal_data.h"
#include <stdio.h>
#include <string.h>

#include "debug_uart/bsp_debug_uart.h"
#include "motor/bsp_motor.h"

/* ---- 命令缓冲 ---- */
#define CMD_BUF_SIZE  16
static char    cmd_buf[CMD_BUF_SIZE];
static uint8_t cmd_idx = 0;

/* ---- 闭环状态 ---- */
static volatile bool      s_closed_loop = false;
static volatile float     s_tag_rpm     = 0.00f;
static volatile motor_dir_t s_stop_mode = MOTOR_DIR_BRAKE;  /* B→BRAKE / C→COAST */

/* ---- 内部声明 ---- */
static void ParseTargetCmd(void);
static void ParseCmd(void);

/* ====== 公开 API ====== */

void Cmd_Init(void) {}

void Cmd_Poll(void)
{
    while (UART_RxAvailable())
    {
        char ch = UART_GetChar();

        if (ch == '\r' || ch == '\n')
        {
            if (cmd_idx > 0)
            {
                cmd_buf[cmd_idx] = '\0';
                ParseCmd();
                cmd_idx = 0;
            }
            continue;
        }

        if (cmd_idx < CMD_BUF_SIZE - 1)
            cmd_buf[cmd_idx++] = ch;
    }
}

float Cmd_GetTargetRPM(void)
{
    return s_tag_rpm;
}

bool Cmd_IsClosedLoop(void)
{
    return s_closed_loop;
}

motor_dir_t Cmd_GetStopMode(void)
{
    return s_stop_mode;
}

/* ====== 内部实现 ====== */

/**
 * @brief Txxxx  / T-xxxx  → 闭环目标 RPM
 *        T300   → 正转 300 RPM
 *        T-200  → 反转 200 RPM
 *        T0     → 停机 + 退出闭环
 */
static void ParseTargetCmd(void)
{
    if (cmd_idx < 2) return;

    bool    negative = false;
    uint8_t start    = 1;

    if (cmd_buf[1] == '-')
    {
        negative = true;
        start = 2;
        if (cmd_idx < 3) return;
    }

    uint16_t val = 0;
    for (uint8_t i = start; i < cmd_idx; i++)
    {
        char c = cmd_buf[i];
        if (c < '0' || c > '9') return;
        val = val * 10 + (uint16_t)(c - '0');
        if (val > 9999) return;
    }

    if (val == 0)
    {
        s_closed_loop = false;
        s_tag_rpm = 0.00f;
        s_stop_mode = MOTOR_DIR_BRAKE;
        printf("STOP (BRAKE)\r\n");
        return;
    }

    s_tag_rpm     = negative ? -(float)val : (float)val;
    s_closed_loop = true;
    printf("TARGET %.0f RPM\r\n", (double)s_tag_rpm);
}

/**
 * @brief 命令路由
 */
static void ParseCmd(void)
{
    if (cmd_idx == 0) return;

    char first = cmd_buf[0];

    /* 单字符命令 */
    if (cmd_idx == 1)
    {
        if (first == 'B' || first == 'b')
        {
            s_closed_loop = false;
            s_tag_rpm = 0.00f;
            s_stop_mode = MOTOR_DIR_BRAKE;
            printf("STOP (BRAKE)\r\n");
            return;
        }
        if (first == 'C' || first == 'c')
        {
            s_closed_loop = false;
            s_tag_rpm = 0.00f;
            s_stop_mode = MOTOR_DIR_COAST;
            printf("STOP (COAST)\r\n");
            return;
        }
    }

    /* 闭环目标 RPM */
    if (first == 'T' || first == 't')
    {
        ParseTargetCmd();
        return;
    }

    printf("ERR: unknown cmd '%c'\r\n", first);
}
