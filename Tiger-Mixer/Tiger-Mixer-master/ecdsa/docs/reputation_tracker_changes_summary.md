# Reputation Tracker 修改总结

## 修改目标
将声誉计算从基于 `participant_id` 改为基于 `address`（委员会成员地址）

## 需要修改的部分

### 1. 决策记录格式
- 旧格式：`timestamp,request_id,user_address,user_label,participant_id,participant_address,judge_api_result,actual_decision,is_correct`
- 新格式：`timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct`
- 移除 `participant_id` 列

### 2. 统计表格式
- 旧格式：`participant_id,address,total_decisions,correct_decisions,accuracy,consistency,total_reputation,last_update`
- 新格式：`address,total_decisions,correct_decisions,accuracy,consistency,total_reputation,last_update`
- 移除 `participant_id` 列

### 3. 函数签名修改
- `reputation_tracker_record_decision`: 从 `int participant_id` 改为 `const char* participant_address`
- `calculate_accuracy_for_participant`: 改为 `calculate_accuracy_for_address(const char* address)`
- `calculate_consistency_for_participant`: 改为 `calculate_consistency_for_address(const char* address)`

### 4. 统计计算逻辑
- 从决策记录中提取所有唯一的 `participant_address`
- 为每个地址计算准确率和一致性
- 不再依赖 `committee_members.txt` 中的固定3个成员

## 已完成
- ✅ 修改了 `reputation_tracker_record_decision` 函数签名
- ✅ 修改了 `secret_share_receiver.c` 中的调用，传入 `current_address`
- ✅ 修改了决策记录格式（移除participant_id列）
- ✅ 修改了统计表格式（移除participant_id列）
- ✅ 修改了 `calculate_accuracy_for_address` 函数

## 待完成
- ⏳ 修改 `calculate_consistency_for_address` 函数（需要重写，基于address分组）
- ⏳ 修改 `reputation_tracker_calculate_and_save_stats` 函数（从决策记录中提取所有唯一address）

