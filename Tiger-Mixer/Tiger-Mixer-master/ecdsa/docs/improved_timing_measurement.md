# A2L 系统改进的耗时测量方案

## 问题分析

### 当前测量方式的问题

1. **网络等待时间未被排除**：
   - 函数调用（如 `registration()`, `token_share()`）本身不包含等待循环
   - 但等待响应的循环在函数外部（如 `while (!REGISTRATION_COMPLETED)`）
   - 这些等待循环的时间被包含在总时间中

2. **ZMQ 消息发送/接收时间**：
   - ZMQ 使用 `ZMQ_DONTWAIT` 标志（非阻塞）
   - 消息发送时间很短（微秒级），可以忽略
   - 但等待消息到达的时间（网络延迟）应该排除

### 正确的测量方式

**只测量函数调用本身的时间**，这样可以：
- ✅ 包括：密码学计算、ZMQ 消息发送（非阻塞，时间很短）
- ❌ 排除：网络等待时间（等待响应的循环）
- ❌ 排除：区块链交互时间（单独测量并减去）

---

## Alice 的改进测量方案

### 阶段划分

根据代码分析，Alice 可以分为以下阶段：

#### 1. 初始化阶段（已正确测量）
```c
START_TIMER(alice_initialization_computation)
// ... 密码学计算
END_TIMER(alice_initialization_computation)
```
- ✅ 只包含密码学计算，无网络等待
- ✅ 已正确实现

#### 2. 注册阶段（需要改进）

**当前实现**：
```c
START_TIMER(alice_registration_total)
if (registration(state, socket, state->alice_escrow_id) != RLC_OK) {
  RLC_THROW(ERR_CAUGHT);
}
END_TIMER(alice_registration_total)

// 等待注册完成 - 排除网络等待时间
while (!REGISTRATION_COMPLETED) {
  if (receive_message(state, socket, &tx_data) != RLC_OK) {
    RLC_THROW(ERR_CAUGHT);
  }
}
```

**问题**：
- `alice_registration_total` 只测量了 `registration()` 函数本身（✅ 正确）
- 但等待循环在计时器外部（✅ 已排除）
- `registration()` 函数内部调用了 `open_escrow_sync_with_tid()`，包含区块链交互

**改进方案**：
```c
// 测量注册阶段的密码学计算（排除区块链交互）
START_TIMER(alice_registration_computation)
if (registration(state, socket, state->alice_escrow_id) != RLC_OK) {
  RLC_THROW(ERR_CAUGHT);
}
END_TIMER(alice_registration_computation)

// 等待注册完成 - 网络等待时间（已排除）
while (!REGISTRATION_COMPLETED) {
  if (receive_message(state, socket, &tx_data) != RLC_OK) {
    RLC_THROW(ERR_CAUGHT);
  }
}
```

**纯计算时间**：
```
alice_registration_pure_computation = alice_registration_computation - alice_blockchain_escrow_interaction
```

#### 3. Token 分享阶段（已正确测量）

```c
START_TIMER(alice_token_share_computation)
if (token_share(state, socket, state->alice_escrow_id) != RLC_OK) {
  RLC_THROW(ERR_CAUGHT);
}
END_TIMER(alice_token_share_computation)
```

- ✅ `token_share()` 函数只包含密码学计算和 ZMQ 发送（非阻塞）
- ✅ 无等待循环，无区块链交互
- ✅ 已正确实现

#### 4. 支付初始化阶段（已正确测量）

```c
START_TIMER(alice_payment_init_computation)
if (payment_init(state, socket, &tx_data) != RLC_OK) {
  RLC_THROW(ERR_CAUGHT);
}
END_TIMER(alice_payment_init_computation)

// 等待谜题解决 - 网络等待时间（已排除）
while (!PUZZLE_SOLVED) {
  if (receive_message(state, socket, &tx_data) != RLC_OK) {
    RLC_THROW(ERR_CAUGHT);
  }
}
```

- ✅ `payment_init()` 函数只包含密码学计算和 ZMQ 发送
- ✅ 等待循环在计时器外部（已排除）
- ✅ 已正确实现

#### 5. 谜题解决方案分享阶段（已正确测量）

```c
START_TIMER(alice_puzzle_solution_share_computation)
if (puzzle_solution_share(state, socket) != RLC_OK) {
  RLC_THROW(ERR_CAUGHT);
}
END_TIMER(alice_puzzle_solution_share_computation)
```

- ✅ 只包含密码学计算和 ZMQ 发送
- ✅ 已正确实现

---

## 总结：当前实现的正确性

### ✅ 已正确实现的部分

1. **网络等待时间已排除**：
   - 所有等待循环（`while (!REGISTRATION_COMPLETED)`, `while (!PUZZLE_SHARED)`, `while (!PUZZLE_SOLVED)`）都在计时器外部
   - 这些等待时间不会被计入计算时间

2. **ZMQ 消息发送时间**：
   - 使用 `ZMQ_DONTWAIT`（非阻塞），时间很短（微秒级）
   - 包含在计算时间内是合理的

3. **区块链交互时间**：
   - 单独测量：`alice_blockchain_escrow_interaction`
   - 最后从总时间中减去

### ⚠️ 需要注意的部分

1. **`registration()` 函数内部**：
   - 包含区块链交互调用 `open_escrow_sync_with_tid()`
   - 这个调用已经被 `alice_blockchain_escrow_interaction` 计时器包裹
   - 所以 `alice_registration_total` 包含了区块链交互时间
   - **需要减去** `alice_blockchain_escrow_interaction` 才能得到纯计算时间

2. **其他函数**：
   - `token_share()`, `payment_init()`, `puzzle_solution_share()` 都不包含区块链交互
   - 这些函数的计时器已经是纯计算时间

---

## 最终测量公式

### Alice 纯计算时间

```
alice_pure_computation_time = 
    alice_initialization_computation +
    (alice_registration_total - alice_blockchain_escrow_interaction) +
    alice_token_share_computation +
    alice_payment_init_computation +
    alice_puzzle_solution_share_computation
```

或者：

```
alice_pure_computation_time = 
    alice_total_computation_time - 
    alice_blockchain_escrow_interaction
```

**注意**：`alice_total_computation_time` 包含了所有阶段，减去区块链交互时间即可得到纯计算时间。

---

## 验证方法

### 1. 检查等待循环位置

确认所有等待循环都在计时器外部：

```bash
grep -n "while.*COMPLETED\|while.*SHARED\|while.*SOLVED" src/alice.c
```

### 2. 检查区块链交互计时器

确认区块链交互被单独测量：

```bash
grep -n "alice_blockchain_escrow_interaction\|alice_blockchain_interaction" src/alice.c
```

### 3. 验证计算公式

运行程序后，检查：
- `alice_total_computation_time` 是否包含所有阶段
- `alice_blockchain_escrow_interaction` 是否只包含区块链交互
- `alice_pure_computation_time` 是否正确计算

---

## 结论

**当前实现已经基本正确**：
- ✅ 网络等待时间已排除（等待循环在计时器外部）
- ✅ 区块链交互时间已单独测量
- ✅ 纯计算时间 = 总时间 - 区块链交互时间

**唯一需要注意的是**：
- `alice_registration_total` 包含了区块链交互时间
- 计算纯计算时间时需要减去 `alice_blockchain_escrow_interaction`

**建议**：
- 保持当前实现不变
- 在输出时明确说明：纯计算时间已排除网络等待和区块链交互时间
- 可以添加更详细的阶段分解，但当前的总时间减去区块链交互时间的方式已经足够准确







