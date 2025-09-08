/*
 * Copyright (c) 2022 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_cfire

#include <zephyr/device.h>
#include <zephyr/random/random.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static inline uint16_t sys_rand8_get(void) {
    uint8_t ret;

    sys_rand_get(&ret, sizeof(ret));

    return ret;
}

struct behavior_cfire_config {
    uint32_t repeat_term_ms;
};

struct behavior_cfire_state {
    uint32_t repeat_ms;
    uint32_t keycode; // behaviors -> param1
    int64_t timestamp;
    uint32_t interval_ms; // random interval.

    const struct behavior_cfire_config *config;
    uint32_t param1;
    uint32_t param2; // repeat_ms copy

    uint16_t hid_code;

    bool is_running;
    bool need_config;
    struct k_work_delayable timer;
};

static struct zmk_behavior_binding default_key = {.param1 = NUM_7, .behavior_dev = "key_press"};

static struct behavior_cfire_state state = {
    .repeat_ms = 0, .keycode = 0, .is_running = false, .need_config = false};

static int on_cfire_binding_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_cfire_config *cfg = dev->config;
    state.param1 = binding->param1;
    state.param2 = cfg->repeat_term_ms;

    LOG_DBG("[cfire] %d pressed. keycode: 0x%02X", event.position, state.param1);
    state.need_config = true;

    if (!state.is_running) {
        // start the k_work
        LOG_DBG("[cfire] Start kwork.");
        state.is_running = true;
        k_work_schedule(&state.timer, K_MSEC(50));
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_cfire_binding_released(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static void cfire_queue_process_next(struct k_work *work) {
    // check stop
    if (!state.is_running) {
        return;
    }

    if (state.need_config) {
        state.repeat_ms = state.param2;
        state.keycode = state.param1;
        state.hid_code = ZMK_HID_USAGE_ID(state.keycode);

        LOG_DBG("[cfire] do_config. kcode: 0x%02X, repeat_ms: %d", state.keycode, state.repeat_ms);
        default_key.param1 = state.keycode;
        state.need_config = false;
    }

    uint8_t rnd;
    // kp range 55 - 95;
    rnd = 24 + (sys_rand8_get() % 32);

    state.timestamp = k_uptime_get();
    state.interval_ms = state.repeat_ms + 24 + (sys_rand8_get() % 64);

    LOG_DBG("[cfire] key tap. KP: %dms, interval: %dms, ts: %lld", rnd, state.interval_ms,
            state.timestamp);
    // do kp now skip the queue because need TS to verify cfire stop.
    // LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);
    raise_zmk_keycode_state_changed_from_encoded(state.keycode, true, state.timestamp);
    zmk_behavior_queue_add(0, default_key, false, rnd);

    // schedule next repeat
    k_work_schedule(&state.timer, K_MSEC(state.interval_ms));
}

static int behavior_cfire_init(const struct device *dev) {
    k_work_init_delayable(&state.timer, cfire_queue_process_next);
    return 0;
}

static const struct behavior_driver_api behavior_cfire_driver_api = {
    .binding_pressed = on_cfire_binding_pressed,
    .binding_released = on_cfire_binding_released,
};

static int cfire_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_cfire, cfire_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_cfire, zmk_keycode_state_changed);

static int cfire_keycode_state_changed_listener(const zmk_event_t *eh) {
    if (!state.is_running) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // handle kp event
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    // LOG_DBG("[cfire] Check ev. keycode:  0x%02X, ts: %lld, kp: %d", ev->keycode, ev->timestamp, ev->state);
    // LOG_DBG("[cfire] Check st. keycode:  0x%02X, ts: %lld, ", state.hid_code, state.timestamp);
    if (ev->state && ev->keycode == state.hid_code && ev->timestamp != state.timestamp) {
        LOG_DBG("[cfire] Cancelled. keycode: %d", ev->keycode);
        state.is_running = false;
    }

    return ZMK_EV_EVENT_BUBBLE;
}


#define KP_INST(n) \
    static const struct behavior_cfire_config behavior_cfire_config_##n = { \
        .repeat_term_ms = DT_INST_PROP(n, repeat_term_ms), \
        }; \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_cfire_init, NULL, NULL, &behavior_cfire_config_##n, POST_KERNEL, \
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_cfire_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
