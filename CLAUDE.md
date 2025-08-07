# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an I2C driver project for embedded Linux platforms running on ARM Cortex-A78 CPU cores. The project is in its initial stages with comprehensive specifications defined but implementation not yet started.

## Architecture

The driver follows Linux kernel I2C subsystem standards and is designed as a platform driver with the following key components:

- **Core driver**: Main I2C controller implementation with register management
- **DMA support**: Separate DMA handling for large transfers (>32 bytes)  
- **Power management**: Runtime PM integration with DVFS coordination
- **Device tree integration**: Platform driver with OF (device tree) support

Target hardware supports I2C v2.1/SMBus v2.0 with speeds up to 1MHz+ and includes FIFO buffers, DMA channels, and ARM AMBA ACE-Lite coherency.

## Key Specifications

Refer to `I2C_DRIVER_SPECS.md` for complete technical requirements including:
- Register mappings and hardware interface
- Performance targets (95% bus utilization at 100kHz, <100μs transfer initiation)
- A78-specific optimizations (cache management, memory barriers)
- 6-week implementation roadmap

## Development Commands

The project includes a complete build system with comprehensive testing framework:

### Building the Driver
```bash
# Build kernel module
make driver

# Build everything (driver + tests)
make all

# Clean build artifacts
make clean
```

### Testing
```bash
# Run all tests
make tests

# Run specific test types
make -C tests unit
make -C tests integration

# Run with memory checking
make -C tests valgrind

# Generate coverage report
make -C tests coverage
```

### Installation and Loading
```bash
# Install module
sudo make install

# Load/unload module
sudo make load
sudo make unload

# Check kernel logs
dmesg | grep i2c
```

### Code Quality
```bash
# Check coding style
make check

# Generate development tags
make tags

# Setup development environment
make dev-setup
```

## Codebase Structure

- **src/driver/**: Core kernel module implementation
  - `i2c-a78-core.c`: Main platform driver with I2C framework integration
  - `i2c-a78-dma.c`: DMA engine support for large transfers
  - `i2c-a78-pm.c`: Runtime power management
  - `i2c-a78.h`: Register definitions and data structures

- **tests/**: Comprehensive testing framework
  - `mocks/`: Mock implementations of Linux kernel APIs
  - `unit/`: Unit tests for core functionality  
  - `integration/`: Integration tests for transfer scenarios

- **docs/**: Device tree bindings and specifications

## Implementation Notes

- Follows Linux kernel coding standards (checkpatch.pl compliance)
- Uses standard platform driver model with device tree bindings  
- Implements comprehensive error handling and recovery mechanisms
- Includes debugfs interface for runtime diagnostics
- Full runtime power management with autosuspend support
- DMA support for transfers ≥32 bytes, falls back to PIO for smaller transfers
- Supports I2C speeds: 100kHz, 400kHz, 1MHz, 3.4MHz