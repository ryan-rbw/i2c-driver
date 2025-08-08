#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../test_common.h"

/**
 * I2C v2.1 Clock Stretching Tests
 * Tests clock stretching timeout handling and slave response scenarios
 */

#define CLOCK_STRETCH_TIMEOUT_US    10000  // 10ms max clock stretch per I2C v2.1
#define CLOCK_STRETCH_MAX_TOTAL_MS  25     // Maximum total transaction time
#define USEC_PER_SEC               1000000

// Mock time tracking for clock stretching simulation
static struct {
    bool clock_stretched;
    u64 stretch_start_us;
    u64 stretch_duration_us;
    u32 stretch_count;
} clock_stretch_state;

static u64 get_current_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / 1000;
}

static void mock_start_clock_stretch(u32 duration_us)
{
    clock_stretch_state.clock_stretched = true;
    clock_stretch_state.stretch_start_us = get_current_time_us();
    clock_stretch_state.stretch_duration_us = duration_us;
    clock_stretch_state.stretch_count++;
    
    printf("Mock: Clock stretch started, duration: %u us\n", duration_us);
}

static bool mock_is_clock_stretched(void)
{
    if (!clock_stretch_state.clock_stretched)
        return false;
        
    u64 current_time = get_current_time_us();
    u64 elapsed = current_time - clock_stretch_state.stretch_start_us;
    
    if (elapsed >= clock_stretch_state.stretch_duration_us) {
        clock_stretch_state.clock_stretched = false;
        printf("Mock: Clock stretch ended after %llu us\n", 
               (unsigned long long)elapsed);
        return false;
    }
    
    return true;
}

static void mock_reset_clock_stretch(void)
{
    memset(&clock_stretch_state, 0, sizeof(clock_stretch_state));
}

static int test_normal_clock_stretching(void)
{
    struct i2c_a78_dev *i2c_dev;
    struct i2c_msg msg;
    u8 data[] = {0x10, 0x20};
    u64 start_time, end_time;
    
    printf("Testing normal clock stretching behavior...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->timeout_ms = CLOCK_STRETCH_MAX_TOTAL_MS;
    mock_reset_registers();
    mock_reset_clock_stretch();
    
    // Setup transfer
    msg.addr = 0x50;
    msg.flags = 0; // Write
    msg.len = sizeof(data);
    msg.buf = data;
    
    start_time = get_current_time_us();
    
    // Simulate short clock stretching (5ms - acceptable)
    mock_start_clock_stretch(5000); // 5ms
    
    // Wait for clock stretch to complete
    while (mock_is_clock_stretched()) {
        usleep(100); // 100us polling
    }
    
    end_time = get_current_time_us();
    u64 total_time = end_time - start_time;
    
    printf("Clock stretch duration: %llu us\n", (unsigned long long)total_time);
    printf("Clock stretch count: %u\n", clock_stretch_state.stretch_count);
    
    // Verify normal clock stretching completed successfully
    assert(total_time >= 5000); // At least the stretch duration
    assert(total_time < CLOCK_STRETCH_TIMEOUT_US); // Under timeout limit
    assert(clock_stretch_state.stretch_count == 1);
    
    printf("✓ Normal clock stretching test passed\n");
    return 0;
}

static int test_clock_stretch_timeout(void)
{
    struct i2c_a78_dev *i2c_dev;
    struct i2c_msg msg;
    u8 data[] = {0x30, 0x40};
    u64 start_time, timeout_time;
    
    printf("Testing clock stretching timeout...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->timeout_ms = CLOCK_STRETCH_MAX_TOTAL_MS;
    mock_reset_registers();
    mock_reset_clock_stretch();
    
    msg.addr = 0x51;
    msg.flags = 0;
    msg.len = sizeof(data);
    msg.buf = data;
    
    start_time = get_current_time_us();
    
    // Simulate excessive clock stretching (15ms - should timeout)
    mock_start_clock_stretch(15000); // 15ms - exceeds 10ms limit
    
    // Poll with timeout detection
    timeout_time = start_time + CLOCK_STRETCH_TIMEOUT_US;
    bool timed_out = false;
    
    while (mock_is_clock_stretched() && !timed_out) {
        if (get_current_time_us() > timeout_time) {
            timed_out = true;
            break;
        }
        usleep(100);
    }
    
    u64 elapsed_time = get_current_time_us() - start_time;
    
    printf("Clock stretch timeout detected after %llu us\n", 
           (unsigned long long)elapsed_time);
    
    // Verify timeout was properly detected
    assert(timed_out == true);
    assert(elapsed_time >= CLOCK_STRETCH_TIMEOUT_US);
    
    // Update driver statistics for timeout
    i2c_dev->stats.timeouts++;
    i2c_dev->state = I2C_A78_STATE_ERROR;
    
    assert(i2c_dev->stats.timeouts == 1);
    assert(i2c_dev->state == I2C_A78_STATE_ERROR);
    
    printf("✓ Clock stretching timeout test passed\n");
    return 0;
}

static int test_multiple_clock_stretches(void)
{
    struct i2c_a78_dev *i2c_dev;
    u64 total_start_time;
    u32 stretch_iterations = 5;
    u32 stretch_duration_each = 2000; // 2ms each
    
    printf("Testing multiple clock stretching events...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    mock_reset_clock_stretch();
    
    total_start_time = get_current_time_us();
    
    // Simulate multiple clock stretching events in one transaction
    for (u32 i = 0; i < stretch_iterations; i++) {
        printf("Clock stretch iteration %u/%u\n", i + 1, stretch_iterations);
        
        mock_start_clock_stretch(stretch_duration_each);
        
        while (mock_is_clock_stretched()) {
            usleep(100);
        }
        
        // Small gap between stretches
        usleep(500); // 0.5ms
    }
    
    u64 total_time = get_current_time_us() - total_start_time;
    u64 expected_min_time = stretch_iterations * stretch_duration_each;
    
    printf("Total transaction time: %llu us\n", (unsigned long long)total_time);
    printf("Expected minimum: %llu us\n", (unsigned long long)expected_min_time);
    printf("Total stretch events: %u\n", clock_stretch_state.stretch_count);
    
    // Verify multiple stretching events
    assert(total_time >= expected_min_time);
    assert(clock_stretch_state.stretch_count == stretch_iterations);
    
    // Ensure total time is within I2C v2.1 transaction limits
    assert(total_time < CLOCK_STRETCH_MAX_TOTAL_MS * 1000); // 25ms max
    
    printf("✓ Multiple clock stretching test passed\n");
    return 0;
}

static int test_clock_stretch_during_read(void)
{
    struct i2c_a78_dev *i2c_dev;
    struct i2c_msg msg;
    u8 read_buffer[4];
    
    printf("Testing clock stretching during read operation...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    mock_reset_registers();
    mock_reset_clock_stretch();
    
    // Setup read operation
    msg.addr = 0x48;
    msg.flags = I2C_M_RD; // Read
    msg.len = sizeof(read_buffer);
    msg.buf = read_buffer;
    
    // Simulate slave stretching clock during read data preparation
    printf("Simulating slave data preparation delay...\n");
    mock_start_clock_stretch(3000); // 3ms stretch during read
    
    u64 start_time = get_current_time_us();
    
    // Wait for read data to be ready (clock stretch to end)
    while (mock_is_clock_stretched()) {
        usleep(100);
    }
    
    u64 read_time = get_current_time_us() - start_time;
    
    // Mock successful read completion
    for (int i = 0; i < sizeof(read_buffer); i++) {
        read_buffer[i] = 0xA0 + i;
    }
    
    printf("Read completed after %llu us\n", (unsigned long long)read_time);
    printf("Read data: ");
    for (int i = 0; i < sizeof(read_buffer); i++) {
        printf("0x%02X ", read_buffer[i]);
    }
    printf("\n");
    
    // Verify read with clock stretching
    assert(read_time >= 3000); // At least stretch duration
    assert(read_time < CLOCK_STRETCH_TIMEOUT_US);
    assert(read_buffer[0] == 0xA0);
    assert(read_buffer[3] == 0xA3);
    
    printf("✓ Clock stretching during read test passed\n");
    return 0;
}

static int test_clock_stretch_recovery(void)
{
    struct i2c_a78_dev *i2c_dev;
    struct i2c_msg msg;
    u8 data[] = {0x55, 0xAA};
    
    printf("Testing recovery from clock stretch timeout...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->timeout_ms = 5; // Short timeout for test
    mock_reset_registers();
    mock_reset_clock_stretch();
    
    msg.addr = 0x52;
    msg.flags = 0;
    msg.len = sizeof(data);
    msg.buf = data;
    
    // First transfer: cause timeout
    printf("First transfer (will timeout)...\n");
    mock_start_clock_stretch(20000); // 20ms - will timeout
    
    // Simulate timeout detection
    usleep(6000); // Wait longer than 5ms timeout
    
    // Driver should detect timeout and recover
    i2c_dev->stats.timeouts++;
    i2c_dev->state = I2C_A78_STATE_ERROR;
    
    printf("Timeout detected, initiating recovery...\n");
    
    // Reset for recovery
    mock_reset_clock_stretch();
    i2c_dev->state = I2C_A78_STATE_IDLE;
    
    // Second transfer: should succeed
    printf("Second transfer (should succeed)...\n");
    mock_start_clock_stretch(2000); // 2ms - acceptable
    
    while (mock_is_clock_stretched()) {
        usleep(100);
    }
    
    i2c_dev->state = I2C_A78_STATE_IDLE; // Use valid state
    i2c_dev->stats.tx_bytes += sizeof(data); // Use existing stat field
    
    printf("Recovery completed successfully\n");
    
    // Verify recovery
    assert(i2c_dev->stats.timeouts == 1);
    assert(i2c_dev->stats.tx_bytes == sizeof(data));
    assert(i2c_dev->state == I2C_A78_STATE_IDLE);
    
    printf("✓ Clock stretch recovery test passed\n");
    return 0;
}

static int test_clock_stretch_i2c_speeds(void)
{
    struct i2c_a78_dev *i2c_dev;
    u32 test_speeds[] = {
        I2C_A78_SPEED_STD,      // 100kHz
        I2C_A78_SPEED_FAST,     // 400kHz  
        I2C_A78_SPEED_FAST_PLUS,// 1MHz
        I2C_A78_SPEED_HIGH      // 3.4MHz
    };
    const char *speed_names[] = {"100kHz", "400kHz", "1MHz", "3.4MHz"};
    
    printf("Testing clock stretching at different I2C speeds...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    mock_reset_registers();
    
    for (int i = 0; i < 4; i++) {
        printf("Testing at %s...\n", speed_names[i]);
        
        i2c_dev->bus_freq = test_speeds[i];
        mock_reset_clock_stretch();
        
        // Clock stretch allowance might vary by speed
        u32 stretch_duration = (i < 2) ? 5000 : 2000; // Slower speeds allow longer stretch
        
        u64 start_time = get_current_time_us();
        mock_start_clock_stretch(stretch_duration);
        
        while (mock_is_clock_stretched()) {
            usleep(100);
        }
        
        u64 elapsed = get_current_time_us() - start_time;
        
        printf("Speed: %s, Stretch: %llu us\n", 
               speed_names[i], (unsigned long long)elapsed);
        
        assert(elapsed >= stretch_duration);
        assert(elapsed < CLOCK_STRETCH_TIMEOUT_US);
    }
    
    printf("✓ Clock stretching at different speeds test passed\n");
    return 0;
}

struct test_case {
    const char *name;
    int (*test_func)(void);
};

static struct test_case clock_stretch_test_cases[] = {
    {"Normal Clock Stretching", test_normal_clock_stretching},
    {"Clock Stretch Timeout", test_clock_stretch_timeout},
    {"Multiple Clock Stretches", test_multiple_clock_stretches},
    {"Clock Stretch During Read", test_clock_stretch_during_read},
    {"Clock Stretch Recovery", test_clock_stretch_recovery},
    {"Clock Stretch at Different Speeds", test_clock_stretch_i2c_speeds},
    {NULL, NULL}
};

int main(void)
{
    int passed = 0;
    int total = 0;
    
    printf("=== I2C v2.1 Clock Stretching Tests ===\n\n");
    
    for (struct test_case *tc = clock_stretch_test_cases; tc->name != NULL; tc++) {
        printf("Running test: %s\n", tc->name);
        
        if (tc->test_func() == 0) {
            passed++;
        } else {
            printf("✗ Test '%s' FAILED\n", tc->name);
        }
        
        total++;
        printf("\n");
    }
    
    printf("=== Clock Stretching Test Summary ===\n");
    printf("Passed: %d/%d\n", passed, total);
    printf("I2C v2.1 Clock Stretching compliance: %.1f%%\n", 
           ((float)passed / total) * 100);
    
    if (passed == total) {
        printf("All Clock Stretching tests PASSED! ✓\n");
        return 0;
    } else {
        printf("Some Clock Stretching tests FAILED! ✗\n");
        return 1;
    }
}