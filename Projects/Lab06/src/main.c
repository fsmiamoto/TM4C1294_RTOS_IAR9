#include "cmsis_os2.h"
#include "driverbuttons.h"
#include "driverleds.h"
#include "system_TM4C1294.h"
#include <stdint.h>

#define LEDs LED1 | LED2 | LED3 | LED4
#define BUTTONS USW1 | USW2

#define NUM_OF_WORKERS sizeof(workers)/sizeof(pwm_worker)
#define WORKER_QUEUE_SIZE 8
#define MANAGER_QUEUE_SIZE 8
#define DEBOUNCE_TICKS 200
#define NO_WAIT 0
#define MSG_PRIO 0 

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
  {.args = {.led_number = LED1}},
  {.args = {.led_number = LED2}},
  {.args = {.led_number = LED3}},
  {.args = {.led_number = LED4}},
};


void GPIOJ_Handler(void) {
  static uint32_t last_tick_sw1, last_tick_sw2;

  if(ButtonPressed(USW1)){
    ButtonIntClear(USW1);
    if((osKernelGetTickCount() - last_tick_sw1) < DEBOUNCE_TICKS)
      return;
    button_event_t event = SW1_PRESSED;
    osStatus_t status = osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    return;
  }

  if(ButtonPressed(USW2)){
    ButtonIntClear(USW2);
    if((osKernelGetTickCount() - last_tick_sw2) < DEBOUNCE_TICKS)
      return;
    button_event_t event = SW2_PRESSED;
    osStatus_t status = osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    return;
  }
}

void _manager(void* arg) {
  button_event_t event;
  osStatus_t status;
  uint8_t count = 0;


  for(;;) {
    status = osMessageQueueGet(manager.queue_id, &event, NULL, osWaitForever);
    if(status != osOK)
      continue;

    if(event == SW1_PRESSED) {
      count = 0xF;
    }

    if(event == SW2_PRESSED){
      count = 0;
    }

    osMutexAcquire(leds_mutex_id, osWaitForever);
    LEDWrite(LEDs, count);
    osMutexRelease(leds_mutex_id);
  }
}

void _worker(void *arg) {
  pwm_worker_args_t* args = (pwm_worker_args_t*)arg; 
  manager_event_t event; 
  osStatus_t status;

  uint8_t is_selected = 0, value = 0;

  for(;;) {
    status = osMessageQueueGet(args->queue_id,&event,NULL,NO_WAIT);
    if(status == osOK) {
      // Message received
      is_selected = event.is_selected;
    }

    value = is_selected ? 1 << (args->led_number - 1) : 0;

    osMutexAcquire(leds_mutex_id, osWaitForever);
    LEDWrite(args->led_number, value);
    osMutexRelease(leds_mutex_id);
  }
}

void main(void) {
  LEDInit(LEDs);
  ButtonInit(USW1|USW2);
  ButtonIntEnable(USW1|USW2);

  osKernelInitialize();

  manager.thread_id = osThreadNew(_manager, NULL, NULL);
  manager.queue_id = osMessageQueueNew(MANAGER_QUEUE_SIZE, sizeof(button_event_t), NULL);

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}
