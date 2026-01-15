/**
 * 声誉跟踪系统集成示例
 * 展示如何在 secret_share_receiver.c 中集成声誉跟踪功能
 */

#include "reputation_tracker.h"
#include <sys/time.h>

// ============================================
// 1. 初始化（在 main 函数中）
// ============================================

/*
int main(int argc, char* argv[]) {
    // ... 现有代码 ...
    
    // 读取委员会成员地址
    char my_address[64];
    if (read_committee_member_address(participant_id, my_address, sizeof(my_address)) == 0) {
        printf("[MAIN] ✅ 使用地址: %s (成员 #%d)\n", my_address, participant_id);
        
        // 初始化声誉跟踪系统
        if (reputation_tracker_init(participant_id, my_address) == 0) {
            printf("[MAIN] ✅ 声誉跟踪系统初始化成功\n");
        } else {
            printf("[MAIN] ⚠️  声誉跟踪系统初始化失败\n");
        }
    }
    
    // ... 现有代码 ...
    
    // 在程序退出时保存统计数据
    atexit(cleanup_reputation_tracker);
}
*/

// ============================================
// 2. 在 handle_audit_request 中集成
// ============================================

/*
static void handle_audit_request(void* socket, int tag, const char* msg_id, 
                                 const char* pairs_summary_json, 
                                 const char* json_filename, int participant_id) {
    
    // 记录收到请求
    reputation_tracker_record_request(participant_id, msg_id);
    
    // 记录开始时间（用于计算响应时间）
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    // ... 现有的参数验证代码 ...
    
    // 如果tag==0，先调用judge API判断是否需要审计
    if (tag == 0) {
        int judge_result = call_judge_api(pairs_summary_json);
        
        if (judge_result == 1) {
            // 返回0，不需要审计
            const char* response = "NO_AUDIT_NEEDED";
            zmq_send(socket, response, strlen(response), 0);
            
            // 记录"不需要审计"
            reputation_tracker_record_no_audit(participant_id, msg_id);
            return;
        } else if (judge_result == -1) {
            // API调用失败，记录失败
            reputation_tracker_record_failure(participant_id, msg_id, "JUDGE_API_ERROR");
            // 继续执行（容错处理）
        }
    }
    
    // ... 现有的文件检查和加载代码 ...
    
    int load_result = load_all_shares_from_json(json_filename, msg_id, participant_id, 
                                                 shares, max_shares, &num_shares);
    
    if (load_result == 0) {
        // 成功加载shares
        
        // ... 现有的打包和发送代码 ...
        
        int send_result = zmq_send(socket, msg_data, msg_size, 0);
        if (send_result == msg_size) {
            // 计算响应时间
            gettimeofday(&end_time, NULL);
            uint64_t response_time_ms = 
                (end_time.tv_sec - start_time.tv_sec) * 1000 +
                (end_time.tv_usec - start_time.tv_usec) / 1000;
            
            // 记录成功响应
            reputation_tracker_record_success(participant_id, msg_id, num_shares, response_time_ms);
        } else {
            // 记录发送失败
            reputation_tracker_record_failure(participant_id, msg_id, "SEND_ERROR");
        }
        
        free(msg_data);
    } else {
        // 未找到shares
        const char* response = "NOT_FOUND";
        zmq_send(socket, response, strlen(response), 0);
        
        // 记录失败
        reputation_tracker_record_failure(participant_id, msg_id, "NOT_FOUND");
    }
    
    // 释放资源
    if (shares) {
        free(shares);
    }
}
*/

// ============================================
// 3. 在 auditor.c 中集成（可选，用于验证结果反馈）
// ============================================

/*
// 在 verify_share_with_stored_commitment 函数中
int verify_share_with_stored_commitment(auditor_state_t* state, 
                                        secret_share_t* share, 
                                        const char* msg_id) {
    // ... 现有的验证代码 ...
    
    int verify_result = verify_share(share, commitment, params);
    
    if (verify_result == RLC_OK) {
        printf("[VSS][Auditor] Share verification successful\n");
        
        // 记录验证成功（需要知道participant_id，可以从share->x获取）
        int participant_id = share->x;  // x 就是 participant_id
        reputation_tracker_record_verification(participant_id, msg_id, 1);
        
        return 0;
    } else {
        printf("[VSS][Auditor] Share verification failed\n");
        
        // 记录验证失败
        int participant_id = share->x;
        reputation_tracker_record_verification(participant_id, msg_id, 0);
        
        return -1;
    }
}
*/

// ============================================
// 4. 定期保存统计数据
// ============================================

/*
// 方式1: 使用 atexit 在程序退出时保存
void cleanup_reputation_tracker() {
    for (int i = 1; i <= 3; i++) {
        reputation_tracker_save_stats(i);
        reputation_tracker_cleanup(i);
    }
}

// 在 main 函数中注册
atexit(cleanup_reputation_tracker);
*/

/*
// 方式2: 使用定时器定期保存（如每小时）
#include <signal.h>

void save_stats_timer(int sig) {
    (void)sig;  // 忽略参数
    for (int i = 1; i <= 3; i++) {
        reputation_tracker_save_stats(i);
    }
}

// 在 main 函数中设置定时器
signal(SIGALRM, save_stats_timer);
alarm(3600);  // 每小时触发一次
*/

// ============================================
// 5. 查询和显示统计数据（调试用）
// ============================================

/*
void print_reputation_stats(int participant_id) {
    reputation_stats_t stats;
    
    if (reputation_tracker_get_stats(participant_id, &stats) == 0) {
        printf("\n========== 成员 %d 声誉统计 ==========\n", participant_id);
        printf("地址: %s\n", stats.address);
        printf("总请求数: %lu\n", stats.total_requests);
        printf("成功响应: %lu\n", stats.successful_responses);
        printf("失败响应: %lu\n", stats.failed_responses);
        printf("不需要审计: %lu\n", stats.no_audit_needed);
        printf("Shares验证成功: %lu\n", stats.shares_verified);
        printf("Shares验证失败: %lu\n", stats.shares_failed_verify);
        printf("\n计算出的声誉值:\n");
        printf("  参与率: %lu%%\n", stats.participation_rate);
        printf("  准确率: %lu%%\n", stats.accuracy);
        printf("  一致性: %lu%%\n", stats.consistency);
        printf("  综合声誉: %lu\n", stats.total_reputation);
        printf("========================================\n\n");
    } else {
        printf("无法获取成员 %d 的统计数据\n", participant_id);
    }
}
*/


