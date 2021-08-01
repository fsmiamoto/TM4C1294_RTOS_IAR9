#include "cmsis_os2.h"
#include "driverbuttons.h"
#include "driverleds.h"
#include "system_TM4C1294.h"
#include <stdint.h>

#define LEDs LED1 | LED2 | LED3 | LED4

#define NUM_OF_WORKERS (sizeof(workers) / sizeof(worker_t))

#define WORKER_QUEUE_SIZE 8U
#define MANAGER_QUEUE_SIZE 8U

#define DEBOUNCE_TICKS 300U

#define NO_WAIT 0U
#define MSG_PRIO 0U

#define PWM_PERIOD 10U   // ms
#define BLINK_PERIOD 50U // ms

// Push buttons are active in 0
#define ButtonPressed(b) !ButtonRead(b)

#define notifySelectedWorker()                                                 \
  osMessageQueuePut(workers[selected_worker].args.queue_id, &notification,     \
                    MSG_PRIO, osWaitForever)

typedef enum {
  SW1_PRESSED,
  SW2_PRESSED,
} button_event_t;

// Notification to workers, no content needed.
typedef uint8_t manager_notif_t;

// worker_args_t represents the arguments for a worker thread
typedef struct {
  osMessageQueueId_t queue_id;
  uint8_t led_number; // One of LED1, LED2, LED3 and LED4.
  uint16_t on_time;   // In ms
  uint16_t period;    // In ms
} worker_args_t;

// worker_t represents a worker thread and it's arguments
typedef struct {
  osThreadId_t thread_id;
  worker_args_t args;
} worker_t;

// manager_t represents a manager thread
typedef struct {
  osThreadId_t thread_id;
  osMessageQueueId_t queue_id;
} manager_t;

osMutexId_t led_mutex_id;
const osMutexAttr_t led_mutex_attr = {"LEDMutex", osMutexPrioInherit, NULL, 0U};

manager_t manager;
worker_t workers[] = {
    {.args = {.led_number = LED1, .period = BLINK_PERIOD, .on_time = 5}},
    {.args = {.led_number = LED2, .period = PWM_PERIOD, .on_time = 2}},
    {.args = {.led_number = LED3, .period = PWM_PERIOD, .on_time = 8}},
    {.args = {.led_number = LED4, .period = PWM_PERIOD, .on_time = 0}},
};

// SwitchOn switches on a led in a thread-safe manner
void SwitchOn(uint8_t led) {
  osMutexAcquire(led_mutex_id, osWaitForever);
  LEDOn(led);
  osMutexRelease(led_mutex_id);
}

// SwitchOff switches off a led in a thread-safe manner
void SwitchOff(uint8_t led) {
  osMutexAcquire(led_mutex_id, osWaitForever);
  LEDOff(led);
  osMutexRelease(led_mutex_id);
}

void GPIOJ_Handler(void) {
  // Used for debouncing
  static uint32_t tick_last_msg_sw1, tick_last_msg_sw2;

  ButtonIntClear(USW1 | USW2);

  if (ButtonPressed(USW1)) {
    if ((osKernelGetTickCount() - tick_last_msg_sw1) < DEBOUNCE_TICKS)
      return;

    button_event_t event = SW1_PRESSED;
    osStatus_t status =
        osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    if (status == osOK)
      tick_last_msg_sw1 = osKernelGetTickCount();
  }

  if (ButtonPressed(USW2)) {
    if ((osKernelGetTickCount() - tick_last_msg_sw2) < DEBOUNCE_TICKS)
      return;

    button_event_t event = SW2_PRESSED;
    osStatus_t status =
        osMessageQueuePut(manager.queue_id, &event, MSG_PRIO, NO_WAIT);
    if (status == osOK)
      tick_last_msg_sw2 = osKernelGetTickCount();
  }
}

// Manager is the body of the manager thread.
// It receives events through a queue and notifies
// workers that need to update their values.
void Manager(void *arg) {
  button_event_t event;
  manager_notif_t notification;
  uint8_t selected_worker = 0;

  for (;;) {
    osMessageQueueGet(manager.queue_id, &event, NULL, osWaitForever);

    switch (event) {
    case SW1_PRESSED:
      // We need to restore the PWM period of the current selected worker
      workers[selected_worker].args.period = PWM_PERIOD;
      notifySelectedWorker();

      selected_worker = (selected_worker + 1) % NUM_OF_WORKERS;

      workers[selected_worker].args.period = BLINK_PERIOD;
      notifySelectedWorker();
      break;
    case SW2_PRESSED:
      workers[selected_worker].args.on_time += 1;
      if (workers[selected_worker].args.on_time > PWM_PERIOD)
        workers[selected_worker].args.on_time = 0;
      notifySelectedWorker();
      break;
    }
  }
}

// Worker is the body of worker threads.
// It polls for notifications from the manager thread and
// updates it's argument values if needed.
void Worker(void *arg) {
  worker_args_t *args = (worker_args_t *)arg;
  manager_notif_t notification;
  osStatus_t status;

  osMessageQueueId_t queue_id = args->queue_id;
  uint8_t led_number = args->led_number;
  uint8_t on_time = args->on_time;
  uint16_t period = args->period;

  for (;;) {
    status = osMessageQueueGet(queue_id, &notification, NULL, NO_WAIT);
    if (status == osOK) {
      // Notification received, update values
      on_time = args->on_time;
      period = args->period;
    }
    SwitchOn(led_number);
    osDelay(on_time);
    SwitchOff(led_number);
    osDelay(period - on_time);
  }
}

void initializeHardware(void) {
  LEDInit(LEDs);
  ButtonInit(USW1 | USW2);
  ButtonIntEnable(USW1 | USW2);
}

void initializeWorkers(void) {
  for (int i = 0; i < NUM_OF_WORKERS; i++) {
    workers[i].args.queue_id =
        osMessageQueueNew(WORKER_QUEUE_SIZE, sizeof(manager_notif_t), NULL);
    workers[i].thread_id = osThreadNew(Worker, &workers[i].args, NULL);
  }
}

void initializeManager(void) {
  manager.thread_id = osThreadNew(Manager, NULL, NULL);
  manager.queue_id =
      osMessageQueueNew(MANAGER_QUEUE_SIZE, sizeof(button_event_t), NULL);
}

void main(void) {
  initializeHardware();
  osKernelInitialize();
  initializeManager();
  initializeWorkers();
  led_mutex_id = osMutexNew(&led_mutex_attr);

  if (osKernelGetState() == osKernelReady)
    osKernelStart();

  // NOT REACHED
  while (1)
    ;
}
