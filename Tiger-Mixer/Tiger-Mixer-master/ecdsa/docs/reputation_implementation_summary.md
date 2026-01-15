# 基于真实决策数据的声誉更新机制 - 实现总结

## 已完成的修改

### 1. 核心模块 (`reputation_tracker.c`)

- ✅ 实现了决策记录功能
- ✅ 实现了准确率计算（基于address_labels.csv判断决策正确性）
- ✅ 实现了一致性计算（基于与其他成员的决策一致性）
- ✅ 实现了统计数据保存到CSV文件
- ✅ 处理了地址未找到的情况（输出错误并停止函数）

### 2. 头文件 (`reputation_tracker.h`)

- ✅ 定义了API接口
- ✅ 添加了工具函数声明

### 3. 工具函数 (`reputation_tracker_util.c`)

- ✅ 实现了从JSON中提取用户地址的函数

### 4. 更新脚本 (`update_reputation_from_decisions.js`)

- ✅ 实现了从CSV文件读取统计数据
- ✅ 实现了更新到区块链的逻辑
- ✅ 只更新准确率和一致性（不再使用参与率）

## 待完成的修改

### 1. 在 `secret_share_receiver.c` 中集成决策记录

需要在 `handle_audit_request` 函数中添加以下逻辑：

```c
// 在tag==0时，调用judge API后
if (tag == 0) {
    int judge_result = call_judge_api(pairs_summary_json);
    
    // 提取用户地址
    char user_address[64] = {0};
    if (pairs_summary_json && extract_address_from_json(pairs_summary_json, user_address, sizeof(user_address)) == 0) {
        // 构造request_id
        char request_id[256];
        snprintf(request_id, sizeof(request_id), "%s_%ld", msg_id, time(NULL));
        
        if (judge_result == 1) {
            // 不需要审计，发送NO_AUDIT_NEEDED
            const char* response = "NO_AUDIT_NEEDED";
            zmq_send(socket, response, strlen(response), 0);
            
            // 记录决策
            reputation_tracker_record_decision(request_id, user_address, participant_id, 
                                              0, "no_audit_needed");  // judge_result=0表示API返回1
            return;
        } else if (judge_result == 0) {
            // 需要审计，继续执行
            // 记录judge_result（稍后在找到/未找到shares时记录完整决策）
        }
    }
}

// 在找到shares时
if (load_result == 0) {
    // ... 发送shares ...
    
    // 记录决策
    if (tag == 0 && pairs_summary_json) {
        char user_address[64] = {0};
        if (extract_address_from_json(pairs_summary_json, user_address, sizeof(user_address)) == 0) {
            char request_id[256];
            snprintf(request_id, sizeof(request_id), "%s_%ld", msg_id, time(NULL));
            reputation_tracker_record_decision(request_id, user_address, participant_id, 
                                              1, "provided_shares");  // judge_result=1表示API返回0
        }
    }
} else {
    // 未找到shares
    const char* response = "NOT_FOUND";
    zmq_send(socket, response, strlen(response), 0);
    
    // 记录决策
    if (tag == 0 && pairs_summary_json) {
        char user_address[64] = {0};
        if (extract_address_from_json(pairs_summary_json, user_address, sizeof(user_address)) == 0) {
            char request_id[256];
            snprintf(request_id, sizeof(request_id), "%s_%ld", msg_id, time(NULL));
            reputation_tracker_record_decision(request_id, user_address, participant_id, 
                                              1, "not_found");  // judge_result=1表示API返回0
        }
    }
}
```

### 2. 修改 `update_and_rotate_new.js`

将第一部分（更新候选者声誉）改为调用新的脚本：

```javascript
// 第一部分：计算并更新声誉（基于真实决策数据）
console.log("\n========================================");
console.log("   第一部分：计算并更新声誉");
console.log("========================================\n");

// 先计算统计数据（需要在C程序中调用，或创建一个独立的计算脚本）
// 这里假设已经通过其他方式计算好了统计数据

// 然后调用更新脚本
const { execSync } = require('child_process');
try {
    const output = execSync('truffle exec scripts/update_reputation_from_decisions.js --network development', {
        cwd: __dirname + '/..',
        encoding: 'utf8'
    });
    console.log(output);
} catch (error) {
    console.log("⚠️  声誉更新失败:", error.message);
    // 继续执行后续步骤
}
```

### 3. 在CMakeLists.txt中添加新源文件

```cmake
# 添加声誉跟踪模块
set(REPUTATION_TRACKER_SOURCES
    src/reputation_tracker.c
    src/reputation_tracker_util.c
)

# 添加到编译目标
target_sources(your_target PRIVATE ${REPUTATION_TRACKER_SOURCES})
```

## 使用流程

1. **编译代码**：确保包含新的源文件
2. **运行secret_share_receiver**：系统会自动记录决策
3. **定期计算统计数据**：可以创建一个定时任务调用 `reputation_tracker_calculate_and_save_stats()`
4. **运行轮换脚本**：`auto_rotation_simple.sh` 会自动调用 `update_and_rotate_new.js`，该脚本会读取统计数据并更新到区块链

## 文件结构

```
/home/zxx/A2L/A2L-master/ecdsa/
├── log_game/
│   ├── reputation_decisions.csv    # 决策记录表
│   └── reputation_stats.csv         # 声誉统计表
├── bin/
│   └── address_labels.csv          # 地址标签表
└── src/
    ├── reputation_tracker.c         # 核心实现
    ├── reputation_tracker_util.c   # 工具函数
    └── secret_share_receiver.c    # 需要集成决策记录

/home/zxx/Config/truffleProject/truffletest/scripts/
└── update_reputation_from_decisions.js  # 更新脚本
```

## 注意事项

1. **地址未找到处理**：如果address_labels.csv中没有该地址，会输出错误并停止记录决策
2. **时间窗口**：统计全部历史决策
3. **权重**：所有请求权重相同
4. **更新频率**：每次轮换前自动更新（通过auto_rotation_simple.sh）

