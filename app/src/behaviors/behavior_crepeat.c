/*
 * Copyright (c) 2022 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_crepeat

#include <zephyr/device.h>
#include <zephyr/random/random.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>

#include <dt-bindings/zmk/crepeat.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static inline uint16_t sys_rand8_get(void)
{
    uint8_t ret;

    sys_rand_get(&ret, sizeof(ret));

    return ret;
}

struct behavior_crepeat_state {
    uint32_t repeat_ms;
    uint16_t current;
    uint16_t count;
    bool run;
};

static struct zmk_behavior_binding default_key = { .param1 = NUM_7,
    .behavior_dev = "key_press" };

static struct behavior_crepeat_state state = {.repeat_ms = 1000,
                                                .current = 0,
                                                .count = 0,
                                                .run = false };

static struct k_work_delayable repeat_worker;


void crepeat_reset();

static void repeat_queue_process_next(struct k_work *work) {
    // check stop
    if (!state.run) {
        return;
    }

    uint8_t rnd;
    //range 85 - 125;
    rnd = 85 + (sys_rand8_get() % 40);

    LOG_DBG("cRepeat key tap. %dms, repeat %d", rnd, state.current);
    zmk_behavior_queue_add(0, default_key, true, rnd);
    zmk_behavior_queue_add(0, default_key, false, 20);

    if (++state.current <= state.count) {
        rnd = 75 + (sys_rand8_get() % 64);
        LOG_DBG("cRepeat schedule next %dms + %dms, %d/%d", state.repeat_ms, rnd, state.current, state.count);
        k_work_schedule(&repeat_worker, K_MSEC(state.repeat_ms + rnd));
    } else {
        // work done.
        state.run = false;
        crepeat_reset();
    }
}

bool check_is_crepeat_run() {
    return state.run;
}

void crepeat_start() {
    LOG_DBG("cRepeat CMD start.");
    if (check_is_crepeat_run()) {
        LOG_DBG("cRepeat[START]: check failed.");
        return;
    }

    // check state params
    if (state.count <= 1) {
        return;
    }

    // reset current;
    state.current = 1;
    state.run = true;

    // start job
    repeat_queue_process_next(&repeat_worker.work);
}

void crepeat_reset() {
    if (check_is_crepeat_run()) {
        return;
    }
    state.repeat_ms = 0;
    state.current = 0;
    state.count = 0;
}

void crepeat_stop() {
    LOG_DBG("cRepeat CMD stop.");
    if (!check_is_crepeat_run()) {
        LOG_DBG("cRepeat[STOP]: check failed.");
        return;
    }

    // kill kwork queue
    state.run = false;

    crepeat_reset();
}


void crepeat_timer_select(uint32_t wait_ms) {
    if (check_is_crepeat_run()) {
        return;
    }
    state.repeat_ms = wait_ms;
}

void crepeat_timer_change(uint32_t wait_ms, bool increase) {
    if (check_is_crepeat_run()) {
        return;
    }
    uint32_t new_ms = state.repeat_ms + (increase ? 1 : -1) * wait_ms;

    if (new_ms < 500) {
        LOG_DBG("cRepeat[Timer]: repeat timer cannot less then 500ms. %d", new_ms);
        return;
    }
    state.repeat_ms = new_ms;
}

void crepeat_count_change(uint32_t count, bool increase) {
    if (check_is_crepeat_run()) {
        return;
    }
    uint16_t new_cnt = state.count + (increase ? 1 : -1) * count;
    if (new_cnt < 1) {
        LOG_DBG("cRepeat[Count]: count cannot less then 1. %d", new_cnt);
        return;
    }
    state.count = new_cnt;
}


static int behavior_crepeat_init(const struct device *dev) {
    k_work_init_delayable(&repeat_worker, repeat_queue_process_next);
    return 0;
}


static int on_crepeat_binding_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case CR_START:
        crepeat_start();
        break;
    case CR_STOP:
        crepeat_stop();
        break;
    case CR_RESET:
        crepeat_reset();
        break;
    case CR_TM_SEL:
        crepeat_timer_select(binding->param2);
        break;
    case CR_TM_INC:
        crepeat_timer_change(binding->param2, true);
        break;
    case CR_TM_DSC:
        crepeat_timer_change(binding->param2, false);
        break;
    case CR_CT_INC:
        crepeat_count_change(binding->param2, true);
        break;
    case CR_CT_DSC:
        crepeat_count_change(binding->param2, false);
        break;
    default:
        LOG_ERR("Unknown cRepeat command: %d", binding->param1);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}


static int on_crepeat_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}



static const struct behavior_driver_api behavior_crepeat_driver_api = {
    .binding_pressed = on_crepeat_binding_pressed,
    .binding_released = on_crepeat_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_crepeat_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_crepeat_driver_api);


#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
