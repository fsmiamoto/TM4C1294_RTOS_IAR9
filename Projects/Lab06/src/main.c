#include "cmsis_os2.h"
#include "driverbuttons.h"
#include "driverleds.h"
#include "system_TM4C1294.h"
#include <stdint.h>

#define LEDs LED1 | LED2 | LED3 | LED4
#define BUTTONS USW1 | USW2

#define NUM_OF_WORKERS (sizeof(workers) / sizeof(pwm_worker_t))
#define WORKER_QUEUE_SIZE 8
#define MANAGER_QUEUE_SIZE 8
#define DEBOUNCE_TICKS 300
#define NO_WAIT 0
#define MSG_PRIO 0

#define PWM_PERIOD 10
#define BLINK_PERIOD 50

#define ButtonPressed(b) !ButtonRead(b) // Push buttons are low-active

typedef enum {
  SW1_PRESSED,
  SW2_PRESSED,
} button_event_t;

typedef uint8_t manager_event_t;

typedef struct {
  osMessageQueueId_t queue_id;
  uint8_t led_number;
  uint8_t duty_cycle;
  uint16_t period;
} pwm_worker_args_t;

typedef struct {
  osThreadId_t thread_id;
  pwm_worker_args_t args;
} pwm_worker_t;

typedef struct {
  osThreadId_t thread_id;
  osMessageQueueId_t queue_id;
} pwm_manager_t;

osMutexId_t leds_mutex_id;
const osMutexAttr_t leds_mutex_attr = {"LEDMutex", osMutexPrioInherit, NULL,
                                       0U};

pwm_manager_t manager;
pwm_worker_t workers[] = {
    {.args = {.led_number = LED1, .period = BLINK_PERIOD, .duty_cycle = 5}},
    {.args = {.led_number = LED2, .period = PWM_PERIOD, .duty_cycle = 2}},
    {.args = {.led_number = LED3, .period = PWM_PERIOD, .duty_cycle = 8}},
    {.args = {.led_number = LED4, .period = PWM_PERIOD, .duty_cycle = 0}},
};

void SwitchOn(uint8_t led) {
  osMutexAcquire(leds_mutex_id, osWaitForever);
  LEDOn(led);
  osMutexRelease(leds_mutex_id);
}

void SwitchOff(uint8_t led) {
  osMutexAcquire(leds_mutex_id, osWaitForever);
  LEDOff(led);
  osMutexRelease(leds_mutex_id);
}

void GPIOJ_Handler(void) {
  static uint32_t last_msg_sw1, last_msg_sw2;

  ButtonIntClear(USW1 | USW2);

  if (ButtonPressed(USW1)) {
    if ((osKernelGetTickCount() - last_msg_sw1) < DEBOUNCE_TICKS)
      return;

    button_event_t event = SW1_PRESSED;
    osStatus_t status =
        osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    if (status == osOK)
      last_msg_sw1 = osKernelGetTickCount();
  }

  if (ButtonPressed(USW2)) {
    if ((osKernelGetTickCount() - last_msg_sw2) < DEBOUNCE_TICKS)
      return;

    button_event_t event = SW2_PRESSED;
    osStatus_t status =
        osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    if (status == osOK)
      last_msg_sw2 = osKernelGetTickCount();
  }
}

void pwm_manager(void *arg) {
  button_event_t event;
  uint8_t selected_worker = 0;

  for (;;) {
    osMessageQueueGet(manager.queue_id, &event, NULL, osWaitForever);

    if (event == SW1_PRESSED) {
      workers[selected_worker].args.period = PWM_PERIOD;
      selected_worker = (selected_worker + 1) % NUM_OF_WORKERS;
      workers[selected_worker].args.period = BLINK_PERIOD;
    }

    if (event == SW2_PRESSED) {
      uint8_t duty_cycle = workers[selected_worker].args.duty_cycle;
      duty_cycle += 1;
      if (duty_cycle > PWM_PERIOD)
        duty_cycle = 0;
      workers[selected_worker].args.duty_cycle = duty_cycle;
    }

    // Parameters may have changed, notify workers
    for (int i = 0; i < NUM_OF_WORKERS; i++) {
      manager_event_t e;
      osMessageQueuePut(workers[i].args.queue_id, &e, MSG_PRIO, osWaitForever);
    }
  }
}

void pwm_worker(void *arg) {
  pwm_worker_args_t *args = (pwm_worker_args_t *)arg;
  manager_event_t event;
  osStatus_t status;

  osMessageQueueId_t queue_id = args->queue_id;
  uint8_t led_number = args->led_number;
  uint8_t duty_cycle = args->duty_cycle;
  uint16_t period = args->period;

  for (;;) {
    status = osMessageQueueGet(queue_id, &event, NULL, NO_WAIT);
    if (status == osOK) {
      // Message received, update values
      duty_cycle = args->duty_cycle;
      period = args->period;
    }

    SwitchOn(led_number);
    osDelay(duty_cycle);
    SwitchOff(led_number);
    osDelay(period - duty_cycle);
  }
}

void main(void) {
  LEDInit(LEDs);
  ButtonInit(USW1 | USW2);
  ButtonIntEnable(USW1 | USW2);

  osKernelInitialize();

  manager.thread_id = osThreadNew(pwm_manager, NULL, NULL);
  manager.queue_id =
      osMessageQueueNew(MANAGER_QUEUE_SIZE, sizeof(button_event_t), NULL);

  leds_mutex_id = osMutexNew(&leds_mutex_attr);

  for (int i = 0; i < NUM_OF_WORKERS; i++) {
    workers[i].args.queue_id =
        osMessageQueueNew(WORKER_QUEUE_SIZE, sizeof(manager_event_t), NULL);
    workers[i].thread_id = osThreadNew(pwm_worker, &workers[i].args, NULL);
  }

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}
