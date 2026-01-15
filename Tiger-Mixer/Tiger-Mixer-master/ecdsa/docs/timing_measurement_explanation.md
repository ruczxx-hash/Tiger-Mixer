# A2L 系统角色耗时测量说明

## 概述

A2L 系统中，alice、bob、tumbler 三个角色都实现了时间测量功能，用于分析各个步骤的耗时。所有测量都**排除了区块链交互时间**，只测量密码学计算和本地处理时间。

---

## 测量机制

### 1. 计时器系统

所有角色都使用统一的计时器系统：

- **宏定义**: `START_TIMER(name)` 和 `END_TIMER(name)`
- **实现位置**: `include/util.h` 和 `src/util.c`
- **测量精度**: 毫秒（ms）
- **存储方式**: 全局数组 `timing_records[50]`

```c
#define START_TIMER(name) \
    struct timeval start_##name; \
    gettimeofday(&start_##name, NULL);

#define END_TIMER(name) \
    struct timeval end_##name; \
    gettimeofday(&end_##name, NULL); \
    double duration_##name = (end_##name.tv_sec - start_##name.tv_sec) * 1000.0 + \
                            (end_##name.tv_usec - start_##name.tv_usec) / 1000.0; \
    record_timing(#name, duration_##name);
```

### 2. 获取计时器值

使用 `get_timer_value(timer_name)` 函数获取已记录的计时器值：

```c
double get_timer_value(const char* timer_name);
```

---

## Alice 的耗时测量

### 测量方式

1. **总计时器**: `alice_total_computation_time`
   - 包裹整个 Alice 流程
   - 从初始化到完成所有计算

2. **子计时器**:
   - `alice_initialization_computation` - 初始化计算
   - `alice_registration_total` - 注册阶段
   - `alice_token_share_computation` - Token 分享计算
   - `alice_payment_init_computation` - 支付初始化计算
   - `alice_puzzle_solution_share_computation` - 谜题解决方案分享计算
   - `alice_second_puzzle_randomization` - 第二次谜题随机化
   - `alice_zk_proof_generation` - zk 证明生成
   - `alice_ecdsa_signing` - ECDSA 签名
   - `alice_secret_extraction` - 秘密提取
   - `bob_to_alice_zk_verification` - Bob 到 Alice 的 zk 验证

3. **区块链交互计时器**:
   - `alice_blockchain_escrow_interaction` - 区块链交互（**排除**）

### 输出位置

在 `src/alice.c` 的 `main()` 函数末尾（约 1693-1707 行）：

```c
printf("\n=== Alice 时间测量总结 ===\n");
printf("Alice 总计算时间: %.5f 秒\n", get_timer_value("alice_total_computation_time") / 1000.0);
printf("Alice 初始化计算时间: %.5f 秒\n", get_timer_value("alice_initialization_computation") / 1000.0);
// ... 其他子计时器
printf("Alice 区块链交互时间: %.5f 秒\n", get_timer_value("alice_blockchain_escrow_interaction") / 1000.0);

// 计算纯计算时间（排除区块链交互）
double pure_computation_time = (get_timer_value("alice_total_computation_time") - 
                                get_timer_value("alice_blockchain_escrow_interaction")) / 1000.0;
printf("Alice 纯计算时间（排除区块链交互）: %.5f 秒\n", pure_computation_time);
```

### 纯计算时间计算

```
纯计算时间 = 总计算时间 - 区块链交互时间
```

---

## Bob 的耗时测量

### 测量方式

1. **总计时器**: `bob_total_computation_time`
   - 包裹整个 Bob 流程

2. **子计时器**:
   - `bob_initialization_computation` - 初始化计算
   - `bob_promise_init_computation` - Promise 初始化计算
   - `bob_puzzle_share_computation` - Puzzle 分享计算
   - `bob_puzzle_randomization` - 谜题随机化
   - `bob_zk_proof_generation` - zk 证明生成
   - `bob_extract_secret_alpha` - 提取秘密 alpha
   - `tumbler_to_bob_zk_verification` - Tumbler 到 Bob 的 zk 验证

3. **区块链交互计时器**:
   - `bob_blockchain_escrow_interaction` - 区块链交互（**排除**）

### 输出位置

在 `src/bob.c` 的 `main()` 函数末尾（约 2255-2265 行）：

```c
printf("\n=== Bob 时间测量总结 ===\n");
printf("Bob 总计算时间: %.5f 秒\n", get_timer_value("bob_total_computation_time") / 1000.0);
printf("Bob 初始化计算时间: %.5f 秒\n", get_timer_value("bob_initialization_computation") / 1000.0);
// ... 其他子计时器
printf("Bob 区块链交互时间: %.5f 秒\n", get_timer_value("bob_blockchain_escrow_interaction") / 1000.0);

// 计算纯计算时间（排除区块链交互）
double pure_computation_time = (get_timer_value("bob_total_computation_time") - 
                                get_timer_value("bob_blockchain_escrow_interaction")) / 1000.0;
printf("Bob 纯计算时间（排除区块链交互）: %.5f 秒\n", pure_computation_time);
```

---

## Tumbler 的耗时测量

### 测量方式

1. **总计时器**: 使用全局变量 `tumbler_start_time`
   - 在 Tumbler 启动时初始化（使用 `clock_gettime(CLOCK_MONOTONIC)`）
   - 在处理完一个完整流程后计算总时间

2. **子计时器**:
   - `tumbler_initialization_computation` - 初始化计算
   - `registration_phase` - 注册阶段
   - `tumbler_puzzle_generation` - 谜题生成
   - `tumbler_zk_proof_generation` - zk 证明生成
   - `tumbler_layered_proof_handler` - 分层证明处理
   - `tumbler_zk_verification` - zk 证明验证
   - `tumbler_secret_share_phase2` - 秘密分享阶段2

3. **区块链交互计时器**:
   - `tumbler_blockchain_escrow_interaction` - 区块链交互（**排除**）

### 输出位置

在 `src/tumbler.c` 的 `bob_confirm_done_handler()` 函数中（约 550-575 行）：

```c
// 计算总时间（从Tumbler启动到处理完一个完整流程）
struct timespec tumbler_end_time;
clock_gettime(CLOCK_MONOTONIC, &tumbler_end_time);
double total_time_ms = (tumbler_end_time.tv_sec - tumbler_start_time.tv_sec) * 1000.0 + 
                      (tumbler_end_time.tv_nsec - tumbler_start_time.tv_nsec) / 1000000.0;

printf("\n=== Tumbler 时间测量总结 ===\n");
printf("Tumbler 总计算时间: %.5f 秒\n", total_time_ms / 1000.0);
// ... 各个子计时器
printf("Tumbler 区块链交互时间: %.5f 秒\n", get_timer_value("tumbler_blockchain_escrow_interaction") / 1000.0);

// 计算纯计算时间（排除区块链交互）
double pure_computation_time = (total_time_ms - 
                                get_timer_value("tumbler_blockchain_escrow_interaction")) / 1000.0;
printf("Tumbler 纯计算时间（排除区块链交互）: %.5f 秒\n", pure_computation_time);
```

---

## 排除的时间

### 已排除的内容

1. **区块链交互时间**:
   - `alice_blockchain_escrow_interaction`
   - `bob_blockchain_escrow_interaction`
   - `tumbler_blockchain_escrow_interaction`
   - 这些计时器测量的是调用 `truffle exec` 脚本的时间（包括网络延迟、交易确认等待等）

2. **网络传输时间**:
   - HTTP curl 调用（如 `call_http_zk_service`）
   - 这些不在计时器系统中，但如果需要可以添加排除

### 已包括的内容

1. **ZMQ 本地进程间通信**: 
   - ZMQ 消息发送和接收（这是本地通信，不是网络传输）
   - 包括在计算时间内

2. **所有密码学计算**:
   - zk 证明生成和验证
   - 随机化操作
   - ECDSA 签名
   - 秘密提取和分享
   - 等等

---

## 使用建议

### 1. 运行系统并查看输出

运行 A2L 系统后，每个角色都会在程序结束时输出时间测量总结：

```bash
# 运行 Alice
./bin/alice
# 输出: === Alice 时间测量总结 ===

# 运行 Bob
./bin/bob
# 输出: === Bob 时间测量总结 ===

# 运行 Tumbler
./bin/tumbler
# 输出: === Tumbler 时间测量总结 ===
```

### 2. 使用分析脚本

可以使用提供的 Python 脚本分析日志文件：

```bash
python3 scripts/analyze_timing_from_logs.py logs/alice.log
```

### 3. 查看 Excel 输出

Tumbler 会自动输出 Excel 文件（CSV 格式）：
- 文件名: `tumbler_timing_results.csv`
- 位置: 运行 Tumbler 的当前目录

---

## 注意事项

1. **时间单位**: 
   - 计时器内部使用毫秒（ms）
   - 输出时转换为秒（除以 1000）

2. **计时器数量限制**: 
   - 全局数组最多存储 50 个计时器
   - 如果超过限制，后续计时器不会被记录

3. **网络传输时间**:
   - 目前 HTTP curl 调用（如 `call_http_zk_service`）没有被单独计时
   - 如果这些调用在某个计时器内部，会被包含在该计时器中
   - 如果需要排除，需要单独添加计时器

4. **ZMQ 通信**:
   - ZMQ 是本地进程间通信，不是网络传输
   - 这些时间被包含在计算时间内，这是合理的

---

## 总结

- ✅ **Alice**: 使用 `START_TIMER`/`END_TIMER` 宏，有总计时器和多个子计时器，自动排除区块链交互时间
- ✅ **Bob**: 使用 `START_TIMER`/`END_TIMER` 宏，有总计时器和多个子计时器，自动排除区块链交互时间
- ✅ **Tumbler**: 使用全局时间变量 + `START_TIMER`/`END_TIMER` 宏，有总计时器和多个子计时器，自动排除区块链交互时间

所有角色的**纯计算时间**都已经排除了区块链交互时间，可以直接使用。







