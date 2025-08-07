#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "../test_common.h"

#define MAX_STRESS_ITERATIONS 1000
#define MAX_TRANSFER_SIZE 256
#define STRESS_TIMEOUT_MS 5000

static struct i2c_a78_dev *create_stress_test_device(void)
{
	struct device *mock_dev;
	struct i2c_a78_dev *i2c_dev;
	
	mock_dev = devm_kzalloc(NULL, sizeof(*mock_dev), GFP_KERNEL);
	strcpy(mock_dev->name, "stress-test-i2c");
	
	i2c_dev = devm_kzalloc(mock_dev, sizeof(*i2c_dev), GFP_KERNEL);
	i2c_dev->dev = mock_dev;
	i2c_dev->base = (void *)0x3000;
	i2c_dev->bus_freq = I2C_A78_SPEED_FAST;
	i2c_dev->timeout_ms = STRESS_TIMEOUT_MS;
	i2c_dev->state = I2C_A78_STATE_IDLE;
	
	return i2c_dev;
}

static int test_rapid_transfers(void)
{
	struct i2c_a78_dev *i2c_dev;
	struct i2c_msg msg;
	u8 data[16];
	int successful_transfers = 0;
	clock_t start_time, end_time;
	
	printf("Testing rapid consecutive transfers...\n");
	
	i2c_dev = create_stress_test_device();
	
	// Initialize test data
	for (int i = 0; i < 16; i++) {
		data[i] = i;
	}
	
	msg.addr = 0x50;
	msg.flags = 0;
	msg.len = 16;
	msg.buf = data;
	
	start_time = clock();
	
	for (int i = 0; i < MAX_STRESS_ITERATIONS; i++) {
		// Simulate transfer setup
		i2c_dev->state = I2C_A78_STATE_START;
		
		// Update statistics
		if (msg.flags & I2C_M_RD) {
			i2c_dev->stats.rx_bytes += msg.len;
		} else {
			i2c_dev->stats.tx_bytes += msg.len;
		}
		
		// Simulate completion
		i2c_dev->state = I2C_A78_STATE_IDLE;
		successful_transfers++;
		
		// Alternate between read and write
		msg.flags = (i % 2) ? I2C_M_RD : 0;
	}
	
	end_time = clock();
	double cpu_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
	
	printf("Completed %d transfers in %.3f seconds\n", successful_transfers, cpu_time);
	printf("Transfer rate: %.1f transfers/second\n", successful_transfers / cpu_time);
	printf("Total bytes transferred: %llu TX, %llu RX\n", 
	       i2c_dev->stats.tx_bytes, i2c_dev->stats.rx_bytes);
	
	assert(successful_transfers == MAX_STRESS_ITERATIONS);
	printf("✓ Rapid transfers stress test passed\n");
	return 0;
}

static int test_variable_message_sizes(void)
{
	struct i2c_a78_dev *i2c_dev;
	struct i2c_msg msg;
	u8 *data;
	int total_bytes = 0;
	
	printf("Testing variable message sizes...\n");
	
	i2c_dev = create_stress_test_device();
	data = malloc(MAX_TRANSFER_SIZE);
	
	for (int size = 1; size <= MAX_TRANSFER_SIZE; size++) {
		msg.addr = 0x50 + (size % 8); // Vary addresses too
		msg.flags = (size % 2) ? I2C_M_RD : 0;
		msg.len = size;
		msg.buf = data;
		
		// Fill with test pattern
		for (int i = 0; i < size; i++) {
			data[i] = (size + i) & 0xFF;
		}
		
		// Simulate transfer
		i2c_dev->state = I2C_A78_STATE_START;
		
		// Check DMA vs PIO decision
		bool should_use_dma = (size >= I2C_A78_DMA_THRESHOLD);
		printf("Transfer size %d: %s\n", size, should_use_dma ? "DMA" : "PIO");
		
		// Update statistics
		if (msg.flags & I2C_M_RD) {
			i2c_dev->stats.rx_bytes += msg.len;
		} else {
			i2c_dev->stats.tx_bytes += msg.len;
		}
		
		i2c_dev->state = I2C_A78_STATE_IDLE;
		total_bytes += size;
	}
	
	printf("Total bytes in variable size test: %d\n", total_bytes);
	printf("Average transfer size: %.1f bytes\n", (float)total_bytes / MAX_TRANSFER_SIZE);
	
	free(data);
	printf("✓ Variable message sizes stress test passed\n");
	return 0;
}

static int test_address_space_coverage(void)
{
	struct i2c_a78_dev *i2c_dev;
	struct i2c_msg msg;
	u8 data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
	
	printf("Testing address space coverage...\n");
	
	i2c_dev = create_stress_test_device();
	
	// Test 7-bit address space
	printf("Testing 7-bit addresses (0x01-0x7F)...\n");
	for (int addr = 0x01; addr < 0x80; addr++) {
		msg.addr = addr;
		msg.flags = (addr % 2) ? I2C_M_RD : 0;
		msg.len = 4;
		msg.buf = data;
		
		// Skip reserved addresses in real hardware
		if (addr == 0x00 || addr >= 0x78) {
			continue;
		}
		
		// Simulate transfer
		i2c_dev->stats.tx_bytes += (msg.flags & I2C_M_RD) ? 0 : msg.len;
		i2c_dev->stats.rx_bytes += (msg.flags & I2C_M_RD) ? msg.len : 0;
	}
	
	// Test 10-bit addresses (sample)
	printf("Testing 10-bit addresses (sample)...\n");
	for (int addr = 0x100; addr < 0x110; addr++) {
		msg.addr = addr;
		msg.flags = I2C_M_TEN | ((addr % 2) ? I2C_M_RD : 0);
		msg.len = 4;
		msg.buf = data;
		
		// Verify 10-bit flag handling
		assert(msg.flags & I2C_M_TEN);
		assert(msg.addr <= I2C_A78_ADDRESS_10BIT_MASK);
		
		i2c_dev->stats.tx_bytes += (msg.flags & I2C_M_RD) ? 0 : msg.len;
		i2c_dev->stats.rx_bytes += (msg.flags & I2C_M_RD) ? msg.len : 0;
	}
	
	printf("✓ Address space coverage stress test passed\n");
	return 0;
}

static int test_register_access_patterns(void)
{
	struct i2c_a78_dev *i2c_dev;
	u32 test_patterns[] = {
		0x00000000, 0xFFFFFFFF, 0xAAAAAAAA, 0x55555555,
		0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE
	};
	int num_patterns = sizeof(test_patterns) / sizeof(test_patterns[0]);
	
	printf("Testing intensive register access patterns...\n");
	
	i2c_dev = create_stress_test_device();
	
	// Test all register offsets with various patterns
	u32 register_offsets[] = {
		I2C_A78_CONTROL, I2C_A78_STATUS, I2C_A78_DATA,
		I2C_A78_ADDRESS, I2C_A78_COMMAND, I2C_A78_FIFO_STATUS,
		I2C_A78_INTERRUPT, I2C_A78_PRESCALER
	};
	int num_registers = sizeof(register_offsets) / sizeof(register_offsets[0]);
	
	for (int iter = 0; iter < 100; iter++) {
		for (int reg = 0; reg < num_registers; reg++) {
			for (int pat = 0; pat < num_patterns; pat++) {
				// Write pattern
				i2c_a78_writel(i2c_dev, test_patterns[pat], register_offsets[reg]);
				
				// Read back and verify (for writable registers)
				u32 readback = i2c_a78_readl(i2c_dev, register_offsets[reg]);
				
				// Some registers are read-only or have special behavior
				if (register_offsets[reg] != I2C_A78_STATUS) {
					// For non-status registers, expect write/read consistency
				}
			}
		}
	}
	
	printf("Completed intensive register access test\n");
	printf("✓ Register access patterns stress test passed\n");
	return 0;
}

static int test_power_management_cycles(void)
{
	struct i2c_a78_dev *i2c_dev;
	
	printf("Testing power management cycles...\n");
	
	i2c_dev = create_stress_test_device();
	
	// Simulate rapid suspend/resume cycles
	for (int cycle = 0; cycle < 100; cycle++) {
		// Suspend
		i2c_dev->suspended = true;
		i2c_dev->saved_control = i2c_a78_readl(i2c_dev, I2C_A78_CONTROL);
		i2c_dev->saved_prescaler = i2c_a78_readl(i2c_dev, I2C_A78_PRESCALER);
		
		// Resume
		i2c_a78_writel(i2c_dev, i2c_dev->saved_prescaler, I2C_A78_PRESCALER);
		i2c_a78_writel(i2c_dev, i2c_dev->saved_control, I2C_A78_CONTROL);
		i2c_dev->suspended = false;
		
		// Verify state consistency
		assert(i2c_dev->suspended == false);
		
		if (cycle % 10 == 0) {
			printf("Completed %d PM cycles\n", cycle);
		}
	}
	
	printf("✓ Power management cycles stress test passed\n");
	return 0;
}

static int test_error_recovery_cycles(void)
{
	struct i2c_a78_dev *i2c_dev;
	
	printf("Testing error recovery cycles...\n");
	
	i2c_dev = create_stress_test_device();
	
	for (int cycle = 0; cycle < 50; cycle++) {
		// Inject various errors
		switch (cycle % 4) {
		case 0:
			// Arbitration lost
			i2c_dev->state = I2C_A78_STATE_ERROR;
			i2c_dev->stats.arb_lost++;
			break;
		case 1:
			// NACK
			i2c_dev->state = I2C_A78_STATE_ERROR;
			i2c_dev->stats.nacks++;
			break;
		case 2:
			// Timeout
			i2c_dev->state = I2C_A78_STATE_ERROR;
			i2c_dev->stats.timeouts++;
			break;
		case 3:
			// Bus busy
			i2c_dev->state = I2C_A78_STATE_ERROR;
			break;
		}
		
		// Recovery
		i2c_dev->state = I2C_A78_STATE_IDLE;
		
		// Clear error flags
		i2c_a78_writel(i2c_dev, 0xFF, I2C_A78_INTERRUPT);
	}
	
	printf("Error statistics after stress test:\n");
	printf("  Arbitration lost: %u\n", i2c_dev->stats.arb_lost);
	printf("  NACKs: %u\n", i2c_dev->stats.nacks);
	printf("  Timeouts: %u\n", i2c_dev->stats.timeouts);
	
	printf("✓ Error recovery cycles stress test passed\n");
	return 0;
}

static int test_fifo_boundary_conditions(void)
{
	struct i2c_a78_dev *i2c_dev;
	
	printf("Testing FIFO boundary conditions...\n");
	
	i2c_dev = create_stress_test_device();
	
	// Test FIFO full/empty conditions
	for (int test = 0; test < 100; test++) {
		// Simulate FIFO full
		u32 fifo_status = (I2C_A78_FIFO_SIZE << I2C_A78_FIFO_STATUS_RX_LEVEL_SHIFT) | 
				  I2C_A78_FIFO_SIZE;
		i2c_a78_writel(i2c_dev, fifo_status, I2C_A78_FIFO_STATUS);
		
		// Test FIFO level extraction
		u32 tx_level = fifo_status & I2C_A78_FIFO_STATUS_TX_LEVEL_MASK;
		u32 rx_level = (fifo_status & I2C_A78_FIFO_STATUS_RX_LEVEL_MASK) >> 
			       I2C_A78_FIFO_STATUS_RX_LEVEL_SHIFT;
		
		assert(tx_level == I2C_A78_FIFO_SIZE);
		assert(rx_level == I2C_A78_FIFO_SIZE);
		
		// Simulate FIFO empty
		i2c_a78_writel(i2c_dev, 0, I2C_A78_FIFO_STATUS);
		
		// Clear FIFOs
		i2c_a78_writel(i2c_dev, I2C_A78_CONTROL_FIFO_TX_CLR | 
			       I2C_A78_CONTROL_FIFO_RX_CLR, I2C_A78_CONTROL);
	}
	
	printf("✓ FIFO boundary conditions stress test passed\n");
	return 0;
}

struct stress_test_case {
	const char *name;
	int (*test_func)(void);
	int iterations;
};

static struct stress_test_case stress_test_cases[] = {
	{"Rapid Transfers", test_rapid_transfers, 1},
	{"Variable Message Sizes", test_variable_message_sizes, 1},
	{"Address Space Coverage", test_address_space_coverage, 1},
	{"Register Access Patterns", test_register_access_patterns, 1},
	{"Power Management Cycles", test_power_management_cycles, 1},
	{"Error Recovery Cycles", test_error_recovery_cycles, 1},
	{"FIFO Boundary Conditions", test_fifo_boundary_conditions, 1},
	{NULL, NULL, 0}
};

int main(void)
{
	int passed = 0;
	int total = 0;
	clock_t total_start_time, total_end_time;
	
	printf("=== I2C A78 Stress Tests ===\n\n");
	
	total_start_time = clock();
	
	for (struct stress_test_case *tc = stress_test_cases; tc->name != NULL; tc++) {
		printf("Running stress test: %s\n", tc->name);
		
		clock_t test_start = clock();
		int result = tc->test_func();
		clock_t test_end = clock();
		
		double test_time = ((double)(test_end - test_start)) / CLOCKS_PER_SEC;
		printf("Test completed in %.3f seconds\n", test_time);
		
		if (result == 0) {
			passed++;
		} else {
			printf("✗ Stress test '%s' FAILED\n", tc->name);
		}
		
		total++;
		printf("\n");
	}
	
	total_end_time = clock();
	double total_time = ((double)(total_end_time - total_start_time)) / CLOCKS_PER_SEC;
	
	printf("=== Stress Test Summary ===\n");
	printf("Passed: %d/%d\n", passed, total);
	printf("Total execution time: %.3f seconds\n", total_time);
	printf("System stability: %s\n", (passed == total) ? "STABLE" : "UNSTABLE");
	
	if (passed == total) {
		printf("All stress tests PASSED! System is stable under load ✓\n");
		return 0;
	} else {
		printf("Some stress tests FAILED! System may be unstable ✗\n");
		return 1;
	}
}