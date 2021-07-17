#include "system_TM4C1294.h"
#include "driverbuttons.h"
#include "driverleds.h"
#include "cmsis_os2.h"

#define LEDs LED1 | LED2 | LED3 | LED4

#define BUFFER_SIZE 8
#define MAX_COUNT 16

uint8_t counter;

osSemaphoreId_t ready_id, empty_id;
osThreadId_t consumer_id;

uint8_t buffer[BUFFER_SIZE];

void GPIOJ_Handler(void) {
  static uint8_t index = 0;
  osSemaphoreAcquire(empty_id, osWaitForever);
  ButtonIntClear(USW1);
  counter = (counter + 1) % MAX_COUNT;
  buffer[index] = counter;
  osSemaphoreRelease(ready_id);
  index = (index + 1) % BUFFER_SIZE;
}

void consumer(void* arg) {
  uint8_t index = 0, state;

  for(;;) {
    osSemaphoreAcquire(ready_id, osWaitForever);
    state = buffer[index];
    osSemaphoreRelease(empty_id);
    LEDWrite(LEDs, state);
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
