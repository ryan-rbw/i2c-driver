#include "mock-linux-kernel.h"
#include "../test_i2c_driver.h"

static u32 mock_registers[64];
static struct clk mock_clk = {0};
static struct dma_chan mock_tx_chan = {0};
static struct dma_chan mock_rx_chan = {0};
static struct dma_async_tx_descriptor mock_tx_desc = {0};
static struct dma_async_tx_descriptor mock_rx_desc = {0};

static bool mock_clk_enabled = false;
static bool mock_completion_done = false;
static bool mock_pm_suspended = false;
static void *mock_dma_tx_buffer = NULL;
static void *mock_dma_rx_buffer = NULL;

u32 mock_readl(volatile void __iomem *addr)
{
	uintptr_t offset = ((uintptr_t)addr) & 0xFF;
	u32 reg_idx = offset / 4;
	
	if (reg_idx < ARRAY_SIZE(mock_registers)) {
		printf("[MOCK] Reading register 0x%02lx: 0x%08x\n", offset, mock_registers[reg_idx]);
		return mock_registers[reg_idx];
	}
	
	return 0;
}

void mock_writel(u32 val, volatile void __iomem *addr)
{
	uintptr_t offset = ((uintptr_t)addr) & 0xFF;
	u32 reg_idx = offset / 4;
	
	if (reg_idx < ARRAY_SIZE(mock_registers)) {
		printf("[MOCK] Writing register 0x%02lx: 0x%08x\n", offset, val);
		mock_registers[reg_idx] = val;
	}
}

void *devm_kzalloc(struct device *dev, size_t size, int flags)
{
	void *ptr = calloc(1, size);
	printf("[MOCK] Allocated %zu bytes at %p\n", size, ptr);
	return ptr;
}

void *devm_ioremap_resource(struct device *dev, struct resource *res)
{
	void *ptr = malloc(res->end - res->start + 1);
	printf("[MOCK] Mapped resource 0x%lx-0x%lx to %p\n", res->start, res->end, ptr);
	return ptr;
}

struct clk *devm_clk_get(struct device *dev, const char *id)
{
	printf("[MOCK] Getting clock '%s'\n", id);
	return &mock_clk;
}

int devm_request_irq(struct device *dev, unsigned int irq, void *handler,
		     unsigned long irqflags, const char *devname, void *dev_id)
{
	printf("[MOCK] Requesting IRQ %u for device %s\n", irq, devname);
	return 0;
}

int clk_prepare_enable(struct clk *clk)
{
	printf("[MOCK] Enabling clock\n");
	mock_clk_enabled = true;
	return 0;
}

void clk_disable_unprepare(struct clk *clk)
{
	printf("[MOCK] Disabling clock\n");
	mock_clk_enabled = false;
}

unsigned long clk_get_rate(struct clk *clk)
{
	return 100000000; // 100MHz
}

struct resource *platform_get_resource(struct platform_device *pdev,
					unsigned int type, unsigned int num)
{
	static struct resource mock_res = {
		.start = 0x12345000,
		.end = 0x12345FFF,
		.flags = IORESOURCE_MEM,
	};
	
	return &mock_res;
}

int platform_get_irq(struct platform_device *pdev, unsigned int num)
{
	return 56; // Mock IRQ number
}

void platform_set_drvdata(struct platform_device *pdev, void *data)
{
	pdev->dev.driver_data = data;
}

int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value)
{
	if (strcmp(propname, "clock-frequency") == 0) {
		*out_value = 400000;
		return 0;
	} else if (strcmp(propname, "timeout-ms") == 0) {
		*out_value = 1000;
		return 0;
	}
	
	return -EINVAL;
}

void spin_lock_init(spinlock_t *lock)
{
	printf("[MOCK] Initializing spinlock\n");
}

void spin_lock_irqsave(spinlock_t *lock, unsigned long flags)
{
	printf("[MOCK] Acquiring spinlock with IRQ save\n");
}

void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	printf("[MOCK] Releasing spinlock with IRQ restore\n");
}

void init_completion(struct completion *x)
{
	printf("[MOCK] Initializing completion\n");
	mock_completion_done = false;
}

void reinit_completion(struct completion *x)
{
	printf("[MOCK] Reinitializing completion\n");
	mock_completion_done = false;
}

void complete(struct completion *x)
{
	printf("[MOCK] Completing completion\n");
	mock_completion_done = true;
}

unsigned long wait_for_completion_timeout(struct completion *x, unsigned long timeout)
{
	printf("[MOCK] Waiting for completion (timeout=%lu)\n", timeout);
	return mock_completion_done ? timeout : 0;
}

unsigned long msecs_to_jiffies(const unsigned int m)
{
	return m;
}

static void *mock_adapter_data = NULL;

void i2c_set_adapdata(struct i2c_adapter *dev, void *data)
{
	mock_adapter_data = data;
}

void *i2c_get_adapdata(const struct i2c_adapter *dev)
{
	return mock_adapter_data;
}

int i2c_add_numbered_adapter(struct i2c_adapter *adapter)
{
	printf("[MOCK] Adding I2C adapter '%s'\n", adapter->name);
	return 0;
}

void i2c_del_adapter(struct i2c_adapter *adap)
{
	printf("[MOCK] Deleting I2C adapter '%s'\n", adap->name);
}

int pm_runtime_get_sync(struct device *dev)
{
	printf("[MOCK] PM runtime get sync\n");
	return 0;
}

void pm_runtime_put_noidle(struct device *dev)
{
	printf("[MOCK] PM runtime put noidle\n");
}

void pm_runtime_put(struct device *dev)
{
	printf("[MOCK] PM runtime put\n");
}

void pm_runtime_mark_last_busy(struct device *dev)
{
	printf("[MOCK] PM runtime mark last busy\n");
}

void pm_runtime_put_autosuspend(struct device *dev)
{
	printf("[MOCK] PM runtime put autosuspend\n");
}

void pm_runtime_use_autosuspend(struct device *dev)
{
	printf("[MOCK] PM runtime use autosuspend\n");
}

void pm_runtime_set_autosuspend_delay(struct device *dev, int delay)
{
	printf("[MOCK] PM runtime set autosuspend delay: %d\n", delay);
}

void pm_runtime_set_active(struct device *dev)
{
	printf("[MOCK] PM runtime set active\n");
}

void pm_runtime_enable(struct device *dev)
{
	printf("[MOCK] PM runtime enable\n");
}

void pm_runtime_disable(struct device *dev)
{
	printf("[MOCK] PM runtime disable\n");
}

void pm_runtime_get_noresume(struct device *dev)
{
	printf("[MOCK] PM runtime get noresume\n");
}

bool pm_runtime_status_suspended(struct device *dev)
{
	return mock_pm_suspended;
}

void pm_request_autosuspend(struct device *dev)
{
	printf("[MOCK] PM request autosuspend\n");
}

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t len = strlen(src);
	if (size > 0) {
		size_t copy_len = (len < size - 1) ? len : size - 1;
		memcpy(dest, src, copy_len);
		dest[copy_len] = '\0';
	}
	return len;
}

struct dma_chan *dma_request_chan(struct device *dev, const char *name)
{
	printf("[MOCK] Requesting DMA channel '%s'\n", name);
	
	if (strcmp(name, "tx") == 0) {
		return &mock_tx_chan;
	} else if (strcmp(name, "rx") == 0) {
		return &mock_rx_chan;
	}
	
	return (struct dma_chan *)((long)-ENODEV);
}

void dma_release_channel(struct dma_chan *chan)
{
	printf("[MOCK] Releasing DMA channel\n");
}

int dmaengine_slave_config(struct dma_chan *chan, struct dma_slave_config *config)
{
	printf("[MOCK] Configuring DMA slave\n");
	return 0;
}

struct dma_async_tx_descriptor *dmaengine_prep_slave_single(struct dma_chan *chan,
	dma_addr_t buf, size_t len, enum dma_transfer_direction dir, unsigned long flags)
{
	printf("[MOCK] Preparing DMA descriptor (len=%zu, dir=%d)\n", len, dir);
	
	if (chan == &mock_tx_chan) {
		return &mock_tx_desc;
	} else if (chan == &mock_rx_chan) {
		return &mock_rx_desc;
	}
	
	return NULL;
}

dma_cookie_t dmaengine_submit(struct dma_async_tx_descriptor *desc)
{
	printf("[MOCK] Submitting DMA descriptor\n");
	return 1; // Valid cookie
}

void dma_async_issue_pending(struct dma_chan *chan)
{
	printf("[MOCK] Issuing pending DMA\n");
	
	// Simulate DMA completion by calling callback immediately
	if (chan == &mock_tx_chan && mock_tx_desc.callback) {
		mock_tx_desc.callback(mock_tx_desc.callback_param);
	} else if (chan == &mock_rx_chan && mock_rx_desc.callback) {
		mock_rx_desc.callback(mock_rx_desc.callback_param);
	}
}

bool dma_submit_error(dma_cookie_t cookie)
{
	return cookie < 0;
}

int dmaengine_terminate_all(struct dma_chan *chan)
{
	printf("[MOCK] Terminating all DMA operations\n");
	return 0;
}

void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, int flag)
{
	void *ptr = malloc(size);
	*dma_handle = (dma_addr_t)ptr;
	printf("[MOCK] Allocated %zu bytes DMA coherent buffer at %p (dma_addr=0x%x)\n", 
	       size, ptr, *dma_handle);
	
	if (mock_dma_tx_buffer == NULL) {
		mock_dma_tx_buffer = ptr;
	} else {
		mock_dma_rx_buffer = ptr;
	}
	
	return ptr;
}

void dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle)
{
	printf("[MOCK] Freeing %zu bytes DMA coherent buffer at %p (dma_addr=0x%x)\n", 
	       size, cpu_addr, dma_handle);
	free(cpu_addr);
}

void mock_set_pm_suspended(bool suspended)
{
	mock_pm_suspended = suspended;
}

void mock_set_completion_done(bool done)
{
	mock_completion_done = done;
}

void mock_reset_registers(void)
{
	memset(mock_registers, 0, sizeof(mock_registers));
}

// Mock DMA functions that are tested
int i2c_a78_dma_init(struct i2c_a78_dev *i2c_dev)
{
	printf("[MOCK] i2c_a78_dma_init called\n");
	if (i2c_dev) {
		i2c_dev->dma.use_dma = true;
		i2c_dev->dma.tx_chan = (struct dma_chan *)0x1234; // Mock pointer
		i2c_dev->dma.rx_chan = (struct dma_chan *)0x5678; // Mock pointer
		init_completion(&i2c_dev->dma.tx_complete);
		init_completion(&i2c_dev->dma.rx_complete);
	}
	return 0; // Success for testing
}

void i2c_a78_dma_release(struct i2c_a78_dev *i2c_dev)
{
	printf("[MOCK] i2c_a78_dma_release called\n");
	if (i2c_dev) {
		i2c_dev->dma.use_dma = false;
		i2c_dev->dma.tx_chan = NULL;
		i2c_dev->dma.rx_chan = NULL;
	}
}

int i2c_a78_dma_xfer(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg)
{
	(void)i2c_dev;
	(void)msg;
	printf("[MOCK] i2c_a78_dma_xfer called\n");
	return 0; // Success for testing
}

// Mock PM functions that are tested
int i2c_a78_pm_init(struct i2c_a78_dev *i2c_dev)
{
	(void)i2c_dev;
	printf("[MOCK] i2c_a78_pm_init called\n");
	return 0;
}

int i2c_a78_pm_suspend(struct device *dev)
{
	(void)dev;
	printf("[MOCK] i2c_a78_pm_suspend called\n");
	return 0;
}

int i2c_a78_pm_resume(struct device *dev)
{
	(void)dev;
	printf("[MOCK] i2c_a78_pm_resume called\n");
	return 0;
}