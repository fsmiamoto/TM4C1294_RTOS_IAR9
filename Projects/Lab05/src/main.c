#include "cmsis_os2.h"
#include "driverbuttons.h"
#include "driverleds.h"
#include "system_TM4C1294.h"

#define LEDs LED1 | LED2 | LED3 | LED4
#define MAX_COUNT 16
#define BUFFER_SIZE 8
#define NO_WAIT 0
#define TICK_DEBOUNCE 200

uint8_t buffer[BUFFER_SIZE];

osSemaphoreId_t ready_id, empty_id;
osThreadId_t consumer_id;

void GPIOJ_Handler(void) {
  static uint8_t index = 0, counter = 0;
  static uint32_t last_update_tick;

  ButtonIntClear(USW1);
  if((osKernelGetTickCount() - last_update_tick) < TICK_DEBOUNCE)
    return;

  osStatus_t  status = osSemaphoreAcquire(empty_id, NO_WAIT);
  if (status == osOK) {
    counter = (counter + 1) % MAX_COUNT;
    buffer[index] = counter;
    osSemaphoreRelease(ready_id);
    index = (index + 1) % BUFFER_SIZE;
    last_update_tick = osKernelGetTickCount();
  }
}

void consumer(void *arg) {
  uint8_t index = 0, counter;

  for (;;) {
    osSemaphoreAcquire(ready_id, osWaitForever);
    counter = buffer[index];
    osSemaphoreRelease(empty_id);

    LEDWrite(LEDs, counter);
    index = (index + 1) % BUFFER_SIZE;
  }
}

void main(void) {
  LEDInit(LEDs);
  ButtonInit(USW1);
  ButtonIntEnable(USW1);

  osKernelInitialize();

  consumer_id = osThreadNew(consumer, NULL, NULL);

  empty_id = osSemaphoreNew(BUFFER_SIZE, BUFFER_SIZE, NULL);
  ready_id = osSemaphoreNew(BUFFER_SIZE, 0, NULL);

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}