/* pid_study.c - PID控制器实现 */

#include "pid_study.h"
#include <math.h>

void PID_Init(PID_State *state, float target, float output_max, float output_min, int enable_lpf, float integral_limit){

	// 将目标值写入结构体
	state->target		= target;
	state->actual		= 0.0f;

	state->error		= 0.0f;
	state->prev_error	= 0.0f;
	state->integral		= 0.0f;
	state->derivative	= 0.0f;
	state->output		= 0.0f;

	state->time_delta	= 1.0f;

	// 设置输出限幅参数
	state->output_max	= output_max;
	state->output_min	= output_min;

    // 初始化低通滤波参数
    state->enable_lpf   = enable_lpf;
    state->lpf_alpha    = 0.7f; // 滤波系数，可根据需要调整或作为参数传入
    state->last_derivative = 0.0f;

    // 设置变速积分阈值
    state->integral_limit = integral_limit;
}


// 执行PID迭代运算
void PID_Iterate(PID_KpidGain KpidGain, PID_State *state){

	state->prev_error = state->error;

	state->error = state->target - state->actual;								//P

    // 变速积分：误差大时减弱积分，小时增强
    float integral_factor = 1.0f;
    if(state->error > state->integral_limit || state->error < -state->integral_limit) {
        integral_factor = 0.0f; // 误差超过阈值，停止积分
    } else {
        // 简单的线性变速积分示例：误差越小，积分作用越强
        // 也可以根据具体需求设计更复杂的曲线
        integral_factor = 1.0f - (fabsf(state->error) / state->integral_limit);
    }

	// 抗积分饱和：输出极限时停止积分，避免积分失控
	if (state->output < state->output_max && state->output > state->output_min)
	{
		state->integral += (state->error * state->time_delta * integral_factor);	// 积分项 (带变速因子)
	}

    float raw_derivative = (state->error - state->prev_error) / state->time_delta;

    // 低通滤波微分项
    if (state->enable_lpf == 1) {
        // 公式: Y(n) = alpha * X(n) + (1 - alpha) * Y(n-1)
        state->derivative = state->lpf_alpha * raw_derivative + (1.0f - state->lpf_alpha) * state->last_derivative;
        state->last_derivative = state->derivative;
    } else {
        state->derivative = raw_derivative;
    }
    
	// time_delta 为时间间隔，常数时可省略，但建议固定以确保精度

	//数学表达式u(t) = Kp*e(t) + Ki*∫e(t)dt + Kd*de(t)/dt)
	state->output =  (KpidGain.Kp * state->error)
								 + (KpidGain.Ki * state->integral)
								 + (KpidGain.Kd * state->derivative);

	// 限幅处理：位置式PID的最终输出限幅
	if(state->output > state->output_max) state->output = state->output_max;
	if(state->output < state->output_min) state->output = state->output_min;

}
