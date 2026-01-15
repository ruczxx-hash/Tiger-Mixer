# reputation_decisions.csv 文件使用位置详解

## 📁 文件定义

**文件路径**：`/home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv`

**定义位置**：
```c
// reputation_tracker.c (第17行)
#define DECISIONS_FILE "/home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv"
```

## 📝 文件格式

```csv
timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct
```

**字段说明**：
- `timestamp`: 决策时间戳
- `request_id`: 请求唯一标识（msg_id + 时间戳）
- `user_address`: 被审计的用户地址
- `user_label`: 用户标签（legal/illegal，从address_labels.csv查找）
- `participant_address`: 委员会成员地址
- `judge_api_result`: Judge API的原始返回值（0=不需要审计，1=需要审计）
- `actual_decision`: 实际决策（"no_audit_needed" 或 "provided_shares" 或 "not_found"）
- `is_correct`: 决策是否正确（1=正确，0=错误）

## 🔍 使用位置

### 1. **写入位置**（记录决策）

#### 位置1：`reputation_tracker.c` → `reputation_tracker_record_decision()`
```c
// 第229行：打开文件（追加模式）
FILE* fp = fopen(DECISIONS_FILE, "a");

// 第237-239行：写入决策记录
fprintf(fp, "%ld,%s,%s,%s,%s,%d,%s,%d\n",
        timestamp, request_id, user_address, user_label,
        participant_address, judge_result, actual_decision, is_correct);
```

**调用链**：
```
secret_share_receiver.c (处理审计请求)
  ↓
handle_audit_request() (第617行、第795行、第811行)
  ↓
reputation_tracker_record_decision() (记录决策)
  ↓
写入 reputation_decisions.csv
```

**具体调用位置**：
- `secret_share_receiver.c:617` - 当judge API返回0（不需要审计）时
- `secret_share_receiver.c:795` - 当找到并发送shares时
- `secret_share_receiver.c:811` - 当未找到shares时

#### 位置2：`reputation_tracker.c` → `reputation_tracker_init()`
```c
// 第51-60行：初始化时检查文件是否存在，不存在则创建
FILE* test_fp = fopen(DECISIONS_FILE, "r");
if (!test_fp) {
    FILE* new_fp = fopen(DECISIONS_FILE, "w");
    if (new_fp) {
        fprintf(new_fp, "timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct\n");
        fclose(new_fp);
    }
}
```

### 2. **读取位置**（计算声誉）

#### 位置1：`reputation_tracker.c` → `calculate_accuracy_for_address()`
```c
// 第254行：打开文件（读取模式）
FILE* fp = fopen(DECISIONS_FILE, "r");

// 第269-308行：读取所有决策记录，统计该地址的准确率
while (fgets(line, sizeof(line), fp)) {
    // 解析CSV行
    // 统计总决策数和正确决策数
    // 计算准确率 = (正确决策数 / 总决策数) * 100
}
```

**用途**：计算指定委员会成员地址的决策准确率

#### 位置2：`reputation_tracker.c` → `calculate_consistency_for_address()`
```c
// 第323行：打开文件（读取模式）
FILE* fp = fopen(DECISIONS_FILE, "r");

// 第350-450行：读取所有决策记录，按request_id分组
// 计算该地址与其他成员决策的一致性
while (fgets(line, sizeof(line), fp)) {
    // 解析CSV行
    // 按request_id分组
    // 比较该成员的决策与大多数成员的决策
    // 计算一致性 = (与大多数一致的请求数 / 总请求数) * 100
}
```

**用途**：计算指定委员会成员地址的决策一致性

#### 位置3：`reputation_tracker.c` → `reputation_tracker_calculate_and_save_stats()`
```c
// 第504行：打开文件（读取模式）
FILE* decisions_fp = fopen(DECISIONS_FILE, "r");

// 第508-600行：读取所有决策记录
// 1. 收集所有唯一的委员会成员地址
// 2. 对每个地址计算准确率和一致性
// 3. 保存到 reputation_stats.csv
while (fgets(line, sizeof(line), decisions_fp)) {
    // 解析CSV行
    // 提取participant_address
    // 收集唯一地址
    // 计算每个地址的统计数据
}
```

**用途**：批量计算所有委员会成员的声誉统计数据

**调用位置**：
- `calculate_reputation_stats.c` → `main()` → `reputation_tracker_calculate_and_save_stats()`
- `calculate_reputation_stats.sh` → 执行 `calculate_reputation_stats` 程序

## 📊 数据流

```
┌─────────────────────────────────────┐
│  secret_share_receiver.c            │
│  ↓ 处理审计请求                     │
│  ↓ handle_audit_request()           │
│  ↓ 调用judge API                    │
│  ↓ 决定是否提供shares               │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  reputation_tracker_record_decision()│
│  ↓ 记录决策                          │
│  ↓ 写入 reputation_decisions.csv    │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  reputation_decisions.csv             │
│  (存储所有决策记录)                  │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  calculate_reputation_stats          │
│  ↓ 读取 reputation_decisions.csv    │
│  ↓ calculate_accuracy_for_address()  │
│  ↓ calculate_consistency_for_address()│
│  ↓ 计算统计数据                     │
│  ↓ 保存到 reputation_stats.csv     │
└─────────────────────────────────────┘
```

## 🔗 相关文件

### 输入文件
- `address_labels.csv` - 用于查找用户标签（legal/illegal）
- `committee_members.txt` - 用于验证委员会成员地址

### 输出文件
- `reputation_stats.csv` - 基于 `reputation_decisions.csv` 计算生成的统计数据

### 相关函数
- `reputation_tracker_init()` - 初始化，创建文件（如果不存在）
- `reputation_tracker_record_decision()` - 记录决策
- `calculate_accuracy_for_address()` - 计算准确率
- `calculate_consistency_for_address()` - 计算一致性
- `reputation_tracker_calculate_and_save_stats()` - 批量计算统计数据

## 📋 代码位置总结

| 操作 | 文件 | 函数/位置 | 行号 |
|------|------|-----------|------|
| **定义** | `reputation_tracker.c` | `#define DECISIONS_FILE` | 17 |
| **创建** | `reputation_tracker.c` | `reputation_tracker_init()` | 51-60 |
| **写入** | `reputation_tracker.c` | `reputation_tracker_record_decision()` | 229-240 |
| **调用写入** | `secret_share_receiver.c` | `handle_audit_request()` | 617, 795, 811 |
| **读取（准确率）** | `reputation_tracker.c` | `calculate_accuracy_for_address()` | 254-317 |
| **读取（一致性）** | `reputation_tracker.c` | `calculate_consistency_for_address()` | 323-450 |
| **读取（批量统计）** | `reputation_tracker.c` | `reputation_tracker_calculate_and_save_stats()` | 504-641 |

## 🎯 关键点

1. **写入时机**：每次 `secret_share_receiver` 处理审计请求时，都会调用 `reputation_tracker_record_decision()` 记录决策

2. **读取时机**：
   - 计算单个地址的准确率/一致性时（按需）
   - 批量计算所有地址的统计数据时（轮换前）

3. **线程安全**：使用 `pthread_mutex` 保护文件写入操作

4. **文件格式**：CSV格式，第一行为表头，后续行为数据记录

5. **数据来源**：
   - `user_address` 和 `user_label` 来自 `address_labels.csv`
   - `participant_address` 来自 `secret_share_receiver` 的当前地址
   - `judge_api_result` 来自 Judge API的返回值
   - `actual_decision` 来自 `secret_share_receiver` 的实际行为
   - `is_correct` 通过比较 `user_label`、`judge_api_result` 和 `actual_decision` 计算得出


