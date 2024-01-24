/**
 * @file app.c
 * @author Wu TongXing (Lucas@hiwonder.com)
 * @brief 主应用逻辑
 * @version 0.1
 * @date 2023-05-08
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "cmsis_os2.h"
#include "lwmem_porting.h"
#include "global.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "global.h"
#include "adc.h"
#include "u8g2_porting.h"

#include <stdio.h>
#include <string.h>

/* 云台舵机限位参数 */
#define  HOLDER_MIN   500  	//对应舵机的0°
#define  HOLDER_MAX  2500	//对应舵机的180°

/* 蜂鸣器	的频率 */
#define BUZZER_FREQ				 1300

/* 硬件初始化声明 */
void buzzers_init(void);    //蜂鸣器
void motors_init(void);     //电机
void pwm_servos_init(void); //舵机
void chassis_init(void);    //小车参数初始化

//履带车控制函数
void tankblack_control(char msg);
//机械臂控制函数
void arm_control(char msg);
static float x = 235.0;
static float y = 0.0;
static float z = 234;
static float t = 3.14;

/* 用户入口函数 */
void app_task_entry(void *argument)
{
    /* 声明外部句柄 */
    //蜂鸣器句柄
    extern osTimerId_t buzzer_timerHandle;  
	//电量监控句柄
    extern osTimerId_t battery_check_timerHandle;
	
    //运动控制的队列句柄
    //手柄的信号是在 gampad_handle.c 中USBH_HID_EventCallback()回调函数中压入
    //在用户入口函数中取出控制小车运动
    extern osMessageQueueId_t moving_ctrl_queueHandle;  

    /* 硬件初始化 */
    motors_init();      //电机初始化
	pwm_servos_init();  //云台舵机初始化
    buzzers_init();     //蜂鸣器初始化
    //开启蜂鸣器定时器，让其在中断中运作，后面调用接口函数即可
    //参数1：定时器句柄 ， 参数2：定时器的工作间隔 ms
    osTimerStart(buzzer_timerHandle, BUZZER_TASK_PERIOD);   
	//开启电量监控定时器，实时监控电量
    osTimerStart(battery_check_timerHandle, BATTERY_TASK_PERIOD);

	
	HAL_ADC_Start(&hadc1);
	
    char msg = '\0';
    uint8_t msg_prio;
    //初始化 运动控制的队列（参数：队列句柄）
    osMessageQueueReset(moving_ctrl_queueHandle);   

    //初始化底盘电机运动参数
    chassis_init();     

    //选择底盘类型
    // ! ! !
    // 注意：履带车底盘 使用 CHASSIS_TYPE_TANKBLACK  ! ! ! 
    // ! ! !
    set_chassis_type(CHASSIS_TYPE_TANKBLACK);   

	// 循环  : RTOS任务中的循环，必须要有osDelay或者其他系统阻塞函数，否则会导致系统异常
    for(;;) {
        
        //接收 运动控制队列 中的信息，若获取超100ms，则视为不成功，并使电机停止，跳过 这次循环
        //osMessageQueueGet() 取出队列中的消息
        // 参数1 : 消息队列句柄
        // 参数2 : 待放入对象的地址 （这里为char类型地址）
        // 参数3 : 消息优先级
        // 参数4 : 超时设定（可设置等待获取时间）
        if(osMessageQueueGet(moving_ctrl_queueHandle, &msg, &msg_prio, 100) != osOK) {
            chassis->stop(chassis);
            continue;
        }

        printf("msg: %c\r\n", msg); //debug打印

		tankblack_control(msg);     //底盘控制函数
		arm_control(msg);

    }
}

/* 
*  底盘控制函数
*  参数：控制命令
*  该函数通过接收char类型的参数，来判断运行哪一个动作
*/
void tankblack_control(char msg)
{
    //定义电机运动速度
    //建议范围为 [50 , 450]
    static float speed = 300.0f;    

    //定义云台舵机的运动角度
    //舵机的初始角度值为1500（即90°），范围为[500,2500]（即[ 0°,180° ]） 宏定义为 [HOLDER_MIN , HOLDER_MAX]
		static int16_t angle_1 = 1500 , angle_2 = 1500; 

    switch(msg) {

		case 'S': {		//START
            //提示连接成功 ,蜂鸣器响一次
            //buzzer_didi() 蜂鸣器接口
            //参数1 ：自身
            //参数2 ：蜂鸣器频率 （可调节蜂鸣器的音色）s
            //参数3 ；响的时间 ms
            //参数4 ：不响的时间 ms
            //参数5 ：响的次数
            buzzer_didi(buzzers[0] , BUZZER_FREQ , 150 , 200 ,1);
            break;
		}
		
		/* 底盘电机 */
        case 'I': {	//左摇杆 中
			//电机停止
            chassis->stop(chassis);
            break;
        }
        case 'A': { //左摇杆 上
            //以x轴 speed 的线速度 运动（即向前运动）
            //set_velocity()  
            //参数1：自身 ，参数2：x轴线速度 ， 参数3：y轴线速度 ， 参数4：小车的角速度
            chassis->set_velocity(chassis, speed, 0, 0);
						//HAL_UART_Transmit(&huart4, (uint8_t*)"x", 1,HAL_MAX_DELAY);
            break;
        }
        case 'B': { //左摇杆 左上
			//以x轴 speed 的线速度，绕自身左边的半径300mm圆做转圈运动
            //set_velocity_radius() 旋转函数
            //参数1 ：自身
            //参数2 ：x速度
            //参数3 ：半径 mm [左正右负]
            //参数4 ：是否做自转运动
            chassis->set_velocity_radius(chassis, speed, 300, false);
            break;
        }
        case 'C': { //左摇杆 左
            //以 speed 线速度进行左旋
            chassis->set_velocity_radius(chassis, speed, 150, true);
            break;
        }
        case 'D': { //左摇杆 左下
            //以x轴 speed 的线速度，绕自身左边的半径300mm圆做 反向转圈运动
            chassis->set_velocity_radius(chassis, -speed, 300, false);
            break;
        }
        case 'E': { //左摇杆 下
			//以x轴 -speed 的线速度 运动（即向后运动）
            chassis->set_velocity(chassis, -speed, 0, 0);
						//HAL_UART_Transmit_IT(&huart4, (uint8_t*)"x", 1);
            break;
        }
        case 'F': { //左摇杆 右下
            //以x轴 speed 的线速度，绕自身右边的半径300mm圆做 反向转圈运动
            chassis->set_velocity_radius(chassis, -speed, -300, false);
            break;
        }
        case 'G': { //左摇杆 右
            //以 speed 线速度进行右旋
            chassis->set_velocity_radius(chassis, speed, -150, true);
            break;
        }
        case 'H': { //左摇杆 右上
            //以x轴 speed 的线速度，绕自身右边的半径300mm圆做 转圈运动
            chassis->set_velocity_radius(chassis, speed, -300, false);
            break;
        }
		
		/* 右按键 */
		case 'j': { //三角形
            //加速度
            speed += 50;
            speed = speed > 450 ? 450 : speed;
            break;
		}
		case 'l': { //正方形
			break;
		}
        case 'n': { //叉
            //减速度
            speed -= 50;
            speed = speed < 50 ? 50 : speed;
            break;
		}
		case 'p': { //圆
			break;
		}
		
		/* 云台舵机 */
		case 'R': { //右摇杆 中
            break;
        }
		case 'J': { //右摇杆 上
            //将云台上舵机 原角度+50
			angle_1 += 50;
            //判断是否溢出
			angle_1 = (angle_1 >= HOLDER_MAX) ? HOLDER_MAX : angle_1;

            //设置舵机的角度值和转动时间
            //pwm_servo_set_position()  设置舵机的角度和转动的时间
            //参数1 ：舵机对象 
            //参数2 ：指定角度 （角度值范围为[500 , 2500]）
            //参数3 ：旋转的时间
			pwm_servo_set_position(pwm_servos[0] , angle_1 , 120);
            break;
        }
		case 'K': { //右摇杆 左上
            break;
		}
		case 'L': { //右摇杆 左
            //将云台下舵机 原角度+50
			angle_2 += 50;
			angle_2 = (angle_2 >= HOLDER_MAX) ? HOLDER_MAX : angle_2;
			pwm_servo_set_position(pwm_servos[3] , angle_2 , 120);
            break;
        }
		case 'M': { //右摇杆 左下
            break;
        }
		case 'N': { //右摇杆 下
            //将云台上舵机 原角度-50
			angle_1 -= 50;
			angle_1 = (angle_1 <= HOLDER_MIN) ? HOLDER_MIN : angle_1;
			pwm_servo_set_position(pwm_servos[0] , angle_1 , 120);
            break;
        }
		case 'O': { //右摇杆 右下
            break;
        }
		case 'P': { //右摇杆 右
            //将云台下舵机 原角度-50
			angle_2 -= 50;
			angle_2 = (angle_2 <= HOLDER_MIN) ? HOLDER_MIN : angle_2;
			pwm_servo_set_position(pwm_servos[3] , angle_2 , 120);
            break;
        }
		case 'Q': { //右摇杆 右上
            break;
        }
		case 'f': { // R3（右摇杆按下）
            //计算 上舵机与复位角度的差值
            uint16_t run_time = angle_1 > 1500 ? (angle_1 - 1500) : (1500 - angle_1);
            //计算旋转的时间
			run_time >>= 1;		//运动的时间值设置为角度差值的1/2
			angle_1 = 1500;
            //设置舵机转动参数
			pwm_servo_set_position(pwm_servos[0] , angle_1 , run_time);

            //下舵机，重复以上流程
			run_time = angle_2 > 1500 ? (angle_2 - 1500) : (1500 - angle_2);
			run_time >>= 1;	
			angle_2 = 1500;
			pwm_servo_set_position(pwm_servos[3] , angle_2 , run_time);
		}
        default:
            break;
    }
}
void arm_control(char msg)
{
	char json[512];
    // 创建JSON格式的字符串
    // 注意：%.2f 限制浮点数到小数点后两位，根据需要调整
	switch(msg) 
	{
		case 'l': 
		{ //正方形 z+
			z += 5.0;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
			//HAL_UART_Transmit_IT(&huart4, (uint8_t*)"{\"T\":105}", strlen("{\"T\":105}"));
			break;
		}
		case 'p': 
		{ //圆 z-
			z -= 5.0;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}\n", x, y, z, t);
			//sprintf(json, "{\"T\":1041}");
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
			break;
		}
		case 'R': 
		{ //右摇杆 中 
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
            break;
    }
		case 'J': 
		{ //右摇杆 上 y+
			y += 5.0;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
            break;
    }
		case 'L': 
		{ //右摇杆 左 x-
			x -= 5.0;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
					HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
            break;
    }
		case 'N': 
		{ //右摇杆 下 y-
			y -= 5.0;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
            break;
    }
		case 'P':
		{ //右摇杆 右 x+
			x += 5.0;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
            break;
    }
		case 'f': 
		{ // R3（右摇杆按下）
			float x = 235.0;
			float y = 0.0;
			float z = 234;
			float t = 3.14;
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
		}
    default:
		{
			sprintf(json, "{\"T\":1041,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"t\":%.2f}", x, y, z, t);
			HAL_UART_Transmit_IT(&huart4, (uint8_t*)json, strlen(json));
			break;
		}  
	}
}
