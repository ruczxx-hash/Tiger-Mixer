/**
 * 委员会成员声誉跟踪系统（基于真实决策数据）
 * 计算准确率和一致性，用于更新声誉值
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include "reputation_tracker.h"

#define MAX_LOG_ENTRIES 10000
#define DECISIONS_FILE "/home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_decisions.csv"
#define STATS_FILE "/home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_stats.csv"
#define ADDRESS_LABELS_FILE "/home/zxx/A2L/A2L-master/ecdsa/bin/address_labels.csv"
#define COMMITTEE_MEMBERS_FILE "/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt"

// 线程安全的决策记录
typedef struct {
    pthread_mutex_t mutex;
    FILE* decisions_fp;  // 决策记录文件指针
    int initialized;
} tracker_state_t;

static tracker_state_t g_tracker_state = {0};

/**
 * 初始化声誉跟踪系统
 */
int reputation_tracker_init(void) {
    if (g_tracker_state.initialized) {
        return 0;  // 已经初始化
    }
    
    pthread_mutex_init(&g_tracker_state.mutex, NULL);
    
    // 创建日志目录
    char log_dir[512];
    strncpy(log_dir, DECISIONS_FILE, sizeof(log_dir) - 1);
    char* last_slash = strrchr(log_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(log_dir, 0777);
    }
    
    // 检查决策记录文件是否存在，如果不存在则创建并写入表头
    FILE* test_fp = fopen(DECISIONS_FILE, "r");
    if (!test_fp) {
        // 文件不存在，创建并写入表头（添加participant_address列）
        FILE* new_fp = fopen(DECISIONS_FILE, "w");
        if (new_fp) {
            fprintf(new_fp, "timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct\n");
            fclose(new_fp);
            printf("[REPUTATION] 创建决策记录文件: %s\n", DECISIONS_FILE);
        } else {
            fprintf(stderr, "[REPUTATION] 无法创建决策记录文件: %s\n", DECISIONS_FILE);
            pthread_mutex_destroy(&g_tracker_state.mutex);
            return -1;
        }
    } else {
        fclose(test_fp);
    }
    
    // 检查统计文件是否存在，如果不存在则创建并写入表头
    test_fp = fopen(STATS_FILE, "r");
    if (!test_fp) {
        FILE* new_fp = fopen(STATS_FILE, "w");
        if (new_fp) {
            fprintf(new_fp, "address,total_decisions,correct_decisions,accuracy,consistency,total_reputation,last_update\n");
            fclose(new_fp);
            printf("[REPUTATION] 创建统计文件: %s\n", STATS_FILE);
        }
    } else {
        fclose(test_fp);
    }
    
    g_tracker_state.initialized = 1;
    printf("[REPUTATION] 声誉跟踪系统初始化成功\n");
    
    return 0;
}

/**
 * 从address_labels.csv中查找地址标签
 * 返回: 0=找到legal, 1=找到illegal, -1=未找到
 */
static int lookup_address_label(const char* address, char* label_out, size_t label_size) {
    if (!address || !label_out) {
        return -1;
    }
    
    FILE* fp = fopen(ADDRESS_LABELS_FILE, "r");
    if (!fp) {
        fprintf(stderr, "[REPUTATION] 无法打开地址标签文件: %s\n", ADDRESS_LABELS_FILE);
        return -1;
    }
    
    char line[256];
    // 跳过表头
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }
    
    // 转换为小写进行比较
    char address_lower[64];
    strncpy(address_lower, address, sizeof(address_lower) - 1);
    address_lower[sizeof(address_lower) - 1] = '\0';
    for (int i = 0; address_lower[i]; i++) {
        if (address_lower[i] >= 'A' && address_lower[i] <= 'F') {
            address_lower[i] = address_lower[i] - 'A' + 'a';
        }
    }
    
    while (fgets(line, sizeof(line), fp)) {
        // 去除换行符
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            len--;
        }
        
        // 解析CSV: address,label
        char* comma = strchr(line, ',');
        if (!comma) continue;
        
        *comma = '\0';
        char* file_address = line;
        char* file_label = comma + 1;
        
        // 转换为小写比较
        char file_address_lower[64];
        strncpy(file_address_lower, file_address, sizeof(file_address_lower) - 1);
        file_address_lower[sizeof(file_address_lower) - 1] = '\0';
        for (int i = 0; file_address_lower[i]; i++) {
            if (file_address_lower[i] >= 'A' && file_address_lower[i] <= 'F') {
                file_address_lower[i] = file_address_lower[i] - 'A' + 'a';
            }
        }
        
        if (strcmp(file_address_lower, address_lower) == 0) {
            strncpy(label_out, file_label, label_size - 1);
            label_out[label_size - 1] = '\0';
            fclose(fp);
            
            if (strcmp(file_label, "legal") == 0) {
                return 0;
            } else if (strcmp(file_label, "illegal") == 0) {
                return 1;
            } else {
                return -1;
            }
        }
    }
    
    fclose(fp);
    return -1;  // 未找到
}

/**
 * 判断决策是否正确
 * 返回: 1=正确, 0=错误
 */
static int is_decision_correct(const char* user_label, int judge_result, const char* actual_decision) {
    // judge_result: API的原始返回值，0=不需要审计, 1=需要审计
    // actual_decision: "provided_shares" 或 "no_audit_needed" 或 "not_found"
    
    if (strcmp(user_label, "legal") == 0) {
        // 合法用户：应该返回"no_audit_needed"（不给分片）
        // judge_result=0表示API返回0（不需要审计），actual_decision应该是"no_audit_needed"
        if (judge_result == 0 && strcmp(actual_decision, "no_audit_needed") == 0) {
            return 1;  // 正确
        }
        return 0;  // 错误
    } else if (strcmp(user_label, "illegal") == 0) {
        // 非法用户：应该返回"provided_shares"（给分片）
        // judge_result=1表示API返回1（需要审计），actual_decision应该是"provided_shares"
        if (judge_result == 1 && strcmp(actual_decision, "provided_shares") == 0) {
            return 1;  // 正确
        }
        // 如果judge_result=1但actual_decision是"not_found"，这也是错误的（应该提供但未找到）
        return 0;  // 错误
    }
    
    return 0;  // 未知标签，视为错误
}

/**
 * 记录决策
 */
int reputation_tracker_record_decision(const char* request_id, const char* user_address,
                                      const char* participant_address, int judge_result,
                                      const char* actual_decision) {
    if (!g_tracker_state.initialized) {
        fprintf(stderr, "[REPUTATION] 跟踪系统未初始化\n");
        return -1;
    }
    
    if (!request_id || !user_address || !actual_decision || !participant_address) {
        fprintf(stderr, "[REPUTATION] 参数无效\n");
        return -1;
    }
    
    pthread_mutex_lock(&g_tracker_state.mutex);
    
    // 查找地址标签
    char user_label[16] = {0};
    int label_result = lookup_address_label(user_address, user_label, sizeof(user_label));
    
    if (label_result == -1) {
        // 未找到地址，输出提示并停止
        fprintf(stderr, "[REPUTATION] ❌ 错误: 在address_labels.csv中未找到地址: %s\n", user_address);
        fprintf(stderr, "[REPUTATION] 停止记录决策\n");
        pthread_mutex_unlock(&g_tracker_state.mutex);
        return -1;
    }
    
    // 判断决策是否正确
    int is_correct = is_decision_correct(user_label, judge_result, actual_decision);
    
    // 记录时间戳
    time_t timestamp = time(NULL);
    
    // 写入决策记录（基于address，不再使用participant_id）
    FILE* fp = fopen(DECISIONS_FILE, "a");
    if (!fp) {
        fprintf(stderr, "[REPUTATION] 无法打开决策记录文件: %s\n", DECISIONS_FILE);
        pthread_mutex_unlock(&g_tracker_state.mutex);
        return -1;
    }
    
    // 新格式：timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct
    fprintf(fp, "%ld,%s,%s,%s,%s,%d,%s,%d\n",
            timestamp, request_id, user_address, user_label,
            participant_address, judge_result, actual_decision, is_correct);
    fclose(fp);
    
    printf("[REPUTATION] 记录决策: request_id=%s, user_address=%s, label=%s, participant_address=%s, judge=%d, decision=%s, correct=%d\n",
           request_id, user_address, user_label, participant_address, judge_result, actual_decision, is_correct);
    
    pthread_mutex_unlock(&g_tracker_state.mutex);
    
    return 0;
}

/**
 * 计算准确率（基于address）
 */
static uint64_t calculate_accuracy_for_address(const char* address) {
    FILE* fp = fopen(DECISIONS_FILE, "r");
    if (!fp) {
        return 50;  // 默认值
    }
    
    char line[512];
    // 跳过表头
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 50;
    }
    
    uint64_t total = 0;
    uint64_t correct = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // 新格式：timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct
        // 旧格式（兼容）：timestamp,request_id,user_address,user_label,participant_id,participant_address,judge_api_result,actual_decision,is_correct
        char* fields[9];
        int field_count = 0;
        char line_copy[512];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char* token = strtok(line_copy, ",");
        while (token && field_count < 9) {
            fields[field_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (field_count >= 8) {
            // 新格式：participant_address在第4列（索引4），is_correct在第7列（索引7）
            const char* participant_addr = fields[4];
            int is_correct = atoi(fields[7]);
            
            // 比较地址（不区分大小写）
            if (strcasecmp(participant_addr, address) == 0) {
                total++;
                if (is_correct) {
                    correct++;
                }
            }
        } else if (field_count >= 9) {
            // 旧格式：participant_address在第5列（索引5），is_correct在第8列（索引8）
            const char* participant_addr = fields[5];
            int is_correct = atoi(fields[8]);
            
            if (strcasecmp(participant_addr, address) == 0) {
                total++;
                if (is_correct) {
                    correct++;
                }
            }
        }
    }
    
    fclose(fp);
    
    if (total == 0) {
        return 50;  // 默认值
    }
    
    return (correct * 100) / total;
}

/**
 * 计算一致性（基于address）
 */
static uint64_t calculate_consistency_for_address(const char* address) {
    FILE* fp = fopen(DECISIONS_FILE, "r");
    if (!fp) {
        return 50;  // 默认值
    }
    
    // 读取所有决策，按request_id分组（基于address）
    typedef struct {
        char request_id[128];
        char addresses[10][64];  // 最多10个不同的地址
        int decisions[10];       // 对应的决策: 0=no_audit_needed, 1=provided_shares, -1=未参与
        int address_count;
    } request_group_t;
    
    request_group_t* groups = NULL;
    size_t group_count = 0;
    size_t group_capacity = 100;
    groups = (request_group_t*)malloc(sizeof(request_group_t) * group_capacity);
    
    char line[512];
    // 跳过表头
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        free(groups);
        return 50;
    }
    
    // 读取所有决策
    while (fgets(line, sizeof(line), fp)) {
        char* fields[8];
        int field_count = 0;
        char line_copy[512];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char* token = strtok(line_copy, ",");
        while (token && field_count < 8) {
            fields[field_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (field_count >= 8) {
            // 新格式：timestamp,request_id,user_address,user_label,participant_address,judge_api_result,actual_decision,is_correct
            // 旧格式（兼容）：timestamp,request_id,user_address,user_label,participant_id,participant_address,judge_api_result,actual_decision,is_correct
            char* request_id = fields[1];
            const char* participant_addr = (field_count >= 8) ? fields[4] : fields[5];  // 新格式在第4列，旧格式在第5列
            char* decision = (field_count >= 8) ? fields[6] : fields[7];  // 新格式在第6列，旧格式在第7列
            
            // 查找或创建请求组
            int found = 0;
            for (size_t i = 0; i < group_count; i++) {
                if (strcmp(groups[i].request_id, request_id) == 0) {
                    // 找到现有组，添加或更新地址的决策
                    int addr_idx = -1;
                    for (int j = 0; j < groups[i].address_count; j++) {
                        if (strcasecmp(groups[i].addresses[j], participant_addr) == 0) {
                            addr_idx = j;
                            break;
                        }
                    }
                    
                    if (addr_idx == -1) {
                        // 新地址，添加到组中
                        if (groups[i].address_count < 10) {
                            strncpy(groups[i].addresses[groups[i].address_count], participant_addr, 63);
                            groups[i].addresses[groups[i].address_count][63] = '\0';
                            addr_idx = groups[i].address_count;
                            groups[i].address_count++;
                        }
                    }
                    
                    if (addr_idx >= 0) {
                        int decision_value = (strcmp(decision, "provided_shares") == 0) ? 1 : 0;
                        groups[i].decisions[addr_idx] = decision_value;
                    }
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                // 创建新组
                if (group_count >= group_capacity) {
                    group_capacity *= 2;
                    groups = (request_group_t*)realloc(groups, sizeof(request_group_t) * group_capacity);
                }
                
                strncpy(groups[group_count].request_id, request_id, sizeof(groups[group_count].request_id) - 1);
                groups[group_count].request_id[sizeof(groups[group_count].request_id) - 1] = '\0';
                
                groups[group_count].address_count = 0;
                for (int i = 0; i < 10; i++) {
                    groups[group_count].decisions[i] = -1;
                }
                
                strncpy(groups[group_count].addresses[0], participant_addr, 63);
                groups[group_count].addresses[0][63] = '\0';
                int decision_value = (strcmp(decision, "provided_shares") == 0) ? 1 : 0;
                groups[group_count].decisions[0] = decision_value;
                groups[group_count].address_count = 1;
                group_count++;
            }
        }
    }
    
    fclose(fp);
    
    // 计算一致性
    uint64_t total_requests = 0;
    uint64_t consistent_requests = 0;
    
    for (size_t i = 0; i < group_count; i++) {
        if (groups[i].address_count < 2) {
            continue;  // 至少需要2个成员参与
        }
        
        // 统计每个决策的数量
        int no_audit_count = 0;
        int provided_count = 0;
        
        for (int j = 0; j < groups[i].address_count; j++) {
            if (groups[i].decisions[j] == 0) {
                no_audit_count++;
            } else if (groups[i].decisions[j] == 1) {
                provided_count++;
            }
        }
        
        // 确定大多数决策
        int majority_decision = -1;
        if (no_audit_count >= 2) {
            majority_decision = 0;
        } else if (provided_count >= 2) {
            majority_decision = 1;
        }
        
        if (majority_decision != -1) {
            total_requests++;
            // 检查该地址是否与大多数一致
            int address_decision = -1;
            for (int j = 0; j < groups[i].address_count; j++) {
                if (strcasecmp(groups[i].addresses[j], address) == 0) {
                    address_decision = groups[i].decisions[j];
                    break;
                }
            }
            
            if (address_decision == majority_decision) {
                consistent_requests++;
            }
        }
    }
    
    free(groups);
    
    if (total_requests == 0) {
        return 50;  // 默认值
    }
    
    return (consistent_requests * 100) / total_requests;
}

/**
 * 计算参与度（基于address）
 * 参与度 = (响应率 + 任务完成率) / 2
 * - 响应率：该地址响应的请求数 / 总请求数
 * - 任务完成率：该地址完成的任务数（provided_shares） / 该地址响应的请求数
 */
static uint64_t calculate_participation_for_address(const char* address) {
    FILE* fp = fopen(DECISIONS_FILE, "r");
    if (!fp) {
        return 50;  // 默认值
    }
    
    // 读取所有决策，按request_id分组
    typedef struct {
        char request_id[128];
        int has_address_response;  // 该地址是否响应了此请求
        int address_completed;     // 该地址是否完成了任务（provided_shares）
    } request_info_t;
    
    request_info_t* requests = NULL;
    size_t request_count = 0;
    size_t request_capacity = 100;
    requests = (request_info_t*)malloc(sizeof(request_info_t) * request_capacity);
    
    // 统计所有唯一的request_id
    typedef struct {
        char request_id[128];
        int found;
    } unique_request_t;
    
    unique_request_t unique_requests[1000] = {0};
    int unique_request_count = 0;
    
    char line[512];
    // 跳过表头
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        free(requests);
        return 50;
    }
    
    // 第一遍：收集所有唯一的request_id
    FILE* fp2 = fopen(DECISIONS_FILE, "r");
    if (fp2) {
        if (fgets(line, sizeof(line), fp2) == NULL) {  // 跳过表头
            fclose(fp2);
        }
        while (fgets(line, sizeof(line), fp2)) {
            char* fields[8];
            int field_count = 0;
            char line_copy[512];
            strncpy(line_copy, line, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';
            
            char* token = strtok(line_copy, ",");
            while (token && field_count < 8) {
                fields[field_count++] = token;
                token = strtok(NULL, ",");
            }
            
            if (field_count >= 8) {
                char* request_id = fields[1];
                
                // 检查是否已存在
                int found = 0;
                for (int i = 0; i < unique_request_count; i++) {
                    if (strcmp(unique_requests[i].request_id, request_id) == 0) {
                        found = 1;
                        break;
                    }
                }
                
                if (!found && unique_request_count < 1000) {
                    strncpy(unique_requests[unique_request_count].request_id, request_id, 
                           sizeof(unique_requests[unique_request_count].request_id) - 1);
                    unique_requests[unique_request_count].request_id[sizeof(unique_requests[unique_request_count].request_id) - 1] = '\0';
                    unique_request_count++;
                }
            }
        }
        fclose(fp2);
    }
    
    // 第二遍：统计该地址的响应和完成情况
    rewind(fp);
    if (fgets(line, sizeof(line), fp) == NULL) {  // 跳过表头
        fclose(fp);
        free(requests);
        return 50;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        char* fields[8];
        int field_count = 0;
        char line_copy[512];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';
        
        char* token = strtok(line_copy, ",");
        while (token && field_count < 8) {
            fields[field_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (field_count >= 8) {
            char* request_id = fields[1];
            const char* participant_addr = fields[4];
            char* decision = fields[6];
            
            // 检查是否是该地址的决策
            if (strcasecmp(participant_addr, address) != 0) {
                continue;
            }
            
            // 查找或创建请求记录
            int found = 0;
            for (size_t i = 0; i < request_count; i++) {
                if (strcmp(requests[i].request_id, request_id) == 0) {
                    found = 1;
                    requests[i].has_address_response = 1;
                    if (strcmp(decision, "provided_shares") == 0) {
                        requests[i].address_completed = 1;
                    }
                    break;
                }
            }
            
            if (!found) {
                if (request_count >= request_capacity) {
                    request_capacity *= 2;
                    requests = (request_info_t*)realloc(requests, sizeof(request_info_t) * request_capacity);
                }
                
                strncpy(requests[request_count].request_id, request_id, 
                       sizeof(requests[request_count].request_id) - 1);
                requests[request_count].request_id[sizeof(requests[request_count].request_id) - 1] = '\0';
                requests[request_count].has_address_response = 1;
                requests[request_count].address_completed = (strcmp(decision, "provided_shares") == 0) ? 1 : 0;
                request_count++;
            }
        }
    }
    
    fclose(fp);
    
    // 计算参与度
    uint64_t total_requests = unique_request_count;  // 总请求数
    uint64_t responded_requests = request_count;     // 该地址响应的请求数
    uint64_t completed_requests = 0;                 // 该地址完成的任务数
    
    for (size_t i = 0; i < request_count; i++) {
        if (requests[i].address_completed) {
            completed_requests++;
        }
    }
    
    free(requests);
    
    if (total_requests == 0) {
        return 50;  // 默认值
    }
    
    // 计算响应率
    uint64_t response_rate = (responded_requests * 100) / total_requests;
    
    // 计算任务完成率（基于响应的请求）
    uint64_t completion_rate = (responded_requests > 0) ? 
                               (completed_requests * 100) / responded_requests : 0;
    
    // 参与度 = (响应率 + 任务完成率) / 2
    uint64_t participation = (response_rate + completion_rate) / 2;
    
    return participation;
}

/**
 * 计算并更新所有成员的声誉统计
 */
int reputation_tracker_calculate_and_save_stats(void) {
    if (!g_tracker_state.initialized) {
        fprintf(stderr, "[REPUTATION] 跟踪系统未初始化\n");
        return -1;
    }
    
    pthread_mutex_lock(&g_tracker_state.mutex);
    
    // 从决策记录中提取所有唯一的地址
    typedef struct {
        char address[64];
        int found;
    } unique_address_t;
    
    unique_address_t unique_addresses[100] = {0};
    int unique_count = 0;
    
    FILE* decisions_fp = fopen(DECISIONS_FILE, "r");
    if (decisions_fp) {
        char line[512];
        if (fgets(line, sizeof(line), decisions_fp)) {  // 跳过表头
            while (fgets(line, sizeof(line), decisions_fp)) {
                char* fields[9];
                int field_count = 0;
                char line_copy[512];
                strncpy(line_copy, line, sizeof(line_copy) - 1);
                line_copy[sizeof(line_copy) - 1] = '\0';
                
                char* token = strtok(line_copy, ",");
                while (token && field_count < 9) {
                    fields[field_count++] = token;
                    token = strtok(NULL, ",");
                }
                
                if (field_count >= 8) {
                    // 新格式：participant_address在第4列（索引4）
                    // 旧格式（兼容）：participant_address在第5列（索引5）
                    const char* participant_addr = (field_count >= 8) ? fields[4] : fields[5];
                    
                    // 过滤无效地址：如果地址是纯数字（1, 2, 3）或不是0x开头，跳过
                    // 这些可能是旧的格式错误或地址获取失败的情况
                    if (participant_addr == NULL || strlen(participant_addr) == 0) {
                        continue;  // 跳过空地址
                    }
                    
                    // 检查是否是纯数字（可能是participant_id而不是address）
                    int is_numeric = 1;
                    for (size_t j = 0; j < strlen(participant_addr); j++) {
                        if (!isdigit((unsigned char)participant_addr[j])) {
                            is_numeric = 0;
                            break;
                        }
                    }
                    
                    // 如果地址不是以0x开头，可能是格式错误
                    int is_valid_address = (strlen(participant_addr) >= 2 && 
                                           participant_addr[0] == '0' && 
                                           (participant_addr[1] == 'x' || participant_addr[1] == 'X'));
                    
                    // 如果不是有效地址格式，尝试从committee_members.txt查找
                    char actual_address[64] = {0};
                    if (!is_valid_address) {
                        // 如果是纯数字（1-3），尝试作为participant_id查找
                        if (is_numeric && strlen(participant_addr) <= 3) {
                            int pid = atoi(participant_addr);
                            if (pid >= 1 && pid <= 3) {
                                FILE* committee_fp = fopen(COMMITTEE_MEMBERS_FILE, "r");
                                if (committee_fp) {
                                    char line[256];
                                    int line_num = 0;
                                    while (fgets(line, sizeof(line), committee_fp) && line_num < 3) {
                                        line_num++;
                                        if (line_num == pid) {
                                            size_t len = strlen(line);
                                            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                                                line[len-1] = '\0';
                                                len--;
                                            }
                                            if (len > 0) {
                                                strncpy(actual_address, line, sizeof(actual_address) - 1);
                                                actual_address[sizeof(actual_address) - 1] = '\0';
                                            }
                                            break;
                                        }
                                    }
                                    fclose(committee_fp);
                                    
                                    if (actual_address[0] != '\0') {
                                        participant_addr = actual_address;  // 使用找到的地址
                                        is_valid_address = 1;  // 现在有效了
                                    } else {
                                        fprintf(stderr, "[REPUTATION] 警告: 无法从committee_members.txt找到participant_id=%d对应的地址，跳过\n", pid);
                                        continue;  // 找不到，跳过
                                    }
                                } else {
                                    fprintf(stderr, "[REPUTATION] 警告: 无法打开committee_members.txt，跳过无效地址: %s\n", participant_addr);
                                    continue;  // 无法打开文件，跳过
                                }
                            } else {
                                fprintf(stderr, "[REPUTATION] 警告: 无效的participant_id: %s，跳过\n", participant_addr);
                                continue;  // 无效的participant_id，跳过
                            }
                        } else {
                            fprintf(stderr, "[REPUTATION] 警告: 无效的地址格式: %s，跳过\n", participant_addr);
                            continue;  // 不是标准地址格式且不是有效的participant_id，跳过
                        }
                    }
                    
                    // 检查是否已存在
                    int found = 0;
                    for (int i = 0; i < unique_count; i++) {
                        if (strcasecmp(unique_addresses[i].address, participant_addr) == 0) {
                            found = 1;
                            break;
                        }
                    }
                    
                    if (!found && unique_count < 100) {
                        snprintf(unique_addresses[unique_count].address, 
                                sizeof(unique_addresses[unique_count].address), 
                                "%s", participant_addr);
                        unique_count++;
                    }
                }
            }
        }
        fclose(decisions_fp);
    }
    
    // 计算每个地址的声誉
    FILE* stats_fp = fopen(STATS_FILE, "w");
    if (!stats_fp) {
        fprintf(stderr, "[REPUTATION] 无法打开统计文件: %s\n", STATS_FILE);
        pthread_mutex_unlock(&g_tracker_state.mutex);
        return -1;
    }
    
    // 写入表头（添加参与度列）
    fprintf(stats_fp, "address,total_decisions,correct_decisions,accuracy,consistency,participation,total_reputation,last_update\n");
    
    time_t update_time = time(NULL);
    
    for (int i = 0; i < unique_count; i++) {
        const char* addr = unique_addresses[i].address;
        
        // 计算准确率
        uint64_t accuracy = calculate_accuracy_for_address(addr);
        
        // 计算一致性
        uint64_t consistency = calculate_consistency_for_address(addr);
        
        // 计算参与度（新增）
        uint64_t participation = calculate_participation_for_address(addr);
        
        // 计算总决策数和正确决策数
        uint64_t total = 0;
        uint64_t correct = 0;
        
        FILE* decisions_fp2 = fopen(DECISIONS_FILE, "r");
        if (decisions_fp2) {
            char line[512];
            if (fgets(line, sizeof(line), decisions_fp2)) {  // 跳过表头
                while (fgets(line, sizeof(line), decisions_fp2)) {
                    char* fields[9];
                    int field_count = 0;
                    char line_copy[512];
                    strncpy(line_copy, line, sizeof(line_copy) - 1);
                    line_copy[sizeof(line_copy) - 1] = '\0';
                    
                    char* token = strtok(line_copy, ",");
                    while (token && field_count < 9) {
                        fields[field_count++] = token;
                        token = strtok(NULL, ",");
                    }
                    
                    if (field_count >= 8) {
                        // 新格式：participant_address在第4列，is_correct在第7列
                        // 旧格式（兼容）：participant_address在第5列，is_correct在第8列
                        const char* participant_addr = (field_count >= 8) ? fields[4] : fields[5];
                        int is_correct = (field_count >= 8) ? atoi(fields[7]) : atoi(fields[8]);
                        
                        if (strcasecmp(participant_addr, addr) == 0) {
                            total++;
                            if (is_correct) {
                                correct++;
                            }
                        }
                    }
                }
            }
            fclose(decisions_fp2);
        }
        
        uint64_t total_reputation = accuracy + consistency;
        
        fprintf(stats_fp, "%s,%lu,%lu,%lu,%lu,%lu,%lu,%ld\n",
                addr, total, correct, accuracy, consistency, participation, total_reputation, update_time);
        
        printf("[REPUTATION] 地址 %s: 准确率=%lu%%, 一致性=%lu%%, 参与度=%lu%%, 综合声誉=%lu, 总决策=%lu, 正确决策=%lu\n",
               addr, accuracy, consistency, participation, total_reputation, total, correct);
    }
    
    fclose(stats_fp);
    
    printf("[REPUTATION] 已保存声誉统计到: %s\n", STATS_FILE);
    
    pthread_mutex_unlock(&g_tracker_state.mutex);
    
    return 0;
}

/**
 * 清理资源
 */
void reputation_tracker_cleanup(void) {
    if (g_tracker_state.initialized) {
        pthread_mutex_destroy(&g_tracker_state.mutex);
        g_tracker_state.initialized = 0;
    }
}
