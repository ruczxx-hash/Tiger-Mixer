#!/bin/bash

# A2L 性能测试脚本
# 运行 30 次测试并计算平均值

# 不设置 set -e，因为某些命令可能失败但不影响整体流程

# 配置
NUM_RUNS=1
BIN_DIR="/home/zxx/A2L-origin/ecdsa/bin"
RESULTS_DIR="/home/zxx/A2L-origin/ecdsa/bin/benchmark_results"
TUMBLER_ENDPOINT="tcp://localhost:8181"
ALICE_ENDPOINT="tcp://*:8182"
BOB_ENDPOINT="tcp://localhost:8183"

# 创建结果目录
mkdir -p "$RESULTS_DIR"

# 清理函数
cleanup() {
    echo "清理进程..."
    pkill -f "tumbler" || true
    pkill -f "alice" || true
    pkill -f "bob" || true
    sleep 1
}

# 信号处理
trap cleanup EXIT INT TERM

echo "=========================================="
echo "A2L 性能测试 - 运行 $NUM_RUNS 次"
echo "=========================================="
echo ""

# 初始化结果文件
ALICE_AVG_FILE="$RESULTS_DIR/alice_average_results.csv"
BOB_AVG_FILE="$RESULTS_DIR/bob_average_results.csv"
TUMBLER_AVG_FILE="$RESULTS_DIR/tumbler_average_results.csv"

# 用于存储所有运行的结果
declare -A alice_times
declare -A bob_times
declare -A tumbler_times
declare -A alice_counts
declare -A bob_counts
declare -A tumbler_counts

# 运行测试
for run in $(seq 1 $NUM_RUNS); do
    echo "----------------------------------------"
    echo "运行第 $run/$NUM_RUNS 次测试..."
    echo "----------------------------------------"
    
    # 清理之前的进程
    cleanup
    
    # 清理旧的 CSV 文件
    rm -f "$BIN_DIR/alice_timing_results.csv"
    rm -f "$BIN_DIR/bob_timing_results.csv"
    rm -f "$BIN_DIR/tumbler_timing_results.csv"
    
    # 启动 Tumbler（后台运行）
    echo "启动 Tumbler..."
    cd "$BIN_DIR"
    ./tumbler > /tmp/tumbler_${run}.log 2>&1 &
    TUMBLER_PID=$!
    sleep 3  # 等待 Tumbler 启动
    
    # 检查 Tumbler 是否成功启动
    if ! kill -0 $TUMBLER_PID 2>/dev/null; then
        echo "错误: Tumbler 启动失败"
        continue
    fi
    
    # 运行 Alice（后台运行）
    echo "启动 Alice..."
    ./alice > /tmp/alice_${run}.log 2>&1 &
    ALICE_PID=$!
    
    # 运行 Bob
    echo "运行 Bob..."
    ./bob > /tmp/bob_${run}.log 2>&1 &
    BOB_PID=$!
    
    # 等待所有进程完成（最多等待 120 秒）
    echo "等待协议完成..."
    timeout=120
    elapsed=0
    while [ $elapsed -lt $timeout ]; do
        alice_running=$(kill -0 $ALICE_PID 2>/dev/null && echo "1" || echo "0")
        bob_running=$(kill -0 $BOB_PID 2>/dev/null && echo "1" || echo "0")
        if [ "$alice_running" = "0" ] && [ "$bob_running" = "0" ]; then
            break
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    
    if [ $elapsed -ge $timeout ]; then
        echo "警告: 测试超时，强制停止进程"
        kill $ALICE_PID 2>/dev/null || true
        kill $BOB_PID 2>/dev/null || true
    fi
    
    # 等待 Tumbler 处理完最后一个消息
    sleep 3
    
    # 停止 Tumbler
    kill $TUMBLER_PID 2>/dev/null || true
    wait $TUMBLER_PID 2>/dev/null || true
    
    # 等待进程完全退出
    sleep 2
    
    # 收集结果
    if [ -f "$BIN_DIR/alice_timing_results.csv" ]; then
        echo "收集 Alice 结果..."
        while IFS=',' read -r operation time || [ -n "$operation" ]; do
            # 跳过表头和空行
            if [ "$operation" = "操作" ] || [ -z "$operation" ]; then
                continue
            fi
            # 移除可能的空格和换行符
            operation=$(echo "$operation" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            time=$(echo "$time" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            # 检查是否为有效数字
            if [[ "$time" =~ ^[0-9]+\.?[0-9]*$ ]]; then
                if [ -z "${alice_times[$operation]}" ]; then
                    alice_times["$operation"]=0
                    alice_counts["$operation"]=0
                fi
                alice_times["$operation"]=$(echo "${alice_times[$operation]} + $time" | bc -l)
                alice_counts["$operation"]=$((${alice_counts[$operation]} + 1))
            fi
        done < "$BIN_DIR/alice_timing_results.csv"
    fi
    
    if [ -f "$BIN_DIR/bob_timing_results.csv" ]; then
        echo "收集 Bob 结果..."
        while IFS=',' read -r operation time || [ -n "$operation" ]; do
            if [ "$operation" = "操作" ] || [ -z "$operation" ]; then
                continue
            fi
            operation=$(echo "$operation" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            time=$(echo "$time" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            if [[ "$time" =~ ^[0-9]+\.?[0-9]*$ ]]; then
                if [ -z "${bob_times[$operation]}" ]; then
                    bob_times["$operation"]=0
                    bob_counts["$operation"]=0
                fi
                bob_times["$operation"]=$(echo "${bob_times[$operation]} + $time" | bc -l)
                bob_counts["$operation"]=$((${bob_counts[$operation]} + 1))
            fi
        done < "$BIN_DIR/bob_timing_results.csv"
    fi
    
    if [ -f "$BIN_DIR/tumbler_timing_results.csv" ]; then
        echo "收集 Tumbler 结果..."
        while IFS=',' read -r operation time || [ -n "$operation" ]; do
            if [ "$operation" = "操作" ] || [ -z "$operation" ]; then
                continue
            fi
            operation=$(echo "$operation" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            time=$(echo "$time" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
            if [[ "$time" =~ ^[0-9]+\.?[0-9]*$ ]]; then
                if [ -z "${tumbler_times[$operation]}" ]; then
                    tumbler_times["$operation"]=0
                    tumbler_counts["$operation"]=0
                fi
                tumbler_times["$operation"]=$(echo "${tumbler_times[$operation]} + $time" | bc -l)
                tumbler_counts["$operation"]=$((${tumbler_counts[$operation]} + 1))
            fi
        done < "$BIN_DIR/tumbler_timing_results.csv"
    fi
    
    echo "第 $run 次测试完成"
    echo ""
done

# 计算平均值并输出
echo "=========================================="
echo "计算平均值..."
echo "=========================================="

# Alice 平均值
echo "操作,时间(ms)" > "$ALICE_AVG_FILE"
for operation in "${!alice_times[@]}"; do
    if [ ${alice_counts[$operation]} -gt 0 ]; then
        avg=$(echo "scale=2; ${alice_times[$operation]} / ${alice_counts[$operation]}" | bc -l)
        printf "%s,%.2f\n" "$operation" "$avg" >> "$ALICE_AVG_FILE"
        echo "Alice - $operation: $avg ms (${alice_counts[$operation]} 次)"
    fi
done

# Bob 平均值
echo "操作,时间(ms)" > "$BOB_AVG_FILE"
for operation in "${!bob_times[@]}"; do
    if [ ${bob_counts[$operation]} -gt 0 ]; then
        avg=$(echo "scale=2; ${bob_times[$operation]} / ${bob_counts[$operation]}" | bc -l)
        printf "%s,%.2f\n" "$operation" "$avg" >> "$BOB_AVG_FILE"
        echo "Bob - $operation: $avg ms (${bob_counts[$operation]} 次)"
    fi
done

# Tumbler 平均值
echo "操作,时间(ms)" > "$TUMBLER_AVG_FILE"
for operation in "${!tumbler_times[@]}"; do
    if [ ${tumbler_counts[$operation]} -gt 0 ]; then
        avg=$(echo "scale=2; ${tumbler_times[$operation]} / ${tumbler_counts[$operation]}" | bc -l)
        printf "%s,%.2f\n" "$operation" "$avg" >> "$TUMBLER_AVG_FILE"
        echo "Tumbler - $operation: $avg ms (${tumbler_counts[$operation]} 次)"
    fi
done

echo ""
echo "=========================================="
echo "测试完成！"
echo "=========================================="
echo "结果文件："
echo "  - Alice:   $ALICE_AVG_FILE"
echo "  - Bob:     $BOB_AVG_FILE"
echo "  - Tumbler: $TUMBLER_AVG_FILE"
echo ""

# 最终清理
cleanup

