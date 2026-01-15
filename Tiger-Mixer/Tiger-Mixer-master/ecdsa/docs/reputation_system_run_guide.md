# 基于真实决策数据的声誉系统 - 运行指南

## 📋 前置条件

1. ✅ 代码已修改完成
2. ✅ 确保以下文件存在：
   - `/home/zxx/A2L/A2L-master/ecdsa/bin/address_labels.csv`
   - `/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt`
3. ✅ 确保区块链节点运行中（如果使用区块链）

## 🔨 步骤1：重新编译代码

```bash
cd /home/zxx/A2L/A2L-master/ecdsa
mkdir -p build
cd build
cmake ..
make
```

**检查编译结果**：
```bash
# 检查关键程序是否编译成功
ls -lh ../bin/secret_share_receiver
ls -lh ../bin/calculate_reputation_stats
```

应该看到两个可执行文件。

## 🚀 步骤2：启动委员会成员（secret_share_receiver）

需要启动3个委员会成员，每个成员在不同的终端窗口运行：

### 终端1 - 成员1
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./secret_share_receiver 1
```

### 终端2 - 成员2
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./secret_share_receiver 2
```

### 终端3 - 成员3
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./secret_share_receiver 3
```

**验证启动成功**：
- 每个终端应该显示类似信息：
  ```
  [MAIN] ✅ 使用地址: 0x... (成员 #1)
  [MAIN] ✅ 声誉跟踪系统初始化成功
  Receiver 1 listening on tcp://localhost:5555
  ```

## 📊 步骤3：验证决策记录

当有审计请求时，系统会自动记录决策。检查决策记录文件：

```bash
# 查看决策记录（如果已有数据）
cat /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv
```

**文件格式**：
```csv
timestamp,request_id,user_address,user_label,participant_id,judge_api_result,actual_decision,is_correct
1704067200,req_001_1704067200,0x9339...,illegal,1,1,provided_shares,1
```

## 🔄 步骤4：启动自动轮换脚本

在**新的终端窗口**运行：

```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./auto_rotation_simple.sh 60
```

这将每60秒执行一次轮换检查。

**脚本会自动执行**：
1. 计算统计数据（从决策记录）
2. 更新声誉到区块链
3. 生成VRF随机数
4. 执行委员会轮换

## 📈 步骤5：监控系统运行

### 查看轮换日志
```bash
tail -f /home/zxx/A2L/A2L-master/ecdsa/logs/auto_rotation_simple.log
```

### 查看统计数据
```bash
# 查看最新的统计数据
cat /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_stats.csv
```

**文件格式**：
```csv
participant_id,address,total_decisions,correct_decisions,accuracy,consistency,total_reputation,last_update
1,0x9a98...,100,95,95,90,185,1704067200
2,0x0048...,100,92,92,88,180,1704067200
3,0x80bc...,100,98,98,95,193,1704067200
```

### 查看决策记录
```bash
# 查看最新的决策记录
tail -20 /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv
```

## 🧪 步骤6：手动测试（可选）

### 手动计算统计数据
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./calculate_reputation_stats.sh
```

**预期输出**：
```
========================================
   计算声誉统计数据
========================================

✅ 声誉跟踪系统初始化成功

[REPUTATION] 成员 1 (0x...): 准确率=95%, 一致性=90%, 综合声誉=185
[REPUTATION] 成员 2 (0x...): 准确率=92%, 一致性=88%, 综合声誉=180
[REPUTATION] 成员 3 (0x...): 准确率=98%, 一致性=95%, 综合声誉=193
[REPUTATION] 已保存声誉统计到: /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_stats.csv

✅ 统计数据计算完成
========================================
```

### 手动更新声誉到区块链
```bash
cd /home/zxx/Config/truffleProject/truffletest
truffle exec scripts/update_reputation_from_decisions.js --network development
```

## ⚠️ 常见问题排查

### 1. 编译错误
```bash
# 清理并重新编译
cd /home/zxx/A2L/A2L-master/ecdsa/build
make clean
cmake ..
make
```

### 2. secret_share_receiver 启动失败
- 检查 `committee_members.txt` 文件是否存在且格式正确
- 检查端口是否被占用：`lsof -i :5555 -i :5556 -i :5557`
- 检查地址标签文件：`ls -lh /home/zxx/A2L/A2L-master/ecdsa/bin/address_labels.csv`

### 3. 决策记录为空
- 确保有审计请求发生
- 检查 `pairs_summary_json` 是否包含有效的用户地址
- 检查地址是否在 `address_labels.csv` 中

### 4. 统计数据计算失败
- 检查决策记录文件是否存在：`ls -lh /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv`
- 检查文件格式是否正确
- 检查 `committee_members.txt` 文件是否存在

### 5. 声誉更新失败
- 检查区块链节点是否运行
- 检查账户是否解锁
- 检查合约地址是否正确

## 📝 运行检查清单

- [ ] 代码已重新编译
- [ ] 3个secret_share_receiver已启动
- [ ] 声誉跟踪系统初始化成功（每个receiver）
- [ ] 自动轮换脚本已启动
- [ ] 决策记录文件已创建
- [ ] 统计数据文件已生成
- [ ] 轮换日志正常输出

## 🎯 验证系统正常工作

### 1. 检查决策记录
```bash
# 应该有决策记录
wc -l /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv
```

### 2. 检查统计数据
```bash
# 应该有统计数据
cat /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_stats.csv
```

### 3. 检查轮换日志
```bash
# 应该看到轮换成功的信息
grep "轮换成功\|更新成功" /home/zxx/A2L/A2L-master/ecdsa/logs/auto_rotation_simple.log
```

## 🔍 调试模式

如果需要查看详细的调试信息，可以在启动secret_share_receiver时查看输出：

```bash
# 终端会显示：
# - 收到的审计请求
# - 提取的用户地址
# - judge API的调用结果
# - 决策记录信息
# - shares的查找和发送情况
```

## 📊 预期行为

1. **决策记录**：每次审计请求（tag==0）都会记录决策
2. **统计数据**：每次轮换前自动计算
3. **声誉更新**：每次轮换前自动更新到区块链
4. **委员会轮换**：根据更新后的声誉值选择新委员会

## 🛑 停止系统

1. **停止自动轮换**：在运行 `auto_rotation_simple.sh` 的终端按 `Ctrl+C`
2. **停止secret_share_receiver**：在每个运行receiver的终端按 `Ctrl+C`

系统会自动保存统计数据到文件。

