#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

#define PIT_FREQUENCY 1193180

typedef void (*timer_callback_t)(uint32_t tick);

void timer_init(uint32_t frequency);
void timer_set_callback(timer_callback_t callback);
uint32_t timer_get_ticks(void);
void timer_wait(uint32_t ticks);

uint8_t rtc_get_seconds(void);
uint8_t rtc_get_minutes(void);
uint8_t rtc_get_hours(void);
uint8_t rtc_get_day(void);
uint8_t rtc_get_month(void);
uint16_t rtc_get_year(void);

#endif
