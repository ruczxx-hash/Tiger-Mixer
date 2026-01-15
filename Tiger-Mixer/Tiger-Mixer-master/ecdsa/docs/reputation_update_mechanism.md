# 基于真实数据的声誉更新机制

## 概述

本机制基于委员会成员的真实活动数据（参与情况、响应质量、验证结果等）来计算和更新声誉值，替代原有的随机更新方式。

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│  数据收集层 (reputation_tracker.c)                      │
│  - 记录审计请求                                         │
│  - 记录响应结果                                         │
│  - 记录验证结果                                         │
│  - 计算统计数据                                         │
└─────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────┐
│  数据存储层 (reputation_stats_*.json)                  │
│  - 保存统计数据到文件                                   │
│  - JSON格式，便于读取                                   │
└─────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────┐
│  更新脚本层 (update_reputation_from_stats.js)            │
│  - 读取统计数据                                         │
│  - 计算声誉值                                           │
│  - 更新到区块链                                         │
└─────────────────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────────────────┐
│  智能合约层 (ReputationManager.sol)                     │
│  - 存储声誉值                                           │
│  - 用于委员会轮换                                       │
└─────────────────────────────────────────────────────────┘
```

## 数据收集点

### 1. 在 `secret_share_receiver.c` 中集成

#### 1.1 记录审计请求
```c
// 在 receiver_thread 中，收到 AUDIT_REQUEST 时
reputation_tracker_record_request(participant_id, msg_id);
```

#### 1.2 记录成功响应
```c
// 在 handle_audit_request 中，成功发送shares时
uint64_t start_time = get_time_ms();  // 记录开始时间
// ... 处理请求 ...
uint64_t response_time = get_time_ms() - start_time;
reputation_tracker_record_success(participant_id, msg_id, num_shares, response_time);
```

#### 1.3 记录失败响应
```c
// 在 handle_audit_request 中，各种错误情况
reputation_tracker_record_failure(participant_id, msg_id, "NOT_FOUND");
reputation_tracker_record_failure(participant_id, msg_id, "MEMORY_ERROR");
```

#### 1.4 记录"不需要审计"
```c
// 在 handle_audit_request 中，judge API返回0时
reputation_tracker_record_no_audit(participant_id, msg_id);
```

### 2. 在 `auditor.c` 中集成（可选，用于验证结果反馈）

#### 2.1 记录验证成功
```c
// 在 verify_share_with_stored_commitment 中，验证成功时
reputation_tracker_record_verification(participant_id, msg_id, 1);
```

#### 2.2 记录验证失败
```c
// 在 verify_share_with_stored_commitment 中，验证失败时
reputation_tracker_record_verification(participant_id, msg_id, 0);
```

## 声誉计算算法

### 1. 参与率 (Participation Rate) - 0-100

```
参与率 = (成功响应数 + 不需要审计数) / 总请求数 * 100
```

**说明**:
- 成功响应：找到并发送了shares
- 不需要审计：judge API返回0，这是正常情况，应计入参与
- 失败响应：NOT_FOUND、MEMORY_ERROR等，不计入参与

**示例**:
- 总请求: 100
- 成功响应: 85
- 不需要审计: 10
- 失败响应: 5
- **参与率 = (85 + 10) / 100 * 100 = 95%**

### 2. 准确率 (Accuracy) - 0-100

```
准确率 = shares验证成功数 / (验证成功数 + 验证失败数) * 100
```

**说明**:
- 基于auditor对提供的shares的验证结果
- 如果还没有验证数据，默认50%

**示例**:
- 验证成功: 80
- 验证失败: 5
- **准确率 = 80 / (80 + 5) * 100 = 94.1%**

### 3. 一致性 (Consistency) - 0-100

```
一致性 = 基础分(50) + 响应时间一致性(0-30) + 成功率一致性(0-20)
```

**响应时间一致性**:
- 响应时间差异 < 100ms: +30分
- 响应时间差异 < 500ms: +20分
- 响应时间差异 < 1000ms: +10分
- 其他: 0分

**成功率一致性**:
- 成功率 ≥ 95%: +20分
- 成功率 ≥ 90%: +15分
- 成功率 ≥ 80%: +10分
- 成功率 ≥ 70%: +5分
- 其他: 0分

**示例**:
- 基础分: 50
- 响应时间差异: 80ms → +30分
- 成功率: 92% → +15分
- **一致性 = 50 + 30 + 15 = 95**

### 4. 综合声誉

```
综合声誉 = 参与率 + 准确率 + 一致性
```

**范围**: 0-300（理论上），实际通常在 150-280 之间

## 使用流程

### 步骤1: 编译并集成代码

1. 将 `reputation_tracker.c` 和 `reputation_tracker.h` 添加到项目中
2. 在 `CMakeLists.txt` 中添加源文件
3. 在 `secret_share_receiver.c` 中包含头文件并调用API

### 步骤2: 初始化跟踪系统

在 `secret_share_receiver.c` 的 `main` 函数中：

```c
#include "reputation_tracker.h"

// 读取委员会成员地址
char my_address[64];
if (read_committee_member_address(participant_id, my_address, sizeof(my_address)) == 0) {
    // 初始化声誉跟踪
    reputation_tracker_init(participant_id, my_address);
}
```

### 步骤3: 在关键点记录数据

在 `handle_audit_request` 函数中添加记录调用（见上面的代码示例）

### 步骤4: 定期保存统计数据

```c
// 在程序退出时或定期（如每小时）
reputation_tracker_save_stats(participant_id);
```

### 步骤5: 运行更新脚本

```bash
# 在 Truffle 项目中
truffle exec scripts/update_reputation_from_stats.js --network development
```

## 统计数据文件格式

`reputation_stats_*.json` 文件格式：

```json
{
  "participant_id": 1,
  "address": "0x9a982767d77CC8eB537eC6c412db6a4281D1b64d",
  "total_requests": 100,
  "successful_responses": 85,
  "failed_responses": 5,
  "no_audit_needed": 10,
  "shares_verified": 80,
  "shares_failed_verify": 5,
  "participation_rate": 95,
  "accuracy": 94,
  "consistency": 95,
  "total_reputation": 284,
  "window_start_time": 1704067200,
  "last_update_time": 1704070800
}
```

## 优势

1. **真实性**: 基于实际活动数据，而非随机值
2. **公平性**: 所有成员使用相同的计算标准
3. **透明性**: 统计数据可查看，计算过程可验证
4. **动态性**: 声誉值随成员表现实时变化
5. **可追溯**: 所有统计数据都有时间戳

## 注意事项

1. **数据收集时机**: 确保在关键点正确记录数据
2. **线程安全**: 使用互斥锁保护共享数据
3. **文件I/O**: 定期保存，避免数据丢失
4. **默认值**: 当数据不足时使用合理的默认值
5. **更新频率**: 建议每小时或每次轮换前更新一次

## 未来改进

1. **滑动窗口**: 使用时间窗口（如最近24小时）而非全部历史
2. **权重衰减**: 旧数据权重逐渐降低
3. **异常检测**: 检测异常行为并调整声誉
4. **跨节点验证**: 多个节点验证同一成员的表现
5. **激励机制**: 高声誉成员获得更多奖励


