#ifndef __GLOBAL_H_
#define __GLOBAL_H_

#include "global_conf.h"

#include "SEGGER_RTT.h"
#include "main.h"

#include "stm32f4xx_hal.h"

#include "i2c.h"
#include "usart.h"
#include "spi.h"
#include "dma.h"
#include "tim.h"
#include "lwrb.h"
#include "lwmem.h"

#include "button.h"
#include "led.h"
#include "buzzer.h"
#include "display.h"
#include "display_st7735.h"
#include "encoder_motor.h"
#include "pid.h"
#include "log.h"
#include "pwm_servo.h"
#include "imu.h"
#include "packet.h"
#include "usbh_hid_gamepad.h"
#include "object.h"
#include "chassis.h"

// 全系统全局变量
extern struct PacketController packet_controller;
extern ButtonObjectTypeDef *buttons[2];
extern BuzzerObjectTypeDef *buzzers[1];
extern LEDObjectTypeDef *leds[1];
extern DisplayObjectTypeDef *lcd;
extern IMU_ObjectTypeDef *imus[1];
extern PWMServoObjectTypeDef *pwm_servos[4];
extern EncoderMotorObjectTypeDef *motors[4];
extern ChassisTypeDef *chassis;


typedef struct{
	ObjectTypeIDEnum type_id;
	HID_GAMEPAD_Info_TypeDef status;
}GamepadStatusObjectTypeDef;


void set_chassis_type(uint8_t chassis_type);


#endif

