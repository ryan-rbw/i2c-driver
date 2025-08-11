# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for ARM Cortex-A78 I2C Platform Driver
#

obj-$(CONFIG_I2C_A78_PLATFORM) += i2c-a78-platform.o

i2c-a78-platform-y := i2c-a78-core.o
i2c-a78-platform-$(CONFIG_I2C_A78_DMA) += i2c-a78-dma.o
i2c-a78-platform-$(CONFIG_I2C_A78_PM_RUNTIME) += i2c-a78-pm.o

# Include path for driver headers
ccflags-y += -I$(src)/../include

# Enable debug symbols if requested
ifdef CONFIG_I2C_A78_DEBUG
ccflags-y += -DDEBUG
endif

# Compiler flags for ARM Cortex-A78 optimizations
ccflags-y += -mcpu=cortex-a78
ccflags-y += -O2
ccflags-y += -Wall -Wextra

# Enable specific warnings for better code quality
ccflags-y += -Wno-unused-parameter
ccflags-y += -Wno-sign-compare

# Module information
MODULE_NAME := i2c-a78-platform
MODULE_DESCRIPTION := ARM Cortex-A78 I2C Platform Driver
MODULE_AUTHOR := I2C Driver Development Team
MODULE_LICENSE := GPL v2
MODULE_VERSION := 1.0.0

# Help target
help:
	@echo "Available targets:"
	@echo "  all        - Build the I2C driver module"
	@echo "  clean      - Clean build artifacts" 
	@echo "  install    - Install the module"
	@echo "  uninstall  - Uninstall the module"
	@echo "  test       - Run unit tests"
	@echo ""
	@echo "Configuration options:"
	@echo "  CONFIG_I2C_A78_PLATFORM=m    - Build as module"
	@echo "  CONFIG_I2C_A78_DMA=y         - Enable DMA support"
	@echo "  CONFIG_I2C_A78_DEBUG=y       - Enable debug features"
	@echo "  CONFIG_I2C_A78_PM_RUNTIME=y  - Enable runtime PM"

.PHONY: help