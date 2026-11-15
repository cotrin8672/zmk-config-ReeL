"""Compile the actual diagnostic callback with host stubs and check it is passive."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "pointing/reel_input_probe.c").read_text()
source = re.sub(r"^#include <(?:zephyr/|drivers/)[^>]+>\n", "", source, flags=re.M)
stubs = r"""
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
struct device { void *data; };
struct input_event { uint8_t type; uint16_t code; int32_t value; bool sync; };
struct zmk_input_processor_state { int unused; };
struct zmk_input_processor_driver_api {
    int (*handle_event)(const struct device *, struct input_event *,
                        uint32_t, uint32_t, struct zmk_input_processor_state *);
};
static uint16_t mock_lut[11 * 256];
static const struct device mock_rc = { .data = mock_lut };
static int64_t mock_time;
static int log_count;
static int64_t k_uptime_get(void) { return mock_time; }
static void capture_log(const char *fmt, ...) { log_count++; }
#define LOG_MODULE_REGISTER(...)
#define LOG_INF(...) capture_log(__VA_ARGS__)
#define INPUT_EV_REL 2
#define INPUT_REL_X 0
#define INPUT_REL_Y 1
#define ZMK_INPUT_PROC_CONTINUE 0
#define DEVICE_DT_GET(...) (&mock_rc)
#define DT_INST_FOREACH_STATUS_OKAY(...)
"""
checks = r"""
int main(void) {
    struct input_event e = { .type = INPUT_EV_REL, .code = INPUT_REL_X, .value = -7 };
    for (uint32_t stage = 0; stage < 4; stage++) {
        assert(probe_handle_event(NULL, &e, stage, 0, NULL) == 0);
        assert(e.value == -7 && e.code == INPUT_REL_X && !e.sync);
    }
    e.code = INPUT_REL_Y;
    e.value = 4;
    e.sync = true;
    for (uint32_t stage = 0; stage < 4; stage++) {
        assert(probe_handle_event(NULL, &e, stage, 0, NULL) == 0);
        assert(e.value == 4 && e.code == INPUT_REL_Y && e.sync);
        assert(samples[stage][0] == -7 && samples[stage][1] == 4);
    }
    assert(log_count == 1);
    assert(probe_handle_event(NULL, &e, 3, 0, NULL) == 0 && log_count == 1);
    mock_time = 500;
    assert(probe_handle_event(NULL, &e, 3, 0, NULL) == 0 && log_count == 2);
    assert(probe_handle_event(NULL, &e, 4, 0, NULL) == 0);
    assert(last_mode == 1 && samples[0][0] == 0 && samples[0][1] == 4);
    assert(probe_handle_event(NULL, &e, 7, 0, NULL) == 0 && log_count == 3);
    assert(probe_handle_event(NULL, &e, 8, 0, NULL) == -EINVAL && e.value == 4);
    e.type = 1;
    assert(probe_handle_event(NULL, &e, 3, 0, NULL) == 0 && log_count == 3);
    e.type = INPUT_EV_REL;
    e.code = 8;
    assert(probe_handle_event(NULL, &e, 3, 0, NULL) == 0 && log_count == 3);
    puts("Pointer probe: passive snapshots, rate limiting, and RC bypass checks passed");
}
"""
compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
if not compiler:
    raise SystemExit("A host C compiler (cc, clang, or gcc) is required")
with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / "probe-test.c"
    exe = Path(temp) / "probe-test.exe"
    src.write_text(stubs + source + checks)
    subprocess.run([compiler, "-std=c11", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
