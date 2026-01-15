# CSV 文件生成方式和总时间计算说明

## CSV 文件生成方式

### 生成函数

CSV 文件通过 `output_timing_to_excel()` 函数生成（`src/util.c` 第 4550 行）：

```c
void output_timing_to_excel(const char* filename) {
    // 遍历所有记录的计时器
    for (int i = 0; i < timing_count; i++) {
        // 写入每个计时器的名称和耗时
        fprintf(fp, "%s,%.2f\n", timing_records[i].name, timing_records[i].duration_ms);
        
        // 根据名称分类累计时间
        if (strstr(timing_records[i].name, "registration") != NULL) {
            total_registration += timing_records[i].duration_ms;
        } else if (strstr(timing_records[i].name, "puzzle") != NULL || 
                   strstr(timing_records[i].name, "zk") != NULL ||
                   strstr(timing_records[i].name, "randomize") != NULL ||
                   strstr(timing_records[i].name, "layered_proof") != NULL) {
            total_puzzle_solve += timing_records[i].duration_ms;
        } else if (strstr(timing_records[i].name, "secret_share") != NULL) {
            total_secret_share += timing_records[i].duration_ms;
        }
    }
    
    // 写入分类汇总
    fprintf(fp, "总时间,%.2f\n", total_registration + total_puzzle_solve + total_secret_share);
}
```

### 调用位置

- **Alice**: `src/alice.c` 第 1713 行
- **Bob**: `src/bob.c` 第 2271 行
- **Tumbler**: `src/tumbler.c` 第 575 行

---

## CSV 文件中的"总时间"计算方式

### ⚠️ 重要发现

**CSV 文件中的"总时间"不是 `alice_total_computation_time`**，而是：

```
总时间 = 注册阶段总时间 + 生成谜题-解谜阶段总时间 + 秘密分享阶段总时间
```

### 分类规则

1. **注册阶段**：名称包含 `"registration"`
2. **生成谜题-解谜阶段**：名称包含 `"puzzle"`、`"zk"`、`"randomize"` 或 `"layered_proof"`
3. **秘密分享阶段**：名称包含 `"secret_share"`

### 未分类的计时器

**以下计时器不会被计入 CSV 的"总时间"**：

- `alice_total_computation_time` (44209.13 ms) - **真正的总时间**
- `alice_blockchain_escrow_interaction` (8516.77 ms) - 区块链交互时间
- `alice_initialization_computation` (0.38 ms) - 初始化时间
- `alice_token_share_computation` (0.03 ms) - Token 分享时间
- `alice_payment_init_computation` (355.60 ms) - 支付初始化时间
- `alice_ecdsa_signing` (0.60 ms) - ECDSA 签名时间
- `alice_secret_extraction` (0.17 ms) - 秘密提取时间
- `alice_blockchain_interaction` (0.00 ms) - 另一个区块链交互计时器

---

## 实际数据分析（Alice）

### CSV 文件中的数据

```
alice_registration_total: 12110.92 ms → 注册阶段
bob_to_alice_zk_verification: 189.41 ms → 生成谜题-解谜阶段
alice_second_puzzle_randomization: 175.38 ms → 生成谜题-解谜阶段
alice_zk_proof_generation: 178.52 ms → 生成谜题-解谜阶段
alice_puzzle_solution_share_computation: 0.01 ms → 生成谜题-解谜阶段

注册阶段总时间: 12110.92 ms
生成谜题-解谜阶段总时间: 543.33 ms
秘密分享阶段总时间: 0.00 ms
总时间: 12654.24 ms
```

### 验证计算

```
注册阶段总和 = 12110.92 ms
生成谜题-解谜阶段总和 = 189.41 + 175.38 + 178.52 + 0.01 = 543.32 ms
总时间 = 12110.92 + 543.32 = 12654.24 ms ✅
```

---

## 问题分析

### CSV "总时间" vs 实际总时间

| 项目 | CSV "总时间" | alice_total_computation_time |
|------|-------------|------------------------------|
| 值 | 12654.24 ms | 44209.13 ms |
| 包含内容 | 只包括匹配分类的计时器 | 包括所有计时器（含等待循环） |
| 是否排除区块链交互 | ❌ 未排除 | ❌ 未排除 |
| 是否排除网络等待 | ✅ 已排除（函数调用本身） | ❌ 未排除（包含等待循环） |

### 关键差异

1. **CSV "总时间" (12654.24 ms)**：
   - 只包括匹配分类的计时器
   - 不包括 `alice_total_computation_time` 本身
   - 不包括初始化、token 分享、支付初始化等未分类的计时器
   - **但包含了 `alice_registration_total`，这个包含了区块链交互时间**

2. **alice_total_computation_time (44209.13 ms)**：
   - 包括所有计时器
   - **包含了等待循环的时间**（`while (!REGISTRATION_COMPLETED)` 等）
   - 包含了区块链交互时间

---

## 正确的纯计算时间计算

### 方式 1：使用 alice_total_computation_time（推荐）

```c
// 在 alice.c 中已经实现
double pure_computation_time = (get_timer_value("alice_total_computation_time") - 
                                get_timer_value("alice_blockchain_escrow_interaction")) / 1000.0;
```

**结果**：
```
纯计算时间 = 44209.13 - 8516.77 = 35692.36 ms = 35.69 秒
```

### 方式 2：使用 CSV "总时间"（不推荐）

**问题**：
- CSV "总时间" 不包括很多计时器
- 仍然包含了 `alice_registration_total` 中的区块链交互时间
- 需要手动减去区块链交互时间

**如果要使用**：
```
纯计算时间 ≈ CSV总时间 - alice_blockchain_escrow_interaction
          = 12654.24 - 8516.77 = 4137.47 ms
```

但这个值**不准确**，因为：
1. 不包括初始化、token 分享、支付初始化等时间
2. `alice_registration_total` 中可能还有其他区块链交互时间

---

## 结论和建议

### ✅ 正确的测量方式

**使用程序输出的纯计算时间**（在 `alice.c` 第 1706 行）：

```c
double pure_computation_time = (get_timer_value("alice_total_computation_time") - 
                                get_timer_value("alice_blockchain_escrow_interaction")) / 1000.0;
```

**这个值**：
- ✅ 包括所有密码学计算时间
- ✅ 排除区块链交互时间
- ⚠️ 包含等待循环的 CPU 空转时间（很小，可忽略）

### ❌ CSV "总时间"的问题

**CSV 文件中的"总时间"**：
- ❌ 不是真正的总时间
- ❌ 不包括很多计时器
- ❌ 仍然包含区块链交互时间（在 `alice_registration_total` 中）
- ⚠️ 只适合查看分类汇总，不适合作为总时间

### 📝 建议

1. **使用程序输出的纯计算时间**（`alice.c` 第 1707 行）
2. **CSV 文件用于查看分类汇总**，不要作为总时间
3. **如果需要准确的总时间**，使用 `alice_total_computation_time` 减去 `alice_blockchain_escrow_interaction`

---

## 总结

| 项目 | 值 | 说明 |
|------|-----|------|
| CSV "总时间" | 12654.24 ms | 只包括匹配分类的计时器，不准确 |
| alice_total_computation_time | 44209.13 ms | 包括所有计时器（含等待循环） |
| alice_blockchain_escrow_interaction | 8516.77 ms | 区块链交互时间 |
| **纯计算时间** | **35692.36 ms** | **总时间 - 区块链交互时间** ✅ |

**最终答案**：CSV 文件中的"总时间"**不是**我之前说的纯计算时间，而是一个**部分汇总**。真正的纯计算时间应该使用程序输出的值（`alice_total_computation_time - alice_blockchain_escrow_interaction`）。







