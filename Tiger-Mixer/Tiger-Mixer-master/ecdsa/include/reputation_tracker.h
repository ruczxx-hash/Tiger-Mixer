/**
 * 委员会成员声誉跟踪系统头文件（基于真实决策数据）
 */

#ifndef REPUTATION_TRACKER_H
#define REPUTATION_TRACKER_H

#include <stdint.h>
#include <time.h>

// API函数
int reputation_tracker_init(void);
int reputation_tracker_record_decision(const char* request_id, const char* user_address,
                                      const char* participant_address, int judge_result,
                                      const char* actual_decision);
int reputation_tracker_calculate_and_save_stats(void);
void reputation_tracker_cleanup(void);

// 工具函数
int extract_address_from_json(const char* pairs_summary_json, char* address_out, size_t address_size);

#endif // REPUTATION_TRACKER_H
