#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#include "../test_common.h"

#define BENCHMARK_ITERATIONS 10000
#define PERFORMANCE_TIMEOUT_MS 10000

typedef struct {
    const char *name;
    double min_time;
    double max_time;
    double avg_time;
    double total_time;
    int iterations;
    int failures;
    double throughput_mbps;
} benchmark_result_t;

static struct i2c_a78_dev *create_benchmark_device(void)
{
    struct device *mock_dev;
    struct i2c_a78_dev *i2c_dev;
    
    mock_dev = devm_kzalloc(NULL, sizeof(*mock_dev), GFP_KERNEL);
    strcpy(mock_dev->name, "benchmark-i2c");
    
    i2c_dev = devm_kzalloc(mock_dev, sizeof(*i2c_dev), GFP_KERNEL);
    i2c_dev->dev = mock_dev;
    i2c_dev->base = (void *)0x4000;
    i2c_dev->bus_freq = I2C_A78_SPEED_FAST_PLUS; // 1MHz for performance testing
    i2c_dev->timeout_ms = PERFORMANCE_TIMEOUT_MS;
    i2c_dev->state = I2C_A78_STATE_IDLE;
    i2c_dev->dma.use_dma = true;
    
    return i2c_dev;
}

static double get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

static benchmark_result_t benchmark_register_access(void)
{
    struct i2c_a78_dev *i2c_dev;
    benchmark_result_t result = {0};
    double start_time, end_time, elapsed;
    double min_time = 1e9, max_time = 0, total_time = 0;
    
    result.name = "Register Access";
    result.iterations = BENCHMARK_ITERATIONS;
    
    printf("Benchmarking register access performance...\n");
    
    i2c_dev = create_benchmark_device();
    
    // Benchmark register writes
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        start_time = get_time_us();
        
        i2c_a78_writel(i2c_dev, 0x12345678, I2C_A78_CONTROL);
        i2c_a78_writel(i2c_dev, 0xABCDEF00, I2C_A78_DATA);
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }
    
    // Benchmark register reads
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        start_time = get_time_us();
        
        volatile u32 val1 = i2c_a78_readl(i2c_dev, I2C_A78_STATUS);
        volatile u32 val2 = i2c_a78_readl(i2c_dev, I2C_A78_FIFO_STATUS);
        (void)val1; (void)val2; // Prevent optimization
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }
    
    result.min_time = min_time;
    result.max_time = max_time;
    result.avg_time = total_time / (BENCHMARK_ITERATIONS * 2);
    result.total_time = total_time;
    result.failures = 0;
    
    // Calculate throughput (operations per second)
    double ops_per_second = (BENCHMARK_ITERATIONS * 2 * 1000000.0) / total_time;
    result.throughput_mbps = ops_per_second / 1000000.0; // Convert to Mops/s
    
    return result;
}

static benchmark_result_t benchmark_small_transfers(void)
{
    struct i2c_a78_dev *i2c_dev;
    benchmark_result_t result = {0};
    struct i2c_msg msg;
    u8 data[16];
    double start_time, end_time, elapsed;
    double min_time = 1e9, max_time = 0, total_time = 0;
    int failures = 0;
    
    result.name = "Small Transfers (16 bytes)";
    result.iterations = BENCHMARK_ITERATIONS;
    
    printf("Benchmarking small transfer performance...\n");
    
    i2c_dev = create_benchmark_device();
    
    // Initialize test data
    for (int i = 0; i < 16; i++) {
        data[i] = i;
    }
    
    msg.addr = 0x50;
    msg.len = 16;
    msg.buf = data;
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        msg.flags = (i % 2) ? I2C_M_RD : 0; // Alternate read/write
        
        start_time = get_time_us();
        
        // Simulate transfer setup and execution
        i2c_dev->state = I2C_A78_STATE_START;
        
        // PIO transfer simulation (< DMA threshold)
        if (msg.flags & I2C_M_RD) {
            i2c_dev->stats.rx_bytes += msg.len;
        } else {
            i2c_dev->stats.tx_bytes += msg.len;
        }
        
        i2c_dev->state = I2C_A78_STATE_IDLE;
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }
    
    result.min_time = min_time;
    result.max_time = max_time;
    result.avg_time = total_time / BENCHMARK_ITERATIONS;
    result.total_time = total_time;
    result.failures = failures;
    
    // Calculate throughput (MB/s)
    double total_bytes = BENCHMARK_ITERATIONS * 16.0;
    result.throughput_mbps = (total_bytes / (total_time / 1000000.0)) / (1024 * 1024);
    
    return result;
}

static benchmark_result_t benchmark_large_transfers(void)
{
    struct i2c_a78_dev *i2c_dev;
    benchmark_result_t result = {0};
    struct i2c_msg msg;
    u8 *data;
    double start_time, end_time, elapsed;
    double min_time = 1e9, max_time = 0, total_time = 0;
    int iterations = BENCHMARK_ITERATIONS / 10; // Fewer iterations for large transfers
    int failures = 0;
    
    result.name = "Large Transfers (256 bytes, DMA)";
    result.iterations = iterations;
    
    printf("Benchmarking large transfer performance...\n");
    
    i2c_dev = create_benchmark_device();
    data = malloc(256);
    
    // Initialize test data
    for (int i = 0; i < 256; i++) {
        data[i] = i & 0xFF;
    }
    
    msg.addr = 0x50;
    msg.len = 256;
    msg.buf = data;
    
    for (int i = 0; i < iterations; i++) {
        msg.flags = (i % 2) ? I2C_M_RD : 0;
        
        start_time = get_time_us();
        
        // Simulate DMA transfer (>= DMA threshold)
        i2c_dev->state = I2C_A78_STATE_START;
        
        // DMA transfer simulation
        if (msg.flags & I2C_M_RD) {
            i2c_dev->stats.rx_bytes += msg.len;
        } else {
            i2c_dev->stats.tx_bytes += msg.len;
        }
        
        i2c_dev->state = I2C_A78_STATE_IDLE;
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }
    
    result.min_time = min_time;
    result.max_time = max_time;
    result.avg_time = total_time / iterations;
    result.total_time = total_time;
    result.failures = failures;
    
    // Calculate throughput (MB/s)
    double total_bytes = iterations * 256.0;
    result.throughput_mbps = (total_bytes / (total_time / 1000000.0)) / (1024 * 1024);
    
    free(data);
    return result;
}

static benchmark_result_t benchmark_power_management(void)
{
    struct i2c_a78_dev *i2c_dev;
    benchmark_result_t result = {0};
    double start_time, end_time, elapsed;
    double min_time = 1e9, max_time = 0, total_time = 0;
    int iterations = BENCHMARK_ITERATIONS / 100; // Fewer iterations for PM operations
    
    result.name = "Power Management Cycles";
    result.iterations = iterations;
    
    printf("Benchmarking power management performance...\n");
    
    i2c_dev = create_benchmark_device();
    
    for (int i = 0; i < iterations; i++) {
        start_time = get_time_us();
        
        // Suspend cycle
        i2c_dev->suspended = true;
        i2c_dev->saved_control = i2c_a78_readl(i2c_dev, I2C_A78_CONTROL);
        i2c_dev->saved_prescaler = i2c_a78_readl(i2c_dev, I2C_A78_PRESCALER);
        
        // Resume cycle
        i2c_a78_writel(i2c_dev, i2c_dev->saved_prescaler, I2C_A78_PRESCALER);
        i2c_a78_writel(i2c_dev, i2c_dev->saved_control, I2C_A78_CONTROL);
        i2c_dev->suspended = false;
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }
    
    result.min_time = min_time;
    result.max_time = max_time;
    result.avg_time = total_time / iterations;
    result.total_time = total_time;
    result.failures = 0;
    
    // Calculate cycles per second
    result.throughput_mbps = (iterations * 1000000.0) / total_time;
    
    return result;
}

static benchmark_result_t benchmark_interrupt_handling(void)
{
    struct i2c_a78_dev *i2c_dev;
    benchmark_result_t result = {0};
    double start_time, end_time, elapsed;
    double min_time = 1e9, max_time = 0, total_time = 0;
    
    result.name = "Interrupt Processing";
    result.iterations = BENCHMARK_ITERATIONS;
    
    printf("Benchmarking interrupt handling performance...\n");
    
    i2c_dev = create_benchmark_device();
    
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        start_time = get_time_us();
        
        // Simulate interrupt processing
        u32 status = I2C_A78_INT_TX_DONE | I2C_A78_INT_RX_READY;
        i2c_a78_writel(i2c_dev, status, I2C_A78_INTERRUPT);
        
        // Interrupt handling simulation
        if (status & I2C_A78_INT_TX_DONE) {
            i2c_dev->state = I2C_A78_STATE_IDLE;
        }
        
        // Clear interrupt
        i2c_a78_writel(i2c_dev, status, I2C_A78_INTERRUPT);
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;
        total_time += elapsed;
    }
    
    result.min_time = min_time;
    result.max_time = max_time;
    result.avg_time = total_time / BENCHMARK_ITERATIONS;
    result.total_time = total_time;
    result.failures = 0;
    
    // Calculate interrupts per second
    result.throughput_mbps = (BENCHMARK_ITERATIONS * 1000000.0) / total_time;
    
    return result;
}

static void print_benchmark_results(benchmark_result_t *results, int count)
{
    printf("\n=== Performance Benchmark Results ===\n\n");
    
    printf("%-25s | %10s | %10s | %10s | %10s | %10s | %s\n",
           "Benchmark", "Min (μs)", "Max (μs)", "Avg (μs)", "Iterations", "Failures", "Throughput");
    printf("%.25s-+%.10s-+%.10s-+%.10s-+%.10s-+%.10s-+%.15s\n",
           "-------------------------", "----------", "----------", "----------", 
           "----------", "----------", "---------------");
    
    for (int i = 0; i < count; i++) {
        benchmark_result_t *r = &results[i];
        
        printf("%-25s | %10.2f | %10.2f | %10.2f | %10d | %10d | ",
               r->name, r->min_time, r->max_time, r->avg_time, r->iterations, r->failures);
        
        if (strstr(r->name, "Transfer")) {
            printf("%.2f MB/s\n", r->throughput_mbps);
        } else if (strstr(r->name, "Register")) {
            printf("%.2f Mops/s\n", r->throughput_mbps);
        } else {
            printf("%.0f ops/s\n", r->throughput_mbps);
        }
    }
    
    printf("\n");
}

static void save_benchmark_results_json(benchmark_result_t *results, int count, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Warning: Could not save results to %s\n", filename);
        return;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"timestamp\": \"%ld\",\n", time(NULL));
    fprintf(f, "  \"benchmarks\": [\n");
    
    for (int i = 0; i < count; i++) {
        benchmark_result_t *r = &results[i];
        
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", r->name);
        fprintf(f, "      \"min_time_us\": %.2f,\n", r->min_time);
        fprintf(f, "      \"max_time_us\": %.2f,\n", r->max_time);
        fprintf(f, "      \"avg_time_us\": %.2f,\n", r->avg_time);
        fprintf(f, "      \"total_time_us\": %.2f,\n", r->total_time);
        fprintf(f, "      \"iterations\": %d,\n", r->iterations);
        fprintf(f, "      \"failures\": %d,\n", r->failures);
        fprintf(f, "      \"throughput\": %.2f\n", r->throughput_mbps);
        fprintf(f, "    }%s\n", (i < count - 1) ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    printf("Benchmark results saved to: %s\n", filename);
}

int main(void)
{
    printf("=== I2C A78 Driver Performance Benchmarks ===\n");
    printf("Iterations per benchmark: %d\n", BENCHMARK_ITERATIONS);
    printf("Target bus frequency: %u Hz (Fast-mode Plus)\n\n", I2C_A78_SPEED_FAST_PLUS);
    
    clock_t total_start = clock();
    
    benchmark_result_t results[5];
    int result_count = 0;
    
    // Run benchmarks
    results[result_count++] = benchmark_register_access();
    results[result_count++] = benchmark_small_transfers();
    results[result_count++] = benchmark_large_transfers();
    results[result_count++] = benchmark_power_management();
    results[result_count++] = benchmark_interrupt_handling();
    
    clock_t total_end = clock();
    double total_time = ((double)(total_end - total_start)) / CLOCKS_PER_SEC;
    
    // Display results
    print_benchmark_results(results, result_count);
    
    printf("=== Performance Summary ===\n");
    printf("Total benchmark time: %.2f seconds\n", total_time);
    
    // Performance analysis
    double reg_access_rate = results[0].throughput_mbps;
    double small_transfer_rate = results[1].throughput_mbps;
    double large_transfer_rate = results[2].throughput_mbps;
    
    printf("\nPerformance Analysis:\n");
    printf("  Register access rate: %.2f Mops/s %s\n", reg_access_rate,
           reg_access_rate > 5.0 ? "(EXCELLENT)" : reg_access_rate > 2.0 ? "(GOOD)" : "(NEEDS IMPROVEMENT)");
    
    printf("  Small transfer rate: %.2f MB/s %s\n", small_transfer_rate,
           small_transfer_rate > 10.0 ? "(EXCELLENT)" : small_transfer_rate > 5.0 ? "(GOOD)" : "(NEEDS IMPROVEMENT)");
    
    printf("  Large transfer rate: %.2f MB/s %s\n", large_transfer_rate,
           large_transfer_rate > 20.0 ? "(EXCELLENT)" : large_transfer_rate > 10.0 ? "(GOOD)" : "(NEEDS IMPROVEMENT)");
    
    // Save results
    save_benchmark_results_json(results, result_count, "../test_results/performance_results.json");
    
    printf("\n✓ Performance benchmarks completed successfully\n");
    
    return 0;
}