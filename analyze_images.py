#!/usr/bin/env python3
import re

def analyze_image(filepath, width, height):
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Extract all hex bytes
    hex_vals = re.findall(r'0x([0-9a-fA-F]{2})', content)
    bytes_array = [int(h, 16) for h in hex_vals]
    
    expected = width * height * 2
    print(f"\n{filepath}")
    print(f"Bytes found: {len(bytes_array)}, Expected: {expected}")
    
    if len(bytes_array) != expected:
        print("SIZE_MISMATCH!")
        return
    
    # Check right edge (column 127) vs column 126
    col127_sum = 0
    col126_sum = 0
    max_diff = 0
    min_diff = float('inf')
    diffs = []
    col127_pixels = []
    
    for y in range(height):
        # RGB565 is 2 bytes per pixel
        idx127 = (y * width + (width - 1)) * 2
        idx126 = (y * width + (width - 2)) * 2
        
        p127 = (bytes_array[idx127] << 8) | bytes_array[idx127 + 1]
        p126 = (bytes_array[idx126] << 8) | bytes_array[idx126 + 1]
        
        col127_pixels.append(p127)
        col127_sum += p127
        col126_sum += p126
        diff = abs(p127 - p126)
        diffs.append(diff)
        max_diff = max(max_diff, diff)
        min_diff = min(min_diff, diff)
    
    avg127 = col127_sum / height
    avg126 = col126_sum / height
    avg_diff = sum(diffs) / height
    
    print(f"Col 127 avg: {avg127:.2f}, Col 126 avg: {avg126:.2f}")
    print(f"Avg diff: {avg_diff:.2f}, Min diff: {min_diff}, Max diff: {max_diff}")
    
    # Check if col 127 has excessive variation
    variance127 = sum((p - avg127)**2 for p in col127_pixels) / height
    std_dev = variance127 ** 0.5
    print(f"Col 127 StdDev: {std_dev:.2f}")
    
    # Show first and last few pixels of rightmost column
    print(f"Col 127 first 5 rows: {[hex(p) for p in col127_pixels[:5]]}")
    print(f"Col 127 last 5 rows: {[hex(p) for p in col127_pixels[-5:]]}")

print("Analyzing image arrays for corruption/noise...")
analyze_image('src/image_1_128x160_rgb565.c', 128, 160)
analyze_image('src/image_2_128x160_rgb565.c', 128, 160)
