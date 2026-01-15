#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <math.h>

// Python脚本路径
#define PYTHON_DBSCAN_SCRIPT "/home/zxx/A2L/A2L-master/ecdsa/bin/dbscan_clustering.py"

// ========== 可疑地址信息结构 ==========
typedef struct {
    char address[43];
    char tx_hash[67];
    time_t timestamp;
    double anomaly_score;
    int cluster_id;
} suspicious_address_t;

// ========== 交易记录结构 ==========
typedef struct {
    char timestamp[64];
    char from_address[43];
    char to_address[43];
    char tx_hash[67];
    double value;
    time_t parsed_time;
} transaction_record_t;

// ========== 地址统计结构 ==========
typedef struct {
    char address[43];
    int tx_count;
    double total_value;
    time_t *tx_times;
    int tx_times_size;
    int tx_times_capacity;
    time_t first_tx_time;
    time_t last_tx_time;
} address_stat_t;

// ========== 地址特征结构 ==========
typedef struct {
    char address[43];
    double features[8];  // 8维特征向量
    int cluster_id;
    double anomaly_score;
    int is_suspicious;
} address_feature_t;

// ========== 延迟队列项结构 ==========
typedef struct delay_queue_item {
    char tx_hash[67];
    char address[43];
    time_t detection_time;
    time_t ready_time;  // 可以发送给auditor的时间
    int status;         // 0=等待中, 1=已发送, 2=已过期
    struct delay_queue_item *next;
} delay_queue_item_t;

// ========== 延迟队列管理 ==========
typedef struct {
    delay_queue_item_t *head;
    delay_queue_item_t *tail;
    int count;
} delay_queue_t;

// 全局延迟队列
static delay_queue_t g_delay_queue = {NULL, NULL, 0};
static const int DELAY_SECONDS = 30;  // 延迟30秒

// 函数声明
static int update_transaction_status(const char *tx_hash, const char *status);
static int get_pairs_summary_line(const char *address, char *line_buffer, size_t buffer_size);

// ========== 延迟队列管理函数 ==========
static delay_queue_item_t* create_delay_item(const char *tx_hash, const char *address) {
    delay_queue_item_t *item = (delay_queue_item_t*)malloc(sizeof(delay_queue_item_t));
    if (!item) return NULL;
    
    strncpy(item->tx_hash, tx_hash, sizeof(item->tx_hash) - 1);
    item->tx_hash[sizeof(item->tx_hash) - 1] = '\0';
    
    strncpy(item->address, address, sizeof(item->address) - 1);
    item->address[sizeof(item->address) - 1] = '\0';
    
    item->detection_time = time(NULL);
    item->ready_time = item->detection_time + DELAY_SECONDS;
    item->status = 0;  // 等待中
    item->next = NULL;
    
    return item;
}

static void add_to_delay_queue(const char *tx_hash, const char *address) {
    delay_queue_item_t *item = create_delay_item(tx_hash, address);
    if (!item) {
        printf("[DETECTION ERROR] 无法创建延迟队列项\n");
        return;
    }
    
    if (g_delay_queue.tail) {
        g_delay_queue.tail->next = item;
        g_delay_queue.tail = item;
    } else {
        g_delay_queue.head = g_delay_queue.tail = item;
    }
    g_delay_queue.count++;
    
    printf("[DETECTION] 📝 可疑交易已加入延迟队列\n");
    printf("[DETECTION]    交易哈希: %s\n", tx_hash);
    printf("[DETECTION]    地址: %s\n", address);
    printf("[DETECTION]    检测时间: %s", ctime(&item->detection_time));
    printf("[DETECTION]    预计发送时间: %s", ctime(&item->ready_time));
    printf("[DETECTION]    队列长度: %d\n", g_delay_queue.count);
}

static void process_ready_items() {
    time_t current_time = time(NULL);
    delay_queue_item_t *current = g_delay_queue.head;
    int processed_count = 0;
    
    printf("[DETECTION] 🔍 检查延迟队列中的项目...\n");
    
    while (current) {
        printf("[DETECTION]   检查项目: %s\n", current->tx_hash);
        printf("[DETECTION]     状态: %d (0=等待中, 1=已发送, 2=已过期)\n", current->status);
        printf("[DETECTION]     检测时间: %s", ctime(&current->detection_time));
        printf("[DETECTION]     预计发送时间: %s", ctime(&current->ready_time));
        printf("[DETECTION]     当前时间: %s", ctime(&current_time));
        printf("[DETECTION]     时间差: %ld 秒 (需要 >= 0)\n", current_time - current->ready_time);
        
        if (current->status == 0 && current->ready_time <= current_time) {
            // 时间到了，发送给auditor
            printf("\n[DETECTION] ========== 发送给审计员 ==========\n");
            printf("[DETECTION] 交易哈希: %s\n", current->tx_hash);
            printf("[DETECTION] 地址: %s\n", current->address);
            printf("[DETECTION] 延迟时间: %ld 秒\n", current_time - current->detection_time);
            
            // 从pairs_summary.csv获取对应地址的行
            char pairs_summary_line[2048];
            int pairs_result = get_pairs_summary_line(current->address, pairs_summary_line, sizeof(pairs_summary_line));
            if (pairs_result == 0) {
                printf("[DETECTION] pairs_summary行: %s\n", pairs_summary_line);
            } else {
                printf("[DETECTION] ⚠️  无法获取pairs_summary行，使用空字符串\n");
                pairs_summary_line[0] = '\0';
            }
            
            // 调用auditor进行审计（传入三个参数：交易哈希、地址、pairs_summary行）
            // 使用单引号包裹参数，更安全地处理特殊字符
            char auditor_cmd[4096];
            
            // 转义pairs_summary_line中的单引号（在单引号字符串中，单引号需要特殊处理）
            // 方法：将单引号替换为 '\''（结束当前单引号字符串，插入转义的单引号，开始新的单引号字符串）
            char escaped_pairs[4096];
            size_t escaped_idx = 0;
            for (size_t i = 0; i < strlen(pairs_summary_line) && escaped_idx < sizeof(escaped_pairs) - 4; i++) {
                if (pairs_summary_line[i] == '\'') {
                    // 单引号在单引号字符串中需要特殊处理：结束当前字符串，插入转义单引号，开始新字符串
                    escaped_pairs[escaped_idx++] = '\'';
                    escaped_pairs[escaped_idx++] = '\\';
                    escaped_pairs[escaped_idx++] = '\'';
                    escaped_pairs[escaped_idx++] = '\'';
                } else {
                    escaped_pairs[escaped_idx++] = pairs_summary_line[i];
                }
            }
            escaped_pairs[escaped_idx] = '\0';
            
            // 使用单引号包裹所有参数，更安全
            snprintf(auditor_cmd, sizeof(auditor_cmd), 
                    "/home/zxx/A2L/A2L-master/ecdsa/bin/auditor '%s' '%s' '%s' 2>&1", 
                    current->tx_hash, current->address, escaped_pairs);
            
            printf("[DETECTION] 执行审计命令: %s\n", auditor_cmd);
            
            // 使用popen来捕获auditor的输出
            FILE *auditor_pipe = popen(auditor_cmd, "r");
            if (!auditor_pipe) {
                printf("[DETECTION] ❌ 无法启动审计员进程\n");
                current->status = 2;  // 标记为过期
                update_transaction_status(current->tx_hash, "FAILED");
            } else {
                char output_line[1024];
                int found_result = 0;
                char all_output[8192] = {0};  // 保存所有输出用于错误诊断
                size_t output_len = 0;
                
                // 读取auditor的所有输出
                while (fgets(output_line, sizeof(output_line), auditor_pipe) != NULL) {
                    // 保存所有输出
                    size_t line_len = strlen(output_line);
                    if (output_len + line_len < sizeof(all_output) - 1) {
                        strcat(all_output, output_line);
                        output_len += line_len;
                    }
                    
                    // 显示包含确认信息的行，并保持原始格式
                    if (strstr(output_line, "confirm txHash") != NULL) {
                        // 直接输出，不添加额外前缀，保持auditor的原始格式
                        printf("%s", output_line);
                        found_result = 1;
                    }
                }
                
                int result = pclose(auditor_pipe);
                
                if (result == 0 && found_result) {
                    printf("[DETECTION] ✅ 审计员执行成功\n");
                    current->status = 1;  // 已发送
                    // 更新文件状态为 AUDITED
                    update_transaction_status(current->tx_hash, "AUDITED");
                } else {
                    printf("[DETECTION] ❌ 审计员执行失败 (返回码: %d, 找到结果: %d)\n", result, found_result);
                    if (output_len > 0) {
                        printf("[DETECTION] 审计员输出:\n%s", all_output);
                    } else {
                        printf("[DETECTION] 审计员无输出\n");
                    }
                    current->status = 2;  // 标记为过期，避免重复尝试
                    // 更新文件状态为 FAILED
                    update_transaction_status(current->tx_hash, "FAILED");
                }
            }
            
            printf("[DETECTION] ========================================\n\n");
            processed_count++;
        } else {
            if (current->status != 0) {
                printf("[DETECTION]     跳过: 状态不是等待中 (%d)\n", current->status);
            } else {
                printf("[DETECTION]     跳过: 时间未到 (还需要 %ld 秒)\n", current->ready_time - current_time);
            }
        }
        
        // 移动到下一个
        current = current->next;
    }
    
    printf("[DETECTION] 🔍 队列检查完成，处理了 %d 个项目\n", processed_count);
}

static void cleanup_expired_items() {
    delay_queue_item_t *current = g_delay_queue.head;
    delay_queue_item_t *prev = NULL;
    
    while (current) {
        if (current->status == 1 || current->status == 2) {
            // 已发送或已过期，从队列中移除
            if (prev) {
                prev->next = current->next;
            } else {
                g_delay_queue.head = current->next;
            }
            
            if (current == g_delay_queue.tail) {
                g_delay_queue.tail = prev;
            }
            
            delay_queue_item_t *to_free = current;
            current = current->next;
            free(to_free);
            g_delay_queue.count--;
        } else {
            prev = current;
            current = current->next;
        }
    }
}

static void print_queue_status() {
    printf("[DETECTION] 📊 延迟队列状态: %d 项\n", g_delay_queue.count);
    
    delay_queue_item_t *current = g_delay_queue.head;
    int index = 1;
    time_t current_time = time(NULL);
    
    while (current && index <= 5) {  // 只显示前5项
        int remaining = current->ready_time - current_time;
        if (remaining < 0) remaining = 0;
        
        printf("[DETECTION]   [%d] %s (剩余: %d秒, 状态: %s)\n", 
               index, 
               current->tx_hash,
               remaining,
               current->status == 0 ? "等待中" : 
               current->status == 1 ? "已发送" : "已过期");
        
        current = current->next;
        index++;
    }
    
    if (g_delay_queue.count > 5) {
        printf("[DETECTION]   ... 还有 %d 项未显示\n", g_delay_queue.count - 5);
    }
}

// ========== 时间戳解析函数 ==========
static time_t parse_timestamp(const char *timestamp_str) {
    struct tm tm = {0};
    // 格式: "YYYY-MM-DD HH:MM:SS"
    if (sscanf(timestamp_str, "%d-%d-%d %d:%d:%d", 
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900;  // 年份从 1900 开始
        tm.tm_mon -= 1;      // 月份从 0 开始
        return mktime(&tm);
    }
    return 0;
}

// ========== CSV 解析函数 ==========
static int parse_csv_line(const char *line, transaction_record_t *record) {
    if (line == NULL || record == NULL || line[0] == '\0') {
        return -1;
    }
    
    // 跳过表头
    if (strncmp(line, "Timestamp", 9) == 0) {
        return -1;
    }
    
    // 解析 CSV 行（字段用引号包围，逗号分隔）
    char line_copy[2048];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    
    char *fields[9] = {NULL};
    int field_count = 0;
    char *p = line_copy;
    
    while (*p && field_count < 9) {
        // 跳过空格
        while (*p == ' ') p++;
        
        if (*p == '"') {
            // 引号包围的字段
            p++; // 跳过开始引号
            fields[field_count] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') {
                *p = '\0';
                p++;
            }
            field_count++;
        } else {
            // 非引号字段
            fields[field_count] = p;
            while (*p && *p != ',') p++;
            if (*p == ',') {
                *p = '\0';
                p++;
            }
            field_count++;
        }
        
        // 跳过逗号
        while (*p == ',' || *p == ' ') p++;
    }
    
    if (field_count < 9) {
        return -1;
    }
    
    // 复制字段到记录结构
    strncpy(record->timestamp, fields[0], sizeof(record->timestamp) - 1);
    record->timestamp[sizeof(record->timestamp) - 1] = '\0';
    strncpy(record->tx_hash, fields[1], sizeof(record->tx_hash) - 1);
    record->tx_hash[sizeof(record->tx_hash) - 1] = '\0';
    strncpy(record->from_address, fields[2], sizeof(record->from_address) - 1);
    record->from_address[sizeof(record->from_address) - 1] = '\0';
    strncpy(record->to_address, fields[3], sizeof(record->to_address) - 1);
    record->to_address[sizeof(record->to_address) - 1] = '\0';
    record->value = strtod(fields[4], NULL);
    
    // 解析时间戳
    record->parsed_time = parse_timestamp(record->timestamp);
    
    return 0;
}

// ========== 检查交易哈希是否已存在 ==========
static int is_txhash_exists(const char *file_path, const char *tx_hash) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        return 0;  // 文件不存在，哈希不存在
    }
    
    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        // 跳过表头
        if (strncmp(line, "txhash", 6) == 0) {
            continue;
        }
        
        // 检查是否包含该交易哈希
        if (strstr(line, tx_hash) != NULL) {
            fclose(file);
            return 1;  // 哈希已存在
        }
    }
    
    fclose(file);
    return 0;  // 哈希不存在
}

// ========== 调用拦截API ==========
static int call_intercepted_api(const char *address) {
    const char *api_url = "http://127.0.0.1:8000/intercepted";
    
    // 构造 curl 命令
    char curl_cmd[1024];
    snprintf(curl_cmd, sizeof(curl_cmd),
             "curl -s -X POST -H \"Content-Type: application/json\" "
             "-d '{\"id\": \"%s\"}' %s",
             address, api_url);
    
    printf("[DETECTION] 📡 调用拦截API: %s\n", api_url);
    printf("[DETECTION] 📤 发送地址: %s\n", address);
    
    // 执行 curl 命令
    FILE *fp = popen(curl_cmd, "r");
    if (!fp) {
        fprintf(stderr, "[DETECTION ERROR] 无法执行 curl 命令\n");
        return -1;
    }
    
    // 读取响应
    char response[1024];
    size_t total_read = 0;
    while (fgets(response + total_read, sizeof(response) - total_read, fp) != NULL) {
        total_read = strlen(response);
        if (total_read >= sizeof(response) - 1) break;
    }
    
    int status = pclose(fp);
    
    if (status == 0) {
        printf("[DETECTION] ✅ API调用成功\n");
        if (total_read > 0) {
            printf("[DETECTION] 📥 响应: %s\n", response);
        }
        return 0;
    } else {
        fprintf(stderr, "[DETECTION ERROR] API调用失败，返回码: %d\n", status);
        if (total_read > 0) {
            fprintf(stderr, "[DETECTION ERROR] 响应: %s\n", response);
        }
        return -1;
    }
}

// ========== 保存可疑交易到文件 ==========
static int save_suspicious_transaction_to_file(const char *tx_hash, const char *address, time_t latest_time) {
    const char *detect_dir = "/home/zxx/A2L/A2L-master/ecdsa/bin/detect_transaction";
    
    // 确保目录存在
    char mkdir_cmd[512];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", detect_dir);
    int mkdir_result = system(mkdir_cmd);
    if (mkdir_result != 0) {
        fprintf(stderr, "[DETECTION] Warning: Failed to create directory %s\n", detect_dir);
    }
    
    // 生成文件路径
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/suspicious_transactions.csv", detect_dir);
    
    // 检查该交易哈希是否已存在
    if (is_txhash_exists(file_path, tx_hash)) {
        printf("[DETECTION] ⏭️  交易哈希 %s 已存在于文件中，跳过保存\n", tx_hash);
        return 0;
    }
    
    // 检查文件是否存在
    int file_exists = 0;
    FILE *check_file = fopen(file_path, "r");
    if (check_file) {
        file_exists = 1;
        fclose(check_file);
    }
    
    // 追加方式打开文件
    FILE *file = fopen(file_path, "a");
    if (!file) {
        fprintf(stderr, "[DETECTION ERROR] 无法打开文件: %s\n", file_path);
        return -1;
    }
    
    // 如果是新文件，写入表头
    if (!file_exists) {
        fprintf(file, "txhash,address,time,status\n");
    }
    
    // 格式化时间
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&latest_time));
    
    // 写入CSV格式数据
    fprintf(file, "\"%s\",\"%s\",\"%s\",\"PENDING\"\n", tx_hash, address, time_str);
    
    fclose(file);
    
    printf("[DETECTION] ✅ 可疑交易已保存到文件: %s\n", file_path);
    return 0;
}

// ========== 从pairs_summary.csv获取对应地址的行 ==========
static int get_pairs_summary_line(const char *address, char *line_buffer, size_t buffer_size) {
    const char *pairs_summary_file = "/home/zxx/A2L/A2L-master/ecdsa/bin/log_game/pairs_summary.csv";
    
    FILE *file = fopen(pairs_summary_file, "r");
    if (!file) {
        printf("[DETECTION] ⚠️  无法打开pairs_summary.csv文件: %s\n", pairs_summary_file);
        line_buffer[0] = '\0';
        return -1;
    }
    
    char line[2048];
    int found = 0;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // 检查行是否以该地址开头
        if (strncmp(line, address, strlen(address)) == 0) {
            // 找到匹配的行，复制到缓冲区
            size_t line_len = strlen(line);
            // 去除末尾的换行符
            if (line_len > 0 && line[line_len - 1] == '\n') {
                line[line_len - 1] = '\0';
                line_len--;
            }
            
            if (line_len < buffer_size) {
                strncpy(line_buffer, line, buffer_size - 1);
                line_buffer[buffer_size - 1] = '\0';
                found = 1;
            } else {
                // 行太长，截断
                strncpy(line_buffer, line, buffer_size - 1);
                line_buffer[buffer_size - 1] = '\0';
                found = 1;
            }
            break;
        }
    }
    
    fclose(file);
    
    if (!found) {
        printf("[DETECTION] ⚠️  在pairs_summary.csv中未找到地址: %s\n", address);
        line_buffer[0] = '\0';
        return -1;
    }
    
    return 0;
}

// ========== 更新文件中的交易状态 ==========
static int update_transaction_status(const char *tx_hash, const char *status) {
    const char *detect_dir = "/home/zxx/A2L/A2L-master/ecdsa/bin/detect_transaction";
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/suspicious_transactions.csv", detect_dir);
    
    // 读取文件内容
    FILE *file = fopen(file_path, "r");
    if (!file) {
        return -1;
    }
    
    char lines[1000][1024];  // 假设最多1000行
    int line_count = 0;
    char line[1024];
    
    while (fgets(line, sizeof(line), file) && line_count < 1000) {
        strcpy(lines[line_count], line);
        line_count++;
    }
    fclose(file);
    
    // 更新状态
    for (int i = 0; i < line_count; i++) {
        if (strstr(lines[i], tx_hash) != NULL) {
            // 找到对应的行，更新状态
            char *comma_pos = strrchr(lines[i], ',');
            if (comma_pos) {
                snprintf(comma_pos + 1, sizeof(lines[i]) - (comma_pos - lines[i]), "%s\n", status);
            }
            break;
        }
    }
    
    // 写回文件
    file = fopen(file_path, "w");
    if (!file) {
        return -1;
    }
    
    for (int i = 0; i < line_count; i++) {
        fputs(lines[i], file);
    }
    fclose(file);
    
    return 0;
}

// ========== 处理可疑交易（保存到文件 + 加入延迟队列） ==========
static int handle_suspicious_transaction(const char *tx_hash, const char *address, int count, time_t latest_time) {
    // 检查该交易哈希是否已在延迟队列中
    delay_queue_item_t *current = g_delay_queue.head;
    while (current) {
        if (strcmp(current->tx_hash, tx_hash) == 0) {
            printf("[DETECTION] ⏭️  交易哈希 %s 已在延迟队列中，跳过\n", tx_hash);
            return 0;
        }
        current = current->next;
    }
    
    // 1. 保存到文件（用于记录和调试）
    save_suspicious_transaction_to_file(tx_hash, address, latest_time);
    
    // 2. 加入延迟队列（用于延迟审计）
    add_to_delay_queue(tx_hash, address);
    
    // 3. 调用拦截API（立即调用，不需要延迟）
    printf("[DETECTION] ========== 调用拦截API ==========\n");
    int api_result = call_intercepted_api(address);
    if (api_result == 0) {
        printf("[DETECTION] ✅ 拦截API调用成功\n");
    } else {
        fprintf(stderr, "[DETECTION WARNING] ⚠️  拦截API调用失败，但交易已记录\n");
    }
    printf("[DETECTION] ========================================\n");
    
    return 0;
}

// DBSCAN参数（仅用于显示，实际在Python中）
#define DBSCAN_EPS 0.3              // 邻域半径
#define DBSCAN_MIN_SAMPLES 3         // 最小样本数
#define FEATURE_DIM 8                // 特征维度

// ========== 解析Python输出的JSON结果 ==========
static int parse_suspicious_addresses_json(const char *json_str, suspicious_address_t *suspicious_list, int max_count) {
    int count = 0;
    const char *p = json_str;
    
    // 查找 "suspicious_addresses": [ 的位置
    const char *array_start = strstr(p, "\"suspicious_addresses\"");
    if (!array_start) {
        printf("[CLUSTERING ERROR] 未找到suspicious_addresses字段\n");
        return 0;
    }
    
    // 找到数组开始位置 [
    array_start = strchr(array_start, '[');
    if (!array_start) {
        printf("[CLUSTERING ERROR] 未找到数组开始标记\n");
        return 0;
    }
    
    p = array_start + 1;  // 跳过 [
    
    // 解析数组中的每个对象
    while (*p && count < max_count) {
        // 跳过空格、逗号、换行符
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        
        if (*p == ']') break;  // 数组结束
        
        if (*p != '{') {
            p++;
            continue;
        }
        
        // 解析一个对象
        suspicious_address_t *item = &suspicious_list[count];
        memset(item, 0, sizeof(suspicious_address_t));
        
        p++;  // 跳过 {
        
        // 解析字段
        while (*p && *p != '}') {
            // 跳过空格
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
            
            if (*p == '}') break;
            
            // 解析字段名
            if (*p == '"') {
                p++;  // 跳过开始引号
                const char *field_start = p;
                while (*p && *p != '"') p++;
                if (*p != '"') break;
                
                size_t field_len = p - field_start;
                p++;  // 跳过结束引号
                
                // 跳过 : 和空格
                while (*p == ' ' || *p == ':') p++;
                
                // 解析字段值
                if (strncmp(field_start, "address", field_len) == 0 && *p == '"') {
                    p++;  // 跳过开始引号
                    const char *val_start = p;
                    while (*p && *p != '"') p++;
                    size_t val_len = p - val_start;
                    if (val_len < sizeof(item->address)) {
                        strncpy(item->address, val_start, val_len);
                        item->address[val_len] = '\0';
                    }
                    if (*p == '"') p++;
                } else if (strncmp(field_start, "tx_hash", field_len) == 0 && *p == '"') {
                    p++;  // 跳过开始引号
                    const char *val_start = p;
                    while (*p && *p != '"') p++;
                    size_t val_len = p - val_start;
                    if (val_len < sizeof(item->tx_hash)) {
                        strncpy(item->tx_hash, val_start, val_len);
                        item->tx_hash[val_len] = '\0';
                    }
                    if (*p == '"') p++;
                } else if (strncmp(field_start, "timestamp", field_len) == 0) {
                    // 解析时间戳（数字）
                    item->timestamp = 0;
                    while (*p >= '0' && *p <= '9') {
                        item->timestamp = item->timestamp * 10 + (*p - '0');
                        p++;
                    }
                } else if (strncmp(field_start, "anomaly_score", field_len) == 0) {
                    // 解析异常分数（浮点数）
                    item->anomaly_score = strtod(p, (char**)&p);
                } else if (strncmp(field_start, "cluster_id", field_len) == 0) {
                    // 解析簇ID（可能是负数）
                    int sign = 1;
                    if (*p == '-') {
                        sign = -1;
                        p++;
                    }
                    item->cluster_id = 0;
                    while (*p >= '0' && *p <= '9') {
                        item->cluster_id = item->cluster_id * 10 + (*p - '0');
                        p++;
                    }
                    item->cluster_id *= sign;
                } else {
                    // 跳过未知字段的值
                    if (*p == '"') {
                        p++;
                        while (*p && *p != '"') p++;
                        if (*p == '"') p++;
                    } else if (*p == '{') {
                        // 嵌套对象，跳过
                        int depth = 1;
                        p++;
                        while (*p && depth > 0) {
                            if (*p == '{') depth++;
                            else if (*p == '}') depth--;
                            p++;
                        }
                    } else {
                        // 数字或其他，跳过到下一个逗号或}
                        while (*p && *p != ',' && *p != '}') p++;
                    }
                }
            } else {
                p++;
            }
            
            // 跳过逗号
            while (*p == ' ' || *p == ',') p++;
        }
        
        if (*p == '}') {
            p++;
            count++;
        }
    }
    
    return count;
}

// ========== 聚类分析检测函数（调用Python脚本） ==========
static int detect_high_frequency_transactions(const char *csv_file) {
    printf("\n[DETECTION] ========== 开始聚类分析检测 ==========\n");
    printf("[DETECTION] 调用Python脚本: %s\n", PYTHON_DBSCAN_SCRIPT);
    printf("[DETECTION] CSV文件: %s\n", csv_file);
    
    // 构建Python命令
    char python_cmd[2048];
    char temp_output_file[512];
    snprintf(temp_output_file, sizeof(temp_output_file), "/tmp/dbscan_output_%d.json", getpid());
    
    // 调用Python脚本（Python脚本内部使用固定的CSV文件路径，输出到临时文件）
    snprintf(python_cmd, sizeof(python_cmd), 
             "python3 %s > %s 2>&1", 
             PYTHON_DBSCAN_SCRIPT, temp_output_file);
    
    printf("[DETECTION] 执行命令: %s\n", python_cmd);
    
    int ret = system(python_cmd);
    if (ret != 0) {
        printf("[DETECTION ERROR] Python脚本执行失败，返回码: %d\n", ret);
        // 读取错误输出
        FILE *err_file = fopen(temp_output_file, "r");
        if (err_file) {
            char err_buf[1024];
            while (fgets(err_buf, sizeof(err_buf), err_file)) {
                printf("[DETECTION ERROR] %s", err_buf);
            }
            fclose(err_file);
        }
        unlink(temp_output_file);
        return 0;
    }
    
    // 读取Python脚本的输出
    FILE *output_file = fopen(temp_output_file, "r");
    if (!output_file) {
        printf("[DETECTION ERROR] 无法读取Python脚本输出\n");
        unlink(temp_output_file);
        return 0;
    }
    
    char json_output[1024 * 1024] = {0};
    size_t output_len = 0;
    char line[4096];
    
    while (fgets(line, sizeof(line), output_file) && output_len < sizeof(json_output) - 1) {
        size_t line_len = strlen(line);
        if (output_len + line_len < sizeof(json_output) - 1) {
            strcat(json_output, line);
            output_len += line_len;
        }
    }
    fclose(output_file);
    
    // 清理临时文件
    unlink(temp_output_file);
    
    // 解析可疑地址列表
    suspicious_address_t suspicious_list[1000];
    int suspicious_count = parse_suspicious_addresses_json(json_output, suspicious_list, 1000);
    
    if (suspicious_count == 0) {
        printf("[DETECTION] 未检测到可疑地址\n");
        printf("[DETECTION] ========== 聚类分析检测完成 ==========\n\n");
        return 0;
    }
    
    printf("[DETECTION] Python脚本识别了 %d 个可疑地址\n", suspicious_count);
    
    // ========== 处理可疑交易 ==========
    int saved_count = 0;
    
    for (int i = 0; i < suspicious_count; i++) {
        printf("\n[DETECTION] ⚠️  检测到可疑地址（聚类分析）!\n");
        printf("[DETECTION] 地址: %s\n", suspicious_list[i].address);
        printf("[DETECTION] 最新交易哈希: %s\n", suspicious_list[i].tx_hash);
        printf("[DETECTION] 异常分数: %.2f\n", suspicious_list[i].anomaly_score);
        printf("[DETECTION] 簇ID: %d\n", suspicious_list[i].cluster_id);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                localtime(&suspicious_list[i].timestamp));
        printf("[DETECTION] 最新交易时间: %s\n", time_str);
        
        // 直接处理可疑交易（加入延迟队列）
        if (handle_suspicious_transaction(suspicious_list[i].tx_hash, 
                                         suspicious_list[i].address, 
                                         0,  // count参数不再使用
                                         suspicious_list[i].timestamp) == 0) {
            saved_count++;
        }
    }
    
    printf("[DETECTION] 共检测到 %d 个可疑地址，全部已处理\n", suspicious_count);
    
    printf("[DETECTION] ========== 聚类分析检测完成 ==========\n\n");
    
    return saved_count;
}

// ========== 信号处理函数 ==========
static void cleanup_and_exit(int sig) {
    printf("\n[DETECTION] 收到退出信号 (%d)，正在清理...\n", sig);
    
    // 清理延迟队列
    delay_queue_item_t *current = g_delay_queue.head;
    while (current) {
        delay_queue_item_t *next = current->next;
        free(current);
        current = next;
    }
    
    printf("[DETECTION] 清理完成，程序退出\n");
    exit(0);
}

// ========== 步骤1：统计地址信息 ==========
static int aggregate_address_statistics(transaction_record_t *records, int record_count,
                                       address_stat_t **address_stats, int *address_count) {
    if (records == NULL || record_count <= 0) {
        return -1;
    }
    
    // 使用简单的哈希表或数组来统计唯一地址
    address_stat_t stats[10000];
    int count = 0;
    
    for (int i = 0; i < record_count; i++) {
        // 查找或创建地址统计
        int found = 0;
        int idx = -1;
        for (int j = 0; j < count; j++) {
            if (strcmp(stats[j].address, records[i].from_address) == 0) {
                found = 1;
                idx = j;
                break;
            }
        }
        
        if (!found) {
            if (count >= 10000) {
                printf("[CLUSTERING ERROR] 地址数量超过限制\n");
                return -1;
            }
            idx = count++;
            memset(&stats[idx], 0, sizeof(address_stat_t));
            strncpy(stats[idx].address, records[i].from_address, sizeof(stats[idx].address) - 1);
            stats[idx].address[sizeof(stats[idx].address) - 1] = '\0';
            stats[idx].tx_times_capacity = 16;
            stats[idx].tx_times = (time_t*)malloc(stats[idx].tx_times_capacity * sizeof(time_t));
        }
        
        address_stat_t *stat = &stats[idx];
        stat->tx_count++;
        double value = records[i].value;
        stat->total_value += value;
        
        // 更新交易时间
        if (stat->tx_times_size >= stat->tx_times_capacity) {
            stat->tx_times_capacity *= 2;
            stat->tx_times = (time_t*)realloc(stat->tx_times, 
                                             stat->tx_times_capacity * sizeof(time_t));
        }
        stat->tx_times[stat->tx_times_size++] = records[i].parsed_time;
        
        // 更新首次和最后交易时间
        if (stat->tx_count == 1) {
            stat->first_tx_time = records[i].parsed_time;
            stat->last_tx_time = records[i].parsed_time;
        } else {
            if (records[i].parsed_time < stat->first_tx_time) {
                stat->first_tx_time = records[i].parsed_time;
            }
            if (records[i].parsed_time > stat->last_tx_time) {
                stat->last_tx_time = records[i].parsed_time;
            }
        }
    }
    
    printf("[CLUSTERING] 共统计 %d 个唯一地址\n", count);
    *address_stats = (address_stat_t*)malloc(count * sizeof(address_stat_t));
    memcpy(*address_stats, stats, count * sizeof(address_stat_t));
    *address_count = count;
    return 0;
}

// ========== 步骤2：特征工程 - 计算特征向量 ==========
static int extract_features(address_stat_t *address_stats, int address_count,
                            address_feature_t **features, double *feature_min, double *feature_max) {
    printf("[CLUSTERING] 步骤2: 特征工程 - 计算特征向量\n");
    
    address_feature_t *feat = (address_feature_t*)calloc(address_count, sizeof(address_feature_t));
    if (!feat) {
        printf("[CLUSTERING ERROR] 内存分配失败\n");
        return -1;
    }
    
    // 初始化特征范围（用于归一化）
    for (int i = 0; i < FEATURE_DIM; i++) {
        feature_min[i] = 1e10;
        feature_max[i] = -1e10;
    }
    
    time_t current_time = time(NULL);
    
    // 为每个地址计算特征
    for (int i = 0; i < address_count; i++) {
        address_stat_t *stat = &address_stats[i];
        strncpy(feat[i].address, stat->address, sizeof(feat[i].address) - 1);
        feat[i].address[sizeof(feat[i].address) - 1] = '\0';
        feat[i].cluster_id = -2;  // 未分类
        feat[i].anomaly_score = 0.0;
        feat[i].is_suspicious = 0;
        
        // 特征0: 总交易数（归一化）
        feat[i].features[0] = (double)stat->tx_count;
        
        // 特征1: 平均交易间隔（秒）
        double avg_interval = 0.0;
        if (stat->tx_count > 1 && stat->tx_times_size > 1) {
            double total_interval = 0.0;
            for (int j = 1; j < stat->tx_times_size; j++) {
                total_interval += (double)(stat->tx_times[j] - stat->tx_times[j-1]);
            }
            avg_interval = total_interval / (stat->tx_times_size - 1);
        }
        feat[i].features[1] = avg_interval;
        
        // 特征2: 交易间隔标准差
        double interval_std = 0.0;
        if (stat->tx_count > 1 && stat->tx_times_size > 1) {
            double sum_sq_diff = 0.0;
            for (int j = 1; j < stat->tx_times_size; j++) {
                double interval = (double)(stat->tx_times[j] - stat->tx_times[j-1]);
                double diff = interval - avg_interval;
                sum_sq_diff += diff * diff;
            }
            interval_std = sqrt(sum_sq_diff / (stat->tx_times_size - 1));
        }
        feat[i].features[2] = interval_std;
        
        // 特征3: 1分钟内交易数
        int tx_1min = 0;
        for (int j = 0; j < stat->tx_times_size; j++) {
            if (current_time - stat->tx_times[j] <= 60) tx_1min++;
        }
        feat[i].features[3] = (double)tx_1min;
        
        // 特征4: 5分钟内交易数
        int tx_5min = 0;
        for (int j = 0; j < stat->tx_times_size; j++) {
            if (current_time - stat->tx_times[j] <= 300) tx_5min++;
        }
        feat[i].features[4] = (double)tx_5min;
        
        // 特征5: 1小时内交易数
        int tx_1hour = 0;
        for (int j = 0; j < stat->tx_times_size; j++) {
            if (current_time - stat->tx_times[j] <= 3600) tx_1hour++;
        }
        feat[i].features[5] = (double)tx_1hour;
        
        // 特征6: 总交易金额（保留，但不使用平均金额）
        feat[i].features[6] = stat->total_value;
        
        // 特征7: 地址活跃时长（秒）
        double active_duration = 0.0;
        if (stat->tx_count > 1) {
            active_duration = (double)(stat->last_tx_time - stat->first_tx_time);
        }
        feat[i].features[7] = active_duration;
        
        // 更新特征范围
        for (int j = 0; j < FEATURE_DIM; j++) {
            if (feat[i].features[j] < feature_min[j]) feature_min[j] = feat[i].features[j];
            if (feat[i].features[j] > feature_max[j]) feature_max[j] = feat[i].features[j];
        }
    }
    
    *features = feat;
    return 0;
}

// ========== 步骤2续：特征归一化 ==========
static void normalize_features(address_feature_t *features, int count,
                               const double *feature_min, const double *feature_max) {
    printf("[CLUSTERING] 步骤2续: 特征归一化\n");
    
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < FEATURE_DIM; j++) {
            double range = feature_max[j] - feature_min[j];
            if (range > 1e-10) {
                // 归一化到 [0, 1]
                features[i].features[j] = (features[i].features[j] - feature_min[j]) / range;
            } else {
                // 如果范围太小，设为0.5
                features[i].features[j] = 0.5;
            }
        }
    }
}

// ========== 辅助函数：解析JSON中的cluster_labels数组 ==========
static int parse_cluster_labels(const char *json_str, int *cluster_labels, int count) {
    // 查找 "cluster_labels": [ 的位置
    const char *labels_start = strstr(json_str, "\"cluster_labels\"");
    if (!labels_start) {
        printf("[CLUSTERING ERROR] 未找到cluster_labels字段\n");
        printf("[CLUSTERING DEBUG] JSON内容: %.500s\n", json_str);
        return -1;
    }
    
    // 找到数组开始位置 [
    const char *array_start = strchr(labels_start, '[');
    if (!array_start) {
        printf("[CLUSTERING ERROR] 未找到数组开始标记\n");
        return -1;
    }
    
    array_start++; // 跳过 [
    
    // 解析数组中的每个数字
    int idx = 0;
    const char *p = array_start;
    
    while (*p && idx < count) {
        // 跳过空格、逗号、换行符
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        
        if (*p == ']') break; // 数组结束
        
        // 解析数字（可能是负数）
        int sign = 1;
        if (*p == '-') {
            sign = -1;
            p++;
        }
        
        if (*p >= '0' && *p <= '9') {
            int value = 0;
            while (*p >= '0' && *p <= '9') {
                value = value * 10 + (*p - '0');
                p++;
            }
            cluster_labels[idx++] = value * sign;
        } else if (*p != ']' && *p != '\0') {
            // 跳过非数字字符（可能是JSON的其他部分）
            p++;
        } else {
            break;
        }
    }
    
    if (idx != count) {
        printf("[CLUSTERING WARNING] 解析了 %d 个标签，期望 %d 个\n", idx, count);
    }
    
    return idx;
}

// ========== 步骤3：DBSCAN聚类（调用Python脚本） ==========
static void dbscan_clustering(address_feature_t *features, int count) {
    printf("[CLUSTERING] 步骤3: DBSCAN聚类分析（使用Python scikit-learn）\n");
    printf("[CLUSTERING] 参数: eps=%.2f, min_samples=%d\n", DBSCAN_EPS, DBSCAN_MIN_SAMPLES);
    
    if (count == 0) {
        printf("[CLUSTERING WARNING] 没有特征数据需要聚类\n");
        return;
    }
    
    // 构建JSON输入数据
    char json_input[1024 * 1024] = {0}; // 1MB缓冲区
    int json_len = 0;
    
    // 开始JSON对象
    json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, 
                        "{\"features\":[");
    
    // 写入特征向量
    for (int i = 0; i < count; i++) {
        if (i > 0) json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, ",");
        json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, "[");
        
        for (int j = 0; j < FEATURE_DIM; j++) {
            if (j > 0) json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, ",");
            json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, 
                               "%.10f", features[i].features[j]);
        }
        json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, "]");
    }
    
    // 写入地址列表
    json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, 
                        "],\"addresses\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, ",");
        json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, 
                           "\"%s\"", features[i].address);
    }
    
    // 写入参数
    json_len += snprintf(json_input + json_len, sizeof(json_input) - json_len, 
                        "],\"eps\":%.2f,\"min_samples\":%d}", 
                        DBSCAN_EPS, DBSCAN_MIN_SAMPLES);
    
    // 使用临时文件方式传递数据（popen只能单向通信）
    char temp_input_file[512];
    char temp_output_file[512];
    snprintf(temp_input_file, sizeof(temp_input_file), "/tmp/dbscan_input_%d.json", getpid());
    snprintf(temp_output_file, sizeof(temp_output_file), "/tmp/dbscan_output_%d.json", getpid());
    
    // 写入输入文件
    FILE *input_file = fopen(temp_input_file, "w");
    if (!input_file) {
        printf("[CLUSTERING ERROR] 无法创建临时输入文件\n");
        return;
    }
    fprintf(input_file, "%s", json_input);
    fclose(input_file);
    
    // 调用Python脚本（从文件读取，输出到文件）
    char python_cmd[2048];
    snprintf(python_cmd, sizeof(python_cmd), 
             "python3 %s < %s > %s 2>&1", 
             PYTHON_DBSCAN_SCRIPT, temp_input_file, temp_output_file);
    
    int ret = system(python_cmd);
    if (ret != 0) {
        printf("[CLUSTERING ERROR] Python脚本执行失败，返回码: %d\n", ret);
        // 读取错误输出
        FILE *err_file = fopen(temp_output_file, "r");
        if (err_file) {
            char err_buf[1024];
            while (fgets(err_buf, sizeof(err_buf), err_file)) {
                printf("[CLUSTERING ERROR] %s", err_buf);
            }
            fclose(err_file);
        }
        unlink(temp_input_file);
        unlink(temp_output_file);
        return;
    }
    
    // 读取Python脚本的输出
    FILE *output_file = fopen(temp_output_file, "r");
    if (!output_file) {
        printf("[CLUSTERING ERROR] 无法读取Python脚本输出\n");
        unlink(temp_input_file);
        unlink(temp_output_file);
        return;
    }
    
    char json_output[1024 * 1024] = {0};
    size_t output_len = 0;
    char line[4096];
    
    while (fgets(line, sizeof(line), output_file) && output_len < sizeof(json_output) - 1) {
        size_t line_len = strlen(line);
        if (output_len + line_len < sizeof(json_output) - 1) {
            strcat(json_output, line);
            output_len += line_len;
        }
    }
    fclose(output_file);
    
    // 清理临时文件
    unlink(temp_input_file);
    unlink(temp_output_file);
    
    // 解析cluster_labels
    int *cluster_labels = (int*)malloc(count * sizeof(int));
    if (!cluster_labels) {
        printf("[CLUSTERING ERROR] 内存分配失败\n");
        return;
    }
    
    int parsed_count = parse_cluster_labels(json_output, cluster_labels, count);
    if (parsed_count != count) {
        printf("[CLUSTERING ERROR] 解析的标签数量 (%d) 与特征数量 (%d) 不匹配\n", 
               parsed_count, count);
        free(cluster_labels);
        return;
    }
    
    // 将聚类结果赋值给features
    int n_clusters = 0;
    int n_noise = 0;
    int max_cluster_id = -1;
    
    for (int i = 0; i < count; i++) {
        features[i].cluster_id = cluster_labels[i];
        if (cluster_labels[i] == -1) {
            n_noise++;
        } else {
            if (cluster_labels[i] > max_cluster_id) {
                max_cluster_id = cluster_labels[i];
            }
        }
    }
    
    n_clusters = max_cluster_id + 1;
    
    free(cluster_labels);
    
    printf("[CLUSTERING] 聚类完成: 共 %d 个簇, %d 个离群点\n", n_clusters, n_noise);
}

// ========== 步骤4：可疑判定 ==========
static int identify_suspicious_addresses(address_feature_t *features, int count,
                                         address_stat_t *address_stats,
                                         transaction_record_t *records, int record_count,
                                         char suspicious_addresses[][43], char suspicious_hashes[][67],
                                         time_t *suspicious_times, int *suspicious_count) {
    printf("[CLUSTERING] 步骤4: 可疑判定\n");
    
    // 统计每个簇的大小
    int cluster_sizes[1000] = {0};
    int max_cluster_id = -1;
    
    for (int i = 0; i < count; i++) {
        if (features[i].cluster_id >= 0 && features[i].cluster_id < 1000) {
            cluster_sizes[features[i].cluster_id]++;
            if (features[i].cluster_id > max_cluster_id) {
                max_cluster_id = features[i].cluster_id;
            }
        }
    }
    
    // 计算异常簇阈值（小于总地址数的5%）
    int anomaly_cluster_threshold = count * 0.05;
    if (anomaly_cluster_threshold < 1) anomaly_cluster_threshold = 1;
    
    int suspicious_idx = 0;
    
    // 判定可疑地址
    for (int i = 0; i < count; i++) {
        int is_suspicious = 0;
        double anomaly_score = 0.0;
        
        // 规则1: 离群点 → 高度可疑
        if (features[i].cluster_id == -1) {
            is_suspicious = 1;
            anomaly_score = 1.0;  // 最高异常分数
            printf("[CLUSTERING] 离群点地址: %s (异常分数: %.2f)\n", 
                   features[i].address, anomaly_score);
        }
        // 规则2: 异常簇（小簇）→ 可疑
        else if (features[i].cluster_id >= 0 && 
                 cluster_sizes[features[i].cluster_id] < anomaly_cluster_threshold) {
            is_suspicious = 1;
            anomaly_score = 0.7;  // 较高异常分数
            printf("[CLUSTERING] 异常簇地址: %s (簇ID: %d, 簇大小: %d, 异常分数: %.2f)\n",
                   features[i].address, features[i].cluster_id, 
                   cluster_sizes[features[i].cluster_id], anomaly_score);
        }
        
        if (is_suspicious && suspicious_idx < 1000) {
            features[i].is_suspicious = 1;
            features[i].anomaly_score = anomaly_score;
            
            // 找到该地址的最新交易
            time_t latest_time = 0;
            char latest_hash[67] = {0};
            
            for (int j = 0; j < record_count; j++) {
                if (strcmp(records[j].from_address, features[i].address) == 0) {
                    if (records[j].parsed_time > latest_time) {
                        latest_time = records[j].parsed_time;
                        strncpy(latest_hash, records[j].tx_hash, sizeof(latest_hash) - 1);
                        latest_hash[sizeof(latest_hash) - 1] = '\0';
                    }
                }
            }
            
            if (latest_time > 0) {
                strncpy(suspicious_addresses[suspicious_idx], features[i].address, 42);
                suspicious_addresses[suspicious_idx][42] = '\0';
                strncpy(suspicious_hashes[suspicious_idx], latest_hash, 66);
                suspicious_hashes[suspicious_idx][66] = '\0';
                suspicious_times[suspicious_idx] = latest_time;
                suspicious_idx++;
            }
        }
    }
    
    *suspicious_count = suspicious_idx;
    printf("[CLUSTERING] 共识别 %d 个可疑地址\n", suspicious_idx);
    return suspicious_idx;
}

// ========== 主程序 ==========
int main(int argc, char* argv[]) {
    // 初始化随机数种子（基于当前时间）
    srand((unsigned int)time(NULL));
    
    // 设置信号处理
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);
    
    printf("========================================\n");
    printf("可疑交易检测系统启动 (延迟队列模式)\n");
    printf("========================================\n\n");
    
    // ========== 主检测循环 ==========
    const char *csv_file = "/home/zxx/A2L/A2L-master/ecdsa/bin/transaction/transaction_details.csv";
    const char *detect_dir = "/home/zxx/A2L/A2L-master/ecdsa/bin/detect_transaction";
    int detection_interval = 10;  // 10秒检测间隔
    
    printf("[DETECTION] 交易文件路径: %s\n", csv_file);
    printf("[DETECTION] 可疑交易保存路径: %s/suspicious_transactions.csv\n", detect_dir);
    printf("[DETECTION] 检测间隔: %d 秒\n", detection_interval);
    printf("[DETECTION] 检测方法: DBSCAN聚类分析（Python scikit-learn）\n");
    printf("[DETECTION] Python脚本: %s\n", PYTHON_DBSCAN_SCRIPT);
    printf("[DETECTION] DBSCAN参数: eps=%.2f, min_samples=%d\n", DBSCAN_EPS, DBSCAN_MIN_SAMPLES);
    printf("[DETECTION] 特征维度: 8 维\n");
    printf("[DETECTION] 延迟时间: %d 秒\n", DELAY_SECONDS);
    printf("[DETECTION] 工作模式: 聚类分析 + 文件记录 + 延迟队列 + 自动审计\n");
    printf("[DETECTION] 开始监控...\n\n");
    
    int cycle_count = 0;
    while (1) {
        time_t current_time = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&current_time));
        cycle_count++;
        
        // 每 detection_interval 秒执行一次检测，其他时候只处理队列
        if (cycle_count % detection_interval == 1) {
            printf("[DETECTION] 执行检测... (时间: %s, 周期: %d)\n", time_str, cycle_count);
            
            // 1. 聚类分析检测可疑交易并加入延迟队列
            int suspicious_count = detect_high_frequency_transactions(csv_file);
            
            if (suspicious_count > 0) {
                printf("\n========================================\n");
                printf("⚠️  发现 %d 个可疑交易，已加入延迟队列\n", suspicious_count);
                printf("========================================\n\n");
            }
        } else {
            printf("[DETECTION] 处理队列... (时间: %s, 周期: %d)\n", time_str, cycle_count);
        }
        
        // 2. 处理延迟队列中已到时的项目（每次都处理）
        process_ready_items();
        
        // 3. 清理已完成的队列项
        cleanup_expired_items();
        
        // 4. 显示队列状态
        if (g_delay_queue.count > 0) {
            print_queue_status();
        }
        
        // 等待下一次处理（1秒间隔，更频繁地处理队列）
        printf("[DETECTION] 等待 1 秒进行下一次处理...\n\n");
        sleep(1);
    }
    
    return 0;
}
