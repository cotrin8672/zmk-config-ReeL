/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT reel_display_state

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/split/bluetooth/service.h>

#include "reel_display_state.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "Define one enabled reel,display-state behavior on every split device");
BUILD_ASSERT(sizeof(DEVICE_DT_NAME(DT_DRV_INST(0))) <= ZMK_SPLIT_RUN_BEHAVIOR_DEV_LEN,
             "The display behavior name must fit in the BLE split payload, including NUL");

ZMK_EVENT_IMPL(reel_display_state_changed);

static struct k_spinlock state_lock;
static struct reel_display_state cached_state;

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static bool peripheral_connected;
#endif

static bool states_equal(struct reel_display_state a, struct reel_display_state b) {
    return a.valid == b.valid && a.layer == b.layer;
}

struct reel_display_state reel_display_state_get(void) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    struct reel_display_state state = cached_state;
    k_spin_unlock(&state_lock, key);
    return state;
}

static int update_state(struct reel_display_state state) {
    k_spinlock_key_t key = k_spin_lock(&state_lock);
#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (!peripheral_connected) {
        k_spin_unlock(&state_lock, key);
        return -ENOTCONN;
    }
#endif
    bool changed = !states_equal(cached_state, state);
    cached_state = state;
    k_spin_unlock(&state_lock, key);

    /* Listeners may call the snapshot API; never raise events under the lock. */
    return changed ? raise_reel_display_state_changed(
                         (struct reel_display_state_changed){.state = state})
                   : 0;
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/iterable_sections.h>

#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>
#include <zmk/split/transport/central.h>

/* Eight replay rounds span 12.7 seconds, then stop until another event. */
#define SYNC_ATTEMPTS 8
#define SYNC_DELAY_MS 100

static atomic_t resync_requested;
static ATOMIC_DEFINE(seen_key_sources, UINT8_MAX + 1);
static ATOMIC_DEFINE(key_fallbacks, UINT8_MAX + 1);
/* The following bookkeeping belongs exclusively to sync_work's work queue. */
static uint8_t attempts_left;
static uint32_t retry_delay_ms;
static int64_t next_retry;
static struct reel_display_state last_sent;
static uint8_t last_sources[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT];
static int last_source_count;

static int get_sources(uint8_t *sources) {
    STRUCT_SECTION_FOREACH(zmk_split_transport_central, transport) {
        if (!transport->api || !transport->api->get_status ||
            !transport->api->get_available_source_ids) {
            continue;
        }
        struct zmk_split_transport_status status = transport->api->get_status();
        if (status.enabled && status.available) {
            return transport->api->get_available_source_ids(sources);
        }
    }
    return -ENODEV;
}

static void sync_work_cb(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(sync_work, sync_work_cb);

static void sync_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    int64_t now = k_uptime_get();
    if (atomic_set(&resync_requested, 0)) {
        attempts_left = SYNC_ATTEMPTS;
        retry_delay_ms = SYNC_DELAY_MS;
        next_retry = now;
        last_sent.valid = false;
    }

    bool replay = attempts_left && now >= next_retry;
    if (replay) {
        attempts_left--;
        next_retry = now + retry_delay_ms;
        retry_delay_ms *= 2;
    }

    int layer = zmk_keymap_highest_layer_active();
    if (layer < 0 || layer > 3) {
        LOG_ERR("Display layer out of range: %d", layer);
        return;
    }
    struct reel_display_state state = {.valid = true, .layer = layer};
    int err = update_state(state);
    if (err < 0) {
        LOG_WRN("Failed to notify display state listeners (%d)", err);
    }

    uint8_t sources[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT];
    int count = get_sources(sources);
    if (count >= 0 && (size_t)count <= ARRAY_SIZE(sources)) {
        bool changed = !states_equal(last_sent, state) || count != last_source_count ||
                       memcmp(last_sources, sources, count) != 0;
        struct zmk_behavior_binding binding = {
            .behavior_dev = DEVICE_DT_NAME(DT_DRV_INST(0)),
            .param1 = state.layer,
            .param2 = 0, // Reserved: keep the two-parameter split behavior ABI.
        };
        struct zmk_behavior_binding_event event = {
            .timestamp = now,
            .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
        };
        bool sent = true;
        for (int i = 0; i < count; i++) {
            bool fallback = atomic_test_and_clear_bit(key_fallbacks, sources[i]);
            if (!changed && !replay && !fallback) {
                continue;
            }
            err = zmk_split_central_invoke_behavior(sources[i], &binding, event, true);
            if (err < 0) {
                LOG_WRN("Failed to queue display state for source %u (%d)", sources[i], err);
                sent = false;
            }
        }
        if (sent) {
            last_sent = state;
            last_source_count = count;
            memcpy(last_sources, sources, count);
        }
    } else {
        LOG_DBG("Split display sources not ready (%d)", count);
    }

    if (attempts_left) {
        k_work_schedule(&sync_work, K_MSEC(MAX(1, next_retry - k_uptime_get())));
    }
}

static int central_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev) {
        if (!ev->state || ev->source == ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL ||
            atomic_test_and_set_bit(seen_key_sources, ev->source)) {
            return ZMK_EV_EVENT_BUBBLE;
        }
        /* One source-specific fallback if discovery outlasted the replay window. */
        atomic_set_bit(key_fallbacks, ev->source);
    }
    /* Coalesce layer transitions into a snapshot. */
    k_work_reschedule(&sync_work, K_MSEC(20));
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(reel_display_state_central, central_listener);
ZMK_SUBSCRIPTION(reel_display_state_central, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(reel_display_state_central, zmk_position_state_changed);

static bool is_split_connection(struct bt_conn *conn) {
    struct bt_conn_info info;
    return bt_conn_get_info(conn, &info) == 0 && info.type == BT_CONN_TYPE_LE &&
           info.role == BT_CONN_ROLE_CENTRAL;
}

static void request_resync(struct bt_conn *conn) {
    if (!is_split_connection(conn)) {
        return;
    }
    atomic_set(&resync_requested, 1);
    /* Upstream's dynamically registered connection callback may still be running. */
    k_work_reschedule(&sync_work, K_MSEC(SYNC_DELAY_MS));
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (!err && is_split_connection(conn)) {
        for (size_t i = 0; i < ARRAY_SIZE(seen_key_sources); i++) {
            atomic_clear(&seen_key_sources[i]);
            atomic_clear(&key_fallbacks[i]);
        }
        request_resync(conn);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {
    if (!err && level >= BT_SECURITY_L2) {
        request_resync(conn);
    }
}

/* In this ZMK revision available source IDs precede GATT discovery completion.
 * Queue success also does not acknowledge a write. Bounded replays after both
 * connection and encryption cover that gap without private transport state,
 * extra GATT discovery, or a permanent polling loop. Delivery is best effort.
 */
BT_CONN_CB_DEFINE(reel_display_state_conn_cb) = {
    .connected = connected,
    .security_changed = security_changed,
};

#else

#include <zmk/events/split_peripheral_status_changed.h>

static int peripheral_listener(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    k_spinlock_key_t key = k_spin_lock(&state_lock);
    peripheral_connected = ev->connected;
    bool changed = !ev->connected && cached_state.valid;
    if (!ev->connected) {
        cached_state = (struct reel_display_state){0};
    }
    struct reel_display_state state = cached_state;
    k_spin_unlock(&state_lock, key);

    if (changed) {
        int err =
            raise_reel_display_state_changed((struct reel_display_state_changed){.state = state});
        if (err < 0) {
            LOG_WRN("Failed to notify display disconnect (%d)", err);
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(reel_display_state_peripheral, peripheral_listener);
ZMK_SUBSCRIPTION(reel_display_state_peripheral, zmk_split_peripheral_status_changed);

#endif

static int binding_pressed(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    ARG_UNUSED(binding);
    return -ENOTSUP;
#else
    if (binding->param1 > 3) {
        return -EINVAL;
    }
    return update_state((struct reel_display_state){
        .valid = true, .layer = binding->param1});
#endif
}

static int binding_released(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api display_state_api = {
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
    .binding_pressed = binding_pressed,
    .binding_released = binding_released,
};

/* Registers a behavior reference even though the keymap never references it. */
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &display_state_api);
