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
#include "tim.h"    /* htim1/htim5 = wheel encoders, htim3 = motor PWM */
#include "usart.h"  /* huart2 -- shared with ST-Link VCP and the micro-ROS transport */
#include "i2c.h"    /* hi2c1 -- MPU6050 IMU on PB8/PB9 */
#include <math.h>
#include <string.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <uxr/client/transport.h>
#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>

#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/int32.h>
#include <geometry_msgs/msg/twist.h>
#include <sensor_msgs/msg/imu.h>
#include <rosidl_runtime_c/string_functions.h>
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
/* Two tasks: defaultTask just brings up the motor/encoder peripherals and
   otherwise idles; microROSTask owns everything ROS2-related. Kept separate
   so a stall on the micro-ROS side (agent unreachable, XRCE-DDS session
   trouble) can't block peripheral bring-up, and vice versa. */
osThreadId_t microROSTaskHandle;
const osThreadAttr_t microROSTask_attributes = {
  .name = "microROSTask",
  /* rcl/rclc + XRCE-DDS serialization call deep into the stack -- this task
     is the most likely place to overflow. configCHECK_FOR_STACK_OVERFLOW is
     on in FreeRTOSConfig.h so an overflow here halts cleanly via
     vApplicationStackOverflowHook() instead of corrupting memory silently. */
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

/* Implemented in Core/Src/dma_transport.c (copied from
   micro_ros_stm32cubemx_utils/extra_sources/microros_transports/) -- wires
   the micro-ROS transport layer to HAL's UART+DMA driver. */
bool cubemx_transport_open(struct uxrCustomTransport * transport);
bool cubemx_transport_close(struct uxrCustomTransport * transport);
size_t cubemx_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

/* Implemented in Core/Src/microros_allocators.c -- routes rcl/rclc's dynamic
   allocations to the dedicated heap in custom_memory_manager.c, kept
   separate from FreeRTOS's own heap since RAM is tight on this chip. */
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
  /* L298N wiring (see the wheel_init() call site in StartMicroROSTask()
     for the full writeup, including the 2026-08-26 rewire/revert history):
       TIM3_CH1 (PA6) = ENA -- right motor speed (PWM, 0..999 = 0..100% duty)
       TIM3_CH2 (PA7) = ENB -- left motor speed (PWM)
       MOTOR_L_IN1/IN2, MOTOR_R_IN1/IN2 (plain GPIO) -- direction.
     ENA/ENB set "how fast", IN1/IN2 set "which way". HAL_TIM_PWM_Start()
     only enables the PWM output pin -- duty stays at whatever the compare
     register holds until something sets it. */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

  /* TIM1/TIM5 are configured (in CubeMX) as hardware Encoder Mode: the timer
     counts quadrature pulses from each wheel encoder directly in hardware,
     no interrupt needed. TIM_CHANNEL_ALL enables both encoder input pins
     (CH1/CH2) on each timer. */
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL);

  /* Boot stopped. Real duty only gets set once cmd_vel_callback() receives a
     command over micro-ROS. Never hardcode a nonzero duty here -- this is
     the one thing that must always hold, since the robot will start moving
     the instant it does. */
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
  HAL_GPIO_WritePin(MOTOR_L_IN1_GPIO_Port, MOTOR_L_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_L_IN2_GPIO_Port, MOTOR_L_IN2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_R_IN1_GPIO_Port, MOTOR_R_IN1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(MOTOR_R_IN2_GPIO_Port, MOTOR_R_IN2_Pin, GPIO_PIN_RESET);

  /* Nothing else to do here -- all real work happens in StartMicroROSTask()
     and in interrupt/DMA context (encoders, PWM, UART). */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* configCHECK_FOR_STACK_OVERFLOW is on in FreeRTOSConfig.h, which requires
   this hook to exist -- FreeRTOS calls it the moment a task's stack pointer
   runs past its allocated stack. USART2 is shared with micro-ROS traffic so
   there's no spare UART for a debug console here; halting cleanly is the
   best this hook can do. If it fires, attach a debugger, hit Suspend, and
   check which task/PC it stopped at. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  __disable_irq();
  for (;;) {}
}

/* ==== Robot geometry ======================================================
   Converts cmd_vel's robot-frame command (linear.x m/s, angular.z rad/s)
   into per-wheel targets, and encoder ticks into wheel speed. Measured on
   the chassis -- update both here and in the ROS2-side wheel_odometry_node
   parameters together if the wheels or track change. */
#define WHEEL_TRACK_M           0.24f  /* distance between the two wheel contact patches */
#define WHEEL_RADIUS_M          0.0325f  /* measured wheel radius */
#define WHEEL_CIRCUMFERENCE_M   (2.0f * (float)M_PI * WHEEL_RADIUS_M)

/* GA25-370 gearmotor + quadrature encoder: measured ticks per one wheel
   revolution (turn the wheel exactly 10 turns by hand, divide the raw tick
   delta by 10 -- see encoder/*_raw_ticks topics below). Re-measure if the
   motor/gearbox/encoder disc is ever swapped. */
#define ENCODER_TICKS_PER_REV   1260.0f
#define ENCODER_ARR             6000     /* TIM1/TIM5 auto-reload value (tim.c) -- counter wraps 0..6000..0 */

#define CONTROL_PERIOD_S        0.05f    /* control loop period (s) -- must match the osDelay(50) below */

/* ==== cmd_vel(m/s) -> PWM duty feedforward =================================
   L298N duty range: TIM3 Period=999 (tim.c) is 100%.

   MIN_DUTY=375 is measured, not guessed: with the robot resting on the
   floor (real load, not free-spinning), duty below ~375 doesn't reliably
   turn the wheel at all. That's the real breakaway point for this
   gearbox/load, not a tunable choice -- any cmd_vel magnitude in
   (0, CMD_V_MIN] gets this duty specifically so slow commands still move
   the robot instead of stalling silently.

   MAX_DUTY=900 is the top of the safe range (leaves PID headroom below
   TIM3's 999=100%).

   Confirm the battery line has a fuse -- the L298N itself has no
   overcurrent protection. */
#define MAX_DUTY        950
#define MIN_DUTY     	400
#define CMD_V_MIN       0.005f

#define CMD_V_MAX       0.13f

/* ==== Measured duty -> rps calibration (what the PID should actually chase)
   2026-08-18: after fixing compute_wheel_rps() to use real elapsed time
   instead of an assumed fixed period (see dt_s), re-measuring at both ends
   of the duty range gave results nowhere near target_v/WHEEL_CIRCUMFERENCE_M's
   physics prediction (0.049..2.45 rev/s for CMD_V_MIN..CMD_V_MAX). The
   earlier "MIN_DUTY gives ~1.38" / "MAX_DUTY gives ~2.31" numbers were
   themselves measured before that dt_s fix and were inflated by the same
   periodic-spike artifact it fixed -- not a real regression, just a bad
   ruler. The physics formula assumes this hardware can reach whatever
   speed a commanded m/s implies; in reality the whole MIN_DUTY..MAX_DUTY
   range only ever produces a narrow band of rev/s, so a physics-based
   target is essentially unreachable everywhere and the PID would spend
   all its effort chasing it instead of converging. v_to_target_rps() below
   maps the same v used for v_to_duty() onto *this* measured curve instead,
   so the PID's setpoint actually matches what the duty it's given can
   achieve. 2026-08-19: re-measured again (0.9..1.52) -- re-measure both
   ends (v=CMD_V_MIN and v=CMD_V_MAX, sustained a few seconds each) if the
   wheels, gearbox, or battery situation changes meaningfully. */
#define MIN_DUTY_RPS    0.3f
#define MAX_DUTY_RPS    1.52f

/* ==== Wheel speed PID (encoder feedback) ===================================
   v_to_duty() feedforward gets the wheel moving immediately and past static
   friction; the PID term on top trims for per-motor/gearbox differences so
   both wheels track the same commanded speed. Set ENABLE_WHEEL_PID to 0 to
   fall back to pure feedforward if the loop turns out unstable.

   KP/KI/KD are separate per wheel (LEFT vs RIGHT below) -- the two motors
   don't have to respond the same way to the same duty, so tune them
   independently. Each Wheel instance gets its own copy at wheel_init() time
   (see the wheel_init() calls in StartMicroROSTask()), so changing e.g.
   WHEEL_PID_KP_RIGHT only affects wheel_right. */
#define ENABLE_WHEEL_PID              0  /* TEMP 2026-08-26: off while diagnosing wiring identity/polarity -- restore to 1 once confirmed */

#define WHEEL_PID_KP_LEFT             300.0f  /* duty per (rps error) */
#define WHEEL_PID_KI_LEFT             0.0f  /* duty per (rps error * s) */
#define WHEEL_PID_KD_LEFT             0.0f

#define WHEEL_PID_KP_RIGHT            300.0f
#define WHEEL_PID_KI_RIGHT            0.0f
#define WHEEL_PID_KD_RIGHT            0.0f

/* Clamp applies to Ki*integral (duty units), not to the raw integral -- see
   wheel_pid_step(). Shared: it's derived from the duty range, not from any
   per-wheel tuning. */
#define WHEEL_PID_INTEGRAL_TERM_MAX  ((float)(MAX_DUTY - MIN_DUTY))

/* ==== Left/right tick-sync (keeps the robot driving straight) =============
   Each wheel's own PID above only looks at its own instantaneous speed, so
   both wheels can track their own target correctly on average and still
   drift apart in *position* over time -- any brief mismatch (asymmetric
   motor response, a bit of slip) never gets corrected, it just accumulates
   into the robot curving off to one side.

   This compares actual accumulated ticks (wheel_left.cumulative_ticks -
   wheel_right.cumulative_ticks) against what that difference *should* be
   given the commanded speeds (0 while driving straight, nonzero while
   turning -- see sync_expected_tick_diff in StartMicroROSTask()), and
   nudges each wheel's PID setpoint -- not its feedforward -- to close the
   gap. Start small: too high and this fights the per-wheel PID instead of
   gently correcting drift. */
#define ENABLE_TICK_SYNC     0  /* TEMP 2026-08-26: off while diagnosing wiring identity/polarity -- restore to 1 once confirmed */
#define SYNC_KP              0.002f  /*

target-rps trim per tick of accumulated drift */
#define SYNC_KI              0.05f
#define SYNC_INTEGRAL_MAX    200.0f  /* clamp on the drift integral (tick*s units) */
/* Clamp on the *final* trim (SYNC_KP*error + SYNC_KI*integral combined),
   not just the integral term -- SYNC_KP*error alone is unbounded, and if
   the tick drift ever grows large (e.g. while the per-wheel PID gains are
   still being tuned and one side is genuinely much faster), an unclamped
   P-term can exceed the wheel's own target_rps and flip its *sign* --
   commanding that wheel to run backward instead of just trimming its
   speed. This is a hard ceiling on how much nudge tick-sync is allowed to
   apply, well under any real target_rps, so it can never do that. */
#define SYNC_TRIM_MAX_RPS    0.1f

/**
 * Reads a quadrature encoder timer and returns the wheel's current speed in
 * revolutions per second. Sign matches commanded duty sign directly -- each
 * encoder's A/B wiring is physically oriented for that (2026-08-19), so no
 * software sign flip is needed here.
 *
 * The timer counts up/down in hardware as the wheel turns, but the counter
 * register wraps (0 -> ENCODER_ARR -> 0, or the reverse) rather than
 * accumulating total distance. So each call: reads the counter, diffs
 * against the previous reading, corrects for wraparound (a jump of more
 * than half the counter range in one sample period is a wrap, not a real
 * move that fast), and converts ticks-per-sample into rev/s.
 *
 * dt_s is the *actual* time elapsed since the last call, not an assumed
 * constant -- see the comment on dt_s in wheel_step() for why that matters.
 */
static float compute_wheel_rps(TIM_HandleTypeDef *htim, int32_t *last_count, int32_t *out_delta, float dt_s)
{
  int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(htim);
  int32_t delta = count - *last_count;

  if (delta > (ENCODER_ARR / 2)) {
    delta -= (ENCODER_ARR + 1);
  } else if (delta < -(ENCODER_ARR / 2)) {
    delta += (ENCODER_ARR + 1);
  }

  *last_count = count;
  if (out_delta) *out_delta = delta;

  return ((float)delta / ENCODER_TICKS_PER_REV) / dt_s;
}

/**
 * Drives one L298N channel: two GPIOs set direction, one PWM channel sets
 * speed.
 *
 *   IN1=HIGH, IN2=LOW  -> forward (positive)
 *   IN1=LOW,  IN2=HIGH -> reverse (negative)
 *   IN1=IN2            -> brake/coast (unused here)
 *
 * `duty` carries both sign (direction) and magnitude (0..MAX_DUTY compare
 * value).
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
    duty = -duty;
  }
  __HAL_TIM_SET_COMPARE(pwm_tim, pwm_channel, (uint32_t)duty);
}

/**
 * Converts one wheel's target speed (m/s, signed = direction) into a signed
 * PWM duty (-MAX_DUTY..MAX_DUTY): linearly maps CMD_V_MIN..CMD_V_MAX onto
 * MIN_DUTY..MAX_DUTY, clamped outside that range.
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

/**
 * Companion to v_to_duty(): what rps the wheel actually achieves at the
 * duty v_to_duty(v) picks, per the measured MIN_DUTY_RPS..MAX_DUTY_RPS
 * calibration -- this is what the PID should chase as its setpoint, not
 * v/WHEEL_CIRCUMFERENCE_M (see the calibration comment above). Mirrors
 * v_to_duty()'s clamping/interpolation exactly so the feedforward duty and
 * the PID's target always describe the same point on the curve.
 */
static float v_to_target_rps(float v)
{
  float mag = v;
  int32_t sign = 1;

  if (mag < 0.0f) { sign = -1; mag = -mag; }
  if (mag <= 0.0f) return 0.0f;

  if (mag <= CMD_V_MIN) return (float)sign * MIN_DUTY_RPS;
  if (mag >= CMD_V_MAX) return (float)sign * MAX_DUTY_RPS;

  float t = (mag - CMD_V_MIN) / (CMD_V_MAX - CMD_V_MIN); /* 0..1 */
  return (float)sign * (MIN_DUTY_RPS + t * (MAX_DUTY_RPS - MIN_DUTY_RPS));
}

/**
 * One PID step for a single wheel: target/measured rps in, duty correction
 * out. kp/ki/kd are that wheel's own gains (see Wheel.kp/ki/kd) so left and
 * right can be tuned independently. integral/prev_error are the caller's
 * per-wheel state, updated in place for the next call.
 *
 * Anti-windup clamps ki*integral (duty units) rather than the raw integral
 * (rps*s units) -- clamping the raw integral would need a completely
 * different-magnitude limit since ki scales it up. When the clamp engages,
 * the raw integral is backed out to match, otherwise it keeps growing
 * silently underneath the clamped output and takes a long time to unwind
 * once the error's sign flips (e.g. commanding 0 wouldn't stop the wheel
 * promptly). Skipped entirely if ki is 0 (pure P/D tuning) to avoid
 * dividing by zero.
 */
static float wheel_pid_step(float target_rps, float measured_rps,
                             float kp, float ki, float kd,
                             float *integral, float *prev_error, float dt_s)
{
  float error = target_rps - measured_rps;

  *integral += error * dt_s;

  float integral_term = ki * (*integral);
  if (ki != 0.0f) {
    if (integral_term > WHEEL_PID_INTEGRAL_TERM_MAX) {
      integral_term = WHEEL_PID_INTEGRAL_TERM_MAX;
      *integral = integral_term / ki;
    } else if (integral_term < -WHEEL_PID_INTEGRAL_TERM_MAX) {
      integral_term = -WHEEL_PID_INTEGRAL_TERM_MAX;
      *integral = integral_term / ki;
    }
  }

  float derivative = (error - *prev_error) / dt_s;
  *prev_error = error;

  return kp * error + integral_term + kd * derivative;
}

/* ==== Per-wheel state (fully independent left/right) =======================
   Everything one wheel needs -- which encoder timer, which PWM channel,
   which direction GPIOs, and its own running PID/encoder state -- lives in
   a single struct instance. wheel_step()/wheel_stop() are the only places
   that touch a Wheel, and both take the instance as a parameter, so there
   is no possibility of left's target ending up paired with right's encoder
   or motor pins (or vice versa) anywhere else in this file -- that class of
   bug (which is what caused the left/right "seesaw" earlier) can't happen
   by construction now: fix the wiring once in wheel_left/wheel_right's
   initialization below, and every other line just says "the wheel" without
   caring which physical side it is. */
typedef struct {
  /* Hardware identity -- set once at init below, read-only afterward. */
  TIM_HandleTypeDef *encoder_tim;
  TIM_HandleTypeDef  *pwm_tim;
  uint32_t            pwm_channel;
  GPIO_TypeDef        *in1_port;
  uint16_t             in1_pin;
  GPIO_TypeDef        *in2_port;
  uint16_t             in2_pin;
  float                kp;   /* this wheel's own PID gains -- see WHEEL_PID_K*_LEFT/RIGHT */
  float                ki;
  float                kd;

  /* Running state -- updated once per control period by wheel_step(). */
  int32_t  last_count;
  int32_t  cumulative_ticks; /* raw ticks, no gear-ratio assumption -- for verifying ENCODER_TICKS_PER_REV */
  float    measured_rps;
  float    target_v;         /* written by cmd_vel_callback() */
  float    pid_integral;
  float    pid_prev_error;
} Wheel;

static Wheel wheel_left;
static Wheel wheel_right;

/* Tick-sync running state -- see ENABLE_TICK_SYNC. sync_expected_tick_diff
   is the integral of the *commanded* (target_rps_left - target_rps_right),
   i.e. what wheel_left.cumulative_ticks - wheel_right.cumulative_ticks
   ought to be right now if both wheels tracked their targets perfectly.
   Reset alongside the wheels on disconnect so a stale drift estimate from
   before a reconnect doesn't bias the first few seconds after. */
static float sync_expected_tick_diff = 0.0f;
static float sync_integral = 0.0f;

/**
 * One-time setup for a wheel: binds it to its physical hardware and zeroes
 * its running state. Call once per wheel at task start, before the
 * connection-cycle loop -- the hardware identity never changes across
 * reconnects, only the ROS2 objects do.
 */
static void wheel_init(Wheel *w,
                        TIM_HandleTypeDef *encoder_tim,
                        TIM_HandleTypeDef *pwm_tim, uint32_t pwm_channel,
                        GPIO_TypeDef *in1_port, uint16_t in1_pin,
                        GPIO_TypeDef *in2_port, uint16_t in2_pin,
                        float kp, float ki, float kd)
{
  w->encoder_tim = encoder_tim;
  w->pwm_tim = pwm_tim;
  w->pwm_channel = pwm_channel;
  w->in1_port = in1_port;
  w->in1_pin = in1_pin;
  w->in2_port = in2_port;
  w->in2_pin = in2_pin;
  w->kp = kp;
  w->ki = ki;
  w->kd = kd;

  w->last_count = (int32_t)__HAL_TIM_GET_COUNTER(encoder_tim);
  w->cumulative_ticks = 0;
  w->measured_rps = 0.0f;
  w->target_v = 0.0f;
  w->pid_integral = 0.0f;
  w->pid_prev_error = 0.0f;
}

/**
 * One control-period step for a single wheel: reads its encoder, runs
 * feedforward (+PID if enabled) against its own target_v, and drives its
 * own motor pins. Called once per wheel per loop iteration -- see the
 * bottom of the file for why that's safe from cross-wheel mixups.
 *
 * dt_s is the *measured* time (via HAL_GetTick()) since this wheel was last
 * stepped, not the nominal CONTROL_PERIOD_S. The loop is nominally 20Hz
 * (osDelay(50)), but every ~40th iteration also does an agent ping that can
 * block up to 100ms, and rclc_executor_spin_some() itself can take up to
 * 10ms -- if compute_wheel_rps() divided by a fixed 0.05s regardless, any
 * iteration that actually took longer would make that sample's rps look
 * like a brief speed spike (more ticks really did accumulate, just over
 * more real time than assumed), even though nothing physically changed.
 * Using the real elapsed time removes that artifact.
 *
 * rps_trim is the tick-sync correction from the main loop (see
 * ENABLE_TICK_SYNC) -- added to the PID's target only, so it nudges this
 * wheel toward/away from its partner without changing the feedforward
 * baseline v_to_duty() computes from the actual commanded target_v.
 */
static void wheel_step(Wheel *w, float dt_s, float rps_trim)
{
  int32_t delta = 0;
  float rps = compute_wheel_rps(w->encoder_tim, &w->last_count, &delta, dt_s);
  w->measured_rps = rps;
  w->cumulative_ticks += delta;

  /* A target of exactly 0 bypasses the PID and forces duty to 0 outright --
     whatever the PID's accumulated state, "send 0 and the robot stops" must
     always hold -- and resets this wheel's integral/prev_error so a future
     move doesn't inherit stale state from being parked. */
  int32_t duty;
  if (w->target_v == 0.0f) {
    duty = 0;
    w->pid_integral = 0.0f;
    w->pid_prev_error = 0.0f;
  } else {
    float duty_f = (float)v_to_duty(w->target_v);
#if ENABLE_WHEEL_PID
    float target_rps = v_to_target_rps(w->target_v) + rps_trim;
    duty_f += wheel_pid_step(target_rps, w->measured_rps, w->kp, w->ki, w->kd,
                              &w->pid_integral, &w->pid_prev_error, dt_s);
#endif
    duty = (int32_t)duty_f;
    if (duty >  MAX_DUTY) duty =  MAX_DUTY;
    if (duty < -MAX_DUTY) duty = -MAX_DUTY;
  }

  set_motor(w->pwm_tim, w->pwm_channel, w->in1_port, w->in1_pin, w->in2_port, w->in2_pin, duty);
}

/**
 * Forces a wheel to a full stop: zero target, zero PID state, zero duty out
 * to the motor immediately. Used on agent disconnect.
 */
static void wheel_stop(Wheel *w)
{
  w->target_v = 0.0f;
  w->pid_integral = 0.0f;
  w->pid_prev_error = 0.0f;
  set_motor(w->pwm_tim, w->pwm_channel, w->in1_port, w->in1_pin, w->in2_port, w->in2_pin, 0);
}

/* ==== MPU6050 IMU (I2C1, PB8/PB9) ==========================================
 * 6-axis accel+gyro only -- no onboard sensor fusion, so orientation is left
 * unpopulated. sensor_msgs/Imu's own doc comment says to signal that by
 * setting element 0 of orientation_covariance to -1 (done once at message
 * setup in StartMicroROSTask, not here). angular_velocity/linear_acceleration
 * covariances are left at 0 ("unknown") rather than guessing numbers --
 * fill these in with real characterized noise if this ever feeds an EKF. */
#define MPU6050_I2C_ADDR          (0x68 << 1)  /* AD0 tied low */
#define MPU6050_REG_WHO_AM_I      0x75
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_CONFIG        0x1A
#define MPU6050_REG_SMPLRT_DIV    0x19
#define MPU6050_REG_GYRO_CONFIG   0x1B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_ACCEL_XOUT_H  0x3B
#define MPU6050_WHO_AM_I_VALUE    0x68
#define MPU6050_I2C_TIMEOUT_MS    10

#define MPU6050_ACCEL_SENS_LSB_PER_G   8192.0f  /* +/-4g full scale (ACCEL_CONFIG=0x08) */
#define MPU6050_GYRO_SENS_LSB_PER_DPS  65.5f    /* +/-500dps full scale (GYRO_CONFIG=0x08) */
#define STANDARD_GRAVITY_MPS2          9.80665f

/**
 * One-time IMU bring-up: confirms the chip answers as an MPU6050 (WHO_AM_I),
 * wakes it from its power-on sleep state, and picks a DLPF/sample-rate/
 * full-scale configuration. Returns false (without touching imu_ready's
 * caller) on any failure -- e.g. the IMU isn't wired up -- so the rest of
 * the robot (motors/encoders/cmd_vel) keeps working with the IMU simply
 * absent, rather than this blocking or crashing startup.
 */
static bool mpu6050_init(I2C_HandleTypeDef *hi2c)
{
  uint8_t who_am_i = 0;
  if (HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_WHO_AM_I,
                        I2C_MEMADD_SIZE_8BIT, &who_am_i, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
    return false;
  }
  if (who_am_i != MPU6050_WHO_AM_I_VALUE) {
    return false;
  }

  uint8_t pwr_mgmt_1   = 0x01; /* wake up, gyro X as clock reference */
  uint8_t config       = 0x03; /* DLPF: ~44Hz accel / ~42Hz gyro bandwidth */
  uint8_t smplrt_div   = 9;    /* gyro output rate 1kHz / (1+9) = 100Hz internal sample rate */
  uint8_t gyro_config  = 0x08; /* +/-500 dps */
  uint8_t accel_config = 0x08; /* +/-4g */

  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_PWR_MGMT_1,
                         I2C_MEMADD_SIZE_8BIT, &pwr_mgmt_1, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK) return false;
  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_CONFIG,
                         I2C_MEMADD_SIZE_8BIT, &config, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK) return false;
  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_SMPLRT_DIV,
                         I2C_MEMADD_SIZE_8BIT, &smplrt_div, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK) return false;
  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_GYRO_CONFIG,
                         I2C_MEMADD_SIZE_8BIT, &gyro_config, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK) return false;
  if (HAL_I2C_Mem_Write(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
                         I2C_MEMADD_SIZE_8BIT, &accel_config, 1, MPU6050_I2C_TIMEOUT_MS) != HAL_OK) return false;

  return true;
}

/**
 * Burst-reads all 14 data registers (accel + temp + gyro) in one I2C
 * transaction and fills in msg's angular_velocity/linear_acceleration.
 * Does not touch header/orientation/covariances -- those are set up once
 * elsewhere since they don't change per-sample.
 */
static bool mpu6050_read(I2C_HandleTypeDef *hi2c, sensor_msgs__msg__Imu *msg)
{
  uint8_t raw[14];
  if (HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                        I2C_MEMADD_SIZE_8BIT, raw, sizeof(raw), MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
    return false;
  }

  int16_t accel_x_raw = (int16_t)((raw[0]  << 8) | raw[1]);
  int16_t accel_y_raw = (int16_t)((raw[2]  << 8) | raw[3]);
  int16_t accel_z_raw = (int16_t)((raw[4]  << 8) | raw[5]);
  /* raw[6..7] = temperature, unused */
  int16_t gyro_x_raw  = (int16_t)((raw[8]  << 8) | raw[9]);
  int16_t gyro_y_raw  = (int16_t)((raw[10] << 8) | raw[11]);
  int16_t gyro_z_raw  = (int16_t)((raw[12] << 8) | raw[13]);

  msg->linear_acceleration.x = ((float)accel_x_raw / MPU6050_ACCEL_SENS_LSB_PER_G) * STANDARD_GRAVITY_MPS2;
  msg->linear_acceleration.y = ((float)accel_y_raw / MPU6050_ACCEL_SENS_LSB_PER_G) * STANDARD_GRAVITY_MPS2;
  msg->linear_acceleration.z = ((float)accel_z_raw / MPU6050_ACCEL_SENS_LSB_PER_G) * STANDARD_GRAVITY_MPS2;

  msg->angular_velocity.x = ((float)gyro_x_raw / MPU6050_GYRO_SENS_LSB_PER_DPS) * ((float)M_PI / 180.0f);
  msg->angular_velocity.y = ((float)gyro_y_raw / MPU6050_GYRO_SENS_LSB_PER_DPS) * ((float)M_PI / 180.0f);
  msg->angular_velocity.z = ((float)gyro_z_raw / MPU6050_GYRO_SENS_LSB_PER_DPS) * ((float)M_PI / 180.0f);

  return true;
}

static bool imu_ready = false;

/**
 * /cmd_vel (geometry_msgs/Twist) subscription callback -- called by the
 * rclc executor whenever a new Twist arrives from the agent.
 *
 * Twist is robot-frame: linear.x = forward speed (m/s), angular.z = turn
 * rate (rad/s, positive = counter-clockwise). A differential-drive robot
 * can't strafe or pivot about an arbitrary point, only set each wheel's own
 * speed, so the robot-frame command is converted to two wheel targets:
 *
 *   v_left  = linear.x - angular.z * (track/2)
 *   v_right = linear.x + angular.z * (track/2)
 *
 * Straight (angular.z=0) drives both wheels equally; turning speeds one
 * wheel up and slows/reverses the other, by an amount proportional to how
 * far each wheel sits from the centerline (track/2).
 */
void cmd_vel_callback(const void * msgin)
{
  const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;

  float linear_x  = (float)msg->linear.x;
  float angular_z = (float)msg->angular.z;

  /* angular_z > 0 must always mean CCW (turn left), angular_z < 0 always CW
     (turn right), regardless of linear_x's sign -- a linear_x<=0 sign-flip
     workaround used to live here to match an observed hardware quirk where
     pure rotation/reverse+turn spun opposite to forward+turn for the same
     angular_z, but that flip made nav2's continuously-varying linear_x
     (which dips through 0/negative during ordinary turns, not just true
     reverse) flip direction mid-turn. Removed -- if the underlying
     hardware asymmetry resurfaces, fix it at the source instead of
     patching based on linear_x's instantaneous sign again. */
  /* 2026-09-03: with both wheels' IN1/IN2 now matched to each other (see
     the wheel_init() fix above), a positive linear_x drove the whole robot
     backward -- confirmed by the user, both wheels agreed with each other,
     just in the wrong direction. Negating only the linear_x contribution
     here (not angular_z) flips "forward" to match what the user means by
     it, without touching set_motor()/the encoder sign invariant (duty and
     encoder still agree, see compute_wheel_rps()'s doc comment) and
     without assuming anything about angular_z's CCW/CW sense, which
     hasn't been verified backward the same way -- if pure rotation also
     turns out reversed, fix that separately once confirmed instead of
     guessing it's the same bug. */
  float v_left  = -linear_x - angular_z * (WHEEL_TRACK_M / 2.0f);
  float v_right = -linear_x + angular_z * (WHEEL_TRACK_M / 2.0f);

  /* 2026-09-03: upstream (nav2 -> amr_cmd_vel_scaler on the Pi) can hand us
     linear_x well past CMD_V_MAX -- when both v_left/v_right land above
     CMD_V_MAX, v_to_duty() clamps *each* independently to the same
     MAX_DUTY, silently erasing whatever left/right difference angular_z
     added (turning while driving stopped showing up as any real encoder
     difference). Rather than rely on the Pi-side node also being fixed
     and redeployed, scale both wheels down together, proportionally,
     whenever the larger-magnitude one would exceed CMD_V_MAX -- this
     preserves the v_left/v_right *ratio* (so the turn shape survives)
     instead of two independent per-wheel clamps flattening it. v_to_duty()
     keeps its own per-wheel clamp too, as a last-resort safety net. */
  float max_mag = fmaxf(fabsf(v_left), fabsf(v_right));
  if (max_mag > CMD_V_MAX) {
    float scale = CMD_V_MAX / max_mag;
    v_left  *= scale;
    v_right *= scale;
  }

  wheel_left.target_v  = v_left;
  wheel_right.target_v = v_right;
}

/**
 * micro-ROS task: brings up the ROS2 client stack on the UART transport,
 * creates a node with 5 publishers + 1 subscriber, then loops reading
 * encoders/IMU and servicing ROS2 traffic.
 *
 * The outer for(;;) is a "connection cycle": wait for the agent, create
 * node/publishers/subscription/executor, run the normal work loop (publish
 * encoder data + handle cmd_vel) until a periodic ping shows the agent is
 * gone, tear everything down, and retry from the top. Restarting the agent
 * doesn't require resetting the board.
 */
void StartMicroROSTask(void *argument)
{
  /* Tell the rmw layer how to move bytes: over huart2 (shared with the
     ST-Link virtual COM port), using the open/close/write/read functions in
     dma_transport.c. Everything above this (rcl, rclc) is unaware it's
     using UART instead of a real network. One-time setup, not repeated per
     reconnect. */
  rmw_uros_set_custom_transport(
    true,
    (void *) &huart2,
    cubemx_transport_open,
    cubemx_transport_close,
    cubemx_transport_write,
    cubemx_transport_read);

  /* Route rcutils's default allocator to the FreeRTOS-backed one so every
     ROS2 allocation comes from the dedicated micro-ROS heap in
     custom_memory_manager.c instead of newlib malloc/free, keeping it out
     of FreeRTOS's own heap. Also one-time setup. */
  rcl_allocator_t freeRTOS_allocator = rcutils_get_zero_initialized_allocator();
  freeRTOS_allocator.allocate = microros_allocate;
  freeRTOS_allocator.deallocate = microros_deallocate;
  freeRTOS_allocator.reallocate = microros_reallocate;
  freeRTOS_allocator.zero_allocate = microros_zero_allocate;

  if (!rcutils_set_default_allocator(&freeRTOS_allocator)) {
    Error_Handler();
  }

  /* ==== Wiring -- THE ONLY PLACE physical left/right identity is decided.
     Everything else in this file just says "the wheel" and works off
     whichever Wheel instance it's given:
       left  = ENB(TIM3_CH2/PA7) + IN3/IN4(MOTOR_L_IN1/IN2, PA10/PB3)
               + encoder on A0/A1 (PA0/PA1 = htim5)
       right = ENA(TIM3_CH1/PA6) + the other IN pair (MOTOR_R_IN1/IN2, PB4/PB5)
               + encoder on D7/D8 (PA8/PA9 = htim1)
     (2026-08-26: the user rewired ENA/ENB and the right motor's IN1/IN2 by
     hand while chasing the forward/reverse direction bug, and this file's
     wheel_init() args were changed to match -- but that produced a
     left/right identity mismatch between each wheel's motor and its
     encoder [PID chasing one wheel's target while reading the other
     wheel's encoder], with runaway-looking rps values as the symptom.
     Reverted to this original, long-confirmed-working mapping since the
     incremental fixes on top of the rewire weren't converging. If the
     physical rewire is still in place on the board, that mismatch needs
     to be resolved by re-checking the actual wiring against this comment,
     not by patching wheel_init() again.)
     If a wheel or its encoder is ever replaced, re-check by spinning it
     forward by hand and confirming encoder/{left,right}_rps reads
     positive -- if it reads negative, swap that encoder's A/B leads rather
     than adding a software inversion. */
  /* 2026-08-26: today's PWM-channel swap (TIM_CHANNEL_1<->2 between left
     and right, to match a described ENA/ENB rewire) left left/right
     *identity* swapped -- e.g. a target meant only for left also moved
     the wheel read by the right encoder, and vice versa, which also
     explains the runaway-looking rps values seen mid-debugging (each
     wheel's PID was chasing a setpoint while reading the *other* wheel's
     encoder, since the encoder_tim args here were never touched). Fully
     reverted to the pre-2026-08-26 configuration -- both the PWM channel
     and the IN1/IN2 argument order -- since incremental fixes on top of
     the swap weren't converging. This matches the long-standing,
     previously-confirmed-working baseline. */
  /* 2026-08-26: matches the physical wiring as of right now (user confirmed
     not touching it further) -- left ENB=D12/PA6/TIM3_CH1, IN3/IN4=D2/D3;
     right ENA=D11/PA7/TIM3_CH2, IN1/IN2=D4/D5=PB5/PB4 (the *opposite*
     physical order from the MOTOR_R_IN1/IN2 macros, hence the swapped args
     below). TIM3_CH1=PA6 / TIM3_CH2=PA7 confirmed directly from tim.c's
     generated GPIO comments, not assumed.

     2026-09-03: forward cmd_vel was spinning the left wheel in reverse --
     turns out MOTOR_L_IN1/IN2 don't match this wheel's actual physical IN
     wiring either (same class of mismatch as the right wheel above), so
     swapped the args here too. Only the direction-pin order changed --
     encoder_tim (&htim5) and the PWM channel are untouched, so this can't
     reproduce the old PID-chasing-the-wrong-encoder identity bug. If the
     left motor/H-bridge wiring is ever redone, re-verify by spinning it
     forward by hand and confirming encoder/left_rps reads positive *and*
     a forward cmd_vel turns it the same way -- if forward still spins it
     backward, the physical IN1/IN2 pins moved again and this order needs
     re-checking, not another blind swap. */
  wheel_init(&wheel_left,  &htim5, &htim3, TIM_CHANNEL_1,
             MOTOR_L_IN2_GPIO_Port, MOTOR_L_IN2_Pin, MOTOR_L_IN1_GPIO_Port, MOTOR_L_IN1_Pin,
             WHEEL_PID_KP_LEFT, WHEEL_PID_KI_LEFT, WHEEL_PID_KD_LEFT);
  wheel_init(&wheel_right, &htim1, &htim3, TIM_CHANNEL_2,
             MOTOR_R_IN2_GPIO_Port, MOTOR_R_IN2_Pin, MOTOR_R_IN1_GPIO_Port, MOTOR_R_IN1_Pin,
             WHEEL_PID_KP_RIGHT, WHEEL_PID_KI_RIGHT, WHEEL_PID_KD_RIGHT);

  /* IMU is optional from the rest of the robot's point of view -- if it
     doesn't answer on I2C1 (not wired up, wrong address, faulty), imu_ready
     stays false and every connection cycle below just skips publishing it. */
  imu_ready = mpu6050_init(&hi2c1);

  /* Outer "connection cycle" loop -- one (re)connection per iteration. */
  for (;;)
  {
    /* Wait for the agent to answer -- covers the board booting before the
       agent is up, and the agent restarting later. */
    while (rmw_uros_ping_agent(100, 1) != RMW_RET_OK) {
      osDelay(1000);
    }

    rclc_support_t support;
    rcl_allocator_t allocator = rcl_get_default_allocator();
    rcl_node_t node;

    rclc_support_init(&support, 0, NULL, &allocator);
    rclc_node_init_default(&node, "amr_node", "", &support);

    rcl_publisher_t enc_left_publisher;
    rcl_publisher_t enc_right_publisher;
    rcl_publisher_t enc_left_ticks_publisher;
    rcl_publisher_t enc_right_ticks_publisher;
    rcl_publisher_t imu_publisher;
    rcl_publisher_t speed_publisher;

    std_msgs__msg__Float32 enc_left_msg;
    std_msgs__msg__Float32 enc_right_msg;
    std_msgs__msg__Int32 enc_left_ticks_msg;
    std_msgs__msg__Int32 enc_right_ticks_msg;
    std_msgs__msg__Float32 speed_msg;

    sensor_msgs__msg__Imu imu_msg;
    sensor_msgs__msg__Imu__init(&imu_msg);
    rosidl_runtime_c__String__assign(&imu_msg.header.frame_id, "imu_link");
    /* __init() only fills fields with a default declared in the .msg
       (that's why orientation.{x,y,z,w} came out as identity above) -- the
       fixed-size covariance arrays and header.stamp have no such default,
       so __init() leaves them untouched (i.e. whatever was already on the
       stack -- FreeRTOS paints unused task stack with 0xA5, which is
       exactly what showed up when this went unzeroed: header.stamp read as
       0xA5A5A5A5 and every uninitialized covariance slot read as
       -2.4983353906949635e-127, the double that bit pattern decodes to).
       Zero everything explicitly instead of relying on __init(). */
    memset(&imu_msg.header.stamp, 0, sizeof(imu_msg.header.stamp));
    memset(imu_msg.orientation_covariance, 0, sizeof(imu_msg.orientation_covariance));
    memset(imu_msg.angular_velocity_covariance, 0, sizeof(imu_msg.angular_velocity_covariance));
    memset(imu_msg.linear_acceleration_covariance, 0, sizeof(imu_msg.linear_acceleration_covariance));
    /* No orientation estimate from this sensor -- element 0 of
       orientation_covariance = -1 tells subscribers to disregard
       msg.orientation rather than mistake it for a real (identity) reading. */
    imu_msg.orientation_covariance[0] = -1.0;

    rclc_publisher_init_default(
      &enc_left_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
      "encoder/left_rps");

    rclc_publisher_init_default(
      &enc_right_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
      "encoder/right_rps");

    /* Raw accumulated ticks -- no gear-ratio/PPR assumption baked in, for
       verifying ENCODER_TICKS_PER_REV. */
    rclc_publisher_init_default(
      &enc_left_ticks_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
      "encoder/left_raw_ticks");

    rclc_publisher_init_default(
      &enc_right_ticks_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
      "encoder/right_raw_ticks");

    rclc_publisher_init_default(
      &imu_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
      "imu/data_raw");

    /* Robot-frame linear speed in m/s, for consumers that just want a
       single "how fast" number instead of per-wheel rps (e.g. a frontend
       dashboard) -- same (v_left+v_right)/2 average wheel_odometry_node.py
       uses for /odom's twist.linear.x, computed here directly so it's
       available without that node running. */
    rclc_publisher_init_default(
      &speed_publisher, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32),
      "speed_mps");

    rcl_subscription_t cmd_vel_subscriber;
    geometry_msgs__msg__Twist cmd_vel_msg;

    rclc_subscription_init_default(
      &cmd_vel_subscriber, &node,
      ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
      "cmd_vel");

    /* 1 handle: just the cmd_vel subscription. ON_NEW_DATA runs the
       callback only when a genuinely new message has arrived. */
    rclc_executor_t executor;
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_subscription(
      &executor, &cmd_vel_subscriber, &cmd_vel_msg,
      &cmd_vel_callback, ON_NEW_DATA);

    /* Inner "normal operation" loop, nominally 20Hz (osDelay(50) => 50ms per
       iteration). Exits (connected = false) once a periodic ping shows the
       agent disappeared. */
    bool connected = true;
    uint32_t loop_count = 0;
    /* Primed as if the last iteration ended 50ms ago, so the first real
       dt_s below comes out close to CONTROL_PERIOD_S instead of ~0. */
    uint32_t last_loop_tick_ms = HAL_GetTick() - 50;

    while (connected)
    {
      uint32_t now_ms = HAL_GetTick();
      float dt_s = (float)(now_ms - last_loop_tick_ms) / 1000.0f;
      last_loop_tick_ms = now_ms;
      if (dt_s <= 0.0f) dt_s = CONTROL_PERIOD_S; /* clock hasn't ticked yet -- fall back rather than divide by ~0 */

      /* Tick-sync: advance the "should be" tick difference by however much
         this cycle's commanded speeds imply, compare against the actual
         accumulated difference (as of the end of the *previous* cycle --
         wheel_step() below is what moves cumulative_ticks forward), and
         turn any gap into a small opposing rps trim for each side. */
      float sync_trim_rps = 0.0f;
#if ENABLE_TICK_SYNC
      float target_rps_left  = v_to_target_rps(wheel_left.target_v);
      float target_rps_right = v_to_target_rps(wheel_right.target_v);
      sync_expected_tick_diff += (target_rps_left - target_rps_right) * ENCODER_TICKS_PER_REV * dt_s;

      float actual_tick_diff = (float)(wheel_left.cumulative_ticks - wheel_right.cumulative_ticks);
      float sync_error = actual_tick_diff - sync_expected_tick_diff;

      sync_integral += sync_error * dt_s;
      if (sync_integral >  SYNC_INTEGRAL_MAX) sync_integral =  SYNC_INTEGRAL_MAX;
      if (sync_integral < -SYNC_INTEGRAL_MAX) sync_integral = -SYNC_INTEGRAL_MAX;

      /* sync_error > 0 means left has out-ticked right relative to what was
         commanded, i.e. left is running ahead -- trim left's target down
         and right's up by the same amount to close the gap symmetrically. */
      sync_trim_rps = SYNC_KP * sync_error + SYNC_KI * sync_integral;
      if (sync_trim_rps >  SYNC_TRIM_MAX_RPS) sync_trim_rps =  SYNC_TRIM_MAX_RPS;
      if (sync_trim_rps < -SYNC_TRIM_MAX_RPS) sync_trim_rps = -SYNC_TRIM_MAX_RPS;
#endif

      wheel_step(&wheel_left,  dt_s, -sync_trim_rps);
      wheel_step(&wheel_right, dt_s,  sync_trim_rps);

      enc_left_msg.data  = wheel_left.measured_rps;
      enc_right_msg.data = wheel_right.measured_rps;
      enc_left_ticks_msg.data  = wheel_left.cumulative_ticks;
      enc_right_ticks_msg.data = wheel_right.cumulative_ticks;

      /* 2026-09-04: this topic reports speed magnitude only, not signed
         velocity -- what sign combination of measured_rps/cmd_vel actually
         means "moving forward from the user's point of view" was guessed
         wrong twice in a row (see git history on this line), so rather than
         guess a third time, sidestep it with fabsf(). If a consumer ever
         needs direction (forward vs reverse), get that from /cmd_vel's own
         linear_x sign instead of trying to recover it from this value. */
      float speed_left_mps  = wheel_left.measured_rps  * WHEEL_CIRCUMFERENCE_M;
      float speed_right_mps = wheel_right.measured_rps * WHEEL_CIRCUMFERENCE_M;
      speed_msg.data = fabsf((speed_left_mps + speed_right_mps) / 2.0f);

      rcl_publish(&enc_left_publisher, &enc_left_msg, NULL);
      rcl_publish(&enc_right_publisher, &enc_right_msg, NULL);
      rcl_publish(&enc_left_ticks_publisher, &enc_left_ticks_msg, NULL);
      rcl_publish(&enc_right_ticks_publisher, &enc_right_ticks_msg, NULL);
      rcl_publish(&speed_publisher, &speed_msg, NULL);

      if (imu_ready && mpu6050_read(&hi2c1, &imu_msg)) {
        rcl_publish(&imu_publisher, &imu_msg, NULL);
      }

      /* Give the executor up to 10ms to check for and handle a new cmd_vel
         message (cmd_vel_callback() runs synchronously from here). */
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

      /* Ping roughly every 2s (50ms * 40) rather than every loop -- pinging
         every iteration would add latency to the 20Hz publish/control
         cadence. */
      loop_count++;
      if (loop_count % 40 == 0) {
        if (rmw_uros_ping_agent(100, 1) != RMW_RET_OK) {
          connected = false;
        }
      }

      osDelay(50);
    }

    /* Disconnected -- stop the motors first. With no agent there's no way
       to receive a fresh cmd_vel, so the last command must not keep
       running. */
    wheel_stop(&wheel_left);
    wheel_stop(&wheel_right);
    sync_expected_tick_diff = 0.0f;
    sync_integral = 0.0f;

    /* Tear down everything created above, in reverse order. Skipping this
       before looping back to rclc_support_init() would leak the previous
       session's micro-ROS heap usage, shrinking available heap a little on
       every reconnect until allocations start failing. */
    rclc_executor_fini(&executor);
    rcl_subscription_fini(&cmd_vel_subscriber, &node);
    rcl_publisher_fini(&enc_left_publisher, &node);
    rcl_publisher_fini(&enc_right_publisher, &node);
    rcl_publisher_fini(&enc_left_ticks_publisher, &node);
    rcl_publisher_fini(&enc_right_ticks_publisher, &node);
    rcl_publisher_fini(&imu_publisher, &node);
    rcl_publisher_fini(&speed_publisher, &node);
    sensor_msgs__msg__Imu__fini(&imu_msg);
    rcl_node_fini(&node);
    rclc_support_fini(&support);

    /* Back to the top -- wait for the agent again. Reconnects automatically
       whenever it comes back. */
  }
}
/* USER CODE END Application */
