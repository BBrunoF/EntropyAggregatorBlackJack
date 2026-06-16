#include <lcom/lcf.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kbc.h"
#include "mouse.h"
#include "rtc.h"
#include "timer_entropy.h"
#include "entropy.h"
#include "video.h"
#include "game.h"
#include "analysis.h"

#define MOUSE_MAX_STEP 40

int main(int argc, char *argv[]) {
    lcf_set_language("EN-US");
    lcf_trace_calls("/home/lcom/labs/EntropyAggregatorBlackJack/trace.txt");
    lcf_log_output("/home/lcom/labs/EntropyAggregatorBlackJack/output.txt");

    if (lcf_start(argc, argv))
        return 1;

    lcf_cleanup();
    return 0;
}

int (proj_main_loop)(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++)
        if (argv[i] != NULL && strcmp(argv[i], "analyze") == 0)
            return analysis_run();

    entropy_init();

    uint8_t timer_bit = 0, kbd_bit = 0, mouse_bit = 0;

    if (timer_subscribe_int(&timer_bit) != OK) {
        printf("proj: timer_subscribe_int failed\n");
        return 1;
    }
    if (kbc_subscribe_int(&kbd_bit) != OK) {
        printf("proj: kbc_subscribe_int failed\n");
        timer_unsubscribe_int();
        return 1;
    }

    bool mouse_on = false;
    if (mouse_subscribe_int(&mouse_bit) == OK &&
        mouse_write_cmd(MOUSE_SET_SCALING_1_1) == 0 &&
        mouse_write_cmd(MOUSE_ENABLE_DR) == 0) {
        mouse_on = true;
    } else {
        mouse_unsubscribe_int();
        printf("proj: continuing without mouse\n");
    }

    uint8_t rtc_bit = 0;
    bool rtc_on = false;
    if (rtc_subscribe_int(&rtc_bit) == OK && rtc_enable_updates() == OK) {
        rtc_on = true;
    } else {
        rtc_unsubscribe_int();
        printf("proj: continuing without RTC entropy\n");
    }

    bool done = false;
    if (video_init(0x115) != 0) {
        printf("proj: video_init(0x115) failed\n");
        done = true;
    } else {
        if (game_init() != 0) {
            printf("proj: game_init failed (asset load)\n");
            done = true;
        }
    }

    const uint32_t timer_mask = BIT(timer_bit);
    const uint32_t kbd_mask   = BIT(kbd_bit);
    const uint32_t mouse_mask = mouse_on ? BIT(mouse_bit) : 0;
    const uint32_t rtc_mask   = rtc_on   ? BIT(rtc_bit)   : 0;

    uint8_t pkt[3];
    uint8_t pkt_idx = 0;
    bool    prev_lb = false;     /* for left-button edge detection */
    int     ipc_status;
    message msg;

    printf("\n=== Entropy Aggregator BlackJack ===\n");
    printf("  Stir entropy (type / move the mouse), then click PLAY. ESC quits.\n");

    while (!done) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0)
            continue;
        if (!is_ipc_notify(ipc_status))
            continue;
        if (_ENDPOINT_P(msg.m_source) != HARDWARE)
            continue;

        if (msg.m_notify.interrupts & timer_mask) {
            timer_int_handler();
            entropy_feed_timer();
            game_handle_timer();
            game_render();
            if (game_should_exit())
                done = true;
        }

        if (rtc_on && (msg.m_notify.interrupts & rtc_mask)) {
            rtc_ih();
            uint8_t rb[4];
            int n = rtc_get_entropy(rb, 4);
            entropy_feed_rtc(rb, n);
        }

        if (msg.m_notify.interrupts & kbd_mask) {
            kbc_ih();
            if (!kbc_get_error()) {
                uint8_t b = kbc_get_scancode_byte();
                entropy_feed_kbd(b);
                game_handle_key(b);
                if (b == ESC_BREAK)
                    done = true;
            }
        }

        if (mouse_on && (msg.m_notify.interrupts & mouse_mask)) {
            mouse_ih();
            if (!mouse_get_error()) {
                uint8_t b = mouse_get_byte();
                entropy_feed_mouse(b);

                if (pkt_idx == 0 && !(b & MOUSE_BYTE0_ID))
                    continue;
                pkt[pkt_idx++] = b;

                if (pkt_idx == 3) {
                    pkt_idx = 0;
                    struct packet pp;
                    mouse_parse_packet(pkt, &pp);

                    int dx = pp.x_ov ? (pp.delta_x < 0 ? -MOUSE_MAX_STEP : MOUSE_MAX_STEP)
                                     : pp.delta_x;
                    int dy = pp.y_ov ? (pp.delta_y < 0 ? -MOUSE_MAX_STEP : MOUSE_MAX_STEP)
                                     : pp.delta_y;
                    if (dx >  MOUSE_MAX_STEP) dx =  MOUSE_MAX_STEP;
                    if (dx < -MOUSE_MAX_STEP) dx = -MOUSE_MAX_STEP;
                    if (dy >  MOUSE_MAX_STEP) dy =  MOUSE_MAX_STEP;
                    if (dy < -MOUSE_MAX_STEP) dy = -MOUSE_MAX_STEP;
                    game_handle_mouse(dx, dy);

                    if (pp.lb && !prev_lb)   /* act once per press, not per packet */
                        game_handle_click();
                    prev_lb = pp.lb;
                }
            }
        }
    }

    if (rtc_on) {
        rtc_disable_updates();
        rtc_unsubscribe_int();
    }
    if (mouse_on) {
        mouse_disable_data_reporting();
        mouse_unsubscribe_int();
    }
    kbc_unsubscribe_int();
    timer_unsubscribe_int();

    game_free();
    video_gradient_free();
    video_exit();

    printf("\nGoodbye.\n");
    return 0;
}
