#include "cmsis_os2.h"
#include "driverleds.h"
#include "system_tm4c1294.h"

typedef struct {
  osThreadId_t thread_id;
  uint8_t led_number;         // One of LED1, LED2, LED3, LED4
  uint32_t activation_period; // Number of system ticks
} led_blink_t;

#define NUM_OF_BLINKERS sizeof(blinkers)/sizeof(led_blink_t)

led_blink_t blinkers[] = {
    {.led_number = LED1, .activation_period = 200},
    {.led_number = LED2, .activation_period = 300},
    {.led_number = LED3, .activation_period = 500},
    {.led_number = LED4, .activation_period = 700},
};

void blinker(void *arg) {
  uint8_t state = 0;
  uint32_t tick;
  led_blink_t *b = (led_blink_t *)arg;

  for (;;) {
    tick = osKernelGetTickCount();
    state ^= b->led_number;
    LEDWrite(b->led_number, state);
    osDelayUntil(tick + b->activation_period);
  }
}

void main(void) {
  LEDInit(LED1 | LED2 | LED3 | LED4);

  osKernelInitialize();

  for (int i = 0; i < NUM_OF_BLINKERS; i++)
    blinkers[i].thread_id = osThreadNew(blinker, &blinkers[i], NULL);

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}
