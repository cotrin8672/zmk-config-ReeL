#define DT_DRV_COMPAT reel_input_probe

#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/input_processor.h>

LOG_MODULE_REGISTER(reel_pointer, LOG_LEVEL_INF);

static int32_t samples[4][2];
static int last_mode = -1;
static int64_t next_log;

static int probe_handle_event(const struct device *dev, struct input_event *event,
                              uint32_t param1, uint32_t param2,
                              struct zmk_input_processor_state *state) {
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (param1 > 7) {
        return -EINVAL;
    }

    // 0..3 trace RC3; 4..7 trace the RC0 (bypass) chain on L1/L2.
    int mode = param1 / 4;
    int stage = param1 % 4;
    if (mode != last_mode) {
        memset(samples, 0, sizeof(samples));
        last_mode = mode;
        next_log = 0;
    }
    samples[stage][event->code == INPUT_REL_Y] = event->value;
    if (stage == 3 && event->sync) {
        int64_t now = k_uptime_get();
        if (now >= next_log) {
            // Layout is fixed by the module revision pinned in west.yml.
            const struct device *rc = DEVICE_DT_GET(DT_NODELABEL(zip_response_curve));
            const uint16_t *lut = rc->data;
            LOG_INF("RC%d raw=%d,%d lpf=%d,%d rc=%d,%d scaled=%d,%d lut3=%u,%u,%u",
                    mode ? 0 : 3, samples[0][0], samples[0][1],
                    samples[1][0], samples[1][1], samples[2][0], samples[2][1],
                    samples[3][0], samples[3][1],
                    lut[3 * 256], lut[3 * 256 + 7], lut[4 * 256 - 1]);
            next_log = now + 500;
        }
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api probe_api = {
    .handle_event = probe_handle_event,
};

#define PROBE_INST(n)                                                                                \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                      \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &probe_api);

DT_INST_FOREACH_STATUS_OKAY(PROBE_INST)
