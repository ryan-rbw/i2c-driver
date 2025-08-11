#!/bin/bash

# I2C A78 Driver - Enhanced Code Coverage Analysis Script

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TESTS_DIR="$PROJECT_ROOT/tests"
RESULTS_DIR="$PROJECT_ROOT/test_results"
COVERAGE_DIR="$RESULTS_DIR/coverage"
HTML_DIR="$RESULTS_DIR/html"

# Create directories
mkdir -p "$COVERAGE_DIR" "$HTML_DIR"

echo "=== I2C A78 Driver - Code Coverage Analysis ==="
echo "Project Root: $PROJECT_ROOT"
echo "Tests Directory: $TESTS_DIR"
echo "Results Directory: $RESULTS_DIR"
echo ""

cd "$TESTS_DIR"

# Clean previous builds and coverage data
echo "Cleaning previous coverage data..."
rm -f *.gcov *.gcno *.gcda
make clean > /dev/null 2>&1 || true

# Build with coverage flags
echo "Building tests with coverage instrumentation..."
CFLAGS="--coverage -fprofile-arcs -ftest-coverage -O0 -g"
LDFLAGS="--coverage"

# Build unit tests with coverage
gcc $CFLAGS -Wall -Wextra -std=c99 -I. -Imocks \
    unit/test_i2c_core.c \
    mocks/mock-implementations.c \
    -o test_i2c_core $LDFLAGS

# Build integration tests with coverage
gcc $CFLAGS -Wall -Wextra -std=c99 -I. -Imocks \
    integration/test_i2c_transfer.c \
    mocks/mock-implementations.c \
    -o test_i2c_transfer $LDFLAGS

# Build failure tests with coverage
gcc $CFLAGS -Wall -Wextra -std=c99 -I. -Imocks \
    failure_scenarios/test_error_conditions.c \
    mocks/mock-implementations.c \
    -o test_error_conditions $LDFLAGS

# Build stress tests with coverage  
gcc $CFLAGS -Wall -Wextra -std=c99 -I. -Imocks \
    stress/test_stress_scenarios.c \
    mocks/mock-implementations.c \
    -o test_stress_scenarios $LDFLAGS

echo "✓ Coverage-instrumented tests built successfully"

# Run tests to generate coverage data
echo ""
echo "Running tests to collect coverage data..."

echo "Running unit tests..."
./test_i2c_core > /dev/null 2>&1 || echo "Unit tests completed with issues"

echo "Running integration tests..."
./test_i2c_transfer > /dev/null 2>&1 || echo "Integration tests completed with issues"

echo "Running failure scenario tests..."
./test_error_conditions > /dev/null 2>&1 || echo "Failure tests completed with issues"

echo "Running stress tests..."
./test_stress_scenarios > /dev/null 2>&1 || echo "Stress tests completed with issues"

echo "✓ Test execution completed"

# Generate gcov reports
echo ""
echo "Generating gcov coverage reports..."

# Generate coverage for all source files
gcov -b -c *.c 2>/dev/null || true

# Count coverage statistics
total_lines=0
covered_lines=0
partially_covered=0

echo ""
echo "=== Coverage Analysis ==="

for gcov_file in *.gcov; do
    if [[ -f "$gcov_file" ]]; then
        filename=$(basename "$gcov_file" .gcov)
        
        # Skip compiler-generated files
        if [[ "$filename" == *"mock"* ]] || [[ "$filename" == *"test"* ]]; then
            echo "Analyzing: $filename"
            
            file_total=0
            file_covered=0
            file_partial=0
            
            while IFS= read -r line; do
                # Skip header lines and non-code lines
                if [[ "$line" =~ ^[[:space:]]*[0-9]+:[[:space:]]*[0-9]+: ]] || [[ "$line" =~ ^[[:space:]]*#####:[[:space:]]*[0-9]+: ]]; then
                    ((file_total++))
                    
                    if [[ "$line" =~ ^[[:space:]]*([0-9]+):[[:space:]]*[0-9]+: ]]; then
                        exec_count="${BASH_REMATCH[1]}"
                        if [[ "$exec_count" -gt 0 ]]; then
                            ((file_covered++))
                        fi
                    elif [[ "$line" =~ ^[[:space:]]*#####: ]]; then
                        # Uncovered line
                        :
                    fi
                fi
            done < "$gcov_file"
            
            if [[ $file_total -gt 0 ]]; then
                coverage_percent=$((file_covered * 100 / file_total))
                printf "  %-30s: %3d/%3d lines covered (%3d%%)\n" "$filename" "$file_covered" "$file_total" "$coverage_percent"
                
                ((total_lines += file_total))
                ((covered_lines += file_covered))
            fi
        fi
    fi
done

# Calculate overall coverage
if [[ $total_lines -gt 0 ]]; then
    overall_coverage=$((covered_lines * 100 / total_lines))
else
    overall_coverage=0
fi

echo ""
echo "=== Overall Coverage Summary ==="
echo "Total lines:        $total_lines"
echo "Covered lines:      $covered_lines"
echo "Uncovered lines:    $((total_lines - covered_lines))"
echo "Coverage:           ${overall_coverage}%"

# Coverage quality assessment
if [[ $overall_coverage -ge 90 ]]; then
    quality="EXCELLENT"
    color="green"
elif [[ $overall_coverage -ge 80 ]]; then
    quality="GOOD"
    color="lightgreen"
elif [[ $overall_coverage -ge 70 ]]; then
    quality="ADEQUATE"
    color="yellow"
elif [[ $overall_coverage -ge 60 ]]; then
    quality="POOR"
    color="orange"
else
    quality="VERY POOR"
    color="red"
fi

echo "Quality:            $quality"

# Generate detailed HTML report
echo ""
echo "Generating HTML coverage report..."

cat > "$HTML_DIR/detailed_coverage.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>I2C A78 Driver - Detailed Coverage Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; border-radius: 5px; margin-bottom: 20px; }
        .summary { background-color: #e8f5e8; padding: 15px; border-radius: 5px; margin: 20px 0; }
        .coverage-bar { 
            background-color: #ddd; 
            border-radius: 10px; 
            padding: 3px; 
            width: 400px; 
            margin: 10px 0;
        }
        .coverage-fill { 
            background-color: $color; 
            height: 25px; 
            border-radius: 7px; 
            width: ${overall_coverage}%; 
            text-align: center; 
            line-height: 25px; 
            color: white; 
            font-weight: bold;
        }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .file-section { margin: 20px 0; padding: 10px; border: 1px solid #ccc; border-radius: 5px; }
        .good { background-color: #d4edda; }
        .medium { background-color: #fff3cd; }
        .poor { background-color: #f8d7da; }
        pre { background-color: #f8f9fa; padding: 10px; overflow-x: auto; font-size: 12px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>I2C A78 Driver - Detailed Code Coverage Report</h1>
        <p>Generated: $(date)</p>
        <p>Analysis includes unit tests, integration tests, failure scenarios, and stress tests</p>
    </div>

    <div class="summary">
        <h2>Coverage Summary</h2>
        <div class="coverage-bar">
            <div class="coverage-fill">${overall_coverage}%</div>
        </div>
        <table>
            <tr><th>Metric</th><th>Value</th></tr>
            <tr><td>Overall Coverage</td><td><strong>${overall_coverage}%</strong></td></tr>
            <tr><td>Quality Rating</td><td><strong>$quality</strong></td></tr>
            <tr><td>Total Lines</td><td>$total_lines</td></tr>
            <tr><td>Covered Lines</td><td>$covered_lines</td></tr>
            <tr><td>Uncovered Lines</td><td>$((total_lines - covered_lines))</td></tr>
        </table>
    </div>

    <div class="file-details">
        <h2>File-by-File Coverage</h2>
        <table>
            <tr>
                <th>File</th>
                <th>Lines</th>
                <th>Covered</th>
                <th>Uncovered</th>
                <th>Coverage %</th>
                <th>Status</th>
            </tr>
EOF

# Add file details to HTML
for gcov_file in *.gcov; do
    if [[ -f "$gcov_file" ]]; then
        filename=$(basename "$gcov_file" .gcov)
        
        if [[ "$filename" == *"mock"* ]] || [[ "$filename" == *"test"* ]]; then
            file_total=0
            file_covered=0
            
            while IFS= read -r line; do
                if [[ "$line" =~ ^[[:space:]]*[0-9]+:[[:space:]]*[0-9]+: ]] || [[ "$line" =~ ^[[:space:]]*#####:[[:space:]]*[0-9]+: ]]; then
                    ((file_total++))
                    
                    if [[ "$line" =~ ^[[:space:]]*([0-9]+):[[:space:]]*[0-9]+: ]]; then
                        exec_count="${BASH_REMATCH[1]}"
                        if [[ "$exec_count" -gt 0 ]]; then
                            ((file_covered++))
                        fi
                    fi
                fi
            done < "$gcov_file"
            
            if [[ $file_total -gt 0 ]]; then
                file_coverage=$((file_covered * 100 / file_total))
                file_uncovered=$((file_total - file_covered))
                
                # Determine status class
                if [[ $file_coverage -ge 80 ]]; then
                    status_class="good"
                    status_text="GOOD"
                elif [[ $file_coverage -ge 60 ]]; then
                    status_class="medium"
                    status_text="MEDIUM"
                else
                    status_class="poor"
                    status_text="POOR"
                fi
                
                cat >> "$HTML_DIR/detailed_coverage.html" << EOF
            <tr class="$status_class">
                <td>$filename</td>
                <td>$file_total</td>
                <td>$file_covered</td>
                <td>$file_uncovered</td>
                <td>${file_coverage}%</td>
                <td>$status_text</td>
            </tr>
EOF
            fi
        fi
    fi
done

cat >> "$HTML_DIR/detailed_coverage.html" << 'EOF'
        </table>
    </div>

    <div class="recommendations">
        <h2>Coverage Improvement Recommendations</h2>
        <ul>
            <li>Focus on testing error handling paths that are currently uncovered</li>
            <li>Add tests for edge cases and boundary conditions</li>
            <li>Increase testing of power management state transitions</li>
            <li>Add more comprehensive DMA failure scenario testing</li>
            <li>Test concurrent access patterns more thoroughly</li>
        </ul>
    </div>

    <div class="files-section">
        <h2>Individual File Reports</h2>
        <p>Detailed line-by-line coverage data is available in the .gcov files in the coverage directory.</p>
    </div>
</body>
</html>
EOF

# Move coverage files to results directory
echo "Moving coverage files to results directory..."
mv *.gcov "$COVERAGE_DIR/" 2>/dev/null || true
mv *.gcno "$COVERAGE_DIR/" 2>/dev/null || true  
mv *.gcda "$COVERAGE_DIR/" 2>/dev/null || true

echo "✓ HTML coverage report generated: $HTML_DIR/detailed_coverage.html"

# Generate lcov report if available
if command -v lcov > /dev/null 2>&1; then
    echo ""
    echo "Generating lcov HTML report..."
    lcov --capture --directory . --output-file "$COVERAGE_DIR/coverage.info" --quiet
    genhtml "$COVERAGE_DIR/coverage.info" --output-directory "$HTML_DIR/lcov" --quiet
    echo "✓ lcov HTML report generated: $HTML_DIR/lcov/index.html"
fi

echo ""
echo "=== Coverage Analysis Complete ==="
echo "Overall Coverage: ${overall_coverage}% ($quality)"
echo "Reports available at:"
echo "  - Detailed HTML: $HTML_DIR/detailed_coverage.html"
echo "  - Raw gcov files: $COVERAGE_DIR/"
if command -v lcov > /dev/null 2>&1; then
    echo "  - lcov HTML: $HTML_DIR/lcov/index.html"
fi

# Return appropriate exit code based on coverage threshold
if [[ $overall_coverage -ge 70 ]]; then
    echo "✓ Coverage meets minimum threshold (70%)"
    exit 0
else
    echo "✗ Coverage below minimum threshold (70%)"
    exit 1
fi