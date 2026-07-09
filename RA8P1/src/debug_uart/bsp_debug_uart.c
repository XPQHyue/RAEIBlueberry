/* bsp_debug_uart.c - UART0 调试串口驱动 */

#include "bsp_debug_uart.h"
#include "key/bsp_key_irq.h"

/* ---- 接收环形缓冲区 ---- */
#define UART_RX_BUF_SIZE  64
static volatile char    rx_buf[UART_RX_BUF_SIZE];
static volatile uint8_t rx_wr = 0;
static volatile uint8_t rx_rd = 0;

/* ---- 发送完成标志 (初始 true: 首次 _write 无需等待) ---- */
volatile bool uart_send_complete_flag = true;

/* ---- 初始化 ---- */
void Debug_UART0_Init(void)
{
    fsp_err_t err = R_SCI_B_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);
    assert(FSP_SUCCESS == err);
}

/* ---- 中断回调 ---- */
void debug_uart0_callback(uart_callback_args_t *p_args)
{
    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            uint8_t next = rx_wr + 1U;
            if (next >= UART_RX_BUF_SIZE) next = 0;
            if (next != rx_rd)
            {
                rx_buf[rx_wr] = (char)(p_args->data);
                rx_wr = next;
            }
            break;
        }

        case UART_EVENT_TX_COMPLETE:
        {
            uart_send_complete_flag = true;
            break;
        }

        default:
            break;
    }
}

/* ---- 接收 ---- */
bool UART_RxAvailable(void)
{
    return (rx_rd != rx_wr);
}

char UART_GetChar(void)
{
    if (rx_rd == rx_wr) return '\0';
    char ch = rx_buf[rx_rd];
    if (++rx_rd >= UART_RX_BUF_SIZE) rx_rd = 0;
    return ch;
}

/* ---- printf 重定向 ---- */
int _write(int fd, char *pBuffer, int size)
{
    (void)fd;

    /* 等上一帧发完再启动新帧 (第一帧无需等待, flag 初始为 false) */
    while (uart_send_complete_flag == false);
    uart_send_complete_flag = false;

    R_SCI_B_UART_Write(&g_uart0_ctrl, (uint8_t *)pBuffer, (uint32_t)size);

    /* 不等当前帧! 硬件在后台中断发送, 主循环继续跑 */
    return size;
}

/* 手动行缓冲: printf 逐字符进来, 攒到 \n 才一次性 _write */
#define STDOUT_BUF_SIZE  128
static char   stdout_buf[STDOUT_BUF_SIZE];
static int    stdout_idx = 0;

static int _stdio_put(char c, FILE *f)
{
    (void)f;

    stdout_buf[stdout_idx++] = c;

    /* 遇到换行或缓冲区满 → 一次性发送 */
    if (c == '\n' || stdout_idx >= STDOUT_BUF_SIZE)
    {
        _write(0, stdout_buf, stdout_idx);
        stdout_idx = 0;
    }

    return (unsigned char)c;
}

static FILE __stdio_stdout = FDEV_SETUP_STREAM(_stdio_put, NULL, NULL, _FDEV_SETUP_WRITE);
static FILE __stdio_stdin  = FDEV_SETUP_STREAM(NULL, NULL, NULL, 0);
static FILE __stdio_stderr = FDEV_SETUP_STREAM(_stdio_put, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdout = &__stdio_stdout;
FILE *const stdin  = &__stdio_stdin;
FILE *const stderr = &__stdio_stderr;
