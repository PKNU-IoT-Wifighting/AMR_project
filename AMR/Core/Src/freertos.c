/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "usart.h"
#include "i2c.h"
#include <stdio.h>
#include <string.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <uxr/client/transport.h>
#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>

// imu,엔코더
#include <sensor_msgs/msg/imu.h>
#include <std_msgs/msg/float32.h>

#include <geometry_msgs/msg/twist.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t microROSTaskHandle;
const osThreadAttr_t microROSTask_attributes = {
  .name = "microROSTask",
  .stack_size = 4096 * 4,   /* 4096 words * 4 bytes = 16384 bytes (16KB) --
                               bumped from 10KB for printf() debug logging headroom */
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartMicroROSTask(void *argument);

bool cubemx_transport_open(struct uxrCustomTransport * transport);
bool cubemx_transport_close(struct uxrCustomTransport * transport);
size_t cubemx_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

void * microros_allocate(size_t size, void * state);
void microros_deallocate(void * pointer, void * state);
void * microros_reallocate(void * pointer, size_t size, void * state);
void * microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void * state);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  microROSTaskHandle = osThreadNew(StartMicroROSTask, NULL, &microROSTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* TIM1 CH1/CH2 = left wheel (fwd/rev), CH3/CH4 = right wheel (fwd/rev)
     dual-PWM H-bridge input scheme: assumed onboard driver wiring, adjust if different */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  /* Encoders: right wheel (PE13/PE14 motor) confirmed paired with TIM5 from
     the working single-motor demo. Left wheel (PE9/PE11 motor) encoder TIM
     is an unverified guess (TIM2) -- confirm by hand-spinning left wheel. */
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);

  /* Start stopped. Actual duty is driven by cmd_vel_callback() below once
     the Raspberry Pi starts publishing cmd_vel over micro-ROS. */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);

  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* Debug console on USART2 (PA2/PA3) -- independent of the USART1 micro-ROS
   transport, so plug a second USB-TTL adapter here to see printf output. */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* configCHECK_FOR_STACK_OVERFLOW is enabled in FreeRTOSConfig.h; this hook is
   required once that's on. Uses a raw blocking transmit instead of printf()
   since we're already out of stack when this runs. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  static const char msg[] = "\r\n[FATAL] stack overflow\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg) - 1, HAL_MAX_DELAY);
  __disable_irq();
  for (;;) {}
}

#define WHEEL_TRACK_M   0.20f
#define WHEEL_RADIUS_M  0.033f
#define WHEEL_CIRC_M    (2.0f * 3.14159265f * WHEEL_RADIUS_M) /* ≈0.2073m */
#define MAX_WHEEL_RPS   2.0f
#define MAX_DUTY        125  /* 12V 기준 안전 상한 duty (300은 5V 테스트값, 12V는 전압비만큼 낮춤) */

/* Encoder -> rps conversion. TICKS_PER_CIRCLE assumes a Hiwonder JGB520-style
   encoder motor (11 PPR * 4 (quadrature) * 90 gear ratio = 3960). Recalibrate
   if the actual motor/gearbox differs. */
#define TICKS_PER_CIRCLE 3960.0f
#define SAMPLE_PERIOD_S  0.05f  /* must match the osDelay(50) loop period below */
#define ENCODER_ARR      6000   /* matches TIM2/TIM5 Period set in tim.c */

static float compute_wheel_rps(TIM_HandleTypeDef *htim, int32_t *last_count)
{
  int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(htim);
  int32_t delta = count - *last_count;

  if (delta > (ENCODER_ARR / 2)) {
    delta -= (ENCODER_ARR + 1);
  } else if (delta < -(ENCODER_ARR / 2)) {
    delta += (ENCODER_ARR + 1);
  }

  *last_count = count;

  return ((float)delta / TICKS_PER_CIRCLE) / SAMPLE_PERIOD_S;
}

/* MPU6050 IMU (I2C2, PB10=SCL/PB11=SDA per Hiwonder board docs) */
#define MPU6050_ADDR             (0x68 << 1)
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

static void mpu6050_init(void)
{
  uint8_t data = 0x00; /* wake up from sleep, use internal 8MHz oscillator */
  HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

static void mpu6050_read(sensor_msgs__msg__Imu *msg)
{
  uint8_t buf[14];
  if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 14, 100) != HAL_OK) {
    return; /* keep last published values on read failure */
  }

  int16_t accel_x = (int16_t)((buf[0] << 8) | buf[1]);
  int16_t accel_y = (int16_t)((buf[2] << 8) | buf[3]);
  int16_t accel_z = (int16_t)((buf[4] << 8) | buf[5]);
  /* buf[6..7] = temperature, unused */
  int16_t gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);
  int16_t gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
  int16_t gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

  const float ACCEL_SCALE = 9.80665f / 16384.0f;             /* default +-2g range -> m/s^2 */
  const float GYRO_SCALE  = (3.14159265f / 180.0f) / 131.0f; /* default +-250dps range -> rad/s */

  msg->linear_acceleration.x = accel_x * ACCEL_SCALE;
  msg->linear_acceleration.y = accel_y * ACCEL_SCALE;
  msg->linear_acceleration.z = accel_z * ACCEL_SCALE;

  msg->angular_velocity.x = gyro_x * GYRO_SCALE;
  msg->angular_velocity.y = gyro_y * GYRO_SCALE;
  msg->angular_velocity.z = gyro_z * GYRO_SCALE;
}

void cmd_vel_callback(const void * msgin)
{
  const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;

  /* newlib-nano's printf() doesn't format floats by default (no -u _printf_float
     linker flag in this project), so log scaled integers instead of %f. */
  printf("[cmd_vel] linear.x=%ld angular.z=%ld (x1000)\r\n",
         (long)(msg->linear.x * 1000.0f), (long)(msg->angular.z * 1000.0f));

  float v_left  = (float)msg->linear.x - (float)msg->angular.z * (WHEEL_TRACK_M / 2.0f);
  float v_right = (float)msg->linear.x + (float)msg->angular.z * (WHEEL_TRACK_M / 2.0f);

  float rps_left  = v_left  / WHEEL_CIRC_M;
  float rps_right = v_right / WHEEL_CIRC_M;

  if (rps_left  >  MAX_WHEEL_RPS) rps_left  =  MAX_WHEEL_RPS;
  if (rps_left  < -MAX_WHEEL_RPS) rps_left  = -MAX_WHEEL_RPS;
  if (rps_right >  MAX_WHEEL_RPS) rps_right =  MAX_WHEEL_RPS;
  if (rps_right < -MAX_WHEEL_RPS) rps_right = -MAX_WHEEL_RPS;

  int32_t duty_left  = (int32_t)((rps_left  / MAX_WHEEL_RPS) * MAX_DUTY);
  int32_t duty_right = (int32_t)((rps_right / MAX_WHEEL_RPS) * MAX_DUTY);

  if (duty_left >= 0) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_left);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
  } else {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, -duty_left);
  }

  if (duty_right >= 0) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, duty_right);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
  } else {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, -duty_right);
  }
}

static void print_reset_cause(void)
{
  printf("[reset] RCC->CSR=0x%08lX ->", (unsigned long)RCC->CSR);
  if (RCC->CSR & RCC_CSR_LPWRRSTF) printf(" LOW_POWER");
  if (RCC->CSR & RCC_CSR_WWDGRSTF) printf(" WINDOW_WATCHDOG");
  if (RCC->CSR & RCC_CSR_IWDGRSTF) printf(" INDEPENDENT_WATCHDOG");
  if (RCC->CSR & RCC_CSR_SFTRSTF)  printf(" SOFTWARE");
  if (RCC->CSR & RCC_CSR_PORRSTF)  printf(" POWER_ON/DOWN");
  if (RCC->CSR & RCC_CSR_PINRSTF)  printf(" NRST_PIN");
  if (RCC->CSR & RCC_CSR_BORRSTF)  printf(" BROWN_OUT");
  printf("\r\n");
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

void StartMicroROSTask(void *argument)
{
  print_reset_cause();
  printf("[microROS] task start\r\n");

  rmw_uros_set_custom_transport(
    true,
    (void *) &huart1,
    cubemx_transport_open,
    cubemx_transport_close,
    cubemx_transport_write,
    cubemx_transport_read);

  rcl_allocator_t freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
  freeRTOS_allocator.allocate = microros_allocate;
  freeRTOS_allocator.deallocate = microros_deallocate;
  freeRTOS_allocator.reallocate = microros_reallocate;
  freeRTOS_allocator.zero_allocate = microros_zero_allocate;

  if (!rcutils_set_default_allocator(&freeRTOS_allocator)) {
    printf("Error on default allocators\r\n");
  }

  rclc_support_t support;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rcl_node_t node;

  printf("[microROS] calling rclc_support_init (waiting for agent)...\r\n");
  rclc_support_init(&support, 0, NULL, &allocator);
  printf("[microROS] support init done\r\n");

  rclc_node_init_default(&node, "amr_node", "", &support);
  printf("[microROS] node init done\r\n");

  /* Publisher/Subscriber는 다음 단계에서 여기에 추가 */
  rcl_publisher_t imu_publisher;
  rcl_publisher_t enc_left_publisher;
  rcl_publisher_t enc_right_publisher;

  sensor_msgs__msg__Imu imu_msg;
  std_msgs__msg__Float32 enc_left_msg;
  std_msgs__msg__Float32 enc_right_msg;

  rclc_publisher_init_default(
    &imu_publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
    "imu/data_raw");

  rclc_publisher_init_default(
    &enc_left_publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
    "encoder/left_rps");

  rclc_publisher_init_default(
    &enc_right_publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
    "encoder/right_rps");

  rcl_subscription_t cmd_vel_subscriber;
  geometry_msgs__msg__Twist cmd_vel_msg;

  rclc_subscription_init_default(
    &cmd_vel_subscriber, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
    "cmd_vel");

  rclc_executor_t executor;
  rclc_executor_init(&executor, &support.context, 1, &allocator);
  rclc_executor_add_subscription(
    &executor, &cmd_vel_subscriber, &cmd_vel_msg,
    &cmd_vel_callback, ON_NEW_DATA);


  static char imu_frame_id[] = "imu_link";
  imu_msg.header.frame_id.data = imu_frame_id;
  imu_msg.header.frame_id.size = strlen(imu_frame_id);
  imu_msg.header.frame_id.capacity = sizeof(imu_frame_id);

  /* MPU6050 alone has no sensor fusion, so absolute orientation is unknown.
     Mark it as such per the sensor_msgs/Imu convention (orientation_covariance[0] = -1). */
  imu_msg.orientation.x = 0.0;
  imu_msg.orientation.y = 0.0;
  imu_msg.orientation.z = 0.0;
  imu_msg.orientation.w = 1.0;
  imu_msg.orientation_covariance[0] = -1.0;

  mpu6050_init();

  int32_t last_count_left = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
  int32_t last_count_right = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);

  printf("[microROS] entering main loop\r\n");

  uint32_t loop_count = 0;

  for(;;)
  {
    mpu6050_read(&imu_msg);
    enc_left_msg.data  = compute_wheel_rps(&htim2, &last_count_left);
    enc_right_msg.data = compute_wheel_rps(&htim5, &last_count_right);

    rcl_publish(&imu_publisher, &imu_msg, NULL);
    rcl_publish(&enc_left_publisher, &enc_left_msg, NULL);
    rcl_publish(&enc_right_publisher, &enc_right_msg, NULL);

    rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

    /* Heartbeat every ~1s (50ms loop period) so we can see the task is alive
       even with no agent connected. */
    if ((++loop_count % 20) == 0) {
      printf("[microROS] alive, loop=%lu\r\n", (unsigned long)loop_count);
    }

    osDelay(50);
  }
}
/* USER CODE END Application */

