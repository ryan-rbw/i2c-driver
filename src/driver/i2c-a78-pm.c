#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include "../include/i2c-a78.h"

static void i2c_a78_save_context(struct i2c_a78_dev *i2c_dev)
{
	i2c_dev->saved_control = i2c_a78_readl(i2c_dev, I2C_A78_CONTROL);
	i2c_dev->saved_prescaler = i2c_a78_readl(i2c_dev, I2C_A78_PRESCALER);
	
	dev_dbg(i2c_dev->dev, "Context saved: control=0x%08x, prescaler=0x%08x\n",
		i2c_dev->saved_control, i2c_dev->saved_prescaler);
}

static void i2c_a78_restore_context(struct i2c_a78_dev *i2c_dev)
{
	i2c_a78_writel(i2c_dev, i2c_dev->saved_prescaler, I2C_A78_PRESCALER);
	i2c_a78_writel(i2c_dev, i2c_dev->saved_control, I2C_A78_CONTROL);
	
	i2c_a78_writel(i2c_dev, I2C_A78_CONTROL_FIFO_TX_CLR | I2C_A78_CONTROL_FIFO_RX_CLR,
		       I2C_A78_CONTROL);
	
	i2c_a78_writel(i2c_dev, 0xFF, I2C_A78_INTERRUPT);
	
	dev_dbg(i2c_dev->dev, "Context restored: control=0x%08x, prescaler=0x%08x\n",
		i2c_dev->saved_control, i2c_dev->saved_prescaler);
}

static int i2c_a78_runtime_suspend(struct device *dev)
{
	struct i2c_a78_dev *i2c_dev = dev_get_drvdata(dev);
	unsigned long flags;
	
	spin_lock_irqsave(&i2c_dev->lock, flags);
	
	if (i2c_dev->state != I2C_A78_STATE_IDLE) {
		spin_unlock_irqrestore(&i2c_dev->lock, flags);
		dev_dbg(dev, "Cannot suspend, transfer in progress\n");
		return -EBUSY;
	}
	
	i2c_dev->suspended = true;
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	
	i2c_a78_save_context(i2c_dev);
	
	clk_disable_unprepare(i2c_dev->clk);
	
	dev_dbg(dev, "Runtime suspend completed\n");
	return 0;
}

static int i2c_a78_runtime_resume(struct device *dev)
{
	struct i2c_a78_dev *i2c_dev = dev_get_drvdata(dev);
	unsigned long flags;
	int ret;
	
	ret = clk_prepare_enable(i2c_dev->clk);
	if (ret) {
		dev_err(dev, "Failed to enable clock during resume: %d\n", ret);
		return ret;
	}
	
	udelay(10);
	
	i2c_a78_restore_context(i2c_dev);
	
	spin_lock_irqsave(&i2c_dev->lock, flags);
	i2c_dev->suspended = false;
	spin_unlock_irqrestore(&i2c_dev->lock, flags);
	
	dev_dbg(dev, "Runtime resume completed\n");
	return 0;
}

int i2c_a78_pm_suspend(struct device *dev)
{
	struct i2c_a78_dev *i2c_dev = dev_get_drvdata(dev);
	int ret;
	
	if (!pm_runtime_status_suspended(dev)) {
		ret = i2c_a78_runtime_suspend(dev);
		if (ret)
			return ret;
	}
	
	dev_dbg(dev, "System suspend completed\n");
	return 0;
}

int i2c_a78_pm_resume(struct device *dev)
{
	struct i2c_a78_dev *i2c_dev = dev_get_drvdata(dev);
	int ret;
	
	ret = i2c_a78_runtime_resume(dev);
	if (ret)
		return ret;
	
	if (!pm_runtime_status_suspended(dev)) {
		pm_runtime_mark_last_busy(dev);
		pm_request_autosuspend(dev);
	}
	
	dev_dbg(dev, "System resume completed\n");
	return 0;
}

int i2c_a78_pm_init(struct i2c_a78_dev *i2c_dev)
{
	struct device *dev = i2c_dev->dev;
	
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, I2C_A78_PM_SUSPEND_DELAY_MS);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	
	pm_runtime_get_noresume(dev);
	
	dev_info(dev, "Power management initialized (autosuspend=%dms)\n",
		 I2C_A78_PM_SUSPEND_DELAY_MS);
	
	return 0;
}