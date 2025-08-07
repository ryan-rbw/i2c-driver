#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../test_common.h"

struct test_case {
	const char *name;
	int (*test_func)(void);
};

static int test_device_creation(void)
{
	struct i2c_a78_dev *dev;
	struct device mock_device = {0};
	
	printf("Testing device creation...\n");
	
	dev = devm_kzalloc(&mock_device, sizeof(*dev), GFP_KERNEL);
	assert(dev != NULL);
	
	dev->dev = &mock_device;
	dev->bus_freq = I2C_A78_SPEED_FAST;
	dev->timeout_ms = I2C_A78_TIMEOUT_MS;
	
	assert(dev->bus_freq == 400000);
	assert(dev->timeout_ms == 1000);
	
	printf("✓ Device creation test passed\n");
	return 0;
}

static int test_register_access(void)
{
	struct i2c_a78_dev *dev;
	struct device mock_device = {0};
	void *base_addr = (void *)0x1000;
	
	printf("Testing register access...\n");
	
	dev = devm_kzalloc(&mock_device, sizeof(*dev), GFP_KERNEL);
	assert(dev != NULL);
	
	dev->base = base_addr;
	mock_reset_registers();
	
	i2c_a78_writel(dev, 0x12345678, I2C_A78_CONTROL);
	u32 value = i2c_a78_readl(dev, I2C_A78_CONTROL);
	
	assert(value == 0x12345678);
	
	i2c_a78_writel(dev, 0xABCDEF00, I2C_A78_STATUS);
	value = i2c_a78_readl(dev, I2C_A78_STATUS);
	
	assert(value == 0xABCDEF00);
	
	printf("✓ Register access test passed\n");
	return 0;
}

static int test_speed_configuration(void)
{
	struct i2c_a78_dev *dev;
	struct device mock_device = {0};
	
	printf("Testing speed configuration...\n");
	
	dev = devm_kzalloc(&mock_device, sizeof(*dev), GFP_KERNEL);
	assert(dev != NULL);
	
	assert(I2C_A78_SPEED_STD == 100000);
	assert(I2C_A78_SPEED_FAST == 400000);
	assert(I2C_A78_SPEED_FAST_PLUS == 1000000);
	assert(I2C_A78_SPEED_HIGH == 3400000);
	
	printf("✓ Speed configuration test passed\n");
	return 0;
}

static int test_bit_definitions(void)
{
	printf("Testing bit definitions...\n");
	
	assert(I2C_A78_CONTROL_MASTER_EN == BIT(0));
	assert(I2C_A78_CONTROL_SPEED_STD == (0 << 1));
	assert(I2C_A78_CONTROL_SPEED_FAST == (1 << 1));
	assert(I2C_A78_CONTROL_INT_EN == BIT(3));
	
	assert(I2C_A78_STATUS_BUSY == BIT(0));
	assert(I2C_A78_STATUS_ARB_LOST == BIT(1));
	assert(I2C_A78_STATUS_NACK == BIT(2));
	assert(I2C_A78_STATUS_TX_DONE == BIT(3));
	
	assert(I2C_A78_COMMAND_START == BIT(0));
	assert(I2C_A78_COMMAND_STOP == BIT(1));
	assert(I2C_A78_COMMAND_READ == BIT(2));
	assert(I2C_A78_COMMAND_WRITE == BIT(3));
	
	printf("✓ Bit definitions test passed\n");
	return 0;
}

static int test_dma_structure(void)
{
	struct i2c_a78_dev *dev;
	struct device mock_device = {0};
	
	printf("Testing DMA structure...\n");
	
	dev = devm_kzalloc(&mock_device, sizeof(*dev), GFP_KERNEL);
	assert(dev != NULL);
	
	dev->dma.use_dma = false;
	dev->dma.buf_len = PAGE_SIZE;
	
	assert(dev->dma.use_dma == false);
	assert(dev->dma.buf_len == 4096);
	
	printf("✓ DMA structure test passed\n");
	return 0;
}

static int test_statistics_structure(void)
{
	struct i2c_a78_dev *dev;
	struct device mock_device = {0};
	
	printf("Testing statistics structure...\n");
	
	dev = devm_kzalloc(&mock_device, sizeof(*dev), GFP_KERNEL);
	assert(dev != NULL);
	
	dev->stats.tx_bytes = 100;
	dev->stats.rx_bytes = 200;
	dev->stats.timeouts = 1;
	dev->stats.arb_lost = 2;
	dev->stats.nacks = 3;
	
	assert(dev->stats.tx_bytes == 100);
	assert(dev->stats.rx_bytes == 200);
	assert(dev->stats.timeouts == 1);
	assert(dev->stats.arb_lost == 2);
	assert(dev->stats.nacks == 3);
	
	printf("✓ Statistics structure test passed\n");
	return 0;
}

static int test_address_handling(void)
{
	printf("Testing address handling...\n");
	
	u32 addr_7bit = 0x48;
	u32 addr_10bit = 0x123;
	
	assert((addr_7bit & I2C_A78_ADDRESS_7BIT_MASK) == 0x48);
	assert((addr_10bit & I2C_A78_ADDRESS_10BIT_MASK) == 0x123);
	
	u32 addr_10bit_enabled = addr_10bit | I2C_A78_ADDRESS_10BIT_EN;
	assert((addr_10bit_enabled & I2C_A78_ADDRESS_10BIT_EN) != 0);
	
	printf("✓ Address handling test passed\n");
	return 0;
}

static int test_fifo_status(void)
{
	printf("Testing FIFO status...\n");
	
	u32 status = 0x0A05; // TX level = 5, RX level = 10
	
	u32 tx_level = status & I2C_A78_FIFO_STATUS_TX_LEVEL_MASK;
	u32 rx_level = (status & I2C_A78_FIFO_STATUS_RX_LEVEL_MASK) >> I2C_A78_FIFO_STATUS_RX_LEVEL_SHIFT;
	
	assert(tx_level == 5);
	assert(rx_level == 10);
	assert(I2C_A78_FIFO_SIZE == 16);
	
	printf("✓ FIFO status test passed\n");
	return 0;
}

static struct test_case test_cases[] = {
	{"Device Creation", test_device_creation},
	{"Register Access", test_register_access},
	{"Speed Configuration", test_speed_configuration},
	{"Bit Definitions", test_bit_definitions},
	{"DMA Structure", test_dma_structure},
	{"Statistics Structure", test_statistics_structure},
	{"Address Handling", test_address_handling},
	{"FIFO Status", test_fifo_status},
	{NULL, NULL}
};

int main(void)
{
	int passed = 0;
	int total = 0;
	
	printf("=== I2C A78 Core Unit Tests ===\n\n");
	
	for (struct test_case *tc = test_cases; tc->name != NULL; tc++) {
		printf("Running test: %s\n", tc->name);
		
		if (tc->test_func() == 0) {
			passed++;
		} else {
			printf("✗ Test '%s' FAILED\n", tc->name);
		}
		
		total++;
		printf("\n");
	}
	
	printf("=== Test Summary ===\n");
	printf("Passed: %d/%d\n", passed, total);
	
	if (passed == total) {
		printf("All tests PASSED! ✓\n");
		return 0;
	} else {
		printf("Some tests FAILED! ✗\n");
		return 1;
	}
}