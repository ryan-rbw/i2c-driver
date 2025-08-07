#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>

#include "../include/i2c-a78.h"

static void i2c_a78_hw_init(struct i2c_a78_dev *i2c_dev)
{
	u32 prescaler, control;
	unsigned long clk_rate;
	
	clk_rate = clk_get_rate(i2c_dev->clk);
	prescaler = clk_rate / (i2c_dev->bus_freq * 5) - 1;
	
	i2c_a78_writel(i2c_dev, prescaler, I2C_A78_PRESCALER);
	
	control = I2C_A78_CONTROL_MASTER_EN | I2C_A78_CONTROL_INT_EN;
	
	switch (i2c_dev->bus_freq) {
	case I2C_A78_SPEED_STD:
		control |= I2C_A78_CONTROL_SPEED_STD;
		break;
	case I2C_A78_SPEED_FAST:
		control |= I2C_A78_CONTROL_SPEED_FAST;
		break;
	case I2C_A78_SPEED_FAST_PLUS:
		control |= I2C_A78_CONTROL_SPEED_FAST_PLUS;
		break;
	case I2C_A78_SPEED_HIGH:
		control |= I2C_A78_CONTROL_SPEED_HIGH;
		break;
	default:
		control |= I2C_A78_CONTROL_SPEED_FAST;
		break;
	}
	
	if (i2c_dev->dma.use_dma) {
		control |= I2C_A78_CONTROL_DMA_TX_EN | I2C_A78_CONTROL_DMA_RX_EN;
	}
	
	i2c_a78_writel(i2c_dev, control, I2C_A78_CONTROL);
	
	i2c_a78_writel(i2c_dev, I2C_A78_CONTROL_FIFO_TX_CLR | I2C_A78_CONTROL_FIFO_RX_CLR,
		       I2C_A78_CONTROL);
	
	i2c_a78_writel(i2c_dev, 0xFF, I2C_A78_INTERRUPT);
}

static int i2c_a78_wait_for_completion(struct i2c_a78_dev *i2c_dev)
{
	unsigned long timeout;
	
	timeout = wait_for_completion_timeout(&i2c_dev->msg_complete,
					      msecs_to_jiffies(i2c_dev->timeout_ms));
	if (!timeout) {
		dev_err(i2c_dev->dev, "Transfer timeout\n");
		i2c_dev->stats.timeouts++;
		return -ETIMEDOUT;
	}
	
	return 0;
}

static int i2c_a78_send_address(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg)
{
	u32 addr = msg->addr;
	u32 command = I2C_A78_COMMAND_START;
	
	if (msg->flags & I2C_M_TEN) {
		addr |= I2C_A78_ADDRESS_10BIT_EN;
	}
	
	if (msg->flags & I2C_M_RD) {
		addr |= 1;
		command |= I2C_A78_COMMAND_READ;
	} else {
		command |= I2C_A78_COMMAND_WRITE;
	}
	
	i2c_a78_writel(i2c_dev, addr, I2C_A78_ADDRESS);
	i2c_a78_writel(i2c_dev, command, I2C_A78_COMMAND);
	
	return 0;
}

static int i2c_a78_pio_write(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg)
{
	int i;
	
	for (i = 0; i < msg->len; i++) {
		i2c_a78_writel(i2c_dev, msg->buf[i], I2C_A78_DATA);
		
		if (i < msg->len - 1) {
			i2c_a78_writel(i2c_dev, I2C_A78_COMMAND_WRITE, I2C_A78_COMMAND);
		}
	}
	
	i2c_dev->stats.tx_bytes += msg->len;
	return 0;
}

static int i2c_a78_pio_read(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg)
{
	int i;
	u32 command;
	
	for (i = 0; i < msg->len; i++) {
		command = I2C_A78_COMMAND_READ;
		
		if (i == msg->len - 1) {
			command |= I2C_A78_COMMAND_NACK;
		} else {
			command |= I2C_A78_COMMAND_ACK;
		}
		
		i2c_a78_writel(i2c_dev, command, I2C_A78_COMMAND);
		
		msg->buf[i] = i2c_a78_readl(i2c_dev, I2C_A78_DATA) & 0xFF;
	}
	
	i2c_dev->stats.rx_bytes += msg->len;
	return 0;
}

static int i2c_a78_xfer_msg(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg)
{
	int ret;
	
	reinit_completion(&i2c_dev->msg_complete);
	
	ret = i2c_a78_send_address(i2c_dev, msg);
	if (ret)
		return ret;
	
	if (i2c_dev->dma.use_dma && msg->len >= I2C_A78_DMA_THRESHOLD) {
		ret = i2c_a78_dma_xfer(i2c_dev, msg);
	} else {
		if (msg->flags & I2C_M_RD) {
			ret = i2c_a78_pio_read(i2c_dev, msg);
		} else {
			ret = i2c_a78_pio_write(i2c_dev, msg);
		}
	}
	
	if (ret)
		return ret;
	
	return i2c_a78_wait_for_completion(i2c_dev);
}

static int i2c_a78_master_xfer(struct i2c_adapter *adapter,
			       struct i2c_msg msgs[], int num)
{
	struct i2c_a78_dev *i2c_dev = i2c_get_adapdata(adapter);
	unsigned long flags;
	int ret, i;
	
	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(i2c_dev->dev);
		return ret;
	}
	
	spin_lock_irqsave(&i2c_dev->lock, flags);
	
	if (i2c_dev->suspended) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		pm_runtime_put(i2c_dev->dev);
		return -EBUSY;
	}
	
	i2c_dev->msgs = msgs;
	i2c_dev->num_msgs = num;
	i2c_dev->msg_idx = 0;
	i2c_dev->state = I2C_A78_STATE_START;
	
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	
	for (i = 0; i < num; i++) {
		ret = i2c_a78_xfer_msg(i2c_dev, &msgs[i]);
		if (ret)
			break;
		
		i2c_dev->msg_idx++;
	}
	
	if (i2c_dev->num_msgs > 0) {
		i2c_a78_writel(i2c_dev, I2C_A78_COMMAND_STOP, I2C_A78_COMMAND);
	}
	
	spin_lock_irqsave(&i2c_dev->lock, flags);
	i2c_dev->state = I2C_A78_STATE_IDLE;
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	
	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);
	
	return ret ? ret : num;
}

static u32 i2c_a78_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR;
}

static const struct i2c_algorithm i2c_a78_algo = {
	.master_xfer = i2c_a78_master_xfer,
	.functionality = i2c_a78_func,
};

static irqreturn_t i2c_a78_isr(int irq, void *dev_id)
{
	struct i2c_a78_dev *i2c_dev = dev_id;
	u32 status, int_status;
	
	status = i2c_a78_readl(i2c_dev, I2C_A78_STATUS);
	int_status = i2c_a78_readl(i2c_dev, I2C_A78_INTERRUPT);
	
	if (int_status & I2C_A78_INT_ARB_LOST) {
		dev_err(i2c_dev->dev, "Arbitration lost\n");
		i2c_dev->stats.arb_lost++;
		i2c_dev->state = I2C_A78_STATE_ERROR;
		complete(&i2c_dev->msg_complete);
	}
	
	if (int_status & I2C_A78_INT_NACK) {
		dev_dbg(i2c_dev->dev, "NACK received\n");
		i2c_dev->stats.nacks++;
		i2c_dev->state = I2C_A78_STATE_ERROR;
		complete(&i2c_dev->msg_complete);
	}
	
	if (int_status & I2C_A78_INT_TIMEOUT) {
		dev_err(i2c_dev->dev, "Transfer timeout in ISR\n");
		i2c_dev->stats.timeouts++;
		i2c_dev->state = I2C_A78_STATE_ERROR;
		complete(&i2c_dev->msg_complete);
	}
	
	if (int_status & (I2C_A78_INT_TX_DONE | I2C_A78_INT_RX_READY)) {
		if (i2c_dev->state != I2C_A78_STATE_ERROR) {
			i2c_dev->state = I2C_A78_STATE_IDLE;
			complete(&i2c_dev->msg_complete);
		}
	}
	
	i2c_a78_writel(i2c_dev, int_status, I2C_A78_INTERRUPT);
	
	return IRQ_HANDLED;
}

static int i2c_a78_debugfs_show(struct seq_file *s, void *data)
{
	struct i2c_a78_dev *i2c_dev = s->private;
	
	seq_printf(s, "I2C A78 Debug Information\n");
	seq_printf(s, "=========================\n");
	seq_printf(s, "Bus frequency: %u Hz\n", i2c_dev->bus_freq);
	seq_printf(s, "DMA enabled: %s\n", i2c_dev->dma.use_dma ? "Yes" : "No");
	seq_printf(s, "State: %d\n", i2c_dev->state);
	seq_printf(s, "\nStatistics:\n");
	seq_printf(s, "TX bytes: %llu\n", i2c_dev->stats.tx_bytes);
	seq_printf(s, "RX bytes: %llu\n", i2c_dev->stats.rx_bytes);
	seq_printf(s, "Timeouts: %u\n", i2c_dev->stats.timeouts);
	seq_printf(s, "Arbitration lost: %u\n", i2c_dev->stats.arb_lost);
	seq_printf(s, "NACKs: %u\n", i2c_dev->stats.nacks);
	seq_printf(s, "\nRegisters:\n");
	seq_printf(s, "CONTROL: 0x%08x\n", i2c_a78_readl(i2c_dev, I2C_A78_CONTROL));
	seq_printf(s, "STATUS: 0x%08x\n", i2c_a78_readl(i2c_dev, I2C_A78_STATUS));
	seq_printf(s, "PRESCALER: 0x%08x\n", i2c_a78_readl(i2c_dev, I2C_A78_PRESCALER));
	
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(i2c_a78_debugfs);

static void i2c_a78_debugfs_init(struct i2c_a78_dev *i2c_dev)
{
	struct dentry *root;
	
	root = debugfs_create_dir(dev_name(i2c_dev->dev), NULL);
	if (IS_ERR_OR_NULL(root))
		return;
	
	debugfs_create_file("status", 0444, root, i2c_dev, &i2c_a78_debugfs_fops);
}

static int i2c_a78_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct i2c_a78_dev *i2c_dev;
	struct resource *res;
	int ret;
	
	i2c_dev = devm_kzalloc(dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev)
		return -ENOMEM;
	
	i2c_dev->dev = dev;
	platform_set_drvdata(pdev, i2c_dev);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i2c_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(i2c_dev->base))
		return PTR_ERR(i2c_dev->base);
	
	i2c_dev->clk = devm_clk_get(dev, "i2c");
	if (IS_ERR(i2c_dev->clk)) {
		dev_err(dev, "Failed to get clock\n");
		return PTR_ERR(i2c_dev->clk);
	}
	
	i2c_dev->irq = platform_get_irq(pdev, 0);
	if (i2c_dev->irq < 0)
		return i2c_dev->irq;
	
	ret = devm_request_irq(dev, i2c_dev->irq, i2c_a78_isr,
			       IRQF_SHARED, dev_name(dev), i2c_dev);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d: %d\n", i2c_dev->irq, ret);
		return ret;
	}
	
	of_property_read_u32(dev->of_node, "clock-frequency", &i2c_dev->bus_freq);
	if (!i2c_dev->bus_freq)
		i2c_dev->bus_freq = I2C_A78_SPEED_FAST;
	
	of_property_read_u32(dev->of_node, "timeout-ms", &i2c_dev->timeout_ms);
	if (!i2c_dev->timeout_ms)
		i2c_dev->timeout_ms = I2C_A78_TIMEOUT_MS;
	
	spin_lock_init(&i2c_dev->lock);
	init_completion(&i2c_dev->msg_complete);
	
	ret = clk_prepare_enable(i2c_dev->clk);
	if (ret) {
		dev_err(dev, "Failed to enable clock\n");
		return ret;
	}
	
	ret = i2c_a78_dma_init(i2c_dev);
	if (ret && ret != -EPROBE_DEFER) {
		dev_info(dev, "DMA not available, using PIO mode\n");
		i2c_dev->dma.use_dma = false;
		ret = 0;
	}
	
	i2c_a78_hw_init(i2c_dev);
	
	i2c_dev->adapter.owner = THIS_MODULE;
	i2c_dev->adapter.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	i2c_dev->adapter.algo = &i2c_a78_algo;
	i2c_dev->adapter.dev.parent = dev;
	i2c_dev->adapter.dev.of_node = dev->of_node;
	i2c_dev->adapter.nr = pdev->id;
	strlcpy(i2c_dev->adapter.name, I2C_A78_DRIVER_NAME, sizeof(i2c_dev->adapter.name));
	
	i2c_set_adapdata(&i2c_dev->adapter, i2c_dev);
	
	ret = i2c_add_numbered_adapter(&i2c_dev->adapter);
	if (ret) {
		dev_err(dev, "Failed to add I2C adapter: %d\n", ret);
		goto err_dma;
	}
	
	ret = i2c_a78_pm_init(i2c_dev);
	if (ret)
		goto err_adapter;
	
	i2c_a78_debugfs_init(i2c_dev);
	
	dev_info(dev, "I2C adapter registered (bus_freq=%u Hz)\n", i2c_dev->bus_freq);
	
	return 0;
	
err_adapter:
	i2c_del_adapter(&i2c_dev->adapter);
err_dma:
	i2c_a78_dma_release(i2c_dev);
	clk_disable_unprepare(i2c_dev->clk);
	return ret;
}

static int i2c_a78_remove(struct platform_device *pdev)
{
	struct i2c_a78_dev *i2c_dev = platform_get_drvdata(pdev);
	
	pm_runtime_disable(i2c_dev->dev);
	i2c_del_adapter(&i2c_dev->adapter);
	i2c_a78_dma_release(i2c_dev);
	clk_disable_unprepare(i2c_dev->clk);
	
	return 0;
}

static const struct of_device_id i2c_a78_dt_ids[] = {
	{ .compatible = "arm,a78-i2c", },
	{ }
};
MODULE_DEVICE_TABLE(of, i2c_a78_dt_ids);

static const struct dev_pm_ops i2c_a78_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(i2c_a78_pm_suspend, i2c_a78_pm_resume)
	SET_RUNTIME_PM_OPS(i2c_a78_pm_suspend, i2c_a78_pm_resume, NULL)
};

static struct platform_driver i2c_a78_driver = {
	.probe = i2c_a78_probe,
	.remove = i2c_a78_remove,
	.driver = {
		.name = I2C_A78_DRIVER_NAME,
		.of_match_table = i2c_a78_dt_ids,
		.pm = &i2c_a78_pm_ops,
	},
};

module_platform_driver(i2c_a78_driver);

MODULE_DESCRIPTION("ARM Cortex-A78 I2C Platform Driver");
MODULE_AUTHOR("I2C Driver Development Team");
MODULE_LICENSE("GPL v2");