/* pid_study.h - PID控制器 */

#ifndef INC_PID_STUDY_H_
#define INC_PID_STUDY_H_

typedef struct{

	float Kp;	// 比例增益
	float Ki;	// 积分增益
	float Kd;	// 微分增益

}	PID_KpidGain;


// PID状态结构体
typedef struct{

	float target;			// 设定目标
	float actual;			// 实际值
	float error;			// 当前误差
	float prev_error;		// 上次误差
	float integral;		// 积分项
	float derivative;		// 微分项
	float output;			// PID输出
	float time_delta;		// 时间间隔
	float output_max;		// 输出上限
	float output_min;		// 输出下限

	// 低通滤波与变速积分变量
	int enable_lpf;			// 低通滤波开关 (1:开启, 0:关闭)
	float lpf_alpha;		// 低通滤波系数
	float last_derivative;	// 上次微分值 (用于滤波)
	float integral_limit;	// 变速积分阈值

}	PID_State;

// 初始化PID结构体
void PID_Init(PID_State *state, float target, float output_max, float output_min, int enable_lpf, float integral_limit);


// 执行PID迭代运算
void PID_Iterate(PID_KpidGain KpidGain, PID_State *state);

#endif /* INC_PID_STUDY_H_ */
