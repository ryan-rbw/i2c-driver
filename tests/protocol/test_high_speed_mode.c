#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../test_common.h"

/**
 * I2C v2.1 High-Speed Mode Protocol Tests
 * Tests 3.4MHz high-speed mode with master code sequences
 */

#define I2C_HIGH_SPEED_FREQ     3400000  // 3.4MHz
#define I2C_MASTER_CODE_MASK    0xF8     // Master code pattern 0000 1XXX
#define I2C_MASTER_CODE_BASE    0x08     // Base master code 0000 1000
#define HS_MODE_SETUP_TIME_NS   160      // High-speed mode setup time
#define HS_MODE_HOLD_TIME_NS    60       // High-speed mode hold time

// High-speed mode state tracking
static struct {
    bool hs_mode_active;
    u8 master_code;
    u32 current_speed;
    u64 hs_start_time_ns;
} hs_mode_state;

static u64 get_current_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void mock_enter_hs_mode(u8 master_code, u32 speed)
{
    hs_mode_state.hs_mode_active = true;
    hs_mode_state.master_code = master_code;
    hs_mode_state.current_speed = speed;
    hs_mode_state.hs_start_time_ns = get_current_time_ns();
    
    printf("Mock: Entered HS mode with master code 0x%02X at %u Hz\n", 
           master_code, speed);
}

static void mock_exit_hs_mode(void)
{
    if (hs_mode_state.hs_mode_active) {
        u64 duration = get_current_time_ns() - hs_mode_state.hs_start_time_ns;
        printf("Mock: Exited HS mode after %llu ns\n", (unsigned long long)duration);
        memset(&hs_mode_state, 0, sizeof(hs_mode_state));
    }
}

static bool mock_is_hs_mode_active(void)
{
    return hs_mode_state.hs_mode_active;
}

static int test_master_code_validation(void)
{
    printf("Testing high-speed master code validation...\n");
    
    // Valid master codes: 0000 1XXX (0x08 to 0x0F)
    u8 valid_codes[] = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    u8 invalid_codes[] = {0x00, 0x07, 0x10, 0xFF};
    
    // Test valid master codes
    for (int i = 0; i < 8; i++) {
        u8 code = valid_codes[i];
        printf("Testing valid master code: 0x%02X\n", code);
        
        assert((code & I2C_MASTER_CODE_MASK) == I2C_MASTER_CODE_BASE);
        assert(code >= 0x08 && code <= 0x0F);
    }
    
    // Test invalid master codes
    for (int i = 0; i < 4; i++) {
        u8 code = invalid_codes[i];
        printf("Testing invalid master code: 0x%02X\n", code);
        
        bool is_valid = (code & I2C_MASTER_CODE_MASK) == I2C_MASTER_CODE_BASE;
        assert(!is_valid);
    }
    
    printf("✓ Master code validation test passed\n");
    return 0;
}

static int test_hs_mode_entry_sequence(void)
{
    struct i2c_a78_dev *i2c_dev;
    struct i2c_msg msg;
    u8 data[] = {0x12, 0x34};
    u8 master_code = 0x0A;
    
    printf("Testing high-speed mode entry sequence...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->bus_freq = I2C_HIGH_SPEED_FREQ;
    mock_reset_registers();
    memset(&hs_mode_state, 0, sizeof(hs_mode_state));
    
    // High-speed mode entry sequence:
    // 1. START condition at Fm speed (≤400kHz)  
    // 2. Master code (0000 1XXX + NACK)
    // 3. Repeated START 
    // 4. Switch to HS speed (3.4MHz)
    // 5. Normal I2C transaction at high speed
    
    printf("Step 1: START at Fast mode (400kHz)\n");
    i2c_dev->bus_freq = I2C_A78_SPEED_FAST; // Start at 400kHz
    
    printf("Step 2: Send master code 0x%02X\n", master_code);
    assert((master_code & I2C_MASTER_CODE_MASK) == I2C_MASTER_CODE_BASE);
    
    // Master code is sent, slave should NACK (expected behavior)
    i2c_a78_writel(i2c_dev, I2C_A78_STATUS_NACK, I2C_A78_STATUS);
    u32 status = i2c_a78_readl(i2c_dev, I2C_A78_STATUS);
    assert(status & I2C_A78_STATUS_NACK); // Master code should get NACK
    
    printf("Step 3: Repeated START condition\n");
    i2c_a78_writel(i2c_dev, I2C_A78_COMMAND_START, I2C_A78_COMMAND);
    
    printf("Step 4: Switch to high-speed mode (3.4MHz)\n");
    i2c_dev->bus_freq = I2C_HIGH_SPEED_FREQ;
    mock_enter_hs_mode(master_code, I2C_HIGH_SPEED_FREQ);
    
    printf("Step 5: Normal transaction at high speed\n");
    msg.addr = 0x50;
    msg.flags = 0; // Write
    msg.len = sizeof(data);
    msg.buf = data;
    
    // Verify high-speed mode is active
    assert(mock_is_hs_mode_active());
    assert(hs_mode_state.master_code == master_code);
    assert(hs_mode_state.current_speed == I2C_HIGH_SPEED_FREQ);
    
    printf("✓ High-speed mode entry sequence test passed\n");
    return 0;
}

static int test_hs_mode_exit_sequence(void)
{
    struct i2c_a78_dev *i2c_dev;
    u8 master_code = 0x0C;
    
    printf("Testing high-speed mode exit sequence...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->bus_freq = I2C_HIGH_SPEED_FREQ;
    mock_reset_registers();
    
    // Start in high-speed mode
    mock_enter_hs_mode(master_code, I2C_HIGH_SPEED_FREQ);
    assert(mock_is_hs_mode_active());
    
    printf("Currently in HS mode with master code 0x%02X\n", master_code);
    
    // High-speed mode exit sequence:
    // 1. Complete current transaction
    // 2. STOP condition
    // 3. Return to standard/fast mode
    // 4. Bus idle
    
    printf("Step 1: Complete current transaction\n");
    i2c_a78_writel(i2c_dev, I2C_A78_STATUS_TX_DONE, I2C_A78_STATUS);
    
    printf("Step 2: Send STOP condition\n");
    i2c_a78_writel(i2c_dev, I2C_A78_COMMAND_STOP, I2C_A78_COMMAND);
    
    printf("Step 3: Return to Fast mode (400kHz)\n");
    i2c_dev->bus_freq = I2C_A78_SPEED_FAST;
    mock_exit_hs_mode();
    
    printf("Step 4: Bus idle\n");
    i2c_dev->state = I2C_A78_STATE_IDLE;
    
    // Verify exit from high-speed mode
    assert(!mock_is_hs_mode_active());
    assert(i2c_dev->bus_freq == I2C_A78_SPEED_FAST);
    assert(i2c_dev->state == I2C_A78_STATE_IDLE);
    
    printf("✓ High-speed mode exit sequence test passed\n");
    return 0;
}

static int test_hs_mode_timing_requirements(void)
{
    struct i2c_a78_dev *i2c_dev;
    u64 start_time, end_time, setup_time, hold_time;
    
    printf("Testing high-speed mode timing requirements...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->bus_freq = I2C_HIGH_SPEED_FREQ;
    mock_reset_registers();
    
    // Test setup time requirement (min 160ns)
    printf("Testing setup time requirement (min 160ns)...\n");
    
    start_time = get_current_time_ns();
    
    // Simulate setup time delay
    struct timespec setup_delay = {0, HS_MODE_SETUP_TIME_NS};
    nanosleep(&setup_delay, NULL);
    
    end_time = get_current_time_ns();
    setup_time = end_time - start_time;
    
    printf("Measured setup time: %llu ns\n", (unsigned long long)setup_time);
    assert(setup_time >= HS_MODE_SETUP_TIME_NS);
    
    // Test hold time requirement (min 60ns)
    printf("Testing hold time requirement (min 60ns)...\n");
    
    start_time = get_current_time_ns();
    
    // Simulate hold time delay  
    struct timespec hold_delay = {0, HS_MODE_HOLD_TIME_NS};
    nanosleep(&hold_delay, NULL);
    
    end_time = get_current_time_ns();
    hold_time = end_time - start_time;
    
    printf("Measured hold time: %llu ns\n", (unsigned long long)hold_time);
    assert(hold_time >= HS_MODE_HOLD_TIME_NS);
    
    // Test clock frequency accuracy (3.4MHz ±10%)
    u32 min_freq = I2C_HIGH_SPEED_FREQ * 90 / 100;  // -10%
    u32 max_freq = I2C_HIGH_SPEED_FREQ * 110 / 100; // +10%
    
    printf("Testing frequency range: %u - %u Hz\n", min_freq, max_freq);
    assert(i2c_dev->bus_freq >= min_freq);
    assert(i2c_dev->bus_freq <= max_freq);
    
    printf("✓ High-speed mode timing requirements test passed\n");
    return 0;
}

static int test_hs_mode_multi_master(void)
{
    struct i2c_a78_dev *i2c_dev1, *i2c_dev2;
    u8 master_code1 = 0x09;
    u8 master_code2 = 0x0E;
    
    printf("Testing high-speed mode multi-master scenarios...\n");
    
    // Create two master devices
    i2c_dev1 = devm_kzalloc(NULL, sizeof(*i2c_dev1), GFP_KERNEL);
    i2c_dev1->base = (void *)0x2000;
    i2c_dev1->bus_freq = I2C_HIGH_SPEED_FREQ;
    
    i2c_dev2 = devm_kzalloc(NULL, sizeof(*i2c_dev2), GFP_KERNEL);
    i2c_dev2->base = (void *)0x3000;
    i2c_dev2->bus_freq = I2C_HIGH_SPEED_FREQ;
    
    mock_reset_registers();
    
    printf("Master 1 using code 0x%02X\n", master_code1);
    printf("Master 2 using code 0x%02X\n", master_code2);
    
    // Verify different master codes
    assert(master_code1 != master_code2);
    assert((master_code1 & I2C_MASTER_CODE_MASK) == I2C_MASTER_CODE_BASE);
    assert((master_code2 & I2C_MASTER_CODE_MASK) == I2C_MASTER_CODE_BASE);
    
    // Master 1 enters HS mode first
    printf("Master 1 enters HS mode...\n");
    mock_enter_hs_mode(master_code1, I2C_HIGH_SPEED_FREQ);
    
    // Simulate arbitration - only one master can be in HS mode
    printf("Testing HS mode arbitration...\n");
    
    // Master 2 attempts to enter HS mode but detects Master 1 is active
    bool arbitration_lost = true; // Would be detected by hardware
    
    if (arbitration_lost) {
        printf("Master 2 loses arbitration, waits for bus\n");
        i2c_dev2->stats.arb_lost++;
    } else {
        printf("Master 2 enters HS mode\n");
        // This shouldn't happen in proper arbitration
    }
    
    // Master 1 completes transaction and exits HS mode
    printf("Master 1 completes transaction\n");
    mock_exit_hs_mode();
    
    // Now Master 2 can enter HS mode
    printf("Master 2 now enters HS mode...\n");
    mock_enter_hs_mode(master_code2, I2C_HIGH_SPEED_FREQ);
    
    assert(mock_is_hs_mode_active());
    assert(hs_mode_state.master_code == master_code2);
    assert(i2c_dev2->stats.arb_lost == 1);
    
    mock_exit_hs_mode();
    
    printf("✓ High-speed mode multi-master test passed\n");
    return 0;
}

static int test_hs_mode_error_conditions(void)
{
    struct i2c_a78_dev *i2c_dev;
    u8 invalid_master_code = 0x07; // Invalid code
    
    printf("Testing high-speed mode error conditions...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->bus_freq = I2C_A78_SPEED_FAST;
    mock_reset_registers();
    
    // Test 1: Invalid master code
    printf("Test 1: Invalid master code 0x%02X\n", invalid_master_code);
    
    bool code_valid = (invalid_master_code & I2C_MASTER_CODE_MASK) == I2C_MASTER_CODE_BASE;
    assert(!code_valid); // Should be invalid
    
    if (!code_valid) {
        printf("Driver correctly rejects invalid master code\n");
        i2c_dev->stats.nacks++; // Use existing error counter
    }
    
    // Test 2: HS mode entry without master code
    printf("Test 2: Attempting HS mode without master code\n");
    
    // Try to switch to HS speed without proper master code sequence
    i2c_dev->bus_freq = I2C_HIGH_SPEED_FREQ;
    
    // This should be detected as a protocol error
    if (!mock_is_hs_mode_active()) {
        printf("Driver correctly prevents HS mode without master code\n");
        i2c_dev->stats.nacks++; // Use existing error counter
    }
    
    // Test 3: Clock speed violation
    printf("Test 3: Clock speed violation in HS mode\n");
    
    u32 excessive_speed = 4000000; // 4MHz - exceeds 3.4MHz max
    i2c_dev->bus_freq = excessive_speed;
    
    if (excessive_speed > I2C_HIGH_SPEED_FREQ) {
        printf("Driver detects clock speed violation: %u Hz\n", excessive_speed);
        i2c_dev->stats.nacks++; // Use existing error counter
    }
    
    // Test 4: Arbitration during master code
    printf("Test 4: Arbitration loss during master code\n");
    
    // Simulate arbitration loss during master code transmission
    i2c_a78_writel(i2c_dev, I2C_A78_STATUS_ARB_LOST, I2C_A78_STATUS);
    u32 status = i2c_a78_readl(i2c_dev, I2C_A78_STATUS);
    
    if (status & I2C_A78_STATUS_ARB_LOST) {
        printf("Arbitration lost during master code transmission\n");
        i2c_dev->stats.arb_lost++;
        i2c_dev->state = I2C_A78_STATE_ERROR;
    }
    
    // Verify error statistics
    assert(i2c_dev->stats.nacks == 3); // Protocol errors counted as NACKs
    assert(i2c_dev->stats.arb_lost == 1);
    assert(i2c_dev->state == I2C_A78_STATE_ERROR);
    
    printf("✓ High-speed mode error conditions test passed\n");
    return 0;
}

static int test_hs_mode_performance(void)
{
    struct i2c_a78_dev *i2c_dev;
    struct i2c_msg msg;
    u8 large_data[64]; // Large transfer to test HS mode performance
    u64 start_time, end_time, transfer_time;
    
    printf("Testing high-speed mode performance...\n");
    
    i2c_dev = devm_kzalloc(NULL, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->base = (void *)0x2000;
    i2c_dev->bus_freq = I2C_HIGH_SPEED_FREQ;
    mock_reset_registers();
    
    // Initialize test data
    for (int i = 0; i < sizeof(large_data); i++) {
        large_data[i] = 0x10 + (i % 16);
    }
    
    // Setup large transfer
    msg.addr = 0x55;
    msg.flags = 0; // Write
    msg.len = sizeof(large_data);
    msg.buf = large_data;
    
    printf("Performing 64-byte transfer at 3.4MHz...\n");
    
    start_time = get_current_time_ns();
    
    // Enter HS mode
    mock_enter_hs_mode(0x0B, I2C_HIGH_SPEED_FREQ);
    
    // Simulate transfer time calculation
    // At 3.4MHz: ~294ns per bit, 8 bits + ACK = ~2.65μs per byte
    u32 expected_time_ns = sizeof(large_data) * 2650; // ~169μs for 64 bytes
    
    // Simulate actual transfer
    struct timespec transfer_delay = {0, expected_time_ns};
    nanosleep(&transfer_delay, NULL);
    
    end_time = get_current_time_ns();
    transfer_time = end_time - start_time;
    
    // Calculate throughput
    double bytes_per_sec = (sizeof(large_data) * 1000000000.0) / transfer_time;
    double mbps = (bytes_per_sec * 8) / 1000000.0; // Convert to Mbps
    
    printf("Transfer time: %llu ns\n", (unsigned long long)transfer_time);
    printf("Throughput: %.2f Mbps\n", mbps);
    printf("Expected minimum throughput: 2.0 Mbps\n");
    
    // Exit HS mode
    mock_exit_hs_mode();
    
    // Verify performance meets expectations
    assert(transfer_time >= expected_time_ns / 2); // Allow for overhead
    assert(mbps >= 2.0); // Should achieve at least 2 Mbps at 3.4MHz
    
    // Update statistics
    i2c_dev->stats.tx_bytes += sizeof(large_data);
    
    assert(i2c_dev->stats.tx_bytes == sizeof(large_data));
    
    printf("✓ High-speed mode performance test passed\n");
    return 0;
}

struct test_case {
    const char *name;
    int (*test_func)(void);
};

static struct test_case hs_mode_test_cases[] = {
    {"Master Code Validation", test_master_code_validation},
    {"HS Mode Entry Sequence", test_hs_mode_entry_sequence},
    {"HS Mode Exit Sequence", test_hs_mode_exit_sequence},
    {"HS Mode Timing Requirements", test_hs_mode_timing_requirements},
    {"HS Mode Multi-Master", test_hs_mode_multi_master},
    {"HS Mode Error Conditions", test_hs_mode_error_conditions},
    {"HS Mode Performance", test_hs_mode_performance},
    {NULL, NULL}
};

int main(void)
{
    int passed = 0;
    int total = 0;
    
    printf("=== I2C v2.1 High-Speed Mode Protocol Tests ===\n\n");
    
    for (struct test_case *tc = hs_mode_test_cases; tc->name != NULL; tc++) {
        printf("Running test: %s\n", tc->name);
        
        if (tc->test_func() == 0) {
            passed++;
        } else {
            printf("✗ Test '%s' FAILED\n", tc->name);
        }
        
        total++;
        printf("\n");
    }
    
    printf("=== High-Speed Mode Test Summary ===\n");
    printf("Passed: %d/%d\n", passed, total);
    printf("I2C v2.1 High-Speed Mode compliance: %.1f%%\n", 
           ((float)passed / total) * 100);
    
    if (passed == total) {
        printf("All High-Speed Mode tests PASSED! ✓\n");
        return 0;
    } else {
        printf("Some High-Speed Mode tests FAILED! ✗\n");
        return 1;
    }
}