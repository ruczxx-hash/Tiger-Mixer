# 结果文件格式示例

本文档展示 Result 文件夹中各种文件的格式示例，方便理解和使用结果数据。

## 1. 交易记录文件格式

### 攻击用户交易记录
**文件路径**：`transactions/attack/Rule_Based_User_0.txt` 或 `transactions/attack/LLM_Based_User_0.txt`

**格式示例**：
```
time=0, amount=0.1
time=1, amount=1,10
time=2, amount=100
time=5, amount=0.1,1,10
```

**说明**：
- 每行表示一个时间点的交易
- `time={分钟数}`：交易发生的时间（从实验开始计时）
- `amount={金额1},{金额2},...`：该时间点的所有交易金额（用逗号分隔）

### 正常用户交易记录
**文件路径**：`transactions/normal/RetailUser_0.txt` 或 `transactions/normal/ExchangeUser_0.txt`

**格式示例**：
```
time=0, amount=0.1
time=3, amount=1
time=7, amount=10
time=12, amount=0.1,1
```

**说明**：格式与攻击用户交易记录相同

---

## 2. 统计文件格式

### Auditor统计文件（总体）
**文件路径**：`auditor_stats.txt`

**格式示例**：
```
time=10, auditor_judged_count=15, committee_judged_count=8, inconsistent_count=2, total_comparable=12, inconsistent_ratio=0.166667
time=20, auditor_judged_count=18, committee_judged_count=10, inconsistent_count=1, total_comparable=15, inconsistent_ratio=0.066667
time=30, auditor_judged_count=22, committee_judged_count=12, inconsistent_count=3, total_comparable=18, inconsistent_ratio=0.166667
```

**字段说明**：
- `time`：审计时间点（分钟）
- `auditor_judged_count`：Auditor评判的用户数
- `committee_judged_count`：交给委员会的用户数
- `inconsistent_count`：Auditor与委员会决策不一致的数量
- `total_comparable`：可比较的用户总数
- `inconsistent_ratio`：不一致比例（inconsistent_count / total_comparable）

### Auditor统计文件（合法用户）
**文件路径**：`auditor_stats_normal.txt`

**格式示例**：
```
time=10, auditor_judged_count=5, committee_judged_count=2, inconsistent_count=0, total_comparable=3, inconsistent_ratio=0.000000
time=20, auditor_judged_count=6, committee_judged_count=3, inconsistent_count=1, total_comparable=4, inconsistent_ratio=0.250000
```

**说明**：格式与总体统计文件相同，但仅针对合法用户

### Auditor统计文件（非法用户）
**文件路径**：`auditor_stats_illegal.txt`

**格式示例**：
```
time=10, auditor_judged_count=10, committee_judged_count=6, inconsistent_count=2, total_comparable=9, inconsistent_ratio=0.222222
time=20, auditor_judged_count=12, committee_judged_count=7, inconsistent_count=2, total_comparable=11, inconsistent_ratio=0.181818
```

**说明**：格式与总体统计文件相同，但仅针对非法用户

### 委员会投票统计文件
**文件路径**：`committee_vote_stats.txt`

**格式示例**：
```
time=10, illegal_user_count=3
  user_index=0, legit_approve=8/9 (ratio=0.888889), illegal_approve=2/3 (ratio=0.666667), final_result=ban
  user_index=1, legit_approve=7/9 (ratio=0.777778), illegal_approve=1/3 (ratio=0.333333), final_result=ban
  user_index=2, legit_approve=5/9 (ratio=0.555556), illegal_approve=3/3 (ratio=1.000000), final_result=allow
time=20, illegal_user_count=2
  user_index=3, legit_approve=9/9 (ratio=1.000000), illegal_approve=0/3 (ratio=0.000000), final_result=ban
  user_index=4, legit_approve=6/9 (ratio=0.666667), illegal_approve=2/3 (ratio=0.666667), final_result=ban
```

**字段说明**：
- `time`：投票时间点（分钟）
- `illegal_user_count`：该时间点投票的非法用户数量
- `user_index`：用户索引
- `legit_approve`：合法委员赞成数/合法委员总数
- `illegal_approve`：非法委员赞成数/非法委员总数
- `final_result`：最终投票结果（`ban` 或 `allow`）

---

## 3. 数据文件格式

### 静态数据文件
**文件路径**：`StaticData/{RUN_ID}.txt`

**格式示例**：
```
time=10, first_precision=0.800000, first_recall=0.600000, second_precision=0.750000, second_recall=0.500000, third_precision=0.900000, third_recall=0.700000, total_precision=0.850000, total_recall=0.650000
time=20, first_precision=0.850000, first_recall=0.650000, second_precision=0.800000, second_recall=0.600000, third_precision=0.950000, third_recall=0.750000, total_precision=0.900000, total_recall=0.700000
time=30, first_precision=0.900000, first_recall=0.700000, second_precision=0.850000, second_recall=0.650000, third_precision=0.980000, third_recall=0.800000, total_precision=0.950000, total_recall=0.750000
```

**字段说明**：
- `time`：统计时间点（分钟）
- `first_precision` / `first_recall`：第一道关卡（IF层）的precision和recall
- `second_precision` / `second_recall`：第二道关卡（Auditor层）的precision和recall
- `third_precision` / `third_recall`：第三道关卡（Committee层）的precision和recall
- `total_precision` / `total_recall`：总体precision和recall

### 精度文件
**文件路径**：`StaticData/precision.txt`

**格式示例**：
```
time=10, IF_precision=0.800000, Auditor_precision=0.750000, Committee_precision=0.900000
time=20, IF_precision=0.850000, Auditor_precision=0.800000, Committee_precision=0.950000
time=30, IF_precision=0.900000, Auditor_precision=0.850000, Committee_precision=0.980000
```

**字段说明**：
- `time`：统计时间点（分钟）
- `IF_precision`：Isolation Forest层的累计precision
- `Auditor_precision`：Auditor层的累计precision
- `Committee_precision`：Committee层的累计precision

---

## 4. 信息文件格式

### 委员会信息文件
**文件路径**：`committee_info.txt`

**格式示例**：
```
#0 | type=Legit, weight=1.0, rep=1000
#1 | type=Legit, weight=1.0, rep=1000
#2 | type=Legit, weight=1.0, rep=1000
...
#26 | type=Legit, weight=1.0, rep=1000
#27 | type=Illegal, weight=1.0, rep=1000
#28 | type=Illegal, weight=1.0, rep=1000
#29 | type=Illegal, weight=1.0, rep=1000
```

**字段说明**：
- `#{索引}`：委员会成员索引
- `type`：身份类型（`Legit` 或 `Illegal`）
- `weight`：权重
- `rep`：声誉值

### 用户信息文件
**文件路径**：`rule_users_info.txt` 或 `llm_users_info.txt`

**格式示例**：
```
#0 | alive=False, task=10000
#1 | alive=False, task=10000
#2 | alive=True, task=10000
#3 | alive=False, task=10000
...
#19 | alive=False, task=60000
```

**字段说明**：
- `#{索引}`：用户索引
- `alive`：是否存活（`True` 表示未被禁止，`False` 表示已被禁止）
- `task`：任务量（目标转账金额）

---

## 5. 结果文件格式

### 结果统计文件
**文件路径**：`Result Statistic.txt`

**格式示例**：
```
Money Transferred Ration between Normal and Attacker: 15.234567
Banned Normal User Num: 5
Banned Attacker User Num: 18
Marked Normal User Num: 14
Marked Attacker User Num: 2
```

**字段说明**：
- `Money Transferred Ration between Normal and Attacker`：正常用户与攻击用户的转账金额比例
- `Banned Normal User Num`：被禁止的正常用户数量
- `Banned Attacker User Num`：被禁止的攻击用户数量
- `Marked Normal User Num`：被标记的正常用户数量
- `Marked Attacker User Num`：被标记的攻击用户数量

### 转账总额记录文件
**文件路径**：`transferred_total_records.txt`

**格式示例**：
```
time,transferred_total
0,0
10,1250.5
20,3450.2
30,5670.8
40,7890.3
50,10120.7
```

**字段说明**：
- `time`：时间点（分钟）
- `transferred_total`：所有攻击用户在该时间点的累计总转账金额

---

## 6. 日志文件格式

### Auditor日志文件
**文件路径**：`auditor.txt`

**格式示例**：
```
[时间戳] Auditor检测用户 #0
[时间戳] 特征提取完成
[时间戳] Isolation Forest评分: -0.15
[时间戳] LLM检测结果: suspicious
[时间戳] 最终决策: ban
...
```

**说明**：记录Auditor的详细检测过程和决策日志

---

## 使用建议

1. **数据分析**：可以使用Python的pandas库读取CSV格式的文件（如 `transferred_total_records.txt`）
2. **文本解析**：其他文件可以使用正则表达式或简单的字符串分割来解析
3. **可视化**：可以使用 `StaticData/{RUN_ID}.txt` 和 `StaticData/precision.txt` 绘制precision和recall曲线
4. **对比分析**：可以对比不同 `RUN_ID` 文件夹中的结果，分析不同实验配置的效果
