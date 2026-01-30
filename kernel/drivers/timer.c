#include "../include/timer.h"
#include "../include/idt.h"
#include "../include/ports.h"

static uint32_t tick_count = 0;
static timer_callback_t user_callback = NULL;

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

static void timer_handler(registers_t* regs) {
    (void)regs;
    tick_count++;
    
    if (user_callback != NULL) {
        user_callback(tick_count);
    }
}

void timer_init(uint32_t frequency) {
    register_interrupt_handler(IRQ0, timer_handler);
    
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    outb(PIT_COMMAND, 0x36);
    
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    
    outb(PIT_CHANNEL0, low);
    outb(PIT_CHANNEL0, high);
}

void timer_set_callback(timer_callback_t callback) {
    user_callback = callback;
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

void timer_wait(uint32_t ticks) {
    uint32_t target = tick_count + ticks;
    while (tick_count < target) {
        __asm__ volatile("hlt");
    }
}

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int is_updating(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd & 0x0F);
}

uint8_t rtc_get_seconds(void) {
    while (is_updating());
    uint8_t seconds = cmos_read(0x00);
    uint8_t registerB = cmos_read(0x0B);
    
    if (!(registerB & 0x04)) {
        seconds = bcd_to_binary(seconds);
    }
    return seconds;
}

uint8_t rtc_get_minutes(void) {
    while (is_updating());
    uint8_t minutes = cmos_read(0x02);
    uint8_t registerB = cmos_read(0x0B);
    
    if (!(registerB & 0x04)) {
        minutes = bcd_to_binary(minutes);
    }
    return minutes;
}

uint8_t rtc_get_hours(void) {
    while (is_updating());
    uint8_t hours = cmos_read(0x04);
    uint8_t registerB = cmos_read(0x0B);
    
    if (!(registerB & 0x04)) {
        hours = bcd_to_binary(hours);
    }
    
    if (!(registerB & 0x02) && (hours & 0x80)) {
        hours = ((hours & 0x7F) + 12) % 24;
    }
    
    return hours;
}

uint8_t rtc_get_day(void) {
    while (is_updating());
    uint8_t day = cmos_read(0x07);
    uint8_t registerB = cmos_read(0x0B);
    
    if (!(registerB & 0x04)) {
        day = bcd_to_binary(day);
    }
    return day;
}

uint8_t rtc_get_month(void) {
    while (is_updating());
    uint8_t month = cmos_read(0x08);
    uint8_t registerB = cmos_read(0x0B);
    
    if (!(registerB & 0x04)) {
        month = bcd_to_binary(month);
    }
    return month;
}

uint16_t rtc_get_year(void) {
    while (is_updating());
    uint8_t year = cmos_read(0x09);
    uint8_t registerB = cmos_read(0x0B);
    
    if (!(registerB & 0x04)) {
        year = bcd_to_binary(year);
    }
    
    return 2000 + year;
}
