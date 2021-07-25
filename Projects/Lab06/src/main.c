#include "cmsis_os2.h"
#include "driverbuttons.h"
#include "driverleds.h"
#include "system_TM4C1294.h"

#define LEDs LED1 | LED2 | LED3 | LED4
#define BUTTONS USW1 | USW2

#define NUM_OF_WORKERS 4
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

osThreadId_t manager_thread_id;
osThreadId_t worker_thread_ids[NUM_OF_WORKERS];

osMessageQueueId_t worker_queue_ids[4];
osMessageQueueId_t manager_queue_id;
osMutexId_t leds_mutex_id;

void GPIOJ_Handler(void) {
  static uint32_t last_tick_sw1, last_tick_sw2;

  if(ButtonPressed(USW1)){
    ButtonIntClear(USW1);
    if((osKernelGetTickCount() - last_tick_sw1) < DEBOUNCE_TICKS)
      return;
    button_event_t event = SW1_PRESSED;
    osStatus_t status = osMessageQueuePut(manager_queue_id, &event, MSG_PRIO, NO_WAIT);
    return;
  }

  if(ButtonPressed(USW2)){
    ButtonIntClear(USW2);
    if((osKernelGetTickCount() - last_tick_sw2) < DEBOUNCE_TICKS)
      return;
    button_event_t event = SW2_PRESSED;
    osStatus_t status = osMessageQueuePut(manager_queue_id, &event, MSG_PRIO, NO_WAIT);
    return;
  }
}

void manager(void* arg) {
  button_event_t event;
  osStatus_t status;
  uint8_t count = 0;


  for(;;) {
    status = osMessageQueueGet(manager_queue_id, &event, NULL, osWaitForever);
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

void worker(void *arg) {
}

void main(void) {
  LEDInit(LEDs);
  ButtonInit(USW1|USW2);
  ButtonIntEnable(USW1|USW2);

  osKernelInitialize();

  manager_queue_id = osMessageQueueNew(MANAGER_QUEUE_SIZE, sizeof(button_event_t), NULL);
  manager_thread_id = osThreadNew(manager, NULL, NULL);

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}
