#ifndef __PWM_H__
#define __PWM_H__

#include <stdint.h>

// Initializers
void initializeHardware(void);
void initializeWorkers(void);
void initializeManager(void);

// Thread bodies
void Manager(void *arg);
void Worker(void *arg);

// LED
void SwitchOn(uint8_t led);
void SwitchOff(uint8_t led);
#endif