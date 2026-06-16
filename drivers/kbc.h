#ifndef KBC_H
#define KBC_H

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>

/* Ports */
#ifndef KBC_ST_REG
#define KBC_ST_REG    0x64
#endif
#ifndef KBC_OUT_BUF
#define KBC_OUT_BUF   0x60
#endif
#define KBC_READ_CMD  0x20
#define KBC_WRITE_CMD 0x60
#define KBC_CMD_INT   BIT(0)

/* Status register bits */
#ifndef KBC_OBF
#define KBC_OBF         BIT(0)
#endif
#ifndef KBC_PARITY_ERR
#define KBC_PARITY_ERR  BIT(7)
#endif
#ifndef KBC_TIMEOUT_ERR
#define KBC_TIMEOUT_ERR BIT(6)
#endif

/* IRQ line */
#define KBC_IRQ  1

/* Scancode constants */
#define SCANCODE_TWO_BYTE  0xE0
#define ESC_BREAK          0x81

int kbc_subscribe_int(uint8_t *bit_no);
int kbc_unsubscribe_int(void);
void (kbc_ih)(void);
uint8_t kbc_get_scancode_byte(void);
bool kbc_get_error(void);
int kbc_restore_interrupts(void);

#endif
