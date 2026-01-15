# DataSet 数据集说明文档

本目录包含模拟系统运行所需的所有数据文件。

## 📁 目录结构

```
DataSet/
├── address.csv                    # 地址数据文件（备用）
├── eth_10000.csv                 # 以太坊地址文件1（10000个地址）
├── eth_20000.csv                 # 以太坊地址文件2（20000个地址，用于训练集）
├── ReferenceSamples/             # 参考样本目录
│   ├── reference_illegal_rule.json    # 基于规则的攻击用户参考样本
│   ├── reference_illegal_llm.json     # 基于LLM的攻击用户参考样本
│   └── reference_legitimate.json      # 合法用户参考样本
└── transaction/                  # 交易数据目录
    ├── 0x{address1}.csv          # 用户1的交易记录
    ├── 0x{address2}.csv          # 用户2的交易记录
    └── ...                       # 更多用户交易记录（共12370个文件）
```

## 📄 文件说明

### 1. 地址文件

#### `eth_10000.csv`
- **用途**：用于交易所用户（ExchangeUser）的地址数据
- **格式**：CSV文件，每行一个以太坊地址
- **数量**：10000个地址
- **使用位置**：`Code/config/system_config.py` 中的 `ETH_ADDRESS_FILE`

#### `eth_20000.csv`
- **用途**：用于零售用户（RetailUser）的地址数据
- **格式**：CSV文件，每行一个以太坊地址
- **数量**：20000个地址
- **使用位置**：`Code/config/system_config.py` 中的 `ETH_ADDRESS_TRAIN_FILE`

#### `address.csv`
- **用途**：备用地址文件
- **说明**：当前未使用，保留作为备用

### 2. 交易数据目录 (`transaction/`)

#### 文件命名规则
- **格式**：`{以太坊地址}.csv`
- **示例**：`0x910cbd523d972eb0a6f4cae4618ad62622b39dbf.csv`

#### 文件格式
每个CSV文件包含一个用户的交易记录，包含以下列：

| 列名 | 类型 | 说明 |
|------|------|------|
| DateTime | string | 交易时间，格式：`YYYY/MM/DD HH:MM` |
| Value_IN | float | 交易金额（ETH） |
| Method | string | 交易方法 |

#### 示例数据
```csv
DateTime,Value_IN,Method
2025/04/01 00:00,0.1,transfer
2025/04/01 00:05,1.0,transfer
2025/04/01 00:10,10.0,transfer
```

#### 使用位置
- **配置**：`Code/config/system_config.py` 中的 `TX_DATA_DIR`
- **读取代码**：`Code/models/user.py` 中的 `init_user_from_eth_csv()` 方法

### 3. 参考样本目录 (`ReferenceSamples/`)

参考样本用于委员会投票时的参考，包含合法用户和非法用户的交易模式示例。

#### `reference_illegal_rule.json`
- **用途**：基于规则的攻击用户参考样本
- **生成时机**：当 `ATTACK_MODE="rule"` 时自动生成
- **格式**：JSON文件
- **内容结构**：
```json
{
  "attack_mode": "rule",
  "samples": [
    [
      {"time": 0, "amount": 0.1},
      {"time": 5, "amount": 1.0},
      ...
    ]
  ]
}
```

#### `reference_illegal_llm.json`
- **用途**：基于LLM的攻击用户参考样本
- **生成时机**：当 `ATTACK_MODE="llm"` 时自动生成
- **格式**：JSON文件
- **内容结构**：同 `reference_illegal_rule.json`

#### `reference_legitimate.json`
- **用途**：合法用户参考样本
- **生成时机**：程序首次运行时自动生成
- **格式**：JSON文件
- **内容结构**：
```json
{
  "samples": [
    [
      {"time": 0, "amount": 0.1},
      {"time": 10, "amount": 1.0},
      ...
    ],
    ...
  ]
}
```

#### 使用位置
- **配置**：`Code/config/experiment_config.py` 中的 `REFERENCE_SAMPLES_DIR`
- **读取代码**：`Code/utils/reference_samples.py` 中的 `load_reference_samples()`
- **生成代码**：`Code/utils/reference_samples.py` 中的 `generate_reference_samples()`

## 🔧 配置说明

### 环境变量配置

可以通过环境变量自定义数据文件路径：

```bash
# 设置数据集根目录（默认：DataSet）
export DATA_SET_DIR="DataSet"

# 设置地址文件路径（可选，默认使用DataSet目录下的文件）
export ETH_ADDRESS_FILE="DataSet/eth_10000.csv"
export ETH_ADDRESS_TRAIN_FILE="DataSet/eth_20000.csv"

# 设置交易数据目录（可选，默认使用DataSet/transaction）
export TX_DATA_DIR="DataSet/transaction"
```

### 代码配置

所有数据文件路径在以下配置文件中定义：

1. **`Code/config/system_config.py`**
   - `ETH_ADDRESS_FILE`：地址文件1路径
   - `ETH_ADDRESS_TRAIN_FILE`：地址文件2路径
   - `TX_DATA_DIR`：交易数据目录路径

2. **`Code/config/experiment_config.py`**
   - `REFERENCE_SAMPLES_DIR`：参考样本目录路径

## 📊 数据统计

- **地址文件**：2个（eth_10000.csv, eth_20000.csv）
- **交易文件**：约12,370个CSV文件
- **参考样本文件**：3个JSON文件（根据ATTACK_MODE使用不同的非法用户样本）

## ⚠️ 注意事项

1. **文件完整性**：确保所有必需的数据文件存在，否则程序可能无法正常运行
2. **路径一致性**：确保配置文件中的路径与实际文件位置一致
3. **参考样本生成**：参考样本文件在首次运行时自动生成，如果文件不存在或模式不匹配，程序会自动重新生成
4. **交易文件格式**：交易CSV文件必须包含 `DateTime`、`Value_IN`、`Method` 三列
5. **地址格式**：地址文件中的地址必须是有效的以太坊地址格式

## 🔄 数据更新

### 更新地址文件
如果需要更新地址文件，请：
1. 将新文件放入 `DataSet/` 目录
2. 更新 `Code/config/system_config.py` 中的文件路径（如需要）

### 更新交易数据
如果需要更新交易数据：
1. 将新的交易CSV文件放入 `DataSet/transaction/` 目录
2. 确保文件名格式为 `{以太坊地址}.csv`
3. 确保文件格式符合要求（包含DateTime、Value_IN、Method列）

### 更新参考样本
参考样本会在以下情况自动更新：
- 文件不存在
- 文件中的 `attack_mode` 与当前 `ATTACK_MODE` 不匹配
- 文件格式错误

## 📝 相关文档

- **配置文件说明**：`Code/config/README.md`
- **结果文件说明**：`Code/Result/README.md`
- **参考样本详细说明**：`Code/ReferenceSamples/README.md`

## 🐛 常见问题

### Q1: 程序提示找不到地址文件
**A**: 检查 `Code/config/system_config.py` 中的路径配置是否正确，确保文件存在于指定位置。

### Q2: 用户交易记录为空
**A**: 检查 `DataSet/transaction/` 目录中是否存在对应的地址CSV文件，文件名必须与用户地址完全匹配。

### Q3: 参考样本文件不存在
**A**: 这是正常的，程序会在首次运行时自动生成。如果希望手动生成，可以运行 `Code/utils/reference_samples.py` 中的 `generate_reference_samples()` 函数。

### Q4: 如何添加新的交易数据
**A**: 将新的CSV文件放入 `DataSet/transaction/` 目录，文件名格式为 `{以太坊地址}.csv`，确保包含必需的列。

## 📧 联系与支持

如有问题或建议，请查看项目主README或联系项目维护者。
