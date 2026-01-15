# Result 文件夹说明

本文件夹用于保存实验运行的结果数据。每次运行实验时，会根据运行时间戳创建一个新的子文件夹。

## 目录结构

```
Result/
├── {RUN_ID}/                    # 运行ID（格式：YYYYMMDD_HHMMSS）
│   ├── transactions/           # 交易记录目录
│   │   ├── attack/             # 攻击用户交易记录
│   │   │   ├── Rule_Based_User_0.txt
│   │   │   ├── Rule_Based_User_1.txt
│   │   │   └── ...
│   │   └── normal/             # 正常用户交易记录
│   │       ├── RetailUser_0.txt
│   │       ├── ExchangeUser_0.txt
│   │       └── ...
│   │
│   ├── StaticData/             # 静态数据目录
│   │   ├── {RUN_ID}.txt        # 静态数据文件（包含各层precision和recall）
│   │   ├── precision.txt       # 三层拦截机制累计precision
│   │   └── precision_plot.pdf  # 精度图表（自动生成）
│   │
│   ├── auditor_stats.txt        # Auditor统计文件（总体）
│   ├── auditor_stats_normal.txt # Auditor统计文件（合法用户）
│   ├── auditor_stats_illegal.txt # Auditor统计文件（非法用户）
│   ├── committee_vote_stats.txt # 委员会投票统计文件
│   ├── committee_info.txt      # 委员会信息文件
│   ├── {ATTACK_MODE}_users_info.txt # 用户信息文件（rule_users_info.txt 或 llm_users_info.txt）
│   ├── {ATTACK_MODE}_based_plot.pdf # 用户交易图表（rule_based_plot.pdf 或 llm_based_plot.pdf）
│   ├── Result Statistic.txt    # 结果统计文件
│   ├── transferred_total_records.txt # 转账总额记录文件
│   └── auditor.txt             # Auditor日志文件
│
└── README.md                    # 本文档
```

## 文件说明

### 交易记录文件

#### `transactions/attack/{UserType}_{index}.txt`
- **格式**：每行一条交易记录
- **内容**：`time={时间}, amount={金额1},{金额2},...`
- **说明**：记录攻击用户在每次时间点的交易金额

#### `transactions/normal/{UserType}_{index}.txt`
- **格式**：每行一条交易记录
- **内容**：`time={时间}, amount={金额1},{金额2},...`
- **说明**：记录正常用户在每次时间点的交易金额

### 统计文件

#### `auditor_stats.txt`
- **格式**：每行一条统计记录
- **内容**：`time={时间}, auditor_judged_count={评判用户数}, committee_judged_count={交给委员会数}, inconsistent_count={不一致数}, total_comparable={可比较总数}, inconsistent_ratio={不一致比例}`
- **说明**：记录Auditor与委员会决策的总体一致性统计

#### `auditor_stats_normal.txt`
- **格式**：同 `auditor_stats.txt`
- **说明**：仅针对合法用户的Auditor统计

#### `auditor_stats_illegal.txt`
- **格式**：同 `auditor_stats.txt`
- **说明**：仅针对非法用户的Auditor统计

#### `committee_vote_stats.txt`
- **格式**：每行一条投票统计记录
- **内容**：
  ```
  time={时间}, illegal_user_count={非法用户数}
    user_index={用户索引}, legit_approve={合法委员赞成数}/{合法委员总数} (ratio={比例}), illegal_approve={非法委员赞成数}/{非法委员总数} (ratio={比例}), final_result={最终结果}
  ```
- **说明**：记录委员会对非法用户的投票详情

### 数据文件

#### `StaticData/{RUN_ID}.txt`
- **格式**：每行一条数据记录
- **内容**：`time={时间}, first_precision={第一层precision}, first_recall={第一层recall}, second_precision={第二层precision}, second_recall={第二层recall}, third_precision={第三层precision}, third_recall={第三层recall}, total_precision={总precision}, total_recall={总recall}`
- **说明**：记录三层拦截机制的precision和recall指标

#### `StaticData/precision.txt`
- **格式**：每行一条精度记录
- **内容**：`time={时间}, IF_precision={IF层precision}, Auditor_precision={Auditor层precision}, Committee_precision={Committee层precision}`
- **说明**：记录三层拦截机制的累计precision

### 信息文件

#### `committee_info.txt`
- **格式**：每行一个委员会成员信息
- **内容**：`#{索引} | type={身份类型}, weight={权重}, rep={声誉}`
- **说明**：记录委员会成员的初始状态

#### `{ATTACK_MODE}_users_info.txt`
- **格式**：每行一个用户信息
- **内容**：`#{索引} | alive={是否存活}, task={任务量}`
- **说明**：记录攻击用户的最终状态

### 结果文件

#### `Result Statistic.txt`
- **格式**：多行统计信息
- **内容**：
  ```
  Money Transferred Ration between Normal and Attacker: {比例}
  Banned Normal User Num: {被禁止的正常用户数}
  Banned Attacker User Num: {被禁止的攻击用户数}
  Marked Normal User Num: {被标记的正常用户数}
  Marked Attacker User Num: {被标记的攻击用户数}
  ```
- **说明**：实验最终统计结果

#### `transferred_total_records.txt`
- **格式**：CSV格式，第一行为表头
- **内容**：
  ```
  time,transferred_total
  0,{总金额}
  10,{总金额}
  ...
  ```
- **说明**：记录每10分钟所有攻击用户的总转账金额

### 日志文件

#### `auditor.txt`
- **格式**：Auditor的详细日志
- **说明**：记录Auditor的检测过程和决策详情

### 图表文件

#### `{ATTACK_MODE}_based_plot.pdf`
- **说明**：攻击用户交易行为可视化图表

#### `StaticData/precision_plot.pdf`
- **说明**：三层拦截机制precision和recall的可视化图表

## 使用说明

1. **运行实验**：运行 `Code/main.py` 后，会自动创建新的结果文件夹
2. **查看结果**：根据 `RUN_ID` 进入对应的子文件夹查看结果
3. **分析数据**：可以使用统计文件进行数据分析
4. **可视化**：查看自动生成的PDF图表文件

## 注意事项

1. 每次运行实验都会创建新的 `{RUN_ID}` 文件夹，不会覆盖之前的结果
2. 如果实验中断，已保存的文件仍然有效
3. 文件编码均为 UTF-8
4. 时间单位：分钟（从实验开始计时）
