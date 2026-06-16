#ifndef RTC_H
#define RTC_H

#include <lcom/lcf.h>
#include <stdint.h>

#define RTC_IRQ 8

int  rtc_subscribe_int(uint8_t *bit_no);
int  rtc_unsubscribe_int(void);

/* Enable / disable the update-ended interrupt (fires ~once per second, IRQ 8). */
int  rtc_enable_updates(void);
int  rtc_disable_updates(void);

/* Interrupt handler: reads Register C to acknowledge the interrupt (mandatory --
 * the RTC won't fire again until Reg C is read) and latches the time registers. */
void rtc_ih(void);

/* Copies the bytes captured by the last rtc_ih() (seconds, minutes, hours, and
 * Register C) into out[]; returns the count (<= max). Used as entropy payload. */
int  rtc_get_entropy(uint8_t out[], int max);

#endif
