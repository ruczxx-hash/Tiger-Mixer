#!/usr/bin/env python3
"""
A2L 系统角色耗时分析脚本
从日志文件中提取时间测量数据，排除区块链交互和网络传输时间
"""

import re
import sys
import json
from collections import defaultdict
from pathlib import Path

# 需要排除的计时器（区块链交互和网络传输）
EXCLUDE_TIMERS = {
    "alice_blockchain_escrow_interaction",
    "alice_blockchain_interaction",
    "bob_blockchain_escrow_interaction",
    "tumbler_blockchain_escrow_interaction",
    "call_http_zk_service",
    "http_zk_service",
}

# 角色分类
ROLE_TIMERS = {
    "alice": [
        "alice_initialization_computation",
        "alice_registration_total",
        "alice_token_share_computation",
        "alice_payment_init_computation",
        "alice_puzzle_solution_share_computation",
        "alice_second_puzzle_randomization",
        "alice_zk_proof_generation",
        "alice_ecdsa_signing",
        "alice_secret_extraction",
        "bob_to_alice_zk_verification",
    ],
    "bob": [
        "bob_puzzle_randomization",
        "bob_zk_proof_generation",
        "tumbler_to_bob_zk_verification",
    ],
    "tumbler": [
        "tumbler_secret_share_phase2",
        "tumbler_zk_proof_generation",
        "tumbler_commitment_generation",
        "tumbler_verification",
    ],
}

def parse_timer_log(line):
    """解析 [TIMER] 日志行"""
    # 格式: [TIMER] name 耗时: X.XX ms
    pattern = r'\[TIMER\]\s+(\S+)\s+耗时:\s+([\d.]+)\s+ms'
    match = re.search(pattern, line)
    if match:
        name = match.group(1)
        duration = float(match.group(2))
        return name, duration
    return None, None

def parse_program_output(lines):
    """解析程序输出，提取所有计时器数据"""
    timers = {}
    
    for line in lines:
        timer_name, duration = parse_timer_log(line)
        if timer_name and timer_name not in EXCLUDE_TIMERS:
            if timer_name not in timers:
                timers[timer_name] = []
            timers[timer_name].append(duration)
    
    return timers

def categorize_timers(timers):
    """将计时器按角色分类"""
    categorized = {
        "alice": {},
        "bob": {},
        "tumbler": {},
        "other": {},
    }
    
    for timer_name, durations in timers.items():
        assigned = False
        for role, role_timers in ROLE_TIMERS.items():
            if timer_name in role_timers or timer_name.startswith(role + "_"):
                categorized[role][timer_name] = durations
                assigned = True
                break
        
        if not assigned:
            categorized["other"][timer_name] = durations
    
    return categorized

def calculate_stats(durations):
    """计算统计信息"""
    if not durations:
        return None
    
    durations = sorted(durations)
    return {
        "count": len(durations),
        "min": min(durations),
        "max": max(durations),
        "avg": sum(durations) / len(durations),
        "total": sum(durations),
    }

def print_analysis(categorized):
    """打印分析结果"""
    print("=" * 80)
    print("A2L 系统角色耗时分析（排除区块链交互和网络传输）")
    print("=" * 80)
    print()
    
    total_by_role = {}
    
    for role in ["alice", "bob", "tumbler"]:
        if not categorized[role]:
            continue
        
        print(f"[{role.upper()} 角色]")
        print("-" * 80)
        
        role_total = 0
        for timer_name, durations in sorted(categorized[role].items()):
            stats = calculate_stats(durations)
            if stats:
                role_total += stats["total"]
                print(f"  {timer_name:50s} {stats['avg']:8.2f} ms (总计: {stats['total']:8.2f} ms, {stats['count']} 次)")
        
        total_by_role[role] = role_total
        print(f"  {'总计':50s} {role_total:8.2f} ms ({role_total/1000:.3f} 秒)")
        print()
    
    # 其他计时器
    if categorized["other"]:
        print("[其他计时器]")
        print("-" * 80)
        for timer_name, durations in sorted(categorized["other"].items()):
            stats = calculate_stats(durations)
            if stats:
                print(f"  {timer_name:50s} {stats['avg']:8.2f} ms (总计: {stats['total']:8.2f} ms, {stats['count']} 次)")
        print()
    
    # 总结
    print("=" * 80)
    print("总结")
    print("-" * 80)
    grand_total = sum(total_by_role.values())
    for role, total in total_by_role.items():
        percentage = (total / grand_total * 100) if grand_total > 0 else 0
        print(f"  {role.capitalize():10s} {total:8.2f} ms ({total/1000:.3f} 秒) - {percentage:5.1f}%")
    print(f"  {'总计':10s} {grand_total:8.2f} ms ({grand_total/1000:.3f} 秒)")
    print("=" * 80)
    print()
    print("注意:")
    print("  ✅ 已排除: 区块链交互时间、HTTP 网络传输时间")
    print("  ✅ 已包括: 所有密码学计算时间、ZMQ 本地通信时间")

def main():
    if len(sys.argv) < 2:
        print("用法: python3 analyze_timing_from_logs.py <日志文件>")
        print("示例: python3 analyze_timing_from_logs.py ../logs/alice.log")
        sys.exit(1)
    
    log_file = Path(sys.argv[1])
    if not log_file.exists():
        print(f"错误: 文件不存在: {log_file}")
        sys.exit(1)
    
    print(f"正在读取日志文件: {log_file}")
    print()
    
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    
    timers = parse_program_output(lines)
    
    if not timers:
        print("警告: 未找到任何计时器数据")
        print("请确保日志文件包含 [TIMER] 开头的行")
        sys.exit(1)
    
    categorized = categorize_timers(timers)
    print_analysis(categorized)
    
    # 保存 JSON 结果
    output_file = log_file.parent / f"{log_file.stem}_timing_analysis.json"
    result = {}
    for role in ["alice", "bob", "tumbler", "other"]:
        result[role] = {}
        for timer_name, durations in categorized[role].items():
            stats = calculate_stats(durations)
            if stats:
                result[role][timer_name] = stats
    
    with open(output_file, 'w') as f:
        json.dump(result, f, indent=2)
    
    print(f"\n详细结果已保存到: {output_file}")

if __name__ == "__main__":
    main()







