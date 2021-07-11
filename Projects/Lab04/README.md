# Lab 04

This project creates 4 threads that blink an LED with configurable activation periods.

```c
typedef struct {
  osThreadId_t thread_id;
  uint8_t led_number;         // One of LED1, LED2, LED3, LED4
  uint32_t activation_period; // Number of system ticks
} led_blink_t;
```

You can define which LEDs to blink on the `blinkers` array:
```c
led_blink_t blinkers[] = {
    {.led_number = LED1, .activation_period = 200},
    {.led_number = LED2, .activation_period = 300},
    {.led_number = LED3, .activation_period = 500},
    {.led_number = LED4, .activation_period = 700},
};
```
