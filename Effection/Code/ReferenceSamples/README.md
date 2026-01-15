# ReferenceSamples 参考样本文件夹

## 概述

`ReferenceSamples` 文件夹用于存储参考用户交易记录，这些样本在委员会投票决策时作为对比基准，帮助识别异常交易行为。

## 文件结构

```
ReferenceSamples/
├── reference_illegal_rule.json    # 基于规则的攻击用户参考样本
├── reference_illegal_llm.json      # 基于LLM的攻击用户参考样本
├── reference_legitimate.json      # 合法用户参考样本
└── README.md                       # 本说明文档
```

## 文件说明

### 1. 非法用户参考样本

**文件名格式**：`reference_illegal_{ATTACK_MODE}.json`

- `reference_illegal_rule.json`：基于规则的攻击用户参考样本
- `reference_illegal_llm.json`：基于LLM的攻击用户参考样本

**文件格式**：
```json
{
  "attack_mode": "rule" | "llm",
  "samples": [
    [
      {
        "time": 0,
        "amount": 10
      },
      {
        "time": 3,
        "amount": 100
      }
      // ... 更多交易记录
    ]
  ]
}
```

**字段说明**：
- `attack_mode`：攻击模式（"rule" 或 "llm"），用于匹配当前实验的攻击模式
- `samples`：参考样本数组，每个元素是一个用户的完整交易记录列表
- 每个交易记录包含：
  - `time`：交易时间（分钟）
  - `amount`：交易金额（ETH），必须是允许的金额之一：0.1, 1, 10, 100

### 2. 合法用户参考样本

**文件名**：`reference_legitimate.json`

**文件格式**：
```json
{
  "samples": [
    [
      {
        "time": 130,
        "amount": 0.1
      },
      {
        "time": 202,
        "amount": 1
      }
      // ... 更多交易记录
    ],
    // ... 更多合法用户的交易记录
  ]
}
```

**字段说明**：
- `samples`：参考样本数组，包含多个合法用户的交易记录
- 每个用户的交易记录格式与非法用户相同
- 通常包含：
  - 3个 RetailUser（零售用户）的交易记录
  - 1个 ExchangeUser（交易所用户）的交易记录

## 生成参考样本

参考样本通过 `utils/reference_samples.py` 中的 `generate_reference_samples()` 函数生成：

1. **非法用户参考样本**：
   - 根据 `ATTACK_MODE` 创建对应的攻击用户（Rule_Based_User 或 LLM_Based_User）
   - 运行用户120分钟收集交易记录
   - 保存为对应的 JSON 文件

2. **合法用户参考样本**：
   - 创建3个 RetailUser（零售用户）
   - 创建1个 ExchangeUser（交易所用户）
   - 运行用户120分钟收集交易记录
   - 保存为 `reference_legitimate.json`

## 使用场景

参考样本在以下场景中使用：

1. **委员会投票决策**：
   - 合法委员在投票时会对比目标用户的交易模式与参考样本
   - 通过对比交易频率、交易金额等特征来判断用户是否异常

2. **LLM 策略代理学习**：
   - LLM_Strategy_Agent 可以分析被禁止用户的交易模式
   - 学习如何避免被检测

3. **异常检测**：
   - CryptoMonitor 可以使用参考样本进行特征提取和异常诊断

## 加载和使用

参考样本通过 `utils/reference_samples.py` 中的函数加载：

```python
from utils.reference_samples import load_reference_samples

# 加载参考样本
reference_illegal_samples, reference_legitimate_samples = load_reference_samples()
```

**加载逻辑**：
- 如果文件存在且攻击模式匹配，则加载对应的参考样本
- 如果文件不存在或攻击模式不匹配，需要重新生成参考样本

## 注意事项

1. **攻击模式匹配**：
   - 非法用户参考样本必须与当前实验的 `ATTACK_MODE` 匹配
   - 如果模式不匹配，系统会提示需要重新生成

2. **文件路径**：
   - 参考样本文件路径由 `config/experiment_config.py` 中的 `REFERENCE_SAMPLES_DIR` 配置
   - 默认路径为 `ReferenceSamples/`

3. **数据格式**：
   - 交易金额必须是允许的金额之一：0.1, 1, 10, 100（ETH）
   - 交易时间以分钟为单位

4. **版本控制**：
   - 建议将参考样本文件添加到 `.gitignore`，因为它们是运行时生成的
   - 或者保留示例文件用于文档说明

## 相关文件

- `Code/utils/reference_samples.py`：参考样本处理工具
- `Code/config/experiment_config.py`：实验配置（包含参考样本路径配置）
- `Code/agents/llm_agent.py`：使用参考样本进行投票决策
- `Code/agents/llm_strategy_agent.py`：使用参考样本学习攻击策略
