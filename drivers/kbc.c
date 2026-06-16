#include <lcom/lcf.h>
#include "kbc.h"

static int     hook_id      = KBC_IRQ;
static uint8_t scancode_byte = 0;
static bool    ih_error      = false;

uint8_t kbc_get_scancode_byte(void) { return scancode_byte; }
bool    kbc_get_error(void)         { return ih_error; }

int kbc_subscribe_int(uint8_t *bit_no) {
    *bit_no = hook_id;
    return sys_irqsetpolicy(KBC_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE, &hook_id);
}

int kbc_unsubscribe_int(void) {
    return sys_irqrmpolicy(&hook_id);
}

void (kbc_ih)(void) {
    uint32_t stat, data;

    if (sys_inb(KBC_ST_REG, &stat) != OK) {
        ih_error = true;
        return;
    }

    if (!(stat & KBC_OBF)) {
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

    scancode_byte = (uint8_t) data;
    ih_error = false;
}

int kbc_restore_interrupts(void) {
    uint32_t cmd;

    if (sys_outb(KBC_ST_REG, KBC_READ_CMD) != OK)
        return 1;

    if (sys_inb(KBC_OUT_BUF, &cmd) != OK)
        return 1;

    cmd |= KBC_CMD_INT;

    /* Issue "write command byte" (0x60) to the command register, then the
     * modified command byte to the input buffer. */
    if (sys_outb(KBC_ST_REG, KBC_WRITE_CMD) != OK)
        return 1;

    if (sys_outb(KBC_OUT_BUF, cmd) != OK)
        return 1;

    return 0;
}
