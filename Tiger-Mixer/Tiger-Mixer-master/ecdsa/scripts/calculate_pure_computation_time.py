#!/usr/bin/env python3
"""
A2L 系统纯计算时间计算脚本
从 CSV 文件中提取数据，计算排除区块链交互和等待时间的纯计算时间
"""

import csv
import sys
from pathlib import Path

# 需要排除的计时器（区块链交互和等待时间）
EXCLUDE_TIMERS = {
    'alice_blockchain_escrow_interaction',
    'alice_blockchain_interaction',
    'bob_blockchain_escrow_interaction',
    'tumbler_blockchain_escrow_interaction',
    # 等待时间计时器（如果添加了的话）
    'alice_waiting_registration',
    'alice_waiting_puzzle_share',
    'alice_waiting_puzzle_solved',
    'bob_waiting_*',
    'tumbler_waiting_*',
}

# 总时间计时器（包含所有时间，需要特殊处理）
TOTAL_TIME_TIMERS = {
    'alice_total_computation_time',
    'bob_total_computation_time',
    # tumbler 使用全局时间变量，不在这里
}

def parse_csv(csv_file):
    """解析 CSV 文件，返回计时器字典"""
    timers = {}
    with open(csv_file, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        for row in reader:
            if len(row) >= 2 and row[0] and row[1]:
                try:
                    name = row[0].strip()
                    value = float(row[1].strip())
                    timers[name] = value
                except (ValueError, IndexError):
                    continue
    return timers

def calculate_pure_computation_time(timers, role='alice'):
    """计算纯计算时间"""
    
    # 获取总时间
    total_timer_name = f'{role}_total_computation_time'
    total_time = timers.get(total_timer_name, 0)
    
    # 获取区块链交互时间
    blockchain_timer_name = f'{role}_blockchain_escrow_interaction'
    blockchain_time = timers.get(blockchain_timer_name, 0)
    
    # 方法1：使用总时间减去区块链交互时间
    method1_pure = total_time - blockchain_time
    
    # 方法2：累加所有非排除的计时器（不包括总时间本身）
    method2_pure = 0
    included_timers = []
    excluded_timers = []
    
    for name, value in timers.items():
        # 跳过总时间计时器本身
        if name in TOTAL_TIME_TIMERS:
            continue
        
        # 检查是否需要排除
        should_exclude = False
        for exclude_pattern in EXCLUDE_TIMERS:
            if exclude_pattern.endswith('*'):
                if name.startswith(exclude_pattern[:-1]):
                    should_exclude = True
                    break
            elif name == exclude_pattern:
                should_exclude = True
                break
        
        if should_exclude:
            excluded_timers.append((name, value))
        else:
            included_timers.append((name, value))
            method2_pure += value
    
    return {
        'role': role,
        'total_time': total_time,
        'blockchain_time': blockchain_time,
        'method1_pure': method1_pure,  # 总时间 - 区块链交互
        'method2_pure': method2_pure,  # 累加所有非排除计时器
        'included_timers': included_timers,
        'excluded_timers': excluded_timers,
        'difference': total_time - method2_pure,  # 差异（主要是等待时间）
    }

def print_analysis(result):
    """打印分析结果"""
    role = result['role']
    print(f"\n{'='*80}")
    print(f"{role.upper()} 纯计算时间分析")
    print(f"{'='*80}\n")
    
    print(f"[时间数据]")
    print(f"  总计算时间:              {result['total_time']:10.2f} ms ({result['total_time']/1000:.3f} 秒)")
    print(f"  区块链交互时间:          {result['blockchain_time']:10.2f} ms ({result['blockchain_time']/1000:.3f} 秒)")
    print(f"  差异（等待时间等）:      {result['difference']:10.2f} ms ({result['difference']/1000:.3f} 秒)")
    
    print(f"\n[方法1：总时间 - 区块链交互]")
    print(f"  纯计算时间:              {result['method1_pure']:10.2f} ms ({result['method1_pure']/1000:.3f} 秒)")
    print(f"  ⚠️  注意: 包含等待循环的 CPU 空转时间")
    
    print(f"\n[方法2：累加所有非排除计时器]（推荐）")
    print(f"  纯计算时间:              {result['method2_pure']:10.2f} ms ({result['method2_pure']/1000:.3f} 秒)")
    print(f"  ✅ 优点: 排除等待时间，更接近真实计算时间")
    print(f"  ⚠️  注意: 可能遗漏一些未单独计时的操作")
    
    print(f"\n[包含的计时器] ({len(result['included_timers'])} 个)")
    for name, value in sorted(result['included_timers'], key=lambda x: x[1], reverse=True):
        percentage = (value / result['method2_pure'] * 100) if result['method2_pure'] > 0 else 0
        print(f"    {name:45s} {value:10.2f} ms ({percentage:5.1f}%)")
    
    if result['excluded_timers']:
        print(f"\n[排除的计时器] ({len(result['excluded_timers'])} 个)")
        for name, value in result['excluded_timers']:
            print(f"    {name:45s} {value:10.2f} ms")
    
    print(f"\n[推荐使用]")
    print(f"  方法2的纯计算时间: {result['method2_pure']:.2f} ms ({result['method2_pure']/1000:.3f} 秒)")
    print(f"  这是最接近真实密码学计算时间的值")
    print(f"{'='*80}\n")

def main():
    if len(sys.argv) < 2:
        print("用法: python3 calculate_pure_computation_time.py <CSV文件> [角色]")
        print("示例:")
        print("  python3 calculate_pure_computation_time.py bin/alice_timing_results.csv alice")
        print("  python3 calculate_pure_computation_time.py bin/bob_timing_results.csv bob")
        print("  python3 calculate_pure_computation_time.py bin/tumbler_timing_results.csv tumbler")
        sys.exit(1)
    
    csv_file = Path(sys.argv[1])
    role = sys.argv[2] if len(sys.argv) > 2 else csv_file.stem.replace('_timing_results', '')
    
    if not csv_file.exists():
        print(f"错误: 文件不存在: {csv_file}")
        sys.exit(1)
    
    print(f"正在分析: {csv_file}")
    print(f"角色: {role}")
    
    timers = parse_csv(csv_file)
    if not timers:
        print("错误: 无法从 CSV 文件中解析数据")
        sys.exit(1)
    
    result = calculate_pure_computation_time(timers, role)
    print_analysis(result)
    
    # 保存结果到 JSON
    import json
    output_file = csv_file.parent / f"{role}_pure_computation_analysis.json"
    with open(output_file, 'w') as f:
        json.dump(result, f, indent=2)
    print(f"详细结果已保存到: {output_file}")

if __name__ == "__main__":
    main()







