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
#include "tim.h"    /* htim1(왼쪽 엔코더), htim3(모터 PWM), htim5(오른쪽 엔코더) */
#include "i2c.h"    /* hi2c1 -- MPU6050 IMU 통신용 */
#include "usart.h"  /* huart2 -- ST-Link VCP랑 micro-ROS 통신이 같이 쓰는 포트 */
#include <stdio.h>
#include <string.h>

/* --- micro-ROS / ROS2 클라이언트 라이브러리 헤더들 ---
   rcl    = ROS Client Library (C) -- 노드/퍼블리셔/구독/초기화 등 가장 밑단 기능
   rclc   = rcl 위에 얹힌 "편의 계층" -- executor(구독을 주기적으로 확인해서
            콜백을 실행시켜주는 놈)가 여기서 나옴
   uxr/rmw_microxrcedds/rmw_microros = micro-ROS <-> agent 사이 통신 프로토콜
            (Micro XRCE-DDS)이랑, 그걸 진짜 네트워크 대신 그냥 UART 위로 흘려
            보낼 수 있게 해주는 커스텀 트랜스포트 훅(dma_transport.c에 구현) */
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <uxr/client/transport.h>
#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>

/* 우리가 발행/구독할 ROS2 메시지 타입들의 C 구조체 (자동 생성된 것) */
#include <sensor_msgs/msg/imu.h>
#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/int32.h>  /* 원시 엔코더 누적 틱 -- 기어비/PPR 검증용 (아래 encoder/*_raw_ticks 참고) */
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
/* 이 보드는 FreeRTOS 태스크(스레드) 두 개로 돌아가요:
     - defaultTask   : 모터/엔코더 주변장치를 초기화만 하고 거의 아무것도 안 함
     - microROSTask  : ROS2 관련 진짜 작업이 다 여기서 일어남
   왜 나눴냐면, micro-ROS 쪽에서 뭔가 느리거나 막히는 일이 생겨도 모터 초기화
   같은 건 영향을 안 받게 하려고요 (반대 경우도 마찬가지). */
osThreadId_t microROSTaskHandle;
const osThreadAttr_t microROSTask_attributes = {
  .name = "microROSTask",
  /* 16KB 스택. rcl/rclc + XRCE-DDS 세션/직렬화 코드가 함수 호출을 꽤 깊게
     타고 들어가서 스택을 많이 먹어요 -- 이 태스크가 스택 오버플로우 날
     가능성이 제일 높은 애예요. FreeRTOSConfig.h에서
     configCHECK_FOR_STACK_OVERFLOW를 켜둔 것도 이걸 조용히 메모리 깨먹는
     대신 확실하게 잡아내려는 목적이에요. */
  .stack_size = 4096 * 4,
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

/* 이 4개는 dma_transport.c(micro_ros_stm32cubemx_utils/extra_sources/
   microros_transports/에서 복사해온 파일)에 구현돼 있어요. micro-ROS의
   전송 계층을 HAL의 UART+DMA 드라이버에 연결해주는 역할이에요:
   open/close는 DMA를 시작/정지시키고, write/read는 huart2로 실제 바이트를
   주고받아요. */
bool cubemx_transport_open(struct uxrCustomTransport * transport);
bool cubemx_transport_close(struct uxrCustomTransport * transport);
size_t cubemx_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

/* microros_allocators.c에 구현돼 있어요. rcl/rclc가 메시지나 참여자(participant)
   같은 걸 만들 때 malloc/free 같은 동적 할당이 필요한데, 이 함수들이 그걸
   custom_memory_manager.c의 전용 힙으로 연결해줘요 (FreeRTOS 자체 힙이랑
   완전히 분리된 별도 힙 -- 왜 이렇게 나눴는지는 FreeRTOSConfig.h의
   configTOTAL_HEAP_SIZE 설명 참고, RAM이 빠듯한 칩이라 서로 잡아먹지
   않게 하려고 나눴어요). */
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
  /* L298N 모터 드라이버 배선:
       TIM3_CH1 (PA6) = ENA -- 왼쪽 모터 속도 (PWM, 0..999 = 0..100% 듀티)
       TIM3_CH2 (PA7) = ENB -- 오른쪽 모터 속도 (PWM)
       MOTOR_L_IN1/IN2, MOTOR_R_IN1/IN2 (일반 GPIO) -- 회전 방향 결정.
     ENA/ENB는 "얼마나 빨리"만 담당하고, IN1/IN2가 "어느 방향으로"를 담당해요.
     HAL_TIM_PWM_Start()는 PWM 출력 핀을 활성화만 시키는 거지, 그 자체로
     뭔가 돌리진 않아요 -- 아래에서 duty를 0이 아닌 값으로 설정해야 돌아가요. */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

  /* TIM1/TIM5는 (CubeMX에서) 하드웨어 Encoder Mode로 설정돼 있어요: 타이머가
     바퀴 엔코더의 쿼드러처 펄스를 하드웨어적으로 직접 카운트해줘서 별도
     인터럽트가 필요 없어요. TIM_CHANNEL_ALL은 각 타이머의 엔코더 입력 핀
     두 개(CH1/CH2)를 다 활성화하는 거예요. */
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);

  /* 정지 상태로 시작 -- 실제 duty는 라즈베리파이/터미널에서 micro-ROS로
     cmd_vel을 보내기 시작하면 cmd_vel_callback()이 알아서 설정해요. 이거
     중요한 포인트인데, 예전 버전 펌웨어에서 테스트용 duty 값을 실수로
     여기 하드코딩해놔서 보드가 부팅하자마자 아무 명령도 없이 바퀴가 돌아간
     적이 있어요. 항상 "안전하게 정지된 상태"로 부팅하도록 해야 해요. */
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  HAL_GPIO_WritePin(MOTOR_L_IN1_GPIO_Port, MOTOR_L_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_L_IN2_GPIO_Port, MOTOR_L_IN2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_R_IN1_GPIO_Port, MOTOR_R_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_R_IN2_GPIO_Port, MOTOR_R_IN2_Pin, GPIO_PIN_RESET);

  /* 이 태스크는 더 할 일이 없어요 -- 진짜 작업은 전부 StartMicroROSTask()랑
     인터럽트/DMA 컨텍스트(엔코더, PWM, UART)에서 일어나요. 그냥 스케줄러가
     항상 돌아갈 최하위 우선순위 태스크가 하나 있도록 무한 루프만 돌아요. */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* FreeRTOSConfig.h에서 configCHECK_FOR_STACK_OVERFLOW를 켜뒀는데, 그걸 켜면
   FreeRTOS가 이 훅 함수가 반드시 존재해야 한다고 요구해요 -- 어떤 태스크의
   스택 포인터가 할당된 스택 끝을 넘어가는 순간 스케줄러가 이 함수를 호출해요.
   이 보드는 디버그 콘솔용 여분 UART가 없어요 (USART2가 micro-ROS 트래픽이랑
   같이 쓰이고 있어서 텍스트 로그를 따로 못 찍어요), 그래서 여기서 할 수
   있는 건 실행이 깨진 메모리로 계속 진행되게 두는 대신 그냥 안전하게 멈추는
   것뿐이에요. 이게 실제로 발동하면 디버거 붙여서 Suspend 누르고 어느 태스크/
   어느 PC에서 멈췄는지 확인하면 돼요. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  __disable_irq();
  for (;;) {}
}

/* ---- 로봇/바퀴 기하학 상수 --------------------------------------------
   cmd_vel이 주는 "로봇이 X m/s로 움직이고, Y rad/s로 돌아라" 같은
   로봇 단위 명령을, 좌/우 바퀴 각각의 목표 속도로 변환할 때 쓰는 값이에요.
   이 값이 틀리면 회전 시 좌우 속도 비율이 같은 비율로 틀어져요. */
#define WHEEL_TRACK_M   0.2026f   /* 두 바퀴 접지면 사이 거리 -- 실제 섀시 기준으로 검증 필요 */

/* L298N PWM duty 안전 상한/하한. TIM3 Period=999(tim.c 참고)가 100% duty예요.
   배터리 라인에 퓨즈 달려있는지 꼭 확인 -- L298N 자체엔 과전류 보호가 없어요. */
#define MAX_DUTY        900  /* 90% -- 실측: nav2가 raw로 보내는 값 중 최대인 0.5(m/s)에서 이 duty가 나오도록 */
#define MIN_DUTY        400  /* 40% -- 실측: nav2가 raw로 보내는 값 중 최소인 0.01(m/s)에서 이 duty가 나오도록 (정지마찰 이겨내는 최소치) */

/* cmd_vel 속도(m/s) -> duty 직접 매핑 구간. RPi/nav2가 실제로 보내는 값
   범위를 실측해보니 대략 0.01(느릴 때)~0.5(빠를 때)였음 -- 그 구간을
   MIN_DUTY~MAX_DUTY로 선형 매핑해요. 이 구간 밖은 아래에서 각각 하한/상한으로
   고정. (예전엔 배율(x10)+rps 환산을 거치는 간접 방식이었는데, 실측 기준점
   두 개가 명확해져서 직접 매핑이 더 정확하고 이해하기 쉬움.) */
#define CMD_V_MIN       0.01f
#define CMD_V_MAX       0.5f

/* target_v(m/s)를 목표 rps(rev/s)로 바꿀 때 쓰는 바퀴 지름. 실측 필요 --
   65mm는 GA25-370류 소형 AMR 키트에서 흔한 값이라 임시로 넣어둔 것
   (캘리퍼스/줄자로 재서 검증할 것). 이 값이 틀려도 아래 PID의 좌우 대칭
   보정 자체엔 영향 없음(양쪽 바퀴가 같은 상수를 쓰므로) -- 다만 cmd_vel
   0.1m/s 명령이 실제로 몇 m/s로 나오는지(절대 속도 정확도)는 이 값에
   좌우됨. */
#define WHEEL_DIAMETER_M 0.065f
#define V_TO_RPS(v)      ((v) / (3.14159265f * WHEEL_DIAMETER_M))

/* PID를 실제로 duty에 반영할지 여부. 지금 0(꺼짐)인 이유: WHEEL_DIAMETER_M이
   아직 실측 검증이 안 된 추측값(65mm)인데, 실측해보니(2026-08-14,
   linear.x=0.1 명령) 이 값 기준 목표 rps(약 0.49)가 실제 v_to_duty()
   피드포워드가 만들어내는 실제 rps(약 1.7~2.3, 계속 벌어지는 중)와 크게
   어긋나 있었음 -- PID가 "너무 빠르다"고 착각하고 계속 duty를 깎으려 들면서
   원래 있던 피드포워드랑 서로 반대로 밀고 당기다가 왼쪽/오른쪽이 점점 더
   벌어지는 불안정 현상이 실제로 관찰됨(오히려 PID 없을 때보다 악화).
   WHEEL_DIAMETER_M을 캘리퍼스로 실측해서 넣고 나서 이 값을 1로 바꿀 것. */
#define ENABLE_WHEEL_PID 0

/* ---- 바퀴 속도 PID (엔코더 rps 피드백) ------------------------------------
   v_to_duty()가 주는 duty는 피드포워드로 그대로 쓰고(즉각 반응 + 정지마찰
   극복용 MIN_DUTY 유지), 그 위에 목표 rps와 실측 rps의 오차를 PID로 보정해서
   더해요. 0부터 적분만으로 시작하는 순수 PID보다 기동이 빠르고, 좌우 모터/
   기어박스 개체차 때문에 같은 duty를 줘도 실제 회전 속도가 달라지는 문제를
   오차가 0으로 수렴할 때까지 자동으로 trim해줘요.

   D항 기본값은 0 -- 50ms 샘플링 주기에서 rps를 미분하면 노이즈가 커서
   튜닝 전엔 꺼두는 게 안전. 필요하면 WHEEL_PID_KD를 올려서 실험할 것.
   KP/KI 값도 시작점일 뿐이라 실측 튜닝 필요. */
#define WHEEL_PID_KP                 150.0f  /* duty / (rps 오차) */
#define WHEEL_PID_KI                 40.0f   /* duty / (rps 오차 * s) -- 정상상태 오차 제거용 */
#define WHEEL_PID_KD                 0.0f    /* 기본 비활성 */
/* Ki*integral 항(duty 단위)의 상한 -- integral 원시값이 아니라 Ki를 곱한
   "결과"에 거는 클램프예요. wheel_pid_step() 주석 참고. */
#define WHEEL_PID_INTEGRAL_TERM_MAX  ((float)(MAX_DUTY - MIN_DUTY))

/* ---- 엔코더 -> 바퀴 속도 변환 --------------------------------------------
   TICKS_PER_CIRCLE는 STM32 쪽 설정이 아니라 모터에 붙어있는 엔코더
   하드웨어 고유의 값이에요 -- 모터/기어박스/엔코더 디스크가 바뀌면 이 값도
   다시 계산해야 해요. */
/* ZENG WHCD GA25-370-1260-EN 100RPM 버전: 감속비 45:1은 실제 구매처
   스펙표로 확인됨(확실). PPR(11)은 이 계열 모터에서 흔히 쓰이는 값이라
   일단 가정한 것 -- 100% 확정 아님. 바퀴를 손으로 정확히 10바퀴 돌려서
   카운터가 몇 틱 움직였는지 재보고, 그 값/10으로 이 상수를 실측
   검증/보정하는 걸 추천. */
#define TICKS_PER_CIRCLE 1260.0f /* 실측값(바퀴 10바퀴 돌려서 확인) -- 이론 계산(1980)이랑 안 맞았던 이유는
                                    TIM_ENCODERMODE_TI1이 쿼드러처 x4가 아니라 x2 모드라서. 이제 이 값이
                                    기준이니, 나중에 인코더/기어박스 바뀌면 이 실측을 다시 해야 함. */
#define SAMPLE_PERIOD_S  0.05f   /* 카운터를 얼마 주기로 샘플링하는지(초) -- 아래 osDelay(50) 루프 주기와 반드시 일치해야 함 */
#define ENCODER_ARR      6000    /* 엔코더 타이머의 auto-reload 값(카운터가 0..6000..0으로 순환) -- tim.c에서 설정한 TIM1/TIM5 Period와 일치 */

/**
 * 쿼드러처 엔코더 타이머를 읽어서 바퀴의 현재 속도를 초당 회전수(rev/s)로
 * 반환해요.
 *
 * 타이머는 바퀴가 도는 대로 하드웨어적으로 카운터를 올리거나 내려주지만
 * (그게 "Encoder Mode"가 해주는 일이에요), 카운터 레지스터는 폭이 고정된
 * 값이라 끝까지 가면 다시 랩어라운드(wrap-around)돼요 (0 -> ENCODER_ARR ->
 * 0, 반대 방향이면 0 -> ENCODER_ARR) -- 즉 총 이동 거리를 누적해서 알려주는
 * 게 아니에요. 그래서 호출할 때마다:
 *   1. 현재 카운터 값을 읽고,
 *   2. 이전 값에서 빼서 "이번 샘플링 구간 동안 몇 틱 움직였는지" 구하고,
 *   3. 그 차이가 "랩어라운드된 것처럼" 보이면(한 샘플링 구간에 카운터 범위의
 *      절반 이상 뛰었으면) 보정해요 -- 예를 들어 count=5990에서 count=20으로
 *      바뀐 건 실제로는 -5970이 아니라 +30 움직인 거예요.
 *   4. 이번 구간 동안의 틱 수를 초당 회전수로 변환해요.
 *
 * *last_count는 함수 안에서 갱신되어서, 다음 호출 때 새로운 기준점으로
 * 쓰여요.
 */
static float compute_wheel_rps(TIM_HandleTypeDef *htim, int32_t *last_count, int32_t *out_delta)
{
  int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(htim);
  int32_t delta = count - *last_count;

  /* 랩어라운드 보정: 실제 바퀴는 50ms 한 샘플링 구간 안에 반 바퀴어치
     틱만큼 움직일 수 없으니, 그렇게 큰 점프는 진짜 6000틱 움직인 게
     아니라 카운터가 한 바퀴 돈 것으로 봐야 해요. */
  if (delta > (ENCODER_ARR / 2)) {
    delta -= (ENCODER_ARR + 1);
  } else if (delta < -(ENCODER_ARR / 2)) {
    delta += (ENCODER_ARR + 1);
  }

  *last_count = count;
  /* out_delta: 이번 구간에 실제로 움직인 원시 틱 수 -- 기어비/PPR 가정이
     전혀 안 들어간 순수 raw 값이라, TICKS_PER_CIRCLE 검증용으로 씀
     (누적해서 "10바퀴 돌렸을 때 총 틱 수" 확인하는 용도). */
  if (out_delta) *out_delta = delta;

  /* 틱 수 / 한 바퀴당 틱 수 = 이번 구간에 움직인 회전수;
     그걸 샘플링 주기로 나누면 초당 회전수(rev/s)가 나와요. */
  return ((float)delta / TICKS_PER_CIRCLE) / SAMPLE_PERIOD_S;
}

/**
 * L298N 모터 채널 하나를 구동해요: GPIO 2개로 방향을 정하고, PWM 채널
 * 하나로 속도를 정해요.
 *
 * L298N 방향 진리표 (채널 하나 기준):
 *   IN1=HIGH, IN2=LOW  -> 한쪽 방향으로 회전 (여기선 "정방향"/양수로 취급)
 *   IN1=LOW,  IN2=HIGH -> 반대 방향 ("역방향"/음수)
 *   IN1=IN2            -> 브레이크/코스트 (여기선 안 씀)
 * `duty` 인자는 부호랑 크기를 동시에 담고 있어요: 부호로 방향 핀을 정하고,
 * 절댓값이 PWM compare 값(0..MAX_DUTY, 타이머의 999=100% 상한보다 훨씬
 * 낮게 잡혀있음)이 돼요.
 */
static void set_motor(TIM_HandleTypeDef *pwm_tim, uint32_t pwm_channel,
                       GPIO_TypeDef *in1_port, uint16_t in1_pin,
                       GPIO_TypeDef *in2_port, uint16_t in2_pin,
                       int32_t duty)
{
  if (duty >= 0) {
    HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_SET);
    duty = -duty;  /* PWM compare 값은 음수가 될 수 없어서 양수로 바꿔줌 */
  }
  __HAL_TIM_SET_COMPARE(pwm_tim, pwm_channel, (uint32_t)duty);
}

/* ---- MPU6050 IMU 드라이버 (I2C1) ------------------------------------------
   MPU6050은 그냥 평범한 I2C 주변장치예요 (가속도계+자이로, 온보드 센서
   퓨전은 없음) -- 벤더 드라이버 라이브러리 없이 레지스터를 직접 읽고 써요. */
#define MPU6050_ADDR             (0x68 << 1)  /* 7비트 I2C 주소 0x68을 왼쪽으로 1비트 시프트 -- HAL의 I2C API가 8비트 형식을 원해서. 보드의 AD0 핀이 GND에 연결돼 있다고 가정 (AD0=HIGH면 주소가 0x69가 됨) */
#define MPU6050_REG_PWR_MGMT_1   0x6B          /* 전원 관리 레지스터 -- 슬립 모드/클럭 소스 제어 */
#define MPU6050_REG_ACCEL_XOUT_H 0x3B          /* 14개 연속 데이터 레지스터의 첫 주소: 가속도 X/Y/Z, 온도, 자이로 X/Y/Z (각 2바이트, 상위바이트 먼저) */

/**
 * MPU6050은 기본적으로 슬립 모드로 부팅돼요 (호스트가 설정해줄 때까지
 * 전력을 아끼려고) -- 레지스터 하나만 써주면 슬립 비트가 꺼지고 칩 내부
 * 8MHz 오실레이터를 클럭 소스로 선택해서, 그걸로 실제 센서 데이터가
 * 흐르기 시작해요.
 */
static void mpu6050_init(void)
{
  uint8_t data = 0x00; /* 슬립 해제, 내부 8MHz 오실레이터 사용 */
  HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
}

/**
 * 14개의 원시 센서 바이트를 I2C 버스트 트랜잭션 한 번으로 읽어서,
 * sensor_msgs/Imu가 기대하는 물리 단위(m/s^2, rad/s)로 변환해요.
 */
static void mpu6050_read(sensor_msgs__msg__Imu *msg)
{
  uint8_t buf[14];
  if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 14, 100) != HAL_OK) {
    return; /* I2C 통신이 잠깐 실패한 것 -- 이상한 값 대신 이전 값을 계속 발행 */
  }

  /* 각 축은 2바이트에 걸쳐있는 16비트 부호있는 값이고, 빅엔디안(상위바이트
     먼저)이라 여기서 다시 조립해요. */
  int16_t accel_x = (int16_t)((buf[0] << 8) | buf[1]);
  int16_t accel_y = (int16_t)((buf[2] << 8) | buf[3]);
  int16_t accel_z = (int16_t)((buf[4] << 8) | buf[5]);
  /* buf[6..7] = 온도, 안 씀 */
  int16_t gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);
  int16_t gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
  int16_t gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

  /* MPU6050은 기본값이 가속도 +-2g 범위, 자이로 +-250deg/s 범위라서 이
     스케일 값들이 나온 거예요 -- 만약 나중에 감도 레지스터를 더 넓은
     범위로 재설정하면 이 값들도 바뀌어야 해요. */
  const float ACCEL_SCALE = 9.80665f / 16384.0f;             /* 원시 LSB -> m/s^2, 기본 +-2g 범위 기준 */
  const float GYRO_SCALE  = (3.14159265f / 180.0f) / 131.0f; /* 원시 LSB -> rad/s, 기본 +-250dps 범위 기준 */

  msg->linear_acceleration.x = accel_x * ACCEL_SCALE;
  msg->linear_acceleration.y = accel_y * ACCEL_SCALE;
  msg->linear_acceleration.z = accel_z * ACCEL_SCALE;

  msg->angular_velocity.x = gyro_x * GYRO_SCALE;
  msg->angular_velocity.y = gyro_y * GYRO_SCALE;
  msg->angular_velocity.z = gyro_z * GYRO_SCALE;
}

/**
 * 바퀴 속도(m/s, 부호=방향) 하나를 duty(부호 있음, -MAX_DUTY..MAX_DUTY)로
 * 바꿔요. CMD_V_MIN~CMD_V_MAX 구간을 MIN_DUTY~MAX_DUTY로 선형 매핑하고,
 * 그 밖은 각각 하한/상한으로 고정해요:
 *
 *   |v| = 0                     -> duty = 0 (정지)
 *   0 < |v| <= CMD_V_MIN(0.01)  -> duty = ±MIN_DUTY (그대로 두면 안 움직이니 최소치로)
 *   CMD_V_MIN < |v| < CMD_V_MAX -> 그 사이를 선형 비례
 *   |v| >= CMD_V_MAX(0.5)       -> duty = ±MAX_DUTY
 */
static int32_t v_to_duty(float v)
{
  float mag = v;
  int32_t sign = 1;

  if (mag < 0.0f) { sign = -1; mag = -mag; }
  if (mag <= 0.0f) return 0;

  if (mag <= CMD_V_MIN) return sign * MIN_DUTY;
  if (mag >= CMD_V_MAX) return sign * MAX_DUTY;

  float t = (mag - CMD_V_MIN) / (CMD_V_MAX - CMD_V_MIN); /* 0..1 */
  return sign * (MIN_DUTY + (int32_t)(t * (float)(MAX_DUTY - MIN_DUTY)));
}

/* cmd_vel_callback()이 여기 목표 속도를 써놓으면, 메인 루프가 20Hz마다
   이 값을 읽어서 v_to_duty()로 피드포워드 duty를 계산하고, 거기에
   wheel_pid_step()의 보정값을 더해요. 두 함수가 물리적으로 다른 곳에 있어서
   (콜백 vs 메인 루프) 이렇게 파일 스코프 변수로 공유해요 -- 근데 실제로는
   둘 다 microROSTask 스레드 안에서 순차적으로만 실행되니까(
   rclc_executor_spin_some()이 콜백을 동기적으로 호출) 별도 뮤텍스는
   필요 없어요. */
static float target_v_left = 0.0f;
static float target_v_right = 0.0f;

/* 좌/우 바퀴 PID의 적분/이전오차 상태. 연결이 끊기면(메인 루프 아래
   "끊김 감지됨" 처리에서) target_v와 같이 0으로 리셋돼요 -- 안 그러면
   agent 없이 끊겨있던 동안 쌓인 적분값이 재연결 직후 엉뚱한 duty로
   튀어나올 수 있어서. */
static float pid_integral_left = 0.0f;
static float pid_integral_right = 0.0f;
static float pid_prev_error_left = 0.0f;
static float pid_prev_error_right = 0.0f;

/**
 * 바퀴 하나의 rps PID 한 스텝: 목표 rps와 측정 rps의 오차로 duty 보정값을
 * 계산해요. integral/prev_error는 호출자가 들고 있는 좌/우 전용 상태
 * 포인터를 넘겨받아서 그 자리에서 갱신해요(다음 호출의 기준점이 되도록).
 *
 * 와인드업 방지는 "Ki를 곱한 결과"(duty 단위)에 걸어요 -- integral 자체
 * (rps*s 단위)에 걸면 클램프 값이 duty 단위 기준이라 Ki를 곱하는 순간
 * 그 배수만큼 커져서 사실상 안 걸리는 거나 마찬가지예요. 클램프가 걸리면
 * integral도 역산해서 같이 깎아내려요 -- 안 그러면 integral은 계속
 * 커지는데 integral_term만 겉보기로 클램프된 것처럼 보이다가, 나중에 error
 * 부호가 바뀌어도 그 쌓인 integral을 다 풀어내는 데 한참 걸려요(0을 보내도
 * 한동안 안 멈추는 원인).
 */
static float wheel_pid_step(float target_rps, float measured_rps,
                             float *integral, float *prev_error)
{
  float error = target_rps - measured_rps;

  *integral += error * SAMPLE_PERIOD_S;

  float integral_term = WHEEL_PID_KI * (*integral);
  if (integral_term > WHEEL_PID_INTEGRAL_TERM_MAX) {
    integral_term = WHEEL_PID_INTEGRAL_TERM_MAX;
    *integral = integral_term / WHEEL_PID_KI;
  } else if (integral_term < -WHEEL_PID_INTEGRAL_TERM_MAX) {
    integral_term = -WHEEL_PID_INTEGRAL_TERM_MAX;
    *integral = integral_term / WHEEL_PID_KI;
  }

  float derivative = (error - *prev_error) / SAMPLE_PERIOD_S;
  *prev_error = error;

  return WHEEL_PID_KP * error + integral_term + WHEEL_PID_KD * derivative;
}

/**
 * /cmd_vel (geometry_msgs/Twist) 구독 콜백이에요. agent로부터 새 Twist
 * 메시지가 도착할 때마다 rclc executor가 이 함수를 호출해줘요.
 *
 * Twist는 로봇 단위의 움직임을 담고 있어요: linear.x = 전진 속도(m/s),
 * angular.z = 회전 속도(rad/s, 양수 = 반시계방향). 차동구동 로봇은
 * 옆으로 움직이거나 임의의 점을 중심으로 "제자리에서" 돌 수는 없고,
 * 각 바퀴 자체의 속도만 정할 수 있어요. 그래서 로봇 단위 명령을 바퀴
 * 단위 속도 두 개로 변환해요:
 *
 *   v_left  = linear.x - angular.z * (track/2)
 *   v_right = linear.x + angular.z * (track/2)
 *
 * 직관적으로: 직진(angular.z=0)이면 양쪽 바퀴 속도가 같아요. 회전
 * (angular.z != 0)하면 한쪽 바퀴는 빨라지고 반대쪽은 느려지거나 역회전
 * 하는데, 그 차이는 각 바퀴가 로봇 중심선에서 얼마나 떨어져있는지
 * (track/2)에 비례해요 -- 바퀴 간격이 넓거나 회전이 급할수록 그 속도
 * 차이가 커져야 해요.
 */
void cmd_vel_callback(const void * msgin)
{
  const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;

  /* Raspberry Pi 쪽에서 보내는 angular.z 부호가 로봇이 물리적으로 도는
     방향이랑 반대로 뒤집혀 들어와서, 여기서 부호를 뒤집어 보정해요
     (전진 방향을 바꾼 뒤로 좌/우 회전 감각이 반대가 됨). 배율(x10) 없이
     raw 값을 그대로 써요 -- CMD_V_MIN/MAX가 이미 nav2의 실측 raw 범위
     (0.01~0.5) 기준으로 잡혀있어서 따로 키울 필요가 없어요. */
  float linear_x  = (float)msg->linear.x;
  float angular_z = -(float)msg->angular.z;

  /* 여기선 duty를 직접 계산 안 하고 "목표 속도"만 저장해요 -- 실제 duty
     계산(v_to_duty)은 메인 루프에서 해요. */
  target_v_left  = linear_x - angular_z * (WHEEL_TRACK_M / 2.0f);
  target_v_right = linear_x + angular_z * (WHEEL_TRACK_M / 2.0f);
}

/**
 * micro-ROS 태스크예요: UART 트랜스포트 위에 ROS2 클라이언트 스택을
 * 띄우고, 퍼블리셔 3개 + 구독 1개짜리 노드 하나를 만든 다음, 센서를
 * 읽고 ROS2 트래픽을 처리하는 걸 무한 반복해요.
 *
 * 바깥쪽 for(;;)는 "연결 사이클"이에요 -- agent가 나타나길 기다렸다가,
 * 노드/퍼블리셔/구독/executor를 새로 만들고, 안쪽 루프에서 평소 작업(센서
 * 발행 + cmd_vel 처리)을 돌리다가, 주기적으로 ping해서 agent가 사라진 게
 * 감지되면 만들었던 걸 전부 정리(fini)하고 바깥 루프 맨 위로 돌아가서
 * 처음부터 다시 연결을 시도해요. agent를 껐다 켜도(재시작해도) 보드를
 * 다시 리셋할 필요 없이 알아서 재연결되는 구조예요.
 */
void StartMicroROSTask(void *argument)
{
  /* 1단계: rmw(ROS 미들웨어 계층)한테 바이트를 실제로 어떻게 옮길지 알려줘요
     -- huart2(ST-Link 가상 COM 포트랑 같이 쓰는 그 USART2)로, open/close/
     write/read 함수는 dma_transport.c에 있는 걸 써요. 이 위에 있는 ROS2
     스택(rcl, rclc)은 자기가 UART로 통신하는지 진짜 네트워크로 통신하는지
     전혀 몰라요 -- 그게 트랜스포트 추상화의 핵심이에요. 이건 연결이 끊겨도
     다시 설정할 필요 없는 부분이라 바깥 루프 밖에서 딱 한 번만 해요. */
  rmw_uros_set_custom_transport(
    true,
    (void *) &huart2,
    cubemx_transport_open,
    cubemx_transport_close,
    cubemx_transport_write,
    cubemx_transport_read);

  /* 2단계: rcutils의 기본 allocator를, newlib의 malloc/free 대신 우리
     FreeRTOS 기반 allocator(microros_allocate/deallocate/...)로 바꿔요.
     그래야 ROS2 쪽에서 일어나는 모든 할당이 custom_memory_manager.c의
     전용 micro-ROS 힙에서 나오고, FreeRTOS 자체 힙이랑 안 부딪혀요. 이것도
     재연결마다 다시 할 필요 없어서 바깥 루프 밖에 둬요. */
  rcl_allocator_t freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
  freeRTOS_allocator.allocate = microros_allocate;
  freeRTOS_allocator.deallocate = microros_deallocate;
  freeRTOS_allocator.reallocate = microros_reallocate;
  freeRTOS_allocator.zero_allocate = microros_zero_allocate;

  if (!rcutils_set_default_allocator(&freeRTOS_allocator)) {
    Error_Handler();
  }

  /* MPU6050 초기화, 엔코더 기준 카운트도 하드웨어 레벨이라 ROS 연결 상태랑
     무관해요 -- 재연결마다 다시 할 필요 없이 딱 한 번만. */
  mpu6050_init();

  int32_t last_count_left = (int32_t)__HAL_TIM_GET_COUNTER(&htim1);
  int32_t last_count_right = (int32_t)__HAL_TIM_GET_COUNTER(&htim5);

  /* 기어비/PPR 검증용 원시 누적 틱 카운터. 재연결과 무관하게 계속 누적되게
     바깥 루프 밖(태스크 시작 시 한 번)에 둠 -- 바퀴를 정확히 N바퀴 돌리고
     이 값이 얼마나 늘었는지 보면 TICKS_PER_CIRCLE을 실측 검증할 수 있어요.
     0으로 리셋하고 싶으면 보드만 리셋하면 됨. */
  int32_t cumulative_ticks_left = 0;
  int32_t cumulative_ticks_right = 0;

  /* 바깥쪽 "연결 사이클" 루프 -- 한 바퀴 돌 때마다 (재)연결 한 번. */
  for (;;)
  {
    /* agent가 응답할 때까지 기다림 -- 보드를 언제 켜거나 agent를 언제
       재시작해도 여기서 자동으로 기다려줘요. */
    while (rmw_uros_ping_agent(100, 1) != RMW_RET_OK) {
      osDelay(1000);
    }

    /* rclc_support_t는 rcl의 "context"(초기화 상태) + allocator를 묶어놓은
       거예요. rclc_support_init()이 호출되는 시점에 실제로 UART로 첫
       바이트가 나가요 -- PC/라즈베리파이 쪽 micro-ROS agent 입장에서는
       클라이언트가 세션을 열려고 시도하는 순간이 바로 여기예요. */
    rclc_support_t support;
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rcl_node_t node;

    rclc_support_init(&support, 0, NULL, &allocator);
    /* "노드"는 ROS2 그래프에서 정체성의 단위예요 -- 여기 적은 이름("amr_node")이
       PC에서 `ros2 node list` 쳤을 때 뜨는 그 이름이에요. */
    rclc_node_init_default(&node, "amr_node", "", &support);

    /* 퍼블리셔 핸들들 + 실제로 발행할 메시지 구조체들. 메시지 구조체는 여기서
       (재연결마다) 선언되고 안쪽 루프 돌 때마다 재사용돼요 -- rcl_publish()는
       호출될 때마다 그 시점의 내용을 복사/직렬화해서 보내는 거라, 매번 새로
       메시지를 할당하는 대신 같은 구조체의 필드 값만 덮어쓰면 돼요. */
    rcl_publisher_t imu_publisher;
    rcl_publisher_t enc_left_publisher;
    rcl_publisher_t enc_right_publisher;
    rcl_publisher_t enc_left_ticks_publisher;
    rcl_publisher_t enc_right_ticks_publisher;

    sensor_msgs__msg__Imu imu_msg;
    std_msgs__msg__Float32 enc_left_msg;
    std_msgs__msg__Float32 enc_right_msg;
    std_msgs__msg__Int32 enc_left_ticks_msg;
    std_msgs__msg__Int32 enc_right_ticks_msg;

    /* ROSIDL_GET_MSG_TYPE_SUPPORT(pkg, msg, Type)는 해당 ROS2 메시지 타입의
       자동생성된 타입서포트 메타데이터(필드 배치, 직렬화 정보)를 가져와요 --
       모든 퍼블리셔/구독이 이걸로 구조체를 어떻게 인코딩/디코딩할지 알아요.
       문자열("imu/data_raw" 등)은 토픽 이름이고, 이게 PC에서
       `ros2 topic list` 쳤을 때 보이는 그 이름이에요. */
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

    /* 기어비 검증용 원시 누적 틱 -- 이 프로젝트에서 가정한 TICKS_PER_CIRCLE
       계산이 전혀 안 들어간 순수 하드웨어 카운트값. */
    rclc_publisher_init_default(
      &enc_left_ticks_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
      "encoder/left_raw_ticks");

    rclc_publisher_init_default(
      &enc_right_ticks_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
      "encoder/right_raw_ticks");

    /* 구독 쪽: cmd_vel_msg는 executor가 들어온 Twist 메시지를 디코딩해서 담아둘
       버퍼예요, 그런 다음 그 포인터를 들고 cmd_vel_callback()을 호출해요. */
    rcl_subscription_t cmd_vel_subscriber;
    geometry_msgs__msg__Twist cmd_vel_msg;

    rclc_subscription_init_default(
      &cmd_vel_subscriber, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
      "cmd_vel");

    /* executor가 실제로 "새 cmd_vel 메시지 왔나?"를 확인하고 왔으면 우리 콜백을
       불러주는 애예요 -- 아래 안쪽 루프에 있는 rclc_executor_spin_some()이
       그 확인 동작을 트리거해요. 여기 "1"은 이 executor가 관리하는 핸들
       (구독/타이머 등) 개수인데, cmd_vel_subscriber 하나만 추가할 거라 1이에요.
       ON_NEW_DATA는 "진짜 새 메시지가 왔을 때만 콜백을 실행해라"라는 뜻이에요
       (예전 데이터로 다시 실행하지 않고). */
    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_subscription(
      &executor, &cmd_vel_subscriber, &cmd_vel_msg,
      &cmd_vel_callback, ON_NEW_DATA);

    /* sensor_msgs/Imu에는 std_msgs/Header 필드가 있고 그 안에 frame_id라는
       문자열이 있어요 -- ROS2의 rosidl_runtime_c String 타입은 그냥 평범한
       C 문자열이 아니라 {data, size, capacity} 세 개짜리 구조체라서, 이렇게
       실제 버퍼를 직접 가리키도록 설정해줘야 해요. imu_msg 자체는 재연결마다
       새로 만들어지는 지역변수라 이 설정도 재연결마다 다시 해줘야 하는데,
       가리키는 문자열 버퍼(imu_frame_id)는 static이라 안 바뀌니 재활용해요. */
    static char imu_frame_id[] = "imu_link";
    imu_msg.header.frame_id.data = imu_frame_id;
    imu_msg.header.frame_id.size = strlen(imu_frame_id);
    imu_msg.header.frame_id.capacity = sizeof(imu_frame_id);

    /* MPU6050 혼자서는 센서 퓨전이 없어서 절대 방향(orientation)을 알 수
       없어요. sensor_msgs/Imu 관례대로 그 사실을 명시해요
       (orientation_covariance[0] = -1). */
    imu_msg.orientation.x = 0.0;
    imu_msg.orientation.y = 0.0;
    imu_msg.orientation.z = 0.0;
    imu_msg.orientation.w = 1.0;
    imu_msg.orientation_covariance[0] = -1.0;

    /* 안쪽 "평소 작업" 루프. connected가 false가 되면(=끊김 감지) 빠져나가서
       아래 정리(fini) 코드로 내려가요. */
    bool connected = true;
    uint32_t loop_count = 0;

    /* 메인 루프: 센서 읽고, 발행하고, executor가 밀린 cmd_vel 메시지 있으면
       처리하게 해주고, 잠깐 자고, 반복 -- 대략 20Hz로 돕니다
       (osDelay(50) = 반복 사이 50ms 대기, SAMPLE_PERIOD_S랑 일치). */
    while (connected)
    {
      mpu6050_read(&imu_msg);
      /* 물리적으로 왼쪽/오른쪽 위치가 뒤바뀐 상태라, 토픽 발행할 때 서로
         바꿔서 내보내요 (htim1=원래 "왼쪽"이었던 엔코더 -> 이제 오른쪽 토픽으로,
         htim5=원래 "오른쪽" -> 이제 왼쪽 토픽으로).

         부호(-) 위치는 실측으로 검증함: +0.1 명령 시 목표는 +0.49rev/s인데
         수정 전엔 양쪽 다 -3~4rev/s(부호 반대, 크기는 목표의 7~8배)로 측정돼
         PID가 duty를 계속 키우는 양성 피드백(런어웨이)에 빠져 MAX_DUTY까지
         포화됐었음. -0.1에서도 마찬가지로 부호가 반대였음. 그래서 음수 부호를
         반대쪽(left)에 붙임 -- htim1/htim5 중 어느 게 왼쪽/오른쪽인지(소스)는
         안 건드리고, duty 부호와 measured rps 부호가 일치하도록(양수 duty ->
         양수 rps) 부호만 바꿈. */
      int32_t delta_htim1 = 0, delta_htim5 = 0;
      enc_right_msg.data = compute_wheel_rps(&htim1, &last_count_left, &delta_htim1);
      enc_left_msg.data  = -compute_wheel_rps(&htim5, &last_count_right, &delta_htim5);

      /* target_v_left/right(cmd_vel_callback이 저장해둔 목표 속도)를 v_to_duty()로
         피드포워드 duty로 바꿔요. ENABLE_WHEEL_PID가 켜져 있으면 그 위에
         wheel_pid_step()의 rps 오차 보정을 더해요 -- 지금은 WHEEL_DIAMETER_M이
         실측 검증 안 된 값이라 꺼져 있음(위 매크로 주석 참고).
         enc_left_msg.data/enc_right_msg.data는 바로 위에서 이미 이번 구간
         측정값으로 갱신됐고 좌우 스왑도 반영돼있어서 그대로 피드백으로 씀.

         단, target_v가 정확히 0(정지 명령)이면 PID를 아예 안 거치고 duty를
         무조건 0으로 박아요 -- PID 보정값이 뭐가 됐든(오차/적분이 남아있든)
         "0을 보내면 반드시 선다"를 보장하기 위한 안전장치예요. 그 바퀴의
         적분/이전오차도 같이 리셋해서, 다음에 다시 움직일 때 정지해있던
         동안의 오차를 끌고 가지 않게 해요. */
      int32_t duty_left, duty_right;

      if (target_v_left == 0.0f) {
        duty_left = 0;
        pid_integral_left = 0.0f;
        pid_prev_error_left = 0.0f;
      } else {
        float duty_left_f = (float)v_to_duty(target_v_left);
#if ENABLE_WHEEL_PID
        duty_left_f += wheel_pid_step(V_TO_RPS(target_v_left), enc_left_msg.data,
                                       &pid_integral_left, &pid_prev_error_left);
#endif
        duty_left = (int32_t)duty_left_f;
        if (duty_left >  MAX_DUTY) duty_left =  MAX_DUTY;
        if (duty_left < -MAX_DUTY) duty_left = -MAX_DUTY;
      }

      if (target_v_right == 0.0f) {
        duty_right = 0;
        pid_integral_right = 0.0f;
        pid_prev_error_right = 0.0f;
      } else {
        float duty_right_f = (float)v_to_duty(target_v_right);
#if ENABLE_WHEEL_PID
        duty_right_f += wheel_pid_step(V_TO_RPS(target_v_right), enc_right_msg.data,
                                        &pid_integral_right, &pid_prev_error_right);
#endif
        duty_right = (int32_t)duty_right_f;
        if (duty_right >  MAX_DUTY) duty_right =  MAX_DUTY;
        if (duty_right < -MAX_DUTY) duty_right = -MAX_DUTY;
      }

      set_motor(&htim3, TIM_CHANNEL_1, MOTOR_L_IN1_GPIO_Port, MOTOR_L_IN1_Pin,
                MOTOR_L_IN2_GPIO_Port, MOTOR_L_IN2_Pin, duty_left);
      set_motor(&htim3, TIM_CHANNEL_2, MOTOR_R_IN1_GPIO_Port, MOTOR_R_IN1_Pin,
                MOTOR_R_IN2_GPIO_Port, MOTOR_R_IN2_Pin, duty_right);

      /* rps 계산이랑 같은 좌/우 swap + 부호를 원시 틱 누적에도 똑같이 적용 --
         안 그러면 rps 토픽이랑 raw_ticks 토픽이 서로 다른 부호/방향을 가리키게 됨. */
      cumulative_ticks_right += delta_htim1;
      cumulative_ticks_left  += -delta_htim5;
      enc_left_ticks_msg.data  = cumulative_ticks_left;
      enc_right_ticks_msg.data = cumulative_ticks_right;

      rcl_publish(&imu_publisher, &imu_msg, NULL);
      rcl_publish(&enc_left_publisher, &enc_left_msg, NULL);
      rcl_publish(&enc_right_publisher, &enc_right_msg, NULL);
      rcl_publish(&enc_left_ticks_publisher, &enc_left_ticks_msg, NULL);
      rcl_publish(&enc_right_ticks_publisher, &enc_right_ticks_msg, NULL);

      /* executor한테 최대 10ms를 줘서 새 cmd_vel 메시지가 왔는지 확인하고
         처리하게 해요 (cmd_vel_callback()이 실제로 여기서 호출돼요, 새
         메시지가 있었다면). */
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

      /* 대략 2초(50ms * 40)에 한 번씩만 ping으로 연결 상태 확인 -- 매
         루프(20Hz)마다 하면 ping 응답 기다리는 시간 때문에 센서 발행
         주기가 밀릴 수 있어서 가끔만 확인해요. */
      loop_count++;
      if (loop_count % 40 == 0) {
        if (rmw_uros_ping_agent(100, 1) != RMW_RET_OK) {
          connected = false;
        }
      }

      osDelay(50);
    }

    /* 끊김 감지됨 -- 안전을 위해 모터부터 세워요. agent가 없으면 새 cmd_vel도
       못 받으니, 마지막으로 받았던 명령을 계속 실행하고 있으면 안 돼요.
       목표 속도도 같이 리셋해서, 재연결됐을 때 옛날 목표를 쫓아가려는
       엉뚱한 동작이 안 나오게 해요. PID 상태(적분/이전오차)도 같이 리셋 --
       안 그러면 끊겨있던 동안 쌓인 적분값이 재연결 직후 duty로 튀어나와요. */
    target_v_left = 0.0f;
    target_v_right = 0.0f;
    pid_integral_left = 0.0f;
    pid_integral_right = 0.0f;
    pid_prev_error_left = 0.0f;
    pid_prev_error_right = 0.0f;
    set_motor(&htim3, TIM_CHANNEL_1, MOTOR_L_IN1_GPIO_Port, MOTOR_L_IN1_Pin,
              MOTOR_L_IN2_GPIO_Port, MOTOR_L_IN2_Pin, 0);
    set_motor(&htim3, TIM_CHANNEL_2, MOTOR_R_IN1_GPIO_Port, MOTOR_R_IN1_Pin,
              MOTOR_R_IN2_GPIO_Port, MOTOR_R_IN2_Pin, 0);

    /* 만들었던 ROS2 객체들을 전부 정리(fini) -- init의 역순으로. 이걸 안 하고
       바깥 루프로 돌아가서 새로 rclc_support_init()을 부르면, 예전 세션이
       쓰던 메모리(micro-ROS 전용 힙)가 안 돌아와서 재연결을 반복할 때마다
       힙이 조금씩 줄어들다가 결국 할당 실패로 이어져요. */
    rclc_executor_fini(&executor);
    rcl_subscription_fini(&cmd_vel_subscriber, &node);
    rcl_publisher_fini(&imu_publisher, &node);
    rcl_publisher_fini(&enc_left_publisher, &node);
    rcl_publisher_fini(&enc_right_publisher, &node);
    rcl_publisher_fini(&enc_left_ticks_publisher, &node);
    rcl_publisher_fini(&enc_right_ticks_publisher, &node);
    rcl_node_fini(&node);
    rclc_support_fini(&support);

    /* 바깥 루프 맨 위로 돌아가서 다시 ping 대기부터 시작 -- agent가 다시
       뜨면 자동으로 재연결돼요. */
  }
}
/* USER CODE END Application */
