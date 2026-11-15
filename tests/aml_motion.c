#include <assert.h>
#include "../src/aml_motion.h"

int main(void) {
    struct aml_motion s = {0};
    for (int i = 0; i < 2000; ++i) {
        assert(!aml_moved(&s, i % 2 ? 1 : -1, 0, i * 4));
    }
    assert(!aml_moved(&s, 6, 0, 9000));
    assert(!aml_moved(&s, 0, 0, 9100));
    assert(!aml_moved(&s, 0, -6, 9101));
    assert(aml_moved(&s, -6, 0, 9201));
    assert(!aml_moved(&s, 6, 0, 9210));
    assert(aml_moved(&s, -12, 0, 9220));
    for (int i = 1; i <= 12; ++i) {
        assert(aml_moved(&s, 0, -1, 9300 + i) == (i == 12));
    }
}
