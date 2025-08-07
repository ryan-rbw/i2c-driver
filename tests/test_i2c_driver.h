#ifndef __TEST_I2C_DRIVER_H__
#define __TEST_I2C_DRIVER_H__

#include "mocks/mock-linux-kernel.h"

#define I2C_A78_DRIVER_NAME	"i2c-a78-platform"

#define I2C_A78_CONTROL		0x00
#define I2C_A78_STATUS		0x04
#define I2C_A78_DATA		0x08
#define I2C_A78_ADDRESS		0x0C
#define I2C_A78_COMMAND		0x10
#define I2C_A78_FIFO_STATUS	0x14
#define I2C_A78_INTERRUPT	0x18
#define I2C_A78_PRESCALER	0x1C

#define I2C_A78_CONTROL_MASTER_EN	BIT(0)
#define I2C_A78_CONTROL_SPEED_STD	(0 << 1)
#define I2C_A78_CONTROL_SPEED_FAST	(1 << 1)
#define I2C_A78_CONTROL_SPEED_FAST_PLUS	(2 << 1)
#define I2C_A78_CONTROL_SPEED_HIGH	(3 << 1)
#define I2C_A78_CONTROL_SPEED_MASK	(3 << 1)
#define I2C_A78_CONTROL_INT_EN		BIT(3)
#define I2C_A78_CONTROL_DMA_TX_EN	BIT(4)
#define I2C_A78_CONTROL_DMA_RX_EN	BIT(5)
#define I2C_A78_CONTROL_FIFO_TX_CLR	BIT(6)
#define I2C_A78_CONTROL_FIFO_RX_CLR	BIT(7)

#define I2C_A78_STATUS_BUSY		BIT(0)
#define I2C_A78_STATUS_ARB_LOST		BIT(1)
#define I2C_A78_STATUS_NACK		BIT(2)
#define I2C_A78_STATUS_TX_DONE		BIT(3)
#define I2C_A78_STATUS_RX_READY		BIT(4)
#define I2C_A78_STATUS_FIFO_TX_FULL	BIT(5)
#define I2C_A78_STATUS_FIFO_RX_EMPTY	BIT(6)
#define I2C_A78_STATUS_TIMEOUT		BIT(7)

#define I2C_A78_ADDRESS_7BIT_MASK	0x7F
#define I2C_A78_ADDRESS_10BIT_MASK	0x3FF
#define I2C_A78_ADDRESS_10BIT_EN	BIT(15)

#define I2C_A78_COMMAND_START		BIT(0)
#define I2C_A78_COMMAND_STOP		BIT(1)
#define I2C_A78_COMMAND_READ		BIT(2)
#define I2C_A78_COMMAND_WRITE		BIT(3)
#define I2C_A78_COMMAND_ACK		BIT(4)
#define I2C_A78_COMMAND_NACK		BIT(5)

#define I2C_A78_FIFO_STATUS_TX_LEVEL_MASK	0x1F
#define I2C_A78_FIFO_STATUS_RX_LEVEL_MASK	(0x1F << 8)
#define I2C_A78_FIFO_STATUS_RX_LEVEL_SHIFT	8

#define I2C_A78_INT_TX_DONE		BIT(0)
#define I2C_A78_INT_RX_READY		BIT(1)
#define I2C_A78_INT_ARB_LOST		BIT(2)
#define I2C_A78_INT_NACK		BIT(3)
#define I2C_A78_INT_TIMEOUT		BIT(4)
#define I2C_A78_INT_FIFO_TX_EMPTY	BIT(5)
#define I2C_A78_INT_FIFO_RX_FULL	BIT(6)

#define I2C_A78_FIFO_SIZE		16
#define I2C_A78_DMA_THRESHOLD		32
#define I2C_A78_TIMEOUT_MS		1000
#define I2C_A78_PM_SUSPEND_DELAY_MS	100

enum i2c_a78_speed {
	I2C_A78_SPEED_STD = 100000,
	I2C_A78_SPEED_FAST = 400000,
	I2C_A78_SPEED_FAST_PLUS = 1000000,
	I2C_A78_SPEED_HIGH = 3400000,
};

enum i2c_a78_state {
	I2C_A78_STATE_IDLE,
	I2C_A78_STATE_START,
	I2C_A78_STATE_ADDR,
	I2C_A78_STATE_DATA,
	I2C_A78_STATE_STOP,
	I2C_A78_STATE_ERROR,
};

struct i2c_a78_dma_data {
	struct dma_chan *tx_chan;
	struct dma_chan *rx_chan;
	dma_addr_t tx_dma_buf;
	dma_addr_t rx_dma_buf;
	void *tx_buf;
	void *rx_buf;
	size_t buf_len;
	struct completion tx_complete;
	struct completion rx_complete;
	bool use_dma;
};

struct i2c_a78_dev {
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	int irq;
	
	struct i2c_adapter adapter;
	struct i2c_msg *msgs;
	int num_msgs;
	int msg_idx;
	
	enum i2c_a78_state state;
	u32 bus_freq;
	u32 timeout_ms;
	
	spinlock_t lock;
	struct completion msg_complete;
	
	struct i2c_a78_dma_data dma;
	
	bool suspended;
	u32 saved_control;
	u32 saved_prescaler;
	
	struct {
		u64 tx_bytes;
		u64 rx_bytes;
		u32 timeouts;
		u32 arb_lost;
		u32 nacks;
	} stats;
};

static inline u32 i2c_a78_readl(struct i2c_a78_dev *i2c_dev, u32 offset)
{
	return mock_readl(i2c_dev->base + offset);
}

static inline void i2c_a78_writel(struct i2c_a78_dev *i2c_dev, u32 value, u32 offset)
{
	mock_writel(value, i2c_dev->base + offset);
}

int i2c_a78_dma_init(struct i2c_a78_dev *i2c_dev);
void i2c_a78_dma_release(struct i2c_a78_dev *i2c_dev);
int i2c_a78_dma_xfer(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg);

int i2c_a78_pm_init(struct i2c_a78_dev *i2c_dev);
int i2c_a78_pm_suspend(struct device *dev);
int i2c_a78_pm_resume(struct device *dev);

#endif /* __TEST_I2C_DRIVER_H__ */