#pragma once
#include <stddef.h>

enum { REEL_TEST_HEAP_BYTES = 8192 };
size_t reel_test_heap_peak(void);
size_t reel_test_heap_failures(void);
size_t reel_test_heap_largest_failure(void);
