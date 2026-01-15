/**
 * 声誉跟踪工具函数
 * 用于从JSON中提取用户地址
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reputation_tracker.h"

/**
 * 从pairs_summary_json中提取用户地址
 * 格式: {"id":"0x...","record":"(0,1)(1,1)..."}
 * 返回: 0=成功, -1=失败
 */
int extract_address_from_json(const char* pairs_summary_json, char* address_out, size_t address_size) {
    if (!pairs_summary_json || !address_out || address_size < 42) {
        return -1;
    }
    
    // 查找 "id":" 后面的地址
    const char* id_key = "\"id\":\"";
    const char* id_pos = strstr(pairs_summary_json, id_key);
    if (!id_pos) {
        return -1;
    }
    
    id_pos += strlen(id_key);
    
    // 查找地址结束的引号
    const char* end_quote = strchr(id_pos, '"');
    if (!end_quote) {
        return -1;
    }
    
    size_t addr_len = end_quote - id_pos;
    if (addr_len >= address_size) {
        return -1;
    }
    
    strncpy(address_out, id_pos, addr_len);
    address_out[addr_len] = '\0';
    
    return 0;
}

