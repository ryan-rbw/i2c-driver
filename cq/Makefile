# SPDX-License-Identifier: GPL-2.0-only
#
# Top-level Makefile for I2C A78 Driver Project
#

# Kernel build system integration
ifneq ($(KERNELRELEASE),)
# Called from kernel build system
include src/driver/Makefile
else

# Default kernel source directory
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Default targets
.PHONY: all clean driver tests install uninstall help check

all: driver

# Build the kernel module
driver:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/src/driver modules

# Clean kernel module build artifacts
clean-driver:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/src/driver clean

# Build and run tests
tests:
	$(MAKE) -C tests

# Clean test build artifacts  
clean-tests:
	$(MAKE) -C tests clean

# Install the kernel module
install: driver
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/src/driver modules_install
	depmod -a

# Uninstall the kernel module
uninstall:
	rm -f /lib/modules/$(shell uname -r)/extra/i2c-a78-platform.ko
	depmod -a

# Load the kernel module
load: install
	modprobe i2c-a78-platform

# Unload the kernel module
unload:
	rmmod i2c-a78-platform || true

# Clean everything
clean: clean-driver clean-tests
	find . -name "*.o" -delete
	find . -name "*.ko" -delete
	find . -name "*.mod" -delete
	find . -name "*.mod.c" -delete
	find . -name ".*.cmd" -delete
	find . -name "*.order" -delete
	find . -name "*.symvers" -delete
	find . -name "*~" -delete

# Check code style
check:
	@echo "Checking code style..."
	@find src/ -name "*.c" -o -name "*.h" | xargs scripts/checkpatch.pl --no-tree --terse || true

# Generate tags for development
tags:
	@echo "Generating ctags..."
	ctags -R src/ tests/ Documentation/ 2>/dev/null || echo "ctags not available"

# Show kernel module information
info: driver
	modinfo src/driver/i2c-a78-platform.ko

# Create development environment
dev-setup:
	@echo "Setting up development environment..."
	@echo "Installing development dependencies..."
	@sudo apt-get update || yum update || true
	@sudo apt-get install -y build-essential linux-headers-$(shell uname -r) || \
	 sudo yum install -y kernel-devel kernel-headers || \
	 echo "Please install kernel headers manually"
	@echo "Development environment setup complete!"

# Package for distribution
package: clean
	@echo "Creating distribution package..."
	tar czf i2c-a78-driver-$(shell date +%Y%m%d).tar.gz \
		src/ tests/ docs/ Makefile README.md I2C_DRIVER_SPECS.md

# Run full test suite
test-all: driver tests
	@echo "Running full test suite..."
	$(MAKE) -C tests test
	@echo "Loading module for hardware testing..."
	-sudo $(MAKE) load
	@echo "Module loaded. Check dmesg for initialization messages."
	@echo "Remember to unload with: sudo make unload"

# Documentation generation
docs:
	@echo "Generating documentation..."
	@mkdir -p docs/html
	@which doxygen > /dev/null && doxygen Doxyfile || echo "Doxygen not available"

# Help target
help:
	@echo "I2C ARM Cortex-A78 Driver Build System"
	@echo "======================================"
	@echo ""
	@echo "Main targets:"
	@echo "  all         - Build kernel module (default)"
	@echo "  driver      - Build kernel module only"  
	@echo "  tests       - Build and run tests"
	@echo "  clean       - Clean all build artifacts"
	@echo ""
	@echo "Installation targets:"
	@echo "  install     - Install kernel module"
	@echo "  uninstall   - Uninstall kernel module"
	@echo "  load        - Load kernel module"
	@echo "  unload      - Unload kernel module"
	@echo ""
	@echo "Development targets:"
	@echo "  check       - Run code style checks"
	@echo "  tags        - Generate ctags"
	@echo "  dev-setup   - Setup development environment"
	@echo "  test-all    - Run comprehensive test suite"
	@echo "  info        - Show module information"
	@echo ""
	@echo "Other targets:"
	@echo "  package     - Create distribution package"
	@echo "  docs        - Generate documentation"
	@echo "  help        - Show this help"
	@echo ""
	@echo "Environment variables:"
	@echo "  KERNEL_SRC  - Kernel source directory (default: /lib/modules/$(shell uname -r)/build)"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build driver"
	@echo "  make tests             # Run tests"
	@echo "  make KERNEL_SRC=/path/to/kernel driver  # Build with custom kernel"
	@echo "  sudo make install      # Install module"

endif