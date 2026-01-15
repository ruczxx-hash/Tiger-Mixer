# 代码结构说明

本文档介绍重构后的代码结构。代码已按照功能模块进行拆分，提高了可读性和可维护性。

## 目录结构

```
Code/
├── config/                    # 配置模块
│   ├── __init__.py
│   └── experiment_config.py   # 实验配置（攻击模式、路径等）
│
├── models/                    # 数据模型模块
│   ├── __init__.py
│   ├── user.py               # 用户相关类（User基类、RetailUser、ExchangeUser、Rule_Based_User、LLM_Based_User）
│   └── committee.py          # 委员会相关类（Candidate、Committee）
│
├── agents/                    # 代理模块
│   ├── __init__.py
│   ├── llm_strategy_agent.py # LLM策略代理（用于攻击用户）
│   └── llm_agent.py          # LLM代理（用于委员会投票）
│
├── monitors/                  # 监控器模块
│   ├── __init__.py
│   └── crypto_monitor.py     # 加密监控器（CryptoMonitor类）
│
├── utils/                     # 工具函数模块
│   ├── __init__.py
│   ├── file_utils.py         # 文件操作工具（保存委员会信息、用户信息、交易记录）
│   └── reference_samples.py  # 参考样本处理（加载、保存、生成参考用户交易记录）
│
├── Result/                    # 结果文件夹
│   ├── README.md             # Result文件夹说明文档
│   ├── EXAMPLE_FORMATS.md    # 结果文件格式示例
│   ├── transactions/         # 交易记录目录
│   │   ├── attack/          # 攻击用户交易记录
│   │   └── normal/          # 正常用户交易记录
│   └── StaticData/          # 静态数据目录
│
├── main.py                    # 主程序入口
└── README.md                  # 本文档
```

## 模块说明

### 1. config/experiment_config.py
**作用**：实验配置管理
- 定义攻击模式（ATTACK_MODE: "rule" 或 "llm"）
- 定义运行ID和结果目录路径
- 创建必要的目录结构

**主要变量**：
- `ATTACK_MODE`: 攻击模式
- `RUN_ID`: 运行标识符
- `RESULT_DIR`: 结果保存目录
- `REFERENCE_SAMPLES_DIR`: 参考样本目录

### 2. models/user.py
**作用**：用户模型定义
- **User**: 用户基类，包含交易记录、特征提取等基础功能
- **RetailUser**: 零售用户（合法用户）
- **ExchangeUser**: 交易所用户（合法用户）
- **Rule_Based_User**: 基于规则的攻击用户
- **LLM_Based_User**: 基于LLM的攻击用户

**主要功能**：
- 交易记录管理
- 特征提取和更新
- 用户决策逻辑

### 3. models/committee.py
**作用**：委员会模型定义
- **Candidate**: 候选人类（基类）
- **Committee**: 委员会类，负责投票决策

**主要功能**：
- 委员会成员投票
- 声誉和权重更新
- 合法/非法委员的投票逻辑

### 4. agents/llm_strategy_agent.py
**作用**：LLM策略代理（用于攻击用户）
- 分析失败案例，学习检测模式
- 生成交易决策计划
- 验证和修复计划

**主要方法**：
- `analyze_failure()`: 分析被禁止用户的交易模式
- `get_decision()`: 生成下一步交易计划

### 5. agents/llm_agent.py
**作用**：LLM代理（用于委员会投票）
- 合法委员投票逻辑
- 非法委员投票逻辑

**主要方法**：
- `get_legit_vote()`: 合法委员投票
- `get_corrupt_vote()`: 非法委员投票

### 6. monitors/crypto_monitor.py
**作用**：加密监控器
- 使用Isolation Forest进行批量审计
- 使用LLM进行单用户检测
- 特征提取和异常诊断

**主要方法**：
- `batch_audit()`: 批量审计（第一道关卡）
- `detect()`: 单用户检测（第二道关卡）
- `train_isolation_forest()`: 训练Isolation Forest模型

### 7. utils/file_utils.py
**作用**：文件操作工具
- `save_committee_info()`: 保存委员会信息
- `save_user_info()`: 保存用户信息
- `save_transactions_to_file()`: 保存交易记录

### 8. utils/reference_samples.py
**作用**：参考样本处理
- `load_reference_samples()`: 加载参考样本
- `save_reference_samples()`: 保存参考样本
- `generate_reference_samples()`: 生成参考样本

### 9. main.py
**作用**：主程序入口
- 初始化用户和委员会
- 运行主循环（三层拦截机制）
- 统计和结果保存
- 绘制图表

**主要流程**：
1. 加载/生成参考样本
2. 初始化用户和委员会
3. 主循环（每10分钟进行一次审计）：
   - 第一道关卡：Isolation Forest批量审计
   - 第二道关卡：Auditor LLM检测
   - 第三道关卡：委员会投票
4. 保存结果和统计信息

## 三层拦截机制

1. **IF层（第一道关卡）**：使用Isolation Forest进行批量异常检测
2. **Auditor层（第二道关卡）**：使用LLM对可疑用户进行详细检测
3. **Committee层（第三道关卡）**：委员会投票决定是否禁止用户

## 注意事项

1. **代码未修改**：重构过程中保持了所有代码逻辑和参数不变，仅进行了模块拆分和导入路径调整
2. **导入路径**：新代码使用相对导入，需要确保Python路径正确
3. **原始文件**：原始文件（main.py, user_4amount.py, candidate.py）保持不变

## 使用说明

1. 确保所有依赖已安装
2. 运行 `Code/main.py` 开始实验
3. 结果保存在 `Result/{RUN_ID}/` 目录下
4. 详细的结果文件说明请参考 `Result/README.md` 和 `Result/EXAMPLE_FORMATS.md`

## 文件对应关系

| 原始文件 | 新文件位置 |
|---------|-----------|
| main.py | Code/main.py（重构后） |
| user_4amount.py | Code/models/user.py + Code/agents/llm_strategy_agent.py + Code/monitors/crypto_monitor.py |
| candidate.py | Code/models/committee.py + Code/agents/llm_agent.py |
