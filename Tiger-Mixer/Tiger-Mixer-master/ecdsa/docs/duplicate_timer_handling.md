# 重复计时器名称的处理方式

## 当前实现

### 1. `record_timing()` 函数

```c
void record_timing(const char* name, double duration_ms) {
    if (timing_count < 50) {
        strncpy(timing_records[timing_count].name, name, 127);
        timing_records[timing_count].name[127] = '\0';
        timing_records[timing_count].duration_ms = duration_ms;
        timing_count++;
    }
}
```

**行为**：
- ✅ 每次调用都会**添加一条新记录**
- ✅ **不会覆盖**已存在的同名记录
- ✅ **不会累加**同名记录的值
- ⚠️ 如果同一个名称被记录多次，会创建多条记录

### 2. `get_timer_value()` 函数

```c
double get_timer_value(const char* timer_name) {
    for (int i = 0; i < timing_count; i++) {
        if (strcmp(timing_records[i].name, timer_name) == 0) {
            return timing_records[i].duration_ms;  // ⚠️ 只返回第一个匹配的值
        }
    }
    return 0.0;
}
```

**行为**：
- ⚠️ **只返回第一个匹配的值**
- ⚠️ **忽略后续的同名记录**

### 3. `output_timing_to_excel()` 函数

```c
for (int i = 0; i < timing_count; i++) {
    const char* name = timing_records[i].name;
    double duration = timing_records[i].duration_ms;
    
    fprintf(fp, "%s,%.2f\n", name, duration);  // ✅ 输出所有记录
    
    // ...
    if (strcmp(name, "tumbler_blockchain_escrow_interaction") == 0) {
        tumbler_blockchain_escrow_interaction = duration;  // ⚠️ 后面的值会覆盖前面的值
    }
}
```

**行为**：
- ✅ CSV 中会**显示所有记录**（包括重复的）
- ⚠️ 在提取值时，**后面的值会覆盖前面的值**
- ⚠️ **不会累加**重复名称的值

---

## 实际案例：tumbler_blockchain_escrow_interaction

### 代码中的使用

```c
// 第一次使用（openEscrow）
START_TIMER(tumbler_blockchain_escrow_interaction)  // 第663行
FILE *fp = popen(cmd, "r");
// ...
END_TIMER(tumbler_blockchain_escrow_interaction)  // 第686行
// 记录: 8827.38 ms

// 第二次使用（confirmEscrow）
START_TIMER(tumbler_blockchain_escrow_interaction)  // 第868行
int rc = system(cmd);
END_TIMER(tumbler_blockchain_escrow_interaction)  // 第870行
// 记录: 2135.54 ms
```

### CSV 文件中的结果

```
行 7:  tumbler_blockchain_escrow_interaction,8827.38
行 12: tumbler_blockchain_escrow_interaction,2135.54
行 26: tumbler_blockchain_escrow_interaction,2135.54  (新公式计算部分)
```

### 问题分析

1. **CSV 输出**：显示两条记录（正确）
2. **新公式计算**：只使用了最后一次的值（2135.54），**丢失了第一次的值（8827.38）**
3. **应该的值**：8827.38 + 2135.54 = **10962.92 ms**

---

## 问题总结

### 当前行为

| 操作 | 结果 |
|------|------|
| 记录重复名称 | ✅ 创建多条记录 |
| CSV 输出 | ✅ 显示所有记录 |
| `get_timer_value()` | ⚠️ 只返回第一个值 |
| 新公式计算 | ⚠️ 只使用最后一个值 |

### 期望行为

对于 `tumbler_blockchain_escrow_interaction`：
- **应该累加**：8827.38 + 2135.54 = 10962.92 ms
- **不应该覆盖**：只使用最后一个值

---

## 解决方案

### 方案 1：修改 `output_timing_to_excel()` 累加重复值（推荐）

```c
// 在提取值时累加
if (strcmp(name, "tumbler_blockchain_escrow_interaction") == 0) {
    tumbler_blockchain_escrow_interaction += duration;  // 累加而不是覆盖
}
```

### 方案 2：修改 `get_timer_value()` 累加所有匹配值

```c
double get_timer_value(const char* timer_name) {
    double total = 0.0;
    for (int i = 0; i < timing_count; i++) {
        if (strcmp(timing_records[i].name, timer_name) == 0) {
            total += timing_records[i].duration_ms;  // 累加所有匹配的值
        }
    }
    return total;
}
```

### 方案 3：使用不同的计时器名称

```c
// 第一次
START_TIMER(tumbler_blockchain_escrow_interaction_open)
// ...
END_TIMER(tumbler_blockchain_escrow_interaction_open)

// 第二次
START_TIMER(tumbler_blockchain_escrow_interaction_confirm)
// ...
END_TIMER(tumbler_blockchain_escrow_interaction_confirm)

// 然后在公式中累加
tumbler_blockchain_escrow_interaction = 
    get_timer_value("tumbler_blockchain_escrow_interaction_open") +
    get_timer_value("tumbler_blockchain_escrow_interaction_confirm");
```

---

## 推荐方案

**使用方案 1**：修改 `output_timing_to_excel()` 函数，在提取 `tumbler_blockchain_escrow_interaction` 时累加所有值。

这样可以：
- ✅ 保持 CSV 输出显示所有记录
- ✅ 在公式计算中使用累加值
- ✅ 不需要修改其他代码







