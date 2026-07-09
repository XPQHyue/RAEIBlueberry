/* bsp_key_irq.c - 按键中断驱动 */
#include "bsp_key_irq.h"


void Key_IRQ_Init(void)
{

    fsp_err_t err = FSP_SUCCESS;


    /*打开ICU模块*/
    err  =  R_ICU_ExternalIrqOpen(&g_external_irq29_ctrl, &g_external_irq29_cfg);


    //使能
    err = R_ICU_ExternalIrqEnable(&g_external_irq29_ctrl);
    assert(FSP_SUCCESS == err);

}


volatile bool KEY_MD_press = false;

void key_external_irq29_callback(external_irq_callback_args_t *p_args)
{

    if( 29 == p_args->channel )
    {
        KEY_MD_press = true;//RA8P1按键USER被按下

    }


}
