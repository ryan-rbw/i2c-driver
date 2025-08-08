# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for I2C A78 Driver Tests
#

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O0
INCLUDES = -I. -Imocks
LIBS = 

# Test directories
UNIT_DIR = unit
INTEGRATION_DIR = integration
FAILURE_DIR = failure_scenarios
STRESS_DIR = stress
PERFORMANCE_DIR = performance
PROTOCOL_DIR = protocol
MOCKS_DIR = mocks

# Source files
MOCK_SOURCES = $(MOCKS_DIR)/mock-implementations.c
UNIT_SOURCES = $(UNIT_DIR)/test_i2c_core.c
INTEGRATION_SOURCES = $(INTEGRATION_DIR)/test_i2c_transfer.c
FAILURE_SOURCES = $(FAILURE_DIR)/test_error_conditions.c
STRESS_SOURCES = $(STRESS_DIR)/test_stress_scenarios.c
PERFORMANCE_SOURCES = $(PERFORMANCE_DIR)/test_performance_benchmarks.c
PROTOCOL_SOURCES = $(PROTOCOL_DIR)/test_smbus_pec.c $(PROTOCOL_DIR)/test_clock_stretching.c $(PROTOCOL_DIR)/test_high_speed_mode.c $(PROTOCOL_DIR)/test_smbus_timing.c

# Object files
MOCK_OBJECTS = $(MOCK_SOURCES:.c=.o)
UNIT_OBJECTS = $(UNIT_SOURCES:.c=.o) 
INTEGRATION_OBJECTS = $(INTEGRATION_SOURCES:.c=.o)
FAILURE_OBJECTS = $(FAILURE_SOURCES:.c=.o)
STRESS_OBJECTS = $(STRESS_SOURCES:.c=.o)
PERFORMANCE_OBJECTS = $(PERFORMANCE_SOURCES:.c=.o)
PROTOCOL_OBJECTS = $(PROTOCOL_SOURCES:.c=.o)

# Test executables
UNIT_TEST = test_i2c_core
INTEGRATION_TEST = test_i2c_transfer
FAILURE_TEST = test_error_conditions
STRESS_TEST = test_stress_scenarios
PERFORMANCE_TEST = test_performance_benchmarks
PROTOCOL_TESTS = test_smbus_pec test_clock_stretching test_high_speed_mode test_smbus_timing

# Driver source files (for integration testing)
DRIVER_DIR = ../src/driver
DRIVER_SOURCES = $(DRIVER_DIR)/i2c-a78-dma.c $(DRIVER_DIR)/i2c-a78-pm.c

.PHONY: all clean test unit integration failure stress performance protocol help comprehensive

all: $(UNIT_TEST) $(INTEGRATION_TEST) $(FAILURE_TEST) $(STRESS_TEST) $(PERFORMANCE_TEST) $(PROTOCOL_TESTS)

$(UNIT_TEST): $(UNIT_OBJECTS) $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(INTEGRATION_TEST): $(INTEGRATION_OBJECTS) $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(FAILURE_TEST): $(FAILURE_OBJECTS) $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(STRESS_TEST): $(STRESS_OBJECTS) $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(PERFORMANCE_TEST): $(PERFORMANCE_OBJECTS) $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Protocol test executables
test_smbus_pec: $(PROTOCOL_DIR)/test_smbus_pec.o $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test_clock_stretching: $(PROTOCOL_DIR)/test_clock_stretching.o $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test_high_speed_mode: $(PROTOCOL_DIR)/test_high_speed_mode.o $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test_smbus_timing: $(PROTOCOL_DIR)/test_smbus_timing.o $(MOCK_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

unit: $(UNIT_TEST)
	@echo "Running unit tests..."
	./$(UNIT_TEST)

integration: $(INTEGRATION_TEST)
	@echo "Running integration tests..."
	./$(INTEGRATION_TEST)

failure: $(FAILURE_TEST)
	@echo "Running failure scenario tests..."
	./$(FAILURE_TEST)

stress: $(STRESS_TEST)
	@echo "Running stress tests..."
	./$(STRESS_TEST)

performance: $(PERFORMANCE_TEST)
	@echo "Running performance benchmarks..."
	mkdir -p ../test_results
	./$(PERFORMANCE_TEST)

protocol: $(PROTOCOL_TESTS)
	@echo "Running protocol compliance tests..."
	@echo "=== SMBus PEC Tests ==="
	./test_smbus_pec
	@echo "=== Clock Stretching Tests ==="
	./test_clock_stretching
	@echo "=== High-Speed Mode Tests ==="
	./test_high_speed_mode
	@echo "=== SMBus Timing Tests ==="
	./test_smbus_timing
	@echo "Protocol compliance tests completed!"

test: unit integration failure stress protocol
	@echo "All core tests completed!"

comprehensive: all
	@echo "Running comprehensive test suite..."
	@echo "=== Running Test Runner ==="
	python3 test_runner.py --project-root=..

clean:
	rm -f $(UNIT_OBJECTS) $(INTEGRATION_OBJECTS) $(MOCK_OBJECTS)
	rm -f $(FAILURE_OBJECTS) $(STRESS_OBJECTS) $(PERFORMANCE_OBJECTS) $(PROTOCOL_OBJECTS)
	rm -f $(UNIT_TEST) $(INTEGRATION_TEST) $(FAILURE_TEST) $(STRESS_TEST) $(PERFORMANCE_TEST) $(PROTOCOL_TESTS)
	rm -f *.o *~ core *.gcov *.gcno *.gcda

install-deps:
	@echo "Installing test dependencies..."
	@which gcc > /dev/null || (echo "ERROR: GCC compiler not found" && exit 1)
	@echo "Dependencies check passed!"

coverage: CFLAGS += --coverage
coverage: clean all test
	@echo "Generating coverage report..."
	gcov $(UNIT_SOURCES) $(INTEGRATION_SOURCES) $(MOCK_SOURCES)
	@echo "Coverage report generated (*.gcov files)"

valgrind: all
	@echo "Running tests with Valgrind..."
	@which valgrind > /dev/null || (echo "ERROR: Valgrind not installed" && exit 1)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(UNIT_TEST)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(INTEGRATION_TEST)

static-analysis:
	@echo "Running static analysis..."
	@which cppcheck > /dev/null || (echo "WARNING: cppcheck not installed, skipping static analysis" && exit 0)
	cppcheck --enable=all --inconclusive --std=c99 $(UNIT_SOURCES) $(INTEGRATION_SOURCES) $(MOCK_SOURCES)

benchmark: $(INTEGRATION_TEST)
	@echo "Running performance benchmarks..."
	time ./$(INTEGRATION_TEST)

help:
	@echo "Available targets:"
	@echo "  all           - Build all test executables"
	@echo "  unit          - Run unit tests"
	@echo "  integration   - Run integration tests"  
	@echo "  failure       - Run failure scenario tests"
	@echo "  stress        - Run stress tests"
	@echo "  performance   - Run performance benchmarks"
	@echo "  protocol      - Run protocol compliance tests (I2C v2.1/SMBus v2.0)"
	@echo "  test          - Run all core tests (unit, integration, failure, stress, protocol)"
	@echo "  comprehensive - Run complete test suite with reporting"
	@echo "  clean         - Clean build artifacts"
	@echo "  install-deps  - Check test dependencies"
	@echo "  coverage      - Generate code coverage report"
	@echo "  valgrind      - Run tests with memory checking"
	@echo "  static-analysis - Run static code analysis"
	@echo "  benchmark     - Run performance benchmarks"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Test executables:"
	@echo "  $(UNIT_TEST)           - Unit tests for core functionality"
	@echo "  $(INTEGRATION_TEST)    - Integration tests for transfers"
	@echo "  $(FAILURE_TEST)        - Failure scenario tests"
	@echo "  $(STRESS_TEST)         - Stress and load tests"
	@echo "  $(PERFORMANCE_TEST)    - Performance benchmarks"
	@echo "  Protocol Tests:"
	@echo "    test_smbus_pec       - SMBus v2.0 Packet Error Checking"
	@echo "    test_clock_stretching - I2C v2.1 Clock Stretching"
	@echo "    test_high_speed_mode - I2C v2.1 High-Speed Mode (3.4MHz)"
	@echo "    test_smbus_timing    - SMBus v2.0 Timing Requirements"

# Dependencies
$(UNIT_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h ../src/include/i2c-a78.h
$(INTEGRATION_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h ../src/include/i2c-a78.h
$(FAILURE_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h ../src/include/i2c-a78.h
$(STRESS_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h ../src/include/i2c-a78.h
$(PERFORMANCE_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h ../src/include/i2c-a78.h
$(PROTOCOL_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h ../src/include/i2c-a78.h
$(MOCK_OBJECTS): $(MOCKS_DIR)/mock-linux-kernel.h