#!/usr/bin/env python3
import re
import sys

def parse_c_image(filename, width, height):
    """Parse C array image file and return raw bytes"""
    with open(filename, 'r') as f:
        content = f.read()
    
    # Extract all 0xNN hex values
    hex_values = re.findall(r'0x([0-9a-fA-F]{2})', content)
    bytes_data = bytes([int(h, 16) for h in hex_values])
    
    expected_size = width * height * 2  # RGB565 = 2 bytes per pixel
    if len(bytes_data) != expected_size:
        print(f"ERROR: {filename}")
        print(f"  Expected {expected_size} bytes (RGB565 {width}x{height})")
        print(f"  Got {len(bytes_data)} bytes")
        return None
    
    print(f"✓ {filename}: {len(bytes_data)} bytes (correct)")
    return bytes_data

def analyze_right_edge(bytes_data, width, height):
    """Analyze rightmost column for anomalies"""
    print(f"\nAnalyzing rightmost column (col {width-1}):")
    
    # Collect pixels from rightmost column
    right_col_pixels = []
    center_col_pixels = []
    
    for row in range(height):
        # Rightmost pixel (column width-1)
        right_idx = (row * width + (width - 1)) * 2
        right_pixel = (bytes_data[right_idx] << 8) | bytes_data[right_idx + 1]
        right_col_pixels.append(right_pixel)
        
        # Center column (for comparison)
        if row < height:
            center_idx = (row * width + (width // 2)) * 2
            center_pixel = (bytes_data[center_idx] << 8) | bytes_data[center_idx + 1]
            center_col_pixels.append(center_pixel)
    
    # Statistics
    right_avg = sum(right_col_pixels) / len(right_col_pixels)
    center_avg = sum(center_col_pixels) / len(center_col_pixels)
    
    # Count unique colors in each column
    right_unique = len(set(right_col_pixels))
    center_unique = len(set(center_col_pixels))
    
    print(f"  Right column: {right_unique} unique colors, avg value: {right_avg:.1f}")
    print(f"  Center column: {center_unique} unique colors, avg value: {center_avg:.1f}")
    
    # Check for suspicious patterns (e.g., random noise)
    right_variance = sum((p - right_avg) ** 2 for p in right_col_pixels) / len(right_col_pixels)
    center_variance = sum((p - center_avg) ** 2 for p in center_col_pixels) / len(center_col_pixels)
    
    print(f"  Right column variance: {right_variance:.1f}")
    print(f"  Center column variance: {center_variance:.1f}")
    
    # Check edge-adjacent columns for pattern
    left_col_pixels = []
    for row in range(height):
        left_idx = (row * width + (width - 2)) * 2
        left_pixel = (bytes_data[left_idx] << 8) | bytes_data[left_idx + 1]
        left_col_pixels.append(left_pixel)
    
    # Compare adjacent columns
    diff_right_left = sum(abs(r - l) for r, l in zip(right_col_pixels, left_col_pixels)) / height
    diff_right_center = sum(abs(r - c) for r, c in zip(right_col_pixels, center_col_pixels)) / height
    
    print(f"  Avg difference (right vs left): {diff_right_left:.1f}")
    print(f"  Avg difference (right vs center): {diff_right_center:.1f}")
    
    # Sample first 10 rows
    print(f"\n  First 10 rows (rightmost 3 columns):")
    for row in range(min(10, height)):
        right_idx = (row * width + (width - 1)) * 2
        left2_idx = (row * width + (width - 2)) * 2
        left3_idx = (row * width + (width - 3)) * 2
        
        p_right = (bytes_data[right_idx] << 8) | bytes_data[right_idx + 1]
        p_left2 = (bytes_data[left2_idx] << 8) | bytes_data[left2_idx + 1]
        p_left3 = (bytes_data[left3_idx] << 8) | bytes_data[left3_idx + 1]
        
        print(f"    Row {row:3d}: col[125]={p_left3:04x}  col[126]={p_left2:04x}  col[127]={p_right:04x}")

def main():
    # Analyze both images
    for img_num in [1, 2]:
        filename = f"src/image_{img_num}_128x160_rgb565.c"
        data = parse_c_image(filename, 128, 160)
        if data:
            analyze_right_edge(data, 128, 160)
            print()

if __name__ == '__main__':
    main()
