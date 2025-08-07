#ifndef __TEST_COMMON_H__
#define __TEST_COMMON_H__

// Include test-specific driver definitions
#include "test_i2c_driver.h"

// Test utility functions
extern void mock_set_pm_suspended(bool suspended);
extern void mock_set_completion_done(bool done);
extern void mock_reset_registers(void);

// Test helper macros
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("ASSERTION FAILED: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

#define TEST_PASSED() \
    do { \
        printf("✓ Test passed\n"); \
        return 0; \
    } while(0)

#define TEST_FAILED(msg) \
    do { \
        printf("✗ Test failed: %s\n", msg); \
        return -1; \
    } while(0)

#endif /* __TEST_COMMON_H__ */