#include <lcom/lcf.h>
#include <stdint.h>

#ifndef OK
#define OK 0
#endif

int(util_get_LSB)(uint16_t val, uint8_t *lsb) {
  if (lsb == NULL) return 1;
  *lsb = (uint8_t) val;
  return OK;
}

int(util_get_MSB)(uint16_t val, uint8_t *msb) {
  if (msb == NULL) return 1;
  *msb = (uint8_t) (val >> 8);
  return OK;
}

int (util_sys_inb)(int port, uint8_t *value) {
  uint32_t tmp;
  int ret;

  if (value == NULL) return 1;

  ret = sys_inb(port, &tmp);
  if (ret != OK) return ret;

  *value = (uint8_t) tmp;
  return OK;
}
