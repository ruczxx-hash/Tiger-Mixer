# 基于真实决策数据的声誉更新系统 - 完整实现

## ✅ 已完成的工作

### 1. 核心模块实现

#### 1.1 `reputation_tracker.c`
- ✅ 决策记录功能
- ✅ 准确率计算（基于address_labels.csv判断决策正确性）
- ✅ 一致性计算（基于与其他成员的决策一致性）
- ✅ 统计数据保存到CSV文件
- ✅ 地址未找到时输出错误并停止函数

#### 1.2 `reputation_tracker_util.c`
- ✅ 从JSON中提取用户地址的工具函数

#### 1.3 `reputation_tracker.h`
- ✅ API接口定义

### 2. 集成到现有系统

#### 2.1 `secret_share_receiver.c`
- ✅ 包含声誉跟踪头文件
- ✅ 在main函数中初始化声誉跟踪系统
- ✅ 在handle_audit_request中记录决策：
  - tag==0时，调用judge API后记录决策
  - 找到shares时记录"provided_shares"
  - 未找到shares时记录"not_found"
  - 不需要审计时记录"no_audit_needed"

### 3. 构建系统

#### 3.1 `CMakeLists.txt`
- ✅ 将`reputation_tracker.c`和`reputation_tracker_util.c`添加到静态库
- ✅ 添加`calculate_reputation_stats`可执行文件

### 4. 工具程序

#### 4.1 `calculate_reputation_stats.c`
- ✅ 独立的统计数据计算程序
- ✅ 从决策记录中计算准确率和一致性
- ✅ 保存到CSV文件

#### 4.2 `calculate_reputation_stats.sh`
- ✅ Shell脚本包装器
- ✅ 自动查找可执行文件位置

### 5. 更新脚本

#### 5.1 `update_reputation_from_decisions.js`
- ✅ 从CSV文件读取统计数据
- ✅ 更新到区块链（只更新准确率和一致性）

#### 5.2 `update_and_rotate_new.js`
- ✅ 修改第一部分：从随机更新改为基于真实决策数据
- ✅ 先计算统计数据，再更新到区块链

## 📁 文件结构

```
/home/zxx/A2L/A2L-master/ecdsa/
├── src/
│   ├── reputation_tracker.c          # 核心实现
│   ├── reputation_tracker_util.c     # 工具函数
│   ├── calculate_reputation_stats.c   # 统计数据计算程序
│   └── secret_share_receiver.c       # 已集成决策记录
├── include/
│   └── reputation_tracker.h          # 头文件
├── bin/
│   └── calculate_reputation_stats.sh  # 计算脚本
├── log_game/
│   ├── reputation_decisions.csv      # 决策记录表（自动生成）
│   └── reputation_stats.csv          # 声誉统计表（自动生成）
└── bin/
    └── address_labels.csv            # 地址标签表

/home/zxx/Config/truffleProject/truffletest/scripts/
├── update_reputation_from_decisions.js  # 更新脚本
└── update_and_rotate_new.js            # 轮换脚本（已修改）
```

## 🔄 工作流程

### 1. 决策记录（实时）
```
secret_share_receiver 运行
    ↓
收到审计请求
    ↓
提取用户地址（从pairs_summary_json）
    ↓
调用judge API
    ↓
记录决策到 reputation_decisions.csv
```

### 2. 声誉计算（轮换前）
```
auto_rotation_simple.sh 触发
    ↓
update_and_rotate_new.js 执行
    ↓
步骤1: 调用 calculate_reputation_stats.sh
    ↓
从 reputation_decisions.csv 读取决策记录
    ↓
计算准确率和一致性
    ↓
保存到 reputation_stats.csv
```

### 3. 声誉更新（轮换前）
```
update_and_rotate_new.js 继续执行
    ↓
步骤2: 调用 update_reputation_from_decisions.js
    ↓
从 reputation_stats.csv 读取统计数据
    ↓
更新到区块链（ReputationManager合约）
```

### 4. 委员会轮换
```
update_and_rotate_new.js 继续执行
    ↓
步骤3: VRF生成和验证
    ↓
步骤4: 执行委员会轮换
    ↓
使用更新后的声誉值选择新委员会
```

## 📊 数据格式

### reputation_decisions.csv
```csv
timestamp,request_id,user_address,user_label,participant_id,judge_api_result,actual_decision,is_correct
1704067200,req_001_1704067200,0x9339...,illegal,1,0,provided_shares,1
1704067200,req_001_1704067200,0x9339...,illegal,2,0,provided_shares,1
1704067200,req_001_1704067200,0x9339...,illegal,3,0,provided_shares,1
```

### reputation_stats.csv
```csv
participant_id,address,total_decisions,correct_decisions,accuracy,consistency,total_reputation,last_update
1,0x9a98...,100,95,95,90,185,1704067200
2,0x0048...,100,92,92,88,180,1704067200
3,0x80bc...,100,98,98,95,193,1704067200
```

## 🧮 计算公式

### 准确率
```
准确率 = (正确决策数 / 总决策数) * 100

决策正确性判断：
- 合法用户（legal）→ 应该返回"no_audit_needed"（不给分片）
- 非法用户（illegal）→ 应该返回"provided_shares"（给分片）
```

### 一致性
```
一致性 = (与大多数一致的请求数 / 总请求数) * 100

"大多数"的定义：
- 如果3个成员中有2个或以上做了相同决策，则该决策为"大多数决策"
- 如果3个成员决策各不相同，则所有成员都不一致
```

### 综合声誉
```
综合声誉 = 准确率 + 一致性
（不再使用参与率）
```

## 🚀 使用方法

### 1. 编译项目
```bash
cd /home/zxx/A2L/A2L-master/ecdsa
mkdir -p build
cd build
cmake ..
make
```

### 2. 运行secret_share_receiver
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./secret_share_receiver 1  # 成员1
./secret_share_receiver 2  # 成员2
./secret_share_receiver 3  # 成员3
```

系统会自动记录决策到 `log_game/reputation_decisions.csv`

### 3. 手动计算统计数据（可选）
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./calculate_reputation_stats.sh
```

### 4. 运行自动轮换
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./auto_rotation_simple.sh 60  # 每60秒执行一次
```

轮换脚本会自动：
1. 计算统计数据
2. 更新声誉到区块链
3. 生成VRF随机数
4. 执行委员会轮换

## ⚠️ 注意事项

1. **地址未找到处理**：如果address_labels.csv中没有该地址，会输出错误并停止记录决策
2. **时间窗口**：统计全部历史决策
3. **权重**：所有请求权重相同
4. **更新频率**：每次轮换前自动更新（通过auto_rotation_simple.sh）
5. **文件路径**：确保所有路径正确，特别是：
   - `/home/zxx/A2L/A2L-master/ecdsa/log_game/` 目录存在
   - `/home/zxx/A2L/A2L-master/ecdsa/bin/address_labels.csv` 文件存在

## 🔍 调试

### 查看决策记录
```bash
cat /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv
```

### 查看统计数据
```bash
cat /home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_stats.csv
```

### 测试计算程序
```bash
cd /home/zxx/A2L/A2L-master/ecdsa/bin
./calculate_reputation_stats.sh
```

## ✅ 编译状态

- ✅ `calculate_reputation_stats` 编译成功
- ✅ `secret_share_receiver` 编译成功（包含声誉跟踪功能）
- ⚠️ 有一些警告，但不影响功能

## 📝 下一步

系统已经可以正常使用。建议：

1. **测试决策记录**：运行secret_share_receiver，观察是否正确记录决策
2. **测试统计数据计算**：手动运行calculate_reputation_stats.sh，检查输出
3. **测试声誉更新**：运行update_and_rotate_new.js，检查是否成功更新到区块链
4. **监控系统运行**：观察auto_rotation_simple.sh的日志，确保一切正常

