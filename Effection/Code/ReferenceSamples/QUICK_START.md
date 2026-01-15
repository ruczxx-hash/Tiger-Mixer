# ReferenceSamples 快速开始指南

## 快速参考

### 文件说明

| 文件名 | 说明 | 生成方式 |
|--------|------|----------|
| `reference_illegal_rule.json` | 基于规则的攻击用户参考样本 | 程序运行时自动生成 |
| `reference_illegal_llm.json` | 基于LLM的攻击用户参考样本 | 程序运行时自动生成 |
| `reference_legitimate.json` | 合法用户参考样本 | 程序运行时自动生成 |

### 使用流程

1. **首次运行**：
   - 程序会自动检测参考样本文件是否存在
   - 如果不存在，会自动生成参考样本
   - 参考样本会保存在 `Code/ReferenceSamples/` 目录下

2. **后续运行**：
   - 如果参考样本已存在且攻击模式匹配，直接加载使用
   - 如果攻击模式不匹配，需要重新生成

3. **手动生成**：
   ```python
   from utils.reference_samples import generate_reference_samples
   
   # 需要提供必要的参数
   reference_illegal_samples, reference_legitimate_samples = generate_reference_samples(
       user_4amount_module, addr_df, addr_df_train
   )
   ```

### 配置说明

参考样本的路径配置在 `config/experiment_config.py` 中：

```python
REFERENCE_SAMPLES_DIR = "ReferenceSamples"
REFERENCE_ILLEGAL_FILE = f"{REFERENCE_SAMPLES_DIR}/reference_illegal_{ATTACK_MODE}.json"
REFERENCE_LEGITIMATE_FILE = f"{REFERENCE_SAMPLES_DIR}/reference_legitimate.json"
```

### 注意事项

- 参考样本文件（.json）通常由程序运行时生成，已添加到 `.gitignore`
- 如果需要版本控制，可以手动将生成的 JSON 文件添加到版本库
- 参考样本收集时间为 120 分钟（可在 `config/system_config.py` 中配置）

### 相关文档

- 详细说明请参考 `README.md`
- 格式示例请参考 `example_format.json`
