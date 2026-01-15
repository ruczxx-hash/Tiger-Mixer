# 配置模块说明

本目录包含项目的所有配置模块，按照功能分类组织。

## 文件结构

```
config/
├── __init__.py              # 配置模块导出
├── experiment_config.py      # 实验配置（攻击模式、路径等）
├── llm_config.py            # LLM API 配置
├── system_config.py         # 系统级配置（委员会、用户数量等）
└── README.md                # 本文档
```

## 配置模块说明

### 1. experiment_config.py

**作用**：实验配置管理

**主要配置项**：
- `ATTACK_MODE`: 攻击模式（"rule" 或 "llm"）
- `RUN_ID`: 运行标识符（基于时间戳）
- `RESULT_DIR`: 结果保存目录
- `TX_DIR`: 交易记录目录
- `REFERENCE_SAMPLES_DIR`: 参考样本目录
- 各种统计文件路径

**特性**：
- 支持通过环境变量 `ATTACK_MODE` 设置攻击模式
- 自动创建必要的目录结构

### 2. llm_config.py

**作用**：LLM API 相关配置

**主要配置项**：
- `LLM_API_KEY`: API 密钥（建议通过环境变量设置）
- `LLM_BASE_URL`: API 基础 URL
- `LLM_MODEL`: 模型名称（默认："qwen3-max"）
- `LLM_TIMEOUT`: 请求超时时间（秒，默认：30.0）
- `LLM_MAX_RETRIES`: 最大重试次数（默认：2）
- `LLM_TEMPERATURE_DEFAULT`: 默认温度（默认：0.6）
- `LLM_TEMPERATURE_STRATEGY`: 策略代理温度（默认：0.3）
- `LLM_TEMPERATURE_MONITOR`: 监控器温度（默认：0.1）
- `LLM_MAX_ATTEMPTS`: 最大尝试次数（默认：3）
- `LLM_SLEEP_TIME`: 重试间隔（秒，默认：60）

**环境变量支持**：
所有配置项都支持通过环境变量设置，例如：
```bash
export LLM_API_KEY="your-api-key"
export LLM_MODEL="qwen3-max"
export LLM_TIMEOUT="60.0"
```

### 3. system_config.py

**作用**：系统级配置和业务参数

**主要配置项**：

**业务参数**：
- `ALLOWED_AMOUNTS`: 允许的交易金额集合（{0.1, 1, 10, 100} ETH）

**委员会配置**：
- `NUM_LEGIT_COMMITTEES`: 合法委员会成员数量（默认：27）
- `NUM_ILLEGAL_COMMITTEES`: 非法委员会成员数量（默认：3）
- `COMMITTEE_SELECT_COUNT`: 每次选择的委员会成员数量（默认：10）
- `COMMITTEE_INITIAL_WEIGHT`: 委员会初始权重（默认：1000）

**用户配置**：
- `NUM_RETAIL_USERS`: 零售用户数量（默认：19980）
- `NUM_EXCHANGE_USERS`: 交易所用户数量（默认：20）
- `NUM_ATTACK_USERS`: 攻击用户总数（默认：10）
- `NUM_SMALL_TASK_ATTACKERS`: 小任务攻击用户数量（默认：16）
- `NUM_LARGE_TASK_ATTACKERS`: 大任务攻击用户数量（默认：4）
- `SMALL_TASK_AMOUNT`: 小任务金额（ETH，默认：10000）
- `LARGE_TASK_AMOUNT`: 大任务金额（ETH，默认：60000）
- `TASK_TIME_LEFT`: 任务剩余时间（分钟，默认：4320）

**数据文件配置**：
- `ETH_ADDRESS_FILE`: 地址文件路径（默认："eth_10000.csv"）
- `ETH_ADDRESS_TRAIN_FILE`: 训练集地址文件路径（默认："eth_20000.csv"）

**监控配置**：
- `IF_CONTAMINATION`: Isolation Forest 污染率（默认：0.1）
- `IF_RANDOM_STATE`: 随机种子（默认：42）
- `AUDIT_INTERVAL`: 审计间隔（分钟，默认：10）
- `REFERENCE_SAMPLE_COLLECTION_TIME`: 参考样本收集时间（分钟，默认：120）

**特征提取配置**：
- `RECENT_TRANSACTION_COUNT`: 用于 LLM 分析的最近交易数量（默认：20）
- `FAILURE_ANALYSIS_LOOKBACK`: 失败分析回看时间（分钟，默认：120）

**委员会投票配置**：
- `CORRUPT_VOTE_THRESHOLD`: 非法委员投票阈值（ETH，默认：8000）

**环境变量支持**：
所有配置项都支持通过环境变量设置。

## 使用方法

### 方式1：直接导入（推荐）

```python
from config import (
    ATTACK_MODE,
    LLM_API_KEY,
    LLM_BASE_URL,
    NUM_LEGIT_COMMITTEES,
    ALLOWED_AMOUNTS
)
```

### 方式2：导入整个配置模块

```python
from config import *
```

### 方式3：导入特定配置模块

```python
from config.experiment_config import ATTACK_MODE, RUN_ID
from config.llm_config import LLM_API_KEY, LLM_MODEL
from config.system_config import NUM_LEGIT_COMMITTEES
```

## 环境变量配置

为了安全起见，建议将敏感信息（如 API 密钥）通过环境变量设置：

**Windows (PowerShell)**：
```powershell
$env:LLM_API_KEY="your-api-key"
$env:ATTACK_MODE="llm"
```

**Linux/Mac**：
```bash
export LLM_API_KEY="your-api-key"
export ATTACK_MODE="llm"
```

或者在 `.env` 文件中设置（需要安装 python-dotenv）：
```
LLM_API_KEY=your-api-key
LLM_BASE_URL=https://dashscope.aliyuncs.com/compatible-mode/v1
LLM_MODEL=qwen3-max
ATTACK_MODE=llm
```

## 注意事项

1. **API 密钥安全**：不要将 API 密钥提交到版本控制系统，建议使用环境变量
2. **配置优先级**：环境变量 > 配置文件默认值
3. **目录创建**：`experiment_config.py` 会在导入时自动创建必要的目录结构
4. **配置修改**：修改配置后需要重启程序才能生效

## 配置验证

可以通过以下方式验证配置是否正确：

```python
from config import *
print(f"攻击模式: {ATTACK_MODE}")
print(f"LLM模型: {LLM_MODEL}")
print(f"合法委员会数量: {NUM_LEGIT_COMMITTEES}")
```
