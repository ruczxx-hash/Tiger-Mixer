# 激励机制实现说明

## 概述

本文档说明激励机制（Incentive Mechanism）的实现，包括奖励池、服务费分配、交易参与统计和质押惩罚机制。

## 已实现的功能

### 1. 奖励池（Reward Pool）

**合约**：`RewardPool.sol`

**功能**：
- ✅ 接收服务费（20%的交易费用）
- ✅ 接收被slash的质押资金
- ✅ 记录每个epoch中每个成员参与的交易数
- ✅ 在每个epoch结束时按交易数量比例分配奖励
- ✅ 成员可以领取奖励

**关键函数**：
- `depositServiceFee()`: 接收服务费
- `depositSlashedFunds()`: 接收slash资金
- `recordTransaction()`: 记录成员参与的交易
- `endEpoch()`: 结束当前epoch
- `distributeRewardsToMembers()`: 按交易数量比例分配奖励
- `claimReward()`: 成员领取奖励

### 2. 服务费收取机制

**修改的合约**：`MixerEscrow.sol`

**实现**：
- ✅ 在交易确认时扣除20%服务费
- ✅ 服务费自动转入奖励池
- ✅ 记录委员会成员参与的交易

**关键代码**：
```solidity
// 计算服务费（20%）
serviceFee = (amount * SERVICE_FEE_PERCENTAGE) / PRECISION;
finalAmount = amount - serviceFee;

// 将服务费转入奖励池
rewardPool.call{value: serviceFee}("");

// 记录交易参与
rewardPool.recordTransaction(e.party2);
```

### 3. 交易参与统计

**实现位置**：
- `RewardPool.recordTransaction()`: 记录每个成员在每个epoch中参与的交易数
- `MixerEscrow.confirm()`: 在交易确认时调用记录

**数据结构**：
```solidity
mapping(uint256 => mapping(address => uint256)) memberTransactions;  // epoch => member => 交易数
```

### 4. Epoch 奖励分配

**实现位置**：`CommitteeRotation.rotateCommittee()`

**流程**：
1. 轮换时结束当前epoch
2. 获取上一轮次的委员会成员
3. 按交易数量比例分配奖励
4. 成员可以后续领取奖励

**分配公式**：
```
成员奖励 = (总奖励 * 成员交易数) / 总交易数
```

### 5. 质押惩罚（Slashing）

**修改的合约**：`StakingManager.sol`

**功能**：
- ✅ `slash()` 函数：惩罚恶意成员
- ✅ 扣除质押金额
- ✅ 被slash的资金转入奖励池
- ✅ 记录slash历史

**权限控制**：
- 只有 owner 或 CommitteeRotation 可以调用 slash

**关键函数**：
```solidity
function slash(address maliciousMember, uint256 penaltyAmount, string memory reason)
```

## 合约集成关系

```
MixerEscrow
    ↓ (服务费 + 交易记录)
RewardPool
    ↑ (slash资金)
StakingManager
    ↓ (触发epoch结束和分配)
CommitteeRotation
```

## 部署顺序

1. 部署 `RewardPool`
2. 部署 `StakingManager`
3. 部署 `ReputationManager`
4. 部署 `CommitteeRotation`（包含 RewardPool 地址）
5. 部署 `MixerEscrow`（设置 RewardPool 地址）
6. 设置合约之间的引用关系

## 使用流程

### 1. 交易流程（自动收取服务费）

```
用户发起交易 → MixerEscrow.openEscrow()
    ↓
双方确认 → MixerEscrow.confirm()
    ↓
扣除20%服务费 → RewardPool.depositServiceFee()
    ↓
记录交易参与 → RewardPool.recordTransaction()
    ↓
转账剩余金额给受益人
```

### 2. Epoch 奖励分配流程

```
轮换触发 → CommitteeRotation.rotateCommittee()
    ↓
结束当前epoch → RewardPool.endEpoch()
    ↓
分配奖励 → RewardPool.distributeRewardsToMembers()
    ↓
成员领取 → RewardPool.claimReward()
```

### 3. Slash 流程

```
检测恶意行为 → StakingManager.slash()
    ↓
扣除质押金额
    ↓
转入奖励池 → RewardPool.depositSlashedFunds()
```

## 配置参数

- **服务费比例**：20% (`SERVICE_FEE_PERCENTAGE = 0.2e18`)
- **Epoch 定义**：每次委员会轮换为一个epoch
- **奖励分配**：按交易数量比例

## 注意事项

1. **交易参与记录**：当前实现假设 `party2` 是委员会成员，实际应该根据业务逻辑验证
2. **委员会成员验证**：需要在 `MixerEscrow` 中添加接口调用验证成员身份
3. **奖励领取**：成员需要主动调用 `claimReward()` 领取奖励
4. **Epoch 管理**：epoch 在轮换时自动结束，无需手动触发

## 后续改进建议

1. 添加委员会成员验证接口
2. 支持多个成员参与同一交易
3. 添加奖励领取期限
4. 添加奖励分配历史查询
5. 优化 Gas 消耗














