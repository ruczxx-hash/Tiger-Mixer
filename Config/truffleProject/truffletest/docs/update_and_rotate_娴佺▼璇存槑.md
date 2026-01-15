# 委员会轮换流程详细说明

## 概述

`update_and_rotate.js` 是一个 Truffle 脚本，用于自动化执行委员会成员声誉更新和轮换流程。该脚本集成了声誉管理系统、VRF 随机数生成系统和委员会轮换机制，确保委员会成员的选择过程公平、透明且具有可验证的随机性。

---

## 整体流程架构

```
┌─────────────────────────────────────────────────────────────┐
│              委员会轮换流程 (update_and_rotate.js)              │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
  ┌─────────┐      ┌──────────────┐      ┌──────────────┐
  │声誉更新  │      │VRF随机数生成  │      │委员会轮换    │
  └─────────┘      └──────────────┘      └──────────────┘
```

---

## 详细流程步骤

### 步骤 1: 更新候选者声誉

#### 1.1 功能描述
从 `CommitteeRotation` 合约获取当前候选池，并为每个候选者更新其在 `ReputationManager` 中的声誉值。

#### 1.2 实现细节

```javascript
const candidatePool = await committeeRotation.getCandidatePool();
```

- **数据源**: `CommitteeRotation.getCandidatePool()` 返回所有已注册的候选者地址数组
- **声誉组成**: 每个候选者的声誉由三个维度组成：
  - **准确率 (Accuracy)**: 60-90 之间的随机值
  - **参与率 (Participation)**: 65-90 之间的随机值  
  - **一致性 (Consistency)**: 55-90 之间的随机值

#### 1.3 声誉计算公式

根据 `ReputationManager.sol` 的实现，综合声誉计算公式为：

```
综合声誉 = 准确率 + 一致性 + 参与混币的活跃度（参与率）
```

**示例计算**:
- 准确率: 80
- 一致性: 75
- 参与率: 70
- **综合声誉 = 80 + 75 + 70 = 225**

#### 1.4 更新过程

```javascript
await reputationManager.updateReputation(
  Math.round(accuracy), 
  Math.round(participation), 
  Math.round(consistency), 
  { from: account }
);
```

- 每个候选者账户需要解锁后才能更新声誉（使用 `web3.eth.personal.unlockAccount`）
- 更新操作由候选者自己调用（`from: account`），符合去中心化设计

---

### 步骤 2: 更新候选者分数

#### 2.1 功能描述
调用 `CommitteeRotation.updateAllCandidateScores()` 批量更新候选池中所有候选者的分数。

#### 2.2 分数计算规则

根据 `CommitteeRotation.sol` 的实现：

```solidity
function updateCandidateScore(address candidate) internal {
    // 完全按照声誉值计算，不加入质押权重
    uint256 reputation = reputationManager.calculateReputation(candidate);
    candidateScore[candidate] = reputation;
}
```

**重要变更**: 
- ✅ **仅使用声誉值**（已移除质押权重影响）
- 分数 = 综合声誉值（准确率 + 一致性 + 参与率）

#### 2.3 更新机制

```javascript
await committeeRotation.updateAllCandidateScores();
```

该函数内部会遍历所有候选者，调用 `updateCandidateScore()` 更新每个候选者的分数。

---

### 步骤 3: 显示当前排名

#### 3.1 功能描述
遍历候选池，查询并显示每个候选者的声誉值和分数，用于验证更新结果。

#### 3.2 查询内容

```javascript
const reputation = await reputationManager.calculateReputation(account);
const score = await committeeRotation.getCandidateScore(account);
```

- **声誉值**: 从 `ReputationManager` 获取（计算公式如步骤 1.3）
- **分数**: 从 `CommitteeRotation` 获取（当前等于声誉值）

#### 3.3 输出格式

```
候选者 1: 0x1234... - 声誉:225, 分数:225
候选者 2: 0x5678... - 声誉:210, 分数:210
...
```

---

### 步骤 3.5: 生成并提交 VRF 随机数

这是整个流程中最复杂的步骤，涉及链下 VRF 生成和链上验证。

#### 3.5.1 VRF 概述

**VRF (Verifiable Random Function)** 是一种密码学原语，提供：
- ✅ **可验证性**: 任何人都可以验证随机数的正确性
- ✅ **确定性**: 相同输入产生相同输出
- ✅ **不可预测性**: 无法提前预测随机数

#### 3.5.2 VRF 消息构造

```javascript
const message = `rotation_${rotationCount}_${candidatePoolHash}`;
```

**消息组成**:
1. **轮次号 (`rotationCount`)**: 当前委员会轮换的轮次编号
2. **候选池哈希 (`candidatePoolHash`)**: 所有候选者地址的 SHA256 哈希值

**确定性保证**: 
- 不包含时间戳，确保所有节点使用相同的消息
- 基于轮次和候选池状态，保证可重现性

#### 3.5.3 候选池哈希计算

```javascript
function calculateCandidatePoolHash(candidatePool) {
  const hash = crypto.createHash('sha256');
  candidatePool.forEach(addr => {
    hash.update(addr);
  });
  return '0x' + hash.digest('hex');
}
```

**计算流程**:
1. 创建 SHA256 哈希对象
2. 按顺序将每个候选者地址加入哈希
3. 生成 32 字节（64 位十六进制）哈希值

#### 3.5.4 VRF 生成（链下）

```javascript
const vrfData = await generateVRFRandom(targetRotationCount, candidatePoolHash);
```

**调用流程**:
1. 检查 VRF 密钥文件是否存在: `/home/zxx/A2L/A2L-master/ecdsa/keys/vrf_generator.key`
2. 调用 C 程序: `/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_cli`
3. 传入参数: 密钥文件路径（可选）和消息
4. 解析 JSON 输出

**VRF CLI 输出格式**:
```json
{
  "randomUint256": "0x...",      // 32 字节随机数（十六进制）
  "proof": "...",                // 81 字节 VRF 证明（十六进制）
  "publicKey": "...",             // 33 字节压缩公钥（十六进制）
  "message": "rotation_0_0x..."  // 原始消息
}
```

#### 3.5.5 VRF 数据提交（链上）

```javascript
await committeeRotation.submitVRFRandomWithProof(
  targetRotationCount,
  vrfData.random,
  proofBytes,
  publicKeyBytes,
  messageBytes,
  { from: accounts[0] }
);
```

**提交的数据**:
- **轮次号**: 标识随机数所属的轮次
- **随机数**: 32 字节的 `bytes32` 值
- **证明**: 81 字节的 VRF 证明（用于链上验证）
- **公钥**: 33 字节的压缩格式公钥（secp256k1）
- **消息**: 用于生成随机数的原始消息

#### 3.5.6 VRF 证明验证（链上）

```javascript
await committeeRotation.verifyVRFProof(targetRotationCount, { from: accounts[0] });
```

**验证过程** (在 `CommitteeRotation.sol` 中):

1. **解码公钥**: 从 33 字节压缩格式转换为 `[x, y]` 坐标
   ```solidity
   uint256[2] memory pubKey = VRF.decodePoint(rotationPublicKey[round]);
   ```

2. **解码证明**: 从 81 字节转换为 `[gamma-x, gamma-y, c, s]`
   ```solidity
   uint256[4] memory proof = VRF.decodeProof(rotationProof[round]);
   ```

3. **验证 VRF**: 使用 `vrf-solidity-master` 库验证
   ```solidity
   bool isValid = VRF.verify(pubKey, proof, rotationMessage[round]);
   ```

4. **状态更新**: 
   - 验证成功: `randomVerified[round] = true`
   - 验证失败: 清空随机数，`rotationRandom[round] = bytes32(0)`

#### 3.5.7 状态检查

```javascript
const verified = await committeeRotation.randomVerified(targetRotationCount);
const randomCheck = await committeeRotation.rotationRandom(targetRotationCount);
```

**检查项**:
- ✅ 随机数是否已设置（非零哈希）
- ✅ 随机数是否已验证（`randomVerified[round] == true`）
- ✅ 随机数是否已使用（`randomUsed[round] == false`）

#### 3.5.8 容错机制

**如果随机数已存在**:
- 检查验证状态
- 如果未验证，尝试立即验证
- 如果已验证，跳过提交步骤

**如果提交失败**:
- 记录错误但不阻止后续流程
- 提示将无法执行带随机性的轮换

---

### 步骤 4: 执行委员会轮换

#### 4.1 前置检查

在执行轮换前，脚本会进行详细的检查：

##### 4.1.1 时间检查

```javascript
const blockTimestamp = parseInt(currentBlock.timestamp);
const lastRotation = rotationInfo.lastRotation.toNumber();
const nextRotation = rotationInfo.nextRotation.toNumber();
const waitTime = nextRotation - blockTimestamp;
```

**检查项**:
- ✅ 当前区块时间戳
- ✅ 上次轮换时间
- ✅ 下次可轮换时间（上次轮换时间 + 60 秒）
- ✅ 剩余等待时间

**轮换间隔**: 60 秒（`ROTATION_INTERVAL = 60`）

##### 4.1.2 VRF 状态检查

```javascript
const currentRandom = await committeeRotation.rotationRandom(currentRotationCount);
const isVerified = await committeeRotation.randomVerified(currentRotationCount);
const isUsed = await committeeRotation.randomUsed(currentRotationCount);
```

**状态要求**:
- ✅ 随机数已设置（`currentRandom != bytes32(0)`）
- ✅ 随机数已验证（`isVerified == true`）
- ✅ 随机数未使用（`isUsed == false`）

##### 4.1.3 可轮换性检查

```javascript
const canRotate = await committeeRotation.canRotate();
```

**检查条件** (在合约中):
```solidity
function canRotate() public view returns (bool) {
    return block.timestamp >= lastRotationTime + ROTATION_INTERVAL;
}
```

#### 4.2 轮换执行

##### 4.2.1 轮换函数调用

```javascript
const tx = await committeeRotation.rotateCommittee();
```

**轮换流程** (在 `CommitteeRotation.sol` 中):

1. **验证前置条件**:
   ```solidity
   require(canRotate(), "Rotation not yet available");
   require(candidatePool.length >= MAX_COMMITTEE_SIZE, "Insufficient candidates");
   require(rotationRandom[rotationCount] != bytes32(0), "VRF random not set");
   require(randomVerified[rotationCount], "VRF random not verified");
   require(!randomUsed[rotationCount], "Random already used for this round");
   ```

2. **选择新委员会**:
   ```solidity
   address[MAX_COMMITTEE_SIZE] memory newCommittee = selectNewCommittee();
   ```

3. **更新状态**:
   ```solidity
   currentCommittee = newCommittee;
   lastRotationTime = block.timestamp;
   rotationCount++;
   randomUsed[rotationCount - 1] = true;  // 标记随机数已使用
   ```

##### 4.2.2 委员会选择算法

`selectNewCommittee()` 使用**加权轮盘赌算法**：

1. **排序候选者**: 按分数从高到低排序
2. **选择 Top K**: 取前 6 名候选者（或所有候选者，如果少于 6 名）
3. **计算权重**: 
   ```solidity
   weights[i] = baseWeight + ((score - minScore) * scale) / scoreRange;
   ```
   - 基础权重: 100
   - 分数差异权重: 根据与最低分的差异计算
4. **轮盘赌选择**: 使用 VRF 随机数进行加权随机选择（不放回）
5. **返回结果**: 3 个选中的委员会成员

**权重归一化示例**:
```
候选者分数: [250, 240, 230, 220, 210, 200]
最小分数: 200, 最大分数: 250

权重计算:
- 候选者1 (250): 100 + ((250-200) * 1000) / 50 = 1100
- 候选者2 (240): 100 + ((240-200) * 1000) / 50 = 900
- 候选者3 (230): 100 + ((230-200) * 1000) / 50 = 700
...
```

#### 4.3 错误处理

**如果时间未到**:
- 显示剩余等待时间
- 跳过轮换步骤，继续执行后续步骤

**如果 VRF 未设置**:
- 检查相邻轮次的随机数状态（调试用）
- 提示确保已执行步骤 3.5

**如果 VRF 未验证**:
- 尝试立即验证
- 验证成功后继续执行轮换

---

### 步骤 5: 获取新的委员会

#### 5.1 查询委员会详情

```javascript
const details = await committeeRotation.getCommitteeDetails();
```

**返回数据**:
- `addresses`: 新委员会成员地址数组（3 个）
- `reputations`: 每个成员的声誉值数组
- `stakes`: 每个成员的质押权重数组（当前固定为 100）
- `scores`: 每个成员的分数数组（等于声誉值）

#### 5.2 格式化输出

```javascript
console.log("新委员会成员 (Top3):");
for (let i = 0; i < addresses.length; i++) {
  console.log(`  成员 ${i + 1}: ${addresses[i]}`);
  console.log(`    声誉: ${reputations[i].toString()}`);
  console.log(`    质押: ${web3.utils.fromWei(stakes[i].toString(), 'ether')} ETH`);
}
```

#### 5.3 写入文件

```javascript
const filePath = '/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt';
fs.writeFileSync(filePath, out);
```

**文件格式**: 每行一个地址，共 3 行

**用途**: 供其他系统（如 ECDSA 相关程序）读取当前委员会成员列表

---

## 关键数据结构

### 声誉数据结构 (ReputationManager)

```solidity
struct MemberReputation {
    uint256 decisionAccuracy;    // 决策准确率 (0-100)
    uint256 participationRate;  // 参与率 (0-100)
    uint256 consistency;        // 一致性 (0-100)
    uint256 lastUpdateTime;     // 最后更新时间
    uint256 totalDecisions;     // 总决策数
    uint256 correctDecisions;   // 正确决策数
}
```

### VRF 数据结构 (CommitteeRotation)

```solidity
mapping(uint256 => bytes32) public rotationRandom;      // 轮次 -> VRF随机数
mapping(uint256 => bytes) public rotationProof;         // 轮次 -> VRF证明
mapping(uint256 => bytes) public rotationPublicKey;    // 轮次 -> VRF公钥
mapping(uint256 => bytes) public rotationMessage;      // 轮次 -> VRF消息
mapping(uint256 => bool) public randomUsed;            // 轮次 -> 随机数是否已使用
mapping(uint256 => bool) public randomVerified;        // 轮次 -> 随机数是否已验证
```

---

## 安全机制

### 1. VRF 可验证性

- ✅ 链下生成随机数，链上验证证明
- ✅ 任何人都可以验证随机数的正确性
- ✅ 防止 Oracle 提交伪造的随机数

### 2. 时间锁定

- ✅ 最小轮换间隔：60 秒
- ✅ 防止频繁轮换导致的系统不稳定

### 3. 状态一致性

- ✅ 随机数必须已验证才能使用
- ✅ 随机数只能使用一次（标记 `randomUsed`）
- ✅ 轮次号递增，防止重复使用

### 4. 权限控制

- ✅ VRF 提交需要授权账户（`vrfGenerator`）
- ✅ 如果 `vrfGenerator == address(0)`，则任何人都可以提交（用于测试）

---

## 依赖关系

### 合约依赖

1. **CommitteeRotation**: 核心轮换逻辑
   - 依赖: `ReputationManager`, `StakingManager`
   
2. **ReputationManager**: 声誉管理
   - 独立合约，无外部依赖
   
3. **StakingManager**: 质押管理
   - 独立合约，无外部依赖

### 外部依赖

1. **VRF CLI**: `/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_cli`
   - C 程序，使用 `secp256k1-vrf` 库
   - 需要 VRF 密钥文件（可选）

2. **VRF Solidity 库**: `vrf-solidity-master`
   - 用于链上验证 VRF 证明
   - 依赖: `EllipticCurve.sol`

---

## 执行方式

### 命令

```bash
cd /home/zxx/Config/truffleProject/truffletest
truffle exec scripts/update_and_rotate.js
```

### 前置条件

1. ✅ Truffle 开发环境已配置
2. ✅ 合约已部署（`CommitteeRotation`, `ReputationManager`）
3. ✅ 候选池中至少 3 个候选者
4. ✅ VRF CLI 程序已编译（`vrf_cli`）
5. ✅ VRF 密钥文件已生成（可选，推荐）

### 输出示例

```
=== 更新声誉并重新选择委员会 ===

--- 步骤1: 更新候选者声誉 ---
候选池中有 7 个候选者
✅ 候选者 1 声誉更新: 0x1234...
...

--- 步骤2: 更新候选者分数 ---
✅ 所有候选者分数更新成功

--- 步骤3: 显示当前排名 ---
候选者 1: 0x1234... - 声誉:225, 分数:225
...

--- 步骤3.5: 生成并提交 VRF 随机数 ---
当前轮次: 0
候选池哈希: 0xabcd...
✅ VRF 随机数生成成功
   随机数: 0x...
✅ VRF 随机数提交成功（含证明）
✅ VRF 证明验证成功

--- 步骤4: 执行委员会轮换 ---
轮换信息:
  当前区块时间戳: 1234567890
  ...
✅ 委员会轮换成功

--- 步骤5: 获取新委员会 ---
新委员会成员 (Top3):
  成员 1: 0x1234...
    声誉: 225
    质押: 1 ETH
...
✅ 已写入新委员会成员到文件
```

---

## 常见问题

### Q1: VRF 随机数生成失败？

**可能原因**:
- VRF CLI 程序路径不正确
- VRF 密钥文件不存在
- 消息格式错误

**解决方案**:
- 检查 `VRF_CLI_PATH` 和 `VRF_KEY_FILE` 路径
- 确保 `vrf_cli` 已编译且可执行
- 检查 C 程序输出格式是否为 JSON

### Q2: 验证失败？

**可能原因**:
- 证明格式不正确（应为 81 字节）
- 公钥格式不正确（应为 33 字节压缩格式）
- 消息不匹配

**解决方案**:
- 检查 VRF CLI 输出的格式
- 确保消息在生成和验证时一致
- 查看合约中的详细错误信息

### Q3: 轮换时间未到？

**现象**: "当前不可轮换，跳过轮换步骤"

**原因**: 距离上次轮换未满 60 秒

**解决方案**:
- 等待 `waitTime` 秒后重试
- 或修改合约中的 `ROTATION_INTERVAL` 常量

### Q4: 随机数未设置？

**可能原因**:
- 步骤 3.5 未成功执行
- 轮次号不匹配
- 提交交易失败

**解决方案**:
- 检查步骤 3.5 的输出
- 确认轮次号与当前轮次一致
- 查看交易回执确认提交成功

---

## 总结

`update_and_rotate.js` 脚本实现了一个完整的委员会轮换流程，结合了：

1. ✅ **声誉系统**: 动态更新候选者表现
2. ✅ **VRF 随机性**: 提供可验证的公平随机选择
3. ✅ **加权选择**: 根据声誉值进行加权随机选择
4. ✅ **状态同步**: 链下生成、链上验证、状态同步

整个流程确保了委员会轮换的**公平性**、**透明性**和**可验证性**。

