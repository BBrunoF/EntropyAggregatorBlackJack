#ifndef MOUSE_H
#define MOUSE_H

#include <lcom/lcf.h>
#include <stdint.h>
#include <stdbool.h>

/* Ports (shared with keyboard) */
#ifndef KBC_ST_REG
#define KBC_ST_REG    0x64
#endif
#define KBC_CMD_REG   0x64
#ifndef KBC_OUT_BUF
#define KBC_OUT_BUF   0x60
#endif
#define KBC_IN_BUF    0x60

/* Status register bits */
#ifndef KBC_OBF
#define KBC_OBF         BIT(0)
#endif
#define KBC_IBF         BIT(1)
#define KBC_AUX         BIT(5)
#ifndef KBC_PARITY_ERR
#define KBC_PARITY_ERR  BIT(7)
#endif
#ifndef KBC_TIMEOUT_ERR
#define KBC_TIMEOUT_ERR BIT(6)
#endif

/* KBC commands */
#define KBC_WRITE_TO_MOUSE  0xD4

/* Mouse commands */
#define MOUSE_ENABLE_DR        0xF4
#define MOUSE_DISABLE_DR       0xF5
#define MOUSE_SET_SCALING_1_1  0xE6   /* linear scaling: no hardware acceleration */

/* Mouse responses */
#define MOUSE_ACK    0xFA
#define MOUSE_NACK   0xFE
#define MOUSE_ERROR  0xFC

/* Mouse IRQ */
#define MOUSE_IRQ  12

/* Packet constants */
#define MOUSE_PACKET_SIZE 3

/* Byte 0 bits */
#define MOUSE_LB       BIT(0)
#define MOUSE_RB       BIT(1)
#define MOUSE_MB       BIT(2)
#define MOUSE_BYTE0_ID BIT(3)
#define MOUSE_X_SIGN   BIT(4)
#define MOUSE_Y_SIGN   BIT(5)
#define MOUSE_X_OVF    BIT(6)
#define MOUSE_Y_OVF    BIT(7)

/* Retry limits */
#define MOUSE_MAX_RETRIES 10
#define MOUSE_RETRY_DELAY 20000

int mouse_subscribe_int(uint8_t *bit_no);
int mouse_unsubscribe_int(void);
void (mouse_ih)(void);
uint8_t mouse_get_byte(void);
bool mouse_get_error(void);
int mouse_write_cmd(uint8_t cmd);
int mouse_disable_data_reporting(void);
void mouse_parse_packet(uint8_t bytes[], struct packet *pp);

#endif
