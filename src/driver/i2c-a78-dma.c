#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/of_dma.h>
#include <linux/slab.h>

#include "../include/i2c-a78.h"

static void i2c_a78_dma_tx_callback(void *data)
{
	struct i2c_a78_dev *i2c_dev = data;
	
	complete(&i2c_dev->dma.tx_complete);
}

static void i2c_a78_dma_rx_callback(void *data)
{
	struct i2c_a78_dev *i2c_dev = data;
	
	complete(&i2c_dev->dma.rx_complete);
}

static int i2c_a78_dma_config_tx(struct i2c_a78_dev *i2c_dev)
{
	struct dma_slave_config tx_conf = {};
	
	tx_conf.direction = DMA_MEM_TO_DEV;
	tx_conf.dst_addr = (dma_addr_t)(i2c_dev->base + I2C_A78_DATA);
	tx_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	tx_conf.dst_maxburst = 1;
	
	return dmaengine_slave_config(i2c_dev->dma.tx_chan, &tx_conf);
}

static int i2c_a78_dma_config_rx(struct i2c_a78_dev *i2c_dev)
{
	struct dma_slave_config rx_conf = {};
	
	rx_conf.direction = DMA_DEV_TO_MEM;
	rx_conf.src_addr = (dma_addr_t)(i2c_dev->base + I2C_A78_DATA);
	rx_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	rx_conf.src_maxburst = 1;
	
	return dmaengine_slave_config(i2c_dev->dma.rx_chan, &rx_conf);
}

int i2c_a78_dma_init(struct i2c_a78_dev *i2c_dev)
{
	struct device *dev = i2c_dev->dev;
	int ret;
	
	i2c_dev->dma.tx_chan = dma_request_chan(dev, "tx");
	if (IS_ERR(i2c_dev->dma.tx_chan)) {
		ret = PTR_ERR(i2c_dev->dma.tx_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request TX DMA channel: %d\n", ret);
		return ret;
	}
	
	i2c_dev->dma.rx_chan = dma_request_chan(dev, "rx");
	if (IS_ERR(i2c_dev->dma.rx_chan)) {
		ret = PTR_ERR(i2c_dev->dma.rx_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request RX DMA channel: %d\n", ret);
		goto err_tx_chan;
	}
	
	ret = i2c_a78_dma_config_tx(i2c_dev);
	if (ret) {
		dev_err(dev, "Failed to configure TX DMA: %d\n", ret);
		goto err_rx_chan;
	}
	
	ret = i2c_a78_dma_config_rx(i2c_dev);
	if (ret) {
		dev_err(dev, "Failed to configure RX DMA: %d\n", ret);
		goto err_rx_chan;
	}
	
	i2c_dev->dma.buf_len = PAGE_SIZE;
	
	i2c_dev->dma.tx_buf = dma_alloc_coherent(dev, i2c_dev->dma.buf_len,
						 &i2c_dev->dma.tx_dma_buf, GFP_KERNEL);
	if (!i2c_dev->dma.tx_buf) {
		ret = -ENOMEM;
		goto err_rx_chan;
	}
	
	i2c_dev->dma.rx_buf = dma_alloc_coherent(dev, i2c_dev->dma.buf_len,
						 &i2c_dev->dma.rx_dma_buf, GFP_KERNEL);
	if (!i2c_dev->dma.rx_buf) {
		ret = -ENOMEM;
		goto err_tx_buf;
	}
	
	init_completion(&i2c_dev->dma.tx_complete);
	init_completion(&i2c_dev->dma.rx_complete);
	
	i2c_dev->dma.use_dma = true;
	
	dev_info(dev, "DMA initialized successfully\n");
	return 0;
	
err_tx_buf:
	dma_free_coherent(dev, i2c_dev->dma.buf_len, i2c_dev->dma.tx_buf,
			  i2c_dev->dma.tx_dma_buf);
err_rx_chan:
	dma_release_channel(i2c_dev->dma.rx_chan);
err_tx_chan:
	dma_release_channel(i2c_dev->dma.tx_chan);
	
	return ret;
}

void i2c_a78_dma_release(struct i2c_a78_dev *i2c_dev)
{
	struct device *dev = i2c_dev->dev;
	
	if (!i2c_dev->dma.use_dma)
		return;
	
	if (i2c_dev->dma.tx_buf) {
		dma_free_coherent(dev, i2c_dev->dma.buf_len, i2c_dev->dma.tx_buf,
				  i2c_dev->dma.tx_dma_buf);
	}
	
	if (i2c_dev->dma.rx_buf) {
		dma_free_coherent(dev, i2c_dev->dma.buf_len, i2c_dev->dma.rx_buf,
				  i2c_dev->dma.rx_dma_buf);
	}
	
	if (!IS_ERR_OR_NULL(i2c_dev->dma.tx_chan)) {
		dmaengine_terminate_all(i2c_dev->dma.tx_chan);
		dma_release_channel(i2c_dev->dma.tx_chan);
	}
	
	if (!IS_ERR_OR_NULL(i2c_dev->dma.rx_chan)) {
		dmaengine_terminate_all(i2c_dev->dma.rx_chan);
		dma_release_channel(i2c_dev->dma.rx_chan);
	}
	
	i2c_dev->dma.use_dma = false;
}

static int i2c_a78_dma_submit_tx(struct i2c_a78_dev *i2c_dev, const u8 *buf, size_t len)
{
	struct dma_async_tx_descriptor *tx_desc;
	dma_cookie_t cookie;
	
	if (len > i2c_dev->dma.buf_len) {
		dev_err(i2c_dev->dev, "TX buffer too large: %zu > %zu\n",
			len, i2c_dev->dma.buf_len);
		return -EINVAL;
	}
	
	memcpy(i2c_dev->dma.tx_buf, buf, len);
	
	reinit_completion(&i2c_dev->dma.tx_complete);
	
	tx_desc = dmaengine_prep_slave_single(i2c_dev->dma.tx_chan,
					      i2c_dev->dma.tx_dma_buf, len,
					      DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT);
	if (!tx_desc) {
		dev_err(i2c_dev->dev, "Failed to prepare TX DMA descriptor\n");
		return -ENOMEM;
	}
	
	tx_desc->callback = i2c_a78_dma_tx_callback;
	tx_desc->callback_param = i2c_dev;
	
	cookie = dmaengine_submit(tx_desc);
	if (dma_submit_error(cookie)) {
		dev_err(i2c_dev->dev, "Failed to submit TX DMA\n");
		return -EIO;
	}
	
	dma_async_issue_pending(i2c_dev->dma.tx_chan);
	
	return 0;
}

static int i2c_a78_dma_submit_rx(struct i2c_a78_dev *i2c_dev, size_t len)
{
	struct dma_async_tx_descriptor *rx_desc;
	dma_cookie_t cookie;
	
	if (len > i2c_dev->dma.buf_len) {
		dev_err(i2c_dev->dev, "RX buffer too large: %zu > %zu\n",
			len, i2c_dev->dma.buf_len);
		return -EINVAL;
	}
	
	reinit_completion(&i2c_dev->dma.rx_complete);
	
	rx_desc = dmaengine_prep_slave_single(i2c_dev->dma.rx_chan,
					      i2c_dev->dma.rx_dma_buf, len,
					      DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT);
	if (!rx_desc) {
		dev_err(i2c_dev->dev, "Failed to prepare RX DMA descriptor\n");
		return -ENOMEM;
	}
	
	rx_desc->callback = i2c_a78_dma_rx_callback;
	rx_desc->callback_param = i2c_dev;
	
	cookie = dmaengine_submit(rx_desc);
	if (dma_submit_error(cookie)) {
		dev_err(i2c_dev->dev, "Failed to submit RX DMA\n");
		return -EIO;
	}
	
	dma_async_issue_pending(i2c_dev->dma.rx_chan);
	
	return 0;
}

int i2c_a78_dma_xfer(struct i2c_a78_dev *i2c_dev, struct i2c_msg *msg)
{
	unsigned long timeout;
	int ret;
	
	if (!i2c_dev->dma.use_dma || msg->len < I2C_A78_DMA_THRESHOLD)
		return -EINVAL;
	
	if (msg->flags & I2C_M_RD) {
		ret = i2c_a78_dma_submit_rx(i2c_dev, msg->len);
		if (ret)
			return ret;
		
		timeout = wait_for_completion_timeout(&i2c_dev->dma.rx_complete,
						      msecs_to_jiffies(i2c_dev->timeout_ms));
		if (!timeout) {
			dev_err(i2c_dev->dev, "RX DMA timeout\n");
			dmaengine_terminate_all(i2c_dev->dma.rx_chan);
			return -ETIMEDOUT;
		}
		
		memcpy(msg->buf, i2c_dev->dma.rx_buf, msg->len);
		i2c_dev->stats.rx_bytes += msg->len;
	} else {
		ret = i2c_a78_dma_submit_tx(i2c_dev, msg->buf, msg->len);
		if (ret)
			return ret;
		
		timeout = wait_for_completion_timeout(&i2c_dev->dma.tx_complete,
						      msecs_to_jiffies(i2c_dev->timeout_ms));
		if (!timeout) {
			dev_err(i2c_dev->dev, "TX DMA timeout\n");
			dmaengine_terminate_all(i2c_dev->dma.tx_chan);
			return -ETIMEDOUT;
		}
		
		i2c_dev->stats.tx_bytes += msg->len;
	}
	
	return 0;
}