#ifndef __MOCK_LINUX_KERNEL_H__
#define __MOCK_LINUX_KERNEL_H__

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define GFP_KERNEL 0
#define GFP_ATOMIC 1
#define PAGE_SIZE 4096
#define BIT(nr) (1UL << (nr))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define IRQF_SHARED 0x00000080
#define IRQ_HANDLED 1

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;
typedef uint32_t dma_addr_t;
typedef int32_t dma_cookie_t;
typedef unsigned long ulong;

#define readl_relaxed(addr) mock_readl(addr)
#define writel_relaxed(val, addr) mock_writel(val, addr)

#define I2C_FUNC_I2C 0x00000001
#define I2C_FUNC_10BIT_ADDR 0x00000002
#define I2C_FUNC_SMBUS_EMUL 0x00000800

#define I2C_M_RD 0x0001
#define I2C_M_TEN 0x0010

#define I2C_CLASS_HWMON (1<<0)
#define I2C_CLASS_SPD (1<<7)

#define EPROBE_DEFER 517
#define ETIMEDOUT 110
#define EBUSY 16
#define EINVAL 22
#define ENOMEM 12
#define EIO 5

#define DMA_MEM_TO_DEV 1
#define DMA_DEV_TO_MEM 2
#define DMA_PREP_INTERRUPT (1 << 0)

#define DMA_SLAVE_BUSWIDTH_1_BYTE 1

struct device {
	char name[64];
	struct device_node *of_node;
	void *driver_data;
};

struct platform_device {
	struct device dev;
	int id;
	char name[32];
};

struct resource {
	ulong start;
	ulong end;
	ulong flags;
};

#define IORESOURCE_MEM 0x00000200

struct clk;
struct completion;
struct mutex;
struct spinlock;

struct i2c_msg {
	u16 addr;
	u16 flags;
	u16 len;
	u8 *buf;
};

struct i2c_adapter {
	struct module *owner;
	unsigned int class;
	const struct i2c_algorithm *algo;
	struct device dev;
	int nr;
	char name[48];
	struct device_node *dev_node;
};

struct i2c_algorithm {
	int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
	u32 (*functionality)(struct i2c_adapter *);
};

struct dma_chan;
struct dma_async_tx_descriptor;
struct dma_slave_config;

typedef void (*dma_async_tx_callback)(void *dma_async_param);

struct dma_async_tx_descriptor {
	dma_async_tx_callback callback;
	void *callback_param;
};

struct device_node;

#define THIS_MODULE ((struct module *)0)

struct module;

static inline void *dev_get_drvdata(const struct device *dev)
{
	return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
	dev->driver_data = data;
}

u32 mock_readl(volatile void __iomem *addr);
void mock_writel(u32 val, volatile void __iomem *addr);

void *devm_kzalloc(struct device *dev, size_t size, int flags);
void *devm_ioremap_resource(struct device *dev, struct resource *res);
struct clk *devm_clk_get(struct device *dev, const char *id);
int devm_request_irq(struct device *dev, unsigned int irq, void *handler,
		     unsigned long irqflags, const char *devname, void *dev_id);

int clk_prepare_enable(struct clk *clk);
void clk_disable_unprepare(struct clk *clk);
unsigned long clk_get_rate(struct clk *clk);

struct resource *platform_get_resource(struct platform_device *pdev,
					unsigned int type, unsigned int num);
int platform_get_irq(struct platform_device *pdev, unsigned int num);
void platform_set_drvdata(struct platform_device *pdev, void *data);

int of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value);

void spin_lock_init(spinlock_t *lock);
void spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

void init_completion(struct completion *x);
void reinit_completion(struct completion *x);
void complete(struct completion *x);
unsigned long wait_for_completion_timeout(struct completion *x, unsigned long timeout);

unsigned long msecs_to_jiffies(const unsigned int m);

void i2c_set_adapdata(struct i2c_adapter *dev, void *data);
void *i2c_get_adapdata(const struct i2c_adapter *dev);
int i2c_add_numbered_adapter(struct i2c_adapter *adapter);
void i2c_del_adapter(struct i2c_adapter *adap);

#define dev_info(dev, fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define dev_err(dev, fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define dev_name(dev) ((dev)->name)

int pm_runtime_get_sync(struct device *dev);
void pm_runtime_put_noidle(struct device *dev);
void pm_runtime_put(struct device *dev);
void pm_runtime_mark_last_busy(struct device *dev);
void pm_runtime_put_autosuspend(struct device *dev);
void pm_runtime_use_autosuspend(struct device *dev);
void pm_runtime_set_autosuspend_delay(struct device *dev, int delay);
void pm_runtime_set_active(struct device *dev);
void pm_runtime_enable(struct device *dev);
void pm_runtime_disable(struct device *dev);
void pm_runtime_get_noresume(struct device *dev);
bool pm_runtime_status_suspended(struct device *dev);
void pm_request_autosuspend(struct device *dev);

size_t strlcpy(char *dest, const char *src, size_t size);

struct dma_chan *dma_request_chan(struct device *dev, const char *name);
void dma_release_channel(struct dma_chan *chan);
int dmaengine_slave_config(struct dma_chan *chan, struct dma_slave_config *config);
struct dma_async_tx_descriptor *dmaengine_prep_slave_single(struct dma_chan *chan,
	dma_addr_t buf, size_t len, enum dma_transfer_direction dir, unsigned long flags);
dma_cookie_t dmaengine_submit(struct dma_async_tx_descriptor *desc);
void dma_async_issue_pending(struct dma_chan *chan);
bool dma_submit_error(dma_cookie_t cookie);
int dmaengine_terminate_all(struct dma_chan *chan);

void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, int flag);
void dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle);

#endif /* __MOCK_LINUX_KERNEL_H__ */