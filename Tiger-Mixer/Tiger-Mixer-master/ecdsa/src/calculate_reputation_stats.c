/**
 * 计算声誉统计数据的独立程序
 * 从决策记录中计算准确率和一致性，并保存到CSV文件
 */

#include <stdio.h>
#include <stdlib.h>
#include "reputation_tracker.h"

int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("   计算声誉统计数据\n");
    printf("========================================\n\n");
    
    // 初始化声誉跟踪系统
    if (reputation_tracker_init() != 0) {
        fprintf(stderr, "❌ 声誉跟踪系统初始化失败\n");
        return 1;
    }
    
    printf("✅ 声誉跟踪系统初始化成功\n\n");
    
    // 计算并保存统计数据
    if (reputation_tracker_calculate_and_save_stats() != 0) {
        fprintf(stderr, "❌ 计算统计数据失败\n");
        reputation_tracker_cleanup();
        return 1;
    }
    
    printf("\n✅ 统计数据计算完成\n");
    printf("========================================\n");
    
    // 清理资源
    reputation_tracker_cleanup();
    
    return 0;
}

