#include <lcom/lcf.h>
#include "rtc.h"

/* MC146818 RTC: index port + data port. Write the register number to the index
 * port, then read/write the value at the data port. */
#define RTC_ADDR_REG 0x70
#define RTC_DATA_REG 0x71

#define RTC_SECONDS  0x00
#define RTC_MINUTES  0x02
#define RTC_HOURS    0x04
#define RTC_REG_A    0x0A
#define RTC_REG_B    0x0B
#define RTC_REG_C    0x0C

#define RTC_UIE      BIT(4)   /* Register B: update-ended interrupt enable */

static int     hook_id = RTC_IRQ;
static uint8_t captured[4];
static int     captured_n = 0;

static int rtc_read(uint8_t reg, uint8_t *val) {
    uint32_t v;
    if (sys_outb(RTC_ADDR_REG, reg) != OK) return 1;
    if (sys_inb(RTC_DATA_REG, &v)  != OK) return 1;
    *val = (uint8_t) v;
    return 0;
}

static int rtc_write(uint8_t reg, uint8_t val) {
    if (sys_outb(RTC_ADDR_REG, reg) != OK) return 1;
    if (sys_outb(RTC_DATA_REG, val) != OK) return 1;
    return 0;
}

int rtc_subscribe_int(uint8_t *bit_no) {
    *bit_no = (uint8_t) hook_id;
    return sys_irqsetpolicy(RTC_IRQ, IRQ_REENABLE, &hook_id);
}

int rtc_unsubscribe_int(void) {
    return sys_irqrmpolicy(&hook_id);
}

int rtc_enable_updates(void) {
    uint8_t b, c;
    if (rtc_read(RTC_REG_B, &b) != OK) return 1;
    b |= RTC_UIE;
    if (rtc_write(RTC_REG_B, b) != OK) return 1;
    rtc_read(RTC_REG_C, &c);   /* clear any pending flags so interrupts start */
    return 0;
}

int rtc_disable_updates(void) {
    uint8_t b;
    if (rtc_read(RTC_REG_B, &b) != OK) return 1;
    b &= (uint8_t) ~RTC_UIE;
    return rtc_write(RTC_REG_B, b);
}

void rtc_ih(void) {
    uint8_t c = 0, s = 0, m = 0, h = 0;
    rtc_read(RTC_REG_C, &c);    /* ACK: reading Register C clears the IRQ flags */
    rtc_read(RTC_SECONDS, &s);
    rtc_read(RTC_MINUTES, &m);
    rtc_read(RTC_HOURS,   &h);
    captured[0] = s;
    captured[1] = m;
    captured[2] = h;
    captured[3] = c;
    captured_n  = 4;
}

int rtc_get_entropy(uint8_t out[], int max) {
    int n = (captured_n < max) ? captured_n : max;
    for (int i = 0; i < n; i++)
        out[i] = captured[i];
    return n;
}
