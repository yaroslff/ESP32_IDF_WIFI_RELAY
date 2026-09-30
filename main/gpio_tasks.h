#pragma once

#include "main.h"

#define RELAY_1_GPIO 1
#define RELAY_2_GPIO 2
#define RELAY_3_GPIO 3
#define RELAY_4_GPIO 4

#define LED_PWM_GPIO 8
void gpio_tasks_init();
void relay_send_command(int relay_num, int state);
void relayTask(void *pvParameter);