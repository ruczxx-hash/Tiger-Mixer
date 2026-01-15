# 委员会轮换逻辑与 reputation_decisions 作用详解

## 一、reputation_decisions.csv 的作用

### 1. 数据记录表

`reputation_decisions.csv` 是**决策记录表**，记录每个委员会成员在每次审计请求中的决策情况。

#### 文件格式
```csv
timestamp,request_id,user_address,user_label,participant_id,judge_api_result,actual_decision,is_correct
1763620347,0x491e930303f08a8be1d0a042bd61767d06f06bbf29164efd900c1d03c3e44d6d_1763620341,0x00250255a1fd94ccf33ff1da362e5059a621fdcb,illegal,1,1,no_audit_needed,0
```

#### 字段说明
- **timestamp**: 决策时间戳
- **request_id**: 请求唯一标识（msg_id + 时间戳）
- **user_address**: 被审计的用户地址
- **user_label**: 用户标签（legal/illegal，从address_labels.csv查找）
- **participant_id**: 委员会成员ID（1-3）
- **judge_api_result**: Judge API的原始返回值（0=不需要审计，1=需要审计）
- **actual_decision**: 实际决策（"no_audit_needed" 或 "provided_shares" 或 "not_found"）
- **is_correct**: 决策是否正确（1=正确，0=错误）

### 2. 核心作用

#### 作用1：计算准确率
```c
// 从 reputation_decisions.csv 读取所有决策
// 对于每个成员，统计：
// - 总决策数（total）
// - 正确决策数（correct）
// 准确率 = (correct / total) * 100
```

**示例**：
- 成员1做了100次决策
- 其中95次正确
- **准确率 = 95%**

#### 作用2：计算一致性
```c
// 对于每个请求（request_id），比较3个成员的决策
// 如果2个或以上成员做了相同决策，则该决策为"大多数决策"
// 一致性 = (与大多数一致的请求数 / 总请求数) * 100
```

**示例**：
- 请求A：成员1和2都返回"provided_shares"，成员3返回"no_audit_needed"
  - 大多数决策：provided_shares
  - 成员1和2与大多数一致 ✓
  - 成员3与大多数不一致 ✗
- 请求B：所有成员都返回"no_audit_needed"
  - 所有成员都与大多数一致 ✓

#### 作用3：生成统计数据
通过 `calculate_reputation_stats` 程序：
1. 读取 `reputation_decisions.csv`
2. 计算每个成员的准确率和一致性
3. 保存到 `reputation_stats.csv`

### 3. 数据流

```
secret_share_receiver 处理审计请求
    ↓
记录决策到 reputation_decisions.csv
    ↓
calculate_reputation_stats 读取决策记录
    ↓
计算准确率和一致性
    ↓
保存到 reputation_stats.csv
    ↓
update_reputation_from_decisions.js 读取统计数据
    ↓
更新到区块链（ReputationManager合约）
```

## 二、委员会轮换逻辑

### 1. 轮换触发条件

#### 时间条件
```solidity
function canRotate() public view returns (bool) {
    return block.timestamp >= lastRotationTime + ROTATION_INTERVAL;
}
```
- **ROTATION_INTERVAL = 60秒**
- 上次轮换后至少60秒才能再次轮换

#### 其他条件
- ✅ 候选池大小 >= 3（MAX_COMMITTEE_SIZE）
- ✅ VRF随机数已设置并验证
- ✅ VRF随机数未被使用

### 2. 轮换流程

```
┌─────────────────────────────────────────┐
│  步骤1: 更新声誉（基于真实决策数据）      │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  步骤2: 生成VRF随机数                     │
│  - 使用C程序生成VRF证明                   │
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

### 3. 选择算法（加权轮盘赌）

#### 步骤1：收集候选者声誉
```solidity
for (uint i = 0; i < candidatePool.length; i++) {
    scores[i] = reputationManager.calculateReputation(candidatePool[i]);
}
```

**声誉来源**：
- 从 `ReputationManager` 合约读取
- 综合声誉 = 准确率 + 一致性（已更新为基于真实决策数据）

#### 步骤2：选择Top-K候选者
```solidity
// TOP_K = 10
// 选择声誉最高的前10名候选者
uint256 topK = candidatePool.length < TOP_K ? candidatePool.length : TOP_K;
```

#### 步骤3：计算权重
```solidity
// BASE_WEIGHT = 1000
// MAX_WEIGHT = 10000
weights[i] = BASE_WEIGHT + ((score - minScore) * (MAX_WEIGHT - BASE_WEIGHT)) / (maxScore - minScore)
```

**权重计算示例**：
```
候选者分数: [250, 240, 230, 220, 210, 200]
最小分数: 200, 最大分数: 250

权重计算:
- 候选者1 (250): 1000 + ((250-200) * 9000) / 50 = 10000
- 候选者2 (240): 1000 + ((240-200) * 9000) / 50 = 8200
- 候选者3 (230): 1000 + ((230-200) * 9000) / 50 = 6400
...
```

#### 步骤4：加权轮盘赌选择（无放回）
```solidity
// 使用VRF随机数进行选择
for (uint selectedCount = 0; selectedCount < MAX_COMMITTEE_SIZE; selectedCount++) {
    // 从VRF随机数中提取一个值
    uint256 randomValue = extractRandomValue(random, selectedCount, ...) % totalWeight;
    
    // 在累积权重中找到对应的候选者
    // 选择后，从候选池中移除（无放回）
}
```

**选择过程**：
1. 计算累积权重：`[1000, 9200, 15600, ...]`
2. 从VRF随机数中提取值（如 5000）
3. 找到对应的候选者（5000 < 9200，选择候选者2）
4. 移除已选中的候选者，更新权重
5. 重复直到选出3个成员

### 4. 完整轮换示例

假设有10个候选者，声誉值如下：

```
候选者1: 280 (准确率95% + 一致性95%)
候选者2: 275 (准确率92% + 一致性93%)
候选者3: 270 (准确率90% + 一致性90%)
候选者4: 265 (准确率88% + 一致性87%)
候选者5: 260 (准确率85% + 一致性85%)
候选者6: 255 (准确率82% + 一致性83%)
候选者7: 250 (准确率80% + 一致性80%)
候选者8: 245 (准确率78% + 一致性77%)
候选者9: 240 (准确率75% + 一致性75%)
候选者10: 235 (准确率72% + 一致性73%)
```

**轮换过程**：

1. **选择Top-10**：所有候选者（因为只有10个）

2. **计算权重**：
   - 最小分数：235
   - 最大分数：280
   - 候选者1权重：10000（最高）
   - 候选者10权重：1000（最低）

3. **加权轮盘赌选择**：
   - 使用VRF随机数
   - 高分候选者被选中的概率更高
   - 但低分候选者也有机会（保证公平性）

4. **结果**：选出3个成员组成新委员会

## 三、reputation_decisions 与轮换的关系

### 数据流关系

```
┌─────────────────────────────────────┐
│  reputation_decisions.csv            │
│  (原始决策记录)                       │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  calculate_reputation_stats          │
│  (计算准确率和一致性)                │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  reputation_stats.csv                │
│  (统计数据)                          │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  update_reputation_from_decisions   │
│  (更新到区块链)                      │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  ReputationManager合约               │
│  (存储声誉值)                        │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  CommitteeRotation.rotateCommittee() │
│  (使用声誉值选择新委员会)            │
└─────────────────────────────────────┘
```

### 关键点

1. **reputation_decisions 是数据源**：
   - 记录所有决策历史
   - 用于计算准确率和一致性

2. **准确率和一致性影响轮换**：
   - 高准确率 → 高声誉 → 更高被选中概率
   - 高一致性 → 高声誉 → 更高被选中概率

3. **轮换使用实时声誉值**：
   - 每次轮换前，从区块链读取最新的声誉值
   - 基于这些声誉值进行加权选择

## 四、实际运行示例

### 场景：一次完整的轮换

1. **决策记录**（secret_share_receiver运行中）：
   ```
   reputation_decisions.csv 持续记录决策
   ```

2. **计算统计数据**（轮换前）：
   ```bash
   ./calculate_reputation_stats.sh
   ```
   输出：
   ```
   成员1: 准确率=95%, 一致性=90%, 综合声誉=185
   成员2: 准确率=92%, 一致性=88%, 综合声誉=180
   成员3: 准确率=98%, 一致性=95%, 综合声誉=193
   ```

3. **更新到区块链**（轮换前）：
   ```bash
   truffle exec scripts/update_reputation_from_decisions.js
   ```

4. **执行轮换**：
   ```solidity
   rotateCommittee() {
       // 从ReputationManager读取所有候选者的声誉值
       // 选择Top-10
       // 加权轮盘赌选择3个成员
   }
   ```

5. **更新委员会文件**：
   ```
   committee_members.txt 更新为新委员会地址
   ```

## 五、总结

### reputation_decisions.csv 的作用

1. ✅ **记录决策历史**：每次审计请求的决策都被记录
2. ✅ **计算准确率**：基于决策正确性统计
3. ✅ **计算一致性**：基于与其他成员的决策一致性统计
4. ✅ **生成统计数据**：为声誉计算提供数据源

### 委员会轮换逻辑

1. ✅ **时间控制**：每60秒可以轮换一次
2. ✅ **基于声誉**：使用准确率+一致性计算综合声誉
3. ✅ **加权选择**：高分候选者概率更高，但保证公平性
4. ✅ **VRF随机性**：使用可验证随机数确保选择不可预测
5. ✅ **自动更新**：轮换后自动更新committee_members.txt

### 两者的关系

- **reputation_decisions** → 提供原始数据
- **reputation_stats** → 统计数据
- **区块链声誉值** → 存储声誉
- **委员会轮换** → 使用声誉值选择新委员会

整个系统形成了一个完整的闭环：**决策 → 统计 → 声誉 → 轮换 → 新委员会 → 新决策**。

