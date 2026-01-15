# 委员会轮换逻辑与声誉计算详解

## 📊 一、声誉计算机制

### 1. 声誉组成

**综合声誉 = 准确率 + 一致性**

- **准确率 (Accuracy)**: 0-100
- **一致性 (Consistency)**: 0-100
- **综合声誉**: 0-200（准确率 + 一致性）

### 2. 准确率计算

**定义**：决策正确的比例

**判断标准**：
- **合法用户 (legal)**：
  - ✅ 正确决策：Judge API返回 `0`（不需要审计）→ 实际决策 `no_audit_needed`
  - ❌ 错误决策：Judge API返回 `0`（不需要审计）→ 实际决策 `provided_shares`
  
- **非法用户 (illegal)**：
  - ✅ 正确决策：Judge API返回 `1`（需要审计）→ 实际决策 `provided_shares`
  - ❌ 错误决策：Judge API返回 `1`（需要审计）→ 实际决策 `no_audit_needed`

**计算公式**：
```c
准确率 = (正确决策数 / 总决策数) * 100
```

**数据来源**：`reputation_decisions.csv`
- 读取该成员的所有决策记录
- 统计 `is_correct` 字段（1=正确，0=错误）

**示例**：
```
成员A的决策记录：
- 决策1: is_correct=1 ✓
- 决策2: is_correct=1 ✓
- 决策3: is_correct=0 ✗
- 决策4: is_correct=1 ✓

准确率 = (3 / 4) * 100 = 75%
```

### 3. 一致性计算

**定义**：与其他成员决策一致的比例

**判断标准**：
1. 对于每个请求（`request_id`），收集所有成员的决策
2. 确定"大多数决策"（2个或以上成员相同的决策）
3. 如果该成员的决策与大多数一致，则计为一致

**计算公式**：
```c
一致性 = (与大多数一致的请求数 / 总请求数) * 100
```

**数据来源**：`reputation_decisions.csv`
- 按 `request_id` 分组
- 比较每个成员的决策与其他成员的决策

**示例**：
```
请求A的决策：
- 成员1: provided_shares
- 成员2: provided_shares
- 成员3: no_audit_needed
→ 大多数决策: provided_shares
→ 成员1和2一致 ✓，成员3不一致 ✗

请求B的决策：
- 成员1: no_audit_needed
- 成员2: no_audit_needed
- 成员3: no_audit_needed
→ 所有成员一致 ✓

成员1的一致性：
- 总请求数: 2
- 一致请求数: 2
- 一致性 = (2 / 2) * 100 = 100%
```

### 4. 声誉更新流程

```
┌─────────────────────────────────────┐
│  1. secret_share_receiver 运行      │
│     ↓ 处理审计请求                   │
│     ↓ 记录决策到 reputation_decisions.csv │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  2. calculate_reputation_stats      │
│     ↓ 读取 reputation_decisions.csv │
│     ↓ 计算每个地址的准确率和一致性   │
│     ↓ 保存到 reputation_stats.csv   │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  3. update_reputation_from_decisions│
│     ↓ 读取 reputation_stats.csv     │
│     ↓ 调用 ReputationManager.updateReputation() │
│     ↓ 更新到区块链                    │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  4. 区块链存储                        │
│     ReputationManager合约            │
│     - decisionAccuracy: 准确率       │
│     - consistency: 一致性            │
│     - calculateReputation(): 准确率+一致性 │
└─────────────────────────────────────┘
```

### 5. 代码实现位置

**准确率计算**：`reputation_tracker.c` → `calculate_accuracy_for_address()`
**一致性计算**：`reputation_tracker.c` → `calculate_consistency_for_address()`
**统计数据生成**：`reputation_tracker.c` → `reputation_tracker_calculate_and_save_stats()`
**区块链更新**：`update_reputation_from_decisions.js`

## 🔄 二、委员会轮换逻辑

### 1. 轮换触发条件

#### 时间条件
```solidity
function canRotate() public view returns (bool) {
    return block.timestamp >= lastRotationTime + ROTATION_INTERVAL;
}
```
- **ROTATION_INTERVAL = 20秒**（源代码中已设置）
- 上次轮换后至少20秒才能再次轮换

#### 其他条件
- ✅ 候选池大小 >= 3（`MAX_COMMITTEE_SIZE`）
- ✅ VRF随机数已设置
- ✅ VRF随机数未被使用

### 2. 轮换完整流程

```
┌─────────────────────────────────────────┐
│  步骤1: 更新声誉（基于真实决策数据）      │
│  - calculate_reputation_stats.sh        │
│  - update_reputation_from_decisions.js   │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  步骤2: 生成VRF随机数                     │
│  - C程序生成VRF证明                      │
│  - 提交到合约并验证                       │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  步骤3: 执行委员会轮换                    │
│  - 选择Top-K候选者                       │
│  - 计算权重                               │
│  - 加权轮盘赌选择                         │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  步骤4: 更新委员会                        │
│  - 更新合约状态                           │
│  - 写入committee_members.txt              │
└─────────────────────────────────────────┘
```

### 3. 选择算法详解（加权轮盘赌）

#### 步骤1：收集候选者声誉
```solidity
for (uint i = 0; i < candidatePool.length; i++) {
    scores[i] = reputationManager.calculateReputation(candidatePool[i]);
}
```

**声誉来源**：
- 从 `ReputationManager` 合约读取
- `calculateReputation(address)` = `decisionAccuracy + consistency`
- 范围：0-200

#### 步骤2：选择Top-K候选者
```solidity
uint256 topK = candidatePool.length < TOP_K ? candidatePool.length : TOP_K;
// TOP_K = 10
```

**逻辑**：
- 从所有候选者中选择声誉最高的前10名
- 如果候选者少于10个，选择所有候选者

#### 步骤3：计算权重
```solidity
// BASE_WEIGHT = 1000
// MAX_WEIGHT = 10000

if (maxScore == minScore) {
    // 所有分数相同，使用平均权重
    weights[i] = BASE_WEIGHT;
} else {
    // 归一化权重
    weights[i] = BASE_WEIGHT + ((scores[i] - minScore) * (MAX_WEIGHT - BASE_WEIGHT)) / (maxScore - minScore);
}
```

**权重计算示例**：
```
候选者分数: [200, 180, 160, 140, 120]
最小分数: 120, 最大分数: 200

权重计算:
- 候选者1 (200): 1000 + ((200-120) * 9000) / 80 = 10000 (最高)
- 候选者2 (180): 1000 + ((180-120) * 9000) / 80 = 7750
- 候选者3 (160): 1000 + ((160-120) * 9000) / 80 = 5500
- 候选者4 (140): 1000 + ((140-120) * 9000) / 80 = 3250
- 候选者5 (120): 1000 + ((120-120) * 9000) / 80 = 1000 (最低)
```

#### 步骤4：加权轮盘赌选择（无放回）
```solidity
// 构建累积权重
cumulativeWeights[0] = weights[0];
for (uint i = 1; i < topK; i++) {
    cumulativeWeights[i] = cumulativeWeights[i-1] + weights[i];
}

// 使用VRF随机数进行选择
for (uint selectedCount = 0; selectedCount < MAX_COMMITTEE_SIZE; selectedCount++) {
    // 从VRF随机数中提取一个值
    uint256 randomValue = ((uint256(random) >> (selectedCount * 32)) & 0xFFFFFFFF) % totalWeight;
    
    // 在累积权重中找到对应的候选者
    // 选择后，从候选池中移除（无放回）
}
```

**选择过程示例**：
```
累积权重: [1000, 8750, 14250, 17500, 18500]
总权重: 18500

第1次选择（randomValue = 5000）:
- 5000 < 8750 → 选择候选者2
- 移除候选者2，更新权重

第2次选择（randomValue = 12000）:
- 从剩余候选者中选择
- 12000 < 14250 → 选择候选者3

第3次选择（randomValue = 15000）:
- 从剩余候选者中选择
- 15000 < 17500 → 选择候选者4

最终委员会: [候选者2, 候选者3, 候选者4]
```

### 4. 轮换特点

1. **基于声誉**：高分候选者被选中的概率更高
2. **保证公平**：低分候选者也有机会（权重不为0）
3. **不可预测**：使用VRF随机数，无法提前预测结果
4. **无放回选择**：已选中的成员不会再次被选中
5. **自动更新**：轮换后自动更新 `committee_members.txt`

## 📋 三、完整数据流

```
┌─────────────────────────────────────┐
│  secret_share_receiver               │
│  ↓ 处理审计请求                      │
│  ↓ 调用judge API                     │
│  ↓ 记录决策到 reputation_decisions.csv │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  reputation_decisions.csv           │
│  - timestamp                         │
│  - request_id                        │
│  - user_address, user_label          │
│  - participant_address               │
│  - judge_api_result                  │
│  - actual_decision                   │
│  - is_correct                        │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  calculate_reputation_stats         │
│  ↓ 读取决策记录                      │
│  ↓ 计算准确率（基于is_correct）      │
│  ↓ 计算一致性（基于request_id分组）  │
│  ↓ 保存到 reputation_stats.csv      │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  reputation_stats.csv               │
│  - address                           │
│  - total_decisions                   │
│  - correct_decisions                 │
│  - accuracy (0-100)                  │
│  - consistency (0-100)               │
│  - total_reputation (accuracy+consistency) │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  update_reputation_from_decisions.js │
│  ↓ 读取统计数据                      │
│  ↓ 调用 ReputationManager.updateReputation() │
│  ↓ 更新到区块链                      │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  ReputationManager合约               │
│  - decisionAccuracy: 准确率          │
│  - consistency: 一致性               │
│  - calculateReputation(): 准确率+一致性 │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  CommitteeRotation.rotateCommittee()  │
│  ↓ 读取所有候选者的声誉值             │
│  ↓ 选择Top-10候选者                   │
│  ↓ 计算权重                           │
│  ↓ 加权轮盘赌选择3个成员              │
│  ↓ 更新委员会                         │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  committee_members.txt               │
│  - 新委员会成员地址（每行一个）       │
└─────────────────────────────────────┘
```

## 🎯 四、关键点总结

### 声誉计算
- ✅ **准确率**：基于决策正确性（与address_labels.csv比较）
- ✅ **一致性**：基于与其他成员决策的一致性
- ✅ **综合声誉**：准确率 + 一致性（0-200）
- ✅ **实时更新**：每次轮换前自动计算和更新

### 委员会轮换
- ✅ **时间控制**：每20秒可以轮换一次
- ✅ **基于声誉**：使用综合声誉值进行选择
- ✅ **加权选择**：高分候选者概率更高，但保证公平
- ✅ **VRF随机性**：使用可验证随机数确保不可预测
- ✅ **自动更新**：轮换后自动更新committee_members.txt

### 数据文件
- **reputation_decisions.csv**：原始决策记录
- **reputation_stats.csv**：统计数据（准确率、一致性）
- **committee_members.txt**：当前委员会成员地址

## 📝 五、实际运行示例

### 场景：一次完整的轮换

1. **决策记录**（持续进行）：
   ```
   reputation_decisions.csv:
   1763620347,0x491e...,0x0025...,illegal,0x9a98...,1,no_audit_needed,0
   1763620387,0x491e...,0x0025...,illegal,0x9a98...,1,no_audit_needed,0
   ```

2. **计算统计数据**（轮换前）：
   ```bash
   ./calculate_reputation_stats.sh
   ```
   输出：
   ```
   地址 0x9a98...: 准确率=75%, 一致性=90%, 综合声誉=165
   地址 0x0048...: 准确率=80%, 一致性=85%, 综合声誉=165
   地址 0x80bc...: 准确率=90%, 一致性=95%, 综合声誉=185
   ```

3. **更新到区块链**（轮换前）：
   ```bash
   truffle exec scripts/update_reputation_from_decisions.js --network private
   ```

4. **执行轮换**：
   ```solidity
   rotateCommittee() {
       // 1. 收集所有候选者声誉
       // 2. 选择Top-10
       // 3. 计算权重
       // 4. 加权轮盘赌选择3个成员
   }
   ```

5. **更新委员会文件**：
   ```
   committee_members.txt:
   0x9483bA82278fD651ED64D5B2CC2d4d2BBFa94025
   0x9339c1E45F56ecF3af4EE2D976f31a12086Ad506
   0x27766bFd44aFDc46584E0550765181422c3BA080
   ```

## 🔍 六、常见问题

### Q1: 为什么轮换时间未到？
**A**: 需要等待 `ROTATION_INTERVAL` 秒（当前设置为20秒）
- 检查：`lastRotationTime + ROTATION_INTERVAL > 当前时间`
- 如果显示需要等待238秒，可能是合约中仍然是60秒，需要重新部署

### Q2: 声誉值如何更新？
**A**: 自动更新流程：
1. `secret_share_receiver` 记录决策
2. `calculate_reputation_stats` 计算统计数据
3. `update_reputation_from_decisions.js` 更新到区块链
4. 每次轮换前自动执行

### Q3: 为什么committee_members.txt没有更新？
**A**: 可能的原因：
1. 轮换未实际执行（时间未到或其他条件不满足）
2. 文件写入失败（检查权限和路径）
3. 合约地址错误（检查update_and_rotate_new.js中的地址）

### Q4: 如何设置轮换间隔为20秒？
**A**: 
1. 源代码已设置为20秒
2. 需要重新编译：`truffle compile`
3. 需要重新部署：`truffle migrate --reset --network private`
4. 更新脚本中的合约地址（如果地址改变）



