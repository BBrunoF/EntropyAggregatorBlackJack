#include <lcom/lcf.h>
#include "mouse.h"

static int hook_id = MOUSE_IRQ;
static uint8_t mouse_byte = 0;
static bool ih_error = false;

uint8_t mouse_get_byte(void) { return mouse_byte; }
bool mouse_get_error(void) { return ih_error; }

int mouse_subscribe_int(uint8_t *bit_no) {
    *bit_no = (uint8_t) hook_id;
    return sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &hook_id);
}

int mouse_unsubscribe_int(void) {
    return sys_irqrmpolicy(&hook_id);
}

void (mouse_ih)(void) {
    uint32_t stat, data;

    if (sys_inb(KBC_ST_REG, &stat) != OK) {
        ih_error = true;
        return;
    }

    if (!(stat & KBC_OBF) || !(stat & KBC_AUX)) {
        ih_error = true;
        return;
    }

    if (sys_inb(KBC_OUT_BUF, &data) != OK) {
        ih_error = true;
        return;
    }

    if (stat & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) {
        ih_error = true;
        return;
    }

    mouse_byte = (uint8_t) data;
    ih_error = false;
}

static int kbc_write(uint16_t port, uint8_t val) {
    uint32_t stat;
    for (int i = 0; i < MOUSE_MAX_RETRIES; i++) {
        if (sys_inb(KBC_ST_REG, &stat) != OK) return 1;
        if (!(stat & KBC_IBF)) {
            return sys_outb(port, val);
        }
        tickdelay(micros_to_ticks(MOUSE_RETRY_DELAY));
    }
    return 1;
}

/* Poll the output buffer for a byte from the auxiliary device (AUX set) and
 * return it. Any non-AUX byte (e.g. a leftover keyboard scancode) is read and
 * discarded rather than left in OBF -- if it stayed there the KBC would never
 * deliver the mouse ACK and every retry would time out. */
static int kbc_read_aux(uint8_t *out) {
    uint32_t stat, data;
    for (int i = 0; i < MOUSE_MAX_RETRIES; i++) {
        if (sys_inb(KBC_ST_REG, &stat) != OK) return 1;
        if (stat & KBC_OBF) {
            if (sys_inb(KBC_OUT_BUF, &data) != OK) return 1;
            if (stat & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) return 1;
            if (stat & KBC_AUX) {
                *out = (uint8_t) data;
                return 0;
            }
            /* Non-AUX byte drained; re-check immediately for the ACK. */
            continue;
        }
        tickdelay(micros_to_ticks(MOUSE_RETRY_DELAY));
    }
    return 1;
}

int mouse_write_cmd(uint8_t cmd) {
    uint8_t ack;
    uint32_t stat, junk;

    /* Drain any stale byte already in OBF so it can't block the ACK read. */
    if (sys_inb(KBC_ST_REG, &stat) == OK && (stat & KBC_OBF))
        sys_inb(KBC_OUT_BUF, &junk);

    for (int i = 0; i < MOUSE_MAX_RETRIES; i++) {
        if (kbc_write(KBC_CMD_REG, KBC_WRITE_TO_MOUSE) != OK) return 1;
        if (kbc_write(KBC_IN_BUF, cmd) != OK) return 1;

        if (kbc_read_aux(&ack) != OK) return 1;

        if (ack == MOUSE_ACK) return 0;
        if (ack == MOUSE_ERROR) return 1;
        /* MOUSE_NACK (0xFE): resend on next iteration. */
    }
    return 1;
}

int mouse_disable_data_reporting(void) {
    return mouse_write_cmd(MOUSE_DISABLE_DR);
}

void mouse_parse_packet(uint8_t bytes[], struct packet *pp) {
    pp->bytes[0] = bytes[0];
    pp->bytes[1] = bytes[1];
    pp->bytes[2] = bytes[2];

    pp->lb = bytes[0] & MOUSE_LB;
    pp->rb = (bytes[0] & MOUSE_RB) != 0;
    pp->mb = (bytes[0] & MOUSE_MB) != 0;

    pp->x_ov = (bytes[0] & MOUSE_X_OVF) != 0;
    pp->y_ov = (bytes[0] & MOUSE_Y_OVF) != 0;

    pp->delta_x = (bytes[0] & MOUSE_X_SIGN) ? (int16_t)(0xFF00 | bytes[1]) : (int16_t) bytes[1];
    pp->delta_y = (bytes[0] & MOUSE_Y_SIGN) ? (int16_t)(0xFF00 | bytes[2]) : (int16_t) bytes[2];
}
