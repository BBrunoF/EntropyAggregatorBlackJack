#include <lcom/lcf.h>
#include <lcom/timer.h>

#include <stdint.h>

#include "i8254.h"
#include "timer_entropy.h"

#ifndef OK
#define OK 0
#endif

static int hook_id = TIMER0_IRQ;
volatile uint32_t timer_counter = 0;

int (timer_set_frequency)(uint8_t timer, uint32_t freq) {
  if (timer > 2) return 1;
  if (freq < 2 || freq > TIMER_FREQ) return 1;

  uint8_t st;
  int ret = timer_get_conf(timer, &st);
  if (ret != OK) return ret;

  uint16_t divider = (uint16_t) (TIMER_FREQ / freq);

  uint8_t timer_sel;
  uint8_t timer_port;
  switch (timer) {
    case 0: timer_sel = TIMER_SEL0; timer_port = TIMER_0; break;
    case 1: timer_sel = TIMER_SEL1; timer_port = TIMER_1; break;
    case 2: timer_sel = TIMER_SEL2; timer_port = TIMER_2; break;
    default: return 1;
  }

  uint8_t preserved_bits = st & 0x0F;
  uint8_t ctrl_word = timer_sel | TIMER_LSB_MSB | preserved_bits;

  ret = sys_outb(TIMER_CTRL, ctrl_word);
  if (ret != OK) return ret;

  uint8_t lsb, msb;

  ret = util_get_LSB(divider, &lsb);
  if (ret != OK) return ret;
  ret = util_get_MSB(divider, &msb);
  if (ret != OK) return ret;
  ret = sys_outb(timer_port, lsb);
  if (ret != OK) return ret;
  ret = sys_outb(timer_port, msb);
  if (ret != OK) return ret;

  return OK;
}

int (timer_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL) return 1;
  *bit_no = (uint8_t) hook_id;
  return sys_irqsetpolicy(TIMER0_IRQ, IRQ_REENABLE, &hook_id);
}

int (timer_unsubscribe_int)() {
  return sys_irqrmpolicy(&hook_id);
}

void (timer_int_handler)() {
  timer_counter++;
}

int (timer_get_conf)(uint8_t timer, uint8_t *st) {
  if (st == NULL) return 1;
  if (timer > 2) return 1;

  uint8_t rb_cmd = TIMER_RB_CMD | TIMER_RB_COUNT_ | TIMER_RB_SEL(timer);

  int ret = sys_outb(TIMER_CTRL, rb_cmd);
  if (ret != OK) return ret;

  uint8_t timer_port;
  switch (timer) {
    case 0: timer_port = TIMER_0; break;
    case 1: timer_port = TIMER_1; break;
    case 2: timer_port = TIMER_2; break;
    default: return 1;
  }
  return util_sys_inb(timer_port, st);
}

int (timer_display_conf)(uint8_t timer, uint8_t st, enum timer_status_field field) {
  union timer_status_field_val conf;

  switch (field) {
    case tsf_all:
      conf.byte = st;
      break;

    case tsf_initial:
      switch ((st >> 4) & 0x3) {
        case 1: conf.in_mode = LSB_only; break;
        case 2: conf.in_mode = MSB_only; break;
        case 3: conf.in_mode = MSB_after_LSB; break;
        default: conf.in_mode = INVAL_val; break;
      }
      break;

    case tsf_mode:
      conf.count_mode = (st >> 1) & 0x7;
      if (conf.count_mode == 6) conf.count_mode = 2;
      if (conf.count_mode == 7) conf.count_mode = 3;
      break;

    case tsf_base:
      conf.bcd = (st & 0x1) ? true : false;
      break;

    default:
      return 1;
  }
  return timer_print_config(timer, field, conf);
}

uint16_t timer_read_count(void) {
  /* Read-back: latch counter 0 (bit5 = 0 = ~COUNT active),
   * leave status unlatched (bit4 = 1 = ~STATUS inactive). */
  uint8_t cmd = TIMER_RB_CMD | TIMER_RB_STATUS_ | TIMER_RB_SEL(0);
  if (sys_outb(TIMER_CTRL, cmd) != OK) return 0;

  uint8_t lsb = 0, msb = 0;
  if (util_sys_inb(TIMER_0, &lsb) != OK) return 0;
  if (util_sys_inb(TIMER_0, &msb) != OK) return 0;
  return ((uint16_t) msb << 8) | lsb;
}
