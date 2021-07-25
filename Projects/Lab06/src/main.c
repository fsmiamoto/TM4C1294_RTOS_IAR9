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
#define DEBOUNCE_TICKS 200
#define NO_WAIT 0
#define MSG_PRIO 0
#define MAX_DUTY_CYCLE 10

#define ButtonPressed(b) !ButtonRead(b)

typedef enum {
  SW1_PRESSED,
  SW2_PRESSED,
} button_event_t;

typedef struct {
  uint8_t is_selected;
  uint8_t should_increase_brightness;
} manager_event_t;

typedef struct {
  osMessageQueueId_t queue_id;
  uint8_t led_number;
  uint8_t duty_cycle;
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

pwm_manager_t manager;
pwm_worker_t workers[] = {
    {.args = {.led_number = LED1, .duty_cycle = 5}},
    {.args = {.led_number = LED2, .duty_cycle = 2}},
    {.args = {.led_number = LED3, .duty_cycle = 8}},
    {.args = {.led_number = LED4, .duty_cycle = 0}},
};

void GPIOJ_Handler(void) {
  static uint32_t last_tick_sw1, last_tick_sw2;

  if (ButtonPressed(USW1)) {
    ButtonIntClear(USW1);
    if ((osKernelGetTickCount() - last_tick_sw1) < DEBOUNCE_TICKS)
      return;
    button_event_t event = SW1_PRESSED;
    osStatus_t status =
        osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    return;
  }

  if (ButtonPressed(USW2)) {
    ButtonIntClear(USW2);
    if ((osKernelGetTickCount() - last_tick_sw2) < DEBOUNCE_TICKS)
      return;
    button_event_t event = SW2_PRESSED;
    osStatus_t status =
        osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    return;
  }
}

void _manager(void *arg) {
  button_event_t event;
  uint8_t selected_worker = 0;

  for (;;) {
    osMessageQueueGet(manager.queue_id, &event, NULL, osWaitForever);

    if (event == SW1_PRESSED) {
      selected_worker = (selected_worker + 1) % NUM_OF_WORKERS;
    }

    if (event == SW2_PRESSED) {
      workers[selected_worker].args.duty_cycle += 1;
      if (workers[selected_worker].args.duty_cycle > MAX_DUTY_CYCLE)
        workers[selected_worker].args.duty_cycle = 0;
    }

    for (int i = 0; i < NUM_OF_WORKERS; i++) {
      manager_event_t e = {.is_selected = i == selected_worker};
      osMessageQueuePut(workers[i].args.queue_id, &e, MSG_PRIO, osWaitForever);
    }
  }
}

void _worker(void *arg) {
  pwm_worker_args_t *args = (pwm_worker_args_t *)arg;
  manager_event_t event;
  osStatus_t status;

  osMessageQueueId_t queue_id = args->queue_id;
  uint8_t led_number = args->led_number;
  uint8_t duty_cycle = args->duty_cycle;

  for (;;) {
    status = osMessageQueueGet(queue_id, &event, NULL, NO_WAIT);
    if (status == osOK) {
      // Message received, update duty cycle
      duty_cycle = args->duty_cycle;
    }

    osMutexAcquire(leds_mutex_id, osWaitForever);
    LEDOn(led_number);
    osMutexRelease(leds_mutex_id);
    osDelay(duty_cycle);
    osMutexAcquire(leds_mutex_id, osWaitForever);
    LEDOff(led_number);
    osMutexRelease(leds_mutex_id);
    osDelay(MAX_DUTY_CYCLE - duty_cycle);
  }
}

void main(void) {
  LEDInit(LEDs);
  ButtonInit(USW1 | USW2);
  ButtonIntEnable(USW1 | USW2);

  osKernelInitialize();

  manager.thread_id = osThreadNew(_manager, NULL, NULL);
  manager.queue_id =
      osMessageQueueNew(MANAGER_QUEUE_SIZE, sizeof(button_event_t), NULL);

  for (int i = 0; i < NUM_OF_WORKERS; i++) {
    workers[i].args.queue_id =
        osMessageQueueNew(WORKER_QUEUE_SIZE, sizeof(manager_event_t), NULL);
    workers[i].thread_id = osThreadNew(_worker, &workers[i].args, NULL);
  }

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}
