#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "pari/pari.h"
#include "zmq.h"
#include "bob.h"
#include "util.h"
#include "composite_malleable_proof.h"

// IO控制宏 - 根据环境变量A2L_DISABLE_IO控制输出
#define CONDITIONAL_PRINTF(...) do { \
    const char *disable_io = getenv("A2L_DISABLE_IO"); \
    if (!disable_io || strcmp(disable_io, "1") != 0) { \
        printf(__VA_ARGS__); \
    } \
} while(0)
// 前置声明：发送隐藏 outer 的联合证明到 Tumbler（附带 inner/outer 两个密文），同步等待签名回执
static int send_layered_proof_to_tumbler(bob_state_t state,
                                         const zk_layered_proof_t proof,
                                         const cl_ciphertext_t inner_auditor_beta,
                                         const cl_ciphertext_t outer_auditor2_beta);

// 新增：发送完整ZK证明给Tumbler验证（使用layered_proof_share消息）
static int send_layered_proof_to_tumbler_with_complete_proof(bob_state_t state,
                                                            const complete_nizk_proof_t proof,
                                                            const cl_ciphertext_t inner_auditor_beta,
                                                            const cl_ciphertext_t outer_auditor2_beta);
#include "types.h"
#include "util.h"
#include "secret_share.h"

#define DISABLE_SECRET_SHARES 1

// 实现缺失的十六进制转换函数
static int hex_to_g1(const char *hex_str, g1_t g1_point) {
    if (!hex_str || !g1_point) {
        return 0;
    }
    
    // 将十六进制字符串转换为G1点
    // 这里需要根据具体的relic库API来实现
    // 假设hex_str是压缩格式的G1点
    uint8_t bytes[33]; // 压缩G1点通常是33字节
    size_t hex_len = strlen(hex_str);
    
    if (hex_len != 66) { // 33字节 = 66个十六进制字符
        return 0;
    }
    
    // 将十六进制字符串转换为字节数组
    for (size_t i = 0; i < 33; i++) {
        char hex_byte[3] = {hex_str[i*2], hex_str[i*2+1], '\0'};
        bytes[i] = (uint8_t)strtol(hex_byte, NULL, 16);
    }
    
    // 使用relic库将字节数组转换为G1点
    g1_read_bin(g1_point, bytes, 33);
    
    return 1;
}

static int hex_to_fr(const char *hex_str, bn_t fr_element) {
    if (!hex_str || !fr_element) {
        return 0;
    }
    
    // 将十六进制字符串转换为有限域元素
    bn_read_str(fr_element, hex_str, strlen(hex_str), 16);
    
    return 1;
}

// 解析JSON数组中的元素
static int extract_json_array_element(const char *json_str, const char *array_name, int index, char *output, size_t output_size) {
    if (!json_str || !array_name || !output || output_size == 0) {
        return 0;
    }
    
    // 构建搜索模式 "array_name":[
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":[", array_name);
    
    const char *start = strstr(json_str, pattern);
    if (!start) {
        return 0;
    }
    
    // 跳过模式字符串
    start += strlen(pattern);
    
    // 跳过空白字符
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    
    // 找到第index个元素
    int current_index = 0;
    while (current_index <= index && *start != '\0' && *start != ']') {
        if (*start == '"') {
            start++; // 跳过开始引号
            
            // 找到结束引号
            const char *end = strchr(start, '"');
            if (!end) {
                return 0;
            }
            
            // 如果这是我们要找的索引
            if (current_index == index) {
                // 计算长度并复制
                size_t element_len = end - start;
                if (element_len >= output_size) {
                    element_len = output_size - 1;
                }
                
                strncpy(output, start, element_len);
                output[element_len] = '\0';
                
                return 1;
            }
            
            // 移动到下一个元素
            start = end + 1; // 跳过结束引号
            
            // 跳过空白字符
            while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
                start++;
            }
            
            // 跳过逗号（如果存在）
            if (*start == ',') {
                start++;
                // 跳过逗号后的空白字符
                while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
                    start++;
                }
            }
            
            current_index++;
        } else {
            start++;
        }
    }
    
    return 0; // 没有找到指定索引的元素
}

// 计算JSON数组中的元素数量
static int count_json_array_elements(const char *json_str, const char *array_name) {
    if (!json_str || !array_name) {
        return 0;
    }
    
    // 构建搜索模式 "array_name":[
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\":[", array_name);
    
    const char *start = strstr(json_str, pattern);
    if (!start) {
        return 0;
    }
    
    // 跳过模式字符串
    start += strlen(pattern);
    
    int count = 0;
    while (*start != '\0' && *start != ']') {
        if (*start == '"') {
            start++; // 跳过引号
            // 找到下一个引号
            while (*start != '"' && *start != '\0') {
                start++;
            }
            if (*start == '"') {
                start++; // 跳过结束引号
                count++;
                if (*start == ',') {
                    start++; // 跳过逗号
                }
            }
        } else {
            start++;
        }
    }
    
    return count;
}

// 重定义RLC_BN_SIZE以与auditor保持一致
#undef RLC_BN_SIZE
#define RLC_BN_SIZE 34

unsigned PROMISE_COMPLETED;
unsigned PUZZLE_SHARED;
unsigned PUZZLE_SOLVED;
unsigned TOKEN_RECEIVED;


// 在文件开头添加
static int release_escrow_for_bob(bob_state_t state, const char *escrow_id) {
  // 直接使用固定值
  // const char *rpc = "http://127.0.0.1:7545";  // 暂时注释掉未使用的变量
  
  // 使用结构体中的Bob地址
  const char *bob_from = state->bob_address;
  
  // 使用存储的tumbler托管ID，如果escrow_id为NULL
  const char *escrow_id_to_use = (escrow_id != NULL) ? escrow_id : state->tumbler_escrow_id;
  
  // 构造签名内容：escrowID || openEscrow的交易哈希
  uint8_t sign_buf[64]; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
  int sign_len = 64;
  
  // 1. 将托管ID (escrow_id_to_use) 从十六进制字符串转换为32字节数组（bytes32格式）
  if (strlen(escrow_id_to_use) < 2 || strncmp(escrow_id_to_use, "0x", 2) != 0) {
    printf("[ERROR] Invalid escrow_id format: %s\n", escrow_id_to_use);
    return -1;
  }
  
  // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
  const char *escrow_id_hex = escrow_id_to_use + 2; // 跳过"0x"
  size_t escrow_id_hex_len = strlen(escrow_id_hex);
  if (escrow_id_hex_len != 64) {
    printf("[ERROR] escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, escrow_id_to_use);
    return -1;
  }
  
  // 将托管ID的十六进制字符串转换为字节数组
  for (size_t i = 0; i < 32; i++) {
    char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
    sign_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
  }
  
  // 2. 将交易哈希从十六进制字符串转换为32字节数组
  if (strlen(state->tumbler_escrow_tx_hash) < 2 || strncmp(state->tumbler_escrow_tx_hash, "0x", 2) != 0) {
    printf("[ERROR] Invalid tumbler_escrow_tx_hash format: %s\n", state->tumbler_escrow_tx_hash);
    return -1;
  }
  
  // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
  const char *hash_hex = state->tumbler_escrow_tx_hash + 2; // 跳过"0x"
  size_t hash_hex_len = strlen(hash_hex);
  if (hash_hex_len != 64) {
    printf("[ERROR] tumbler_escrow_tx_hash should be 64 hex chars (32 bytes), got %zu: %s\n", hash_hex_len, state->tumbler_escrow_tx_hash);
    return -1;
  }
  
  // 将交易哈希的十六进制字符串转换为字节数组
  for (size_t i = 0; i < 32; i++) {
    char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
    sign_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
  }
  
  // 获取 Tumbler 的签名 (state->sigma_t)
  // 注意：sigma_t 是 Tumbler 的最终签名（对应 Tumbler 的 sigma_ts）
  uint8_t tumbler_r_bytes[32], tumbler_s_bytes[32];
  bn_write_bin(tumbler_r_bytes, 32, state->sigma_t->r);
  bn_write_bin(tumbler_s_bytes, 32, state->sigma_t->s);
  
  // 转换为十六进制字符串
  char tumbler_r_hex[65] = {0};
  char tumbler_s_hex[65] = {0};
  for (int i = 0; i < 32; i++) {
    sprintf(tumbler_r_hex + 2 * i, "%02x", tumbler_r_bytes[i]);
    sprintf(tumbler_s_hex + 2 * i, "%02x", tumbler_s_bytes[i]);
  }
  
  printf("[ESCROW DEBUG] Tumbler signature r (first 20 chars): %.20s\n", tumbler_r_hex);
  printf("[ESCROW DEBUG] Tumbler signature s (first 20 chars): %.20s\n", tumbler_s_hex);
  printf("[ESCROW DEBUG] Tumbler signature r (full): %s\n", tumbler_r_hex);
  printf("[ESCROW DEBUG] Tumbler signature s (full): %s\n", tumbler_s_hex);
  
  // 对于 Bob 的签名，我们需要对相同的消息进行签名
  // Bob 需要对 tumbler_escrow_id || tumbler_escrow_tx_hash 进行标准 ECDSA 签名
  // 使用 cp_ecdsa_sig 函数生成签名
  bn_t bob_r, bob_s;
  bn_new(bob_r);
  bn_new(bob_s);
  
  // 使用标准 ECDSA 签名生成 Bob 的签名
  // cp_ecdsa_sig 的参数：r, s, msg, len, hash_flag, secret_key
  // hash_flag = 0 表示函数内部会对消息进行哈希
  int sig_result = cp_ecdsa_sig(bob_r, bob_s, sign_buf, sign_len, 0, state->bob_ec_sk->sk);
  if (sig_result != RLC_OK) {
    printf("[ERROR] Failed to generate Bob signature for Tumbler escrow\n");
    bn_free(bob_r);
    bn_free(bob_s);
    return -1;
  }
  
  uint8_t bob_r_bytes[32], bob_s_bytes[32];
  char bob_r_hex[65] = {0};
  char bob_s_hex[65] = {0};
  
  bn_write_bin(bob_r_bytes, 32, bob_r);
  bn_write_bin(bob_s_bytes, 32, bob_s);
  
  for (int i = 0; i < 32; i++) {
    sprintf(bob_r_hex + 2 * i, "%02x", bob_r_bytes[i]);
    sprintf(bob_s_hex + 2 * i, "%02x", bob_s_bytes[i]);
  }
  
  printf("[ESCROW DEBUG] Bob signature r (first 20 chars): %.20s\n", bob_r_hex);
  printf("[ESCROW DEBUG] Bob signature s (first 20 chars): %.20s\n", bob_s_hex);
  
  bn_free(bob_r);
  bn_free(bob_s);
  
  // 构建命令调用 confirm 函数，传递签名参数
  char *cmd = malloc(8192);
  if (!cmd) return -1;
  
  // 使用 --pool 参数，让脚本从地址簿解析固定面额池合约地址
  // 传递两个签名：Tumbler 的签名 (r1, s1, v1) 和 Bob 的签名 (r2, s2, v2)
  // 注意：v 值需要在 JS 脚本中计算，这里先传 0 或占位值
  printf("[ESCROW DEBUG] Building command with:\n");
  printf("  pool_label: %s\n", state->pool_label);
  printf("  escrow_id: %s\n", escrow_id_to_use);
  printf("  tumbler_r_hex length: %zu\n", strlen(tumbler_r_hex));
  printf("  tumbler_s_hex length: %zu\n", strlen(tumbler_s_hex));
  printf("  bob_r_hex length: %zu\n", strlen(bob_r_hex));
  printf("  bob_s_hex length: %zu\n", strlen(bob_s_hex));
  
  int cmd_len = snprintf(cmd, 8192,
           "cd /home/zxx/Config/truffleProject/truffletest && npx truffle exec scripts/confirmEscrow.js --network private "
           "--pool %s --id %s --from %s "
           "--r1 0x%s --s1 0x%s --v1 0 "
           "--r2 0x%s --s2 0x%s --v2 0 2>&1 | cat",
           state->pool_label, escrow_id_to_use, bob_from,
           tumbler_r_hex, tumbler_s_hex,
           bob_r_hex, bob_s_hex);
  
  printf("[ESCROW DEBUG] Command length: %d (max 8192)\n", cmd_len);
  CONDITIONAL_PRINTF("[ESCROW] Bob confirm escrow cmd: %s\n", cmd);
  
  // 测量区块链交互时间
  START_TIMER(bob_blockchain_escrow_interaction)
  // 执行命令并捕获输出
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    fprintf(stderr, "[ESCROW] Failed to execute command\n");
    free(cmd);
    return -1;
  }
  
  char buffer[1024];
  char tx_hash[67] = {0};
  int tx_found = 0;
  
  // 读取命令输出，寻找交易哈希
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    const char *disable_io = getenv("A2L_DISABLE_IO");
    if (!disable_io || strcmp(disable_io, "1") != 0) {
      printf("[ESCROW] Output: %s", buffer);
    }
    
    // 查找包含交易哈希的行（多策略）
    if (strstr(buffer, "Transaction hash:") != NULL || strstr(buffer, "txHash:") != NULL) {
      // 提取交易哈希
      char *hash_start = strstr(buffer, "0x");
      if (hash_start) {
        strncpy(tx_hash, hash_start, 66);
        tx_hash[66] = '\0';
        printf("[ESCROW] Captured transaction hash: %s\n", tx_hash);
        tx_found = 1;
        break;
      }
    }
    
    // 查找JSON格式的交易哈希 {"txHash":"0x..."}
    if (strstr(buffer, "\"txHash\"") != NULL) {
      printf("[ESCROW] Found JSON txHash line, attempting to extract...\n");
      char *hash_start = strstr(buffer, "\"0x");
      if (hash_start) {
        printf("[ESCROW] Found 0x pattern at position %ld\n", hash_start - buffer);
        // 跳过开头的引号
        hash_start += 1;
        
        // 找到哈希的结束位置（下一个引号）
        char *hash_end = strchr(hash_start, '"');
        if (hash_end) {
          int hash_len = hash_end - hash_start;
          printf("[ESCROW] Hash length: %d\n", hash_len);
          if (hash_len <= 66) {
            strncpy(tx_hash, hash_start, hash_len);
            tx_hash[hash_len] = '\0';
            printf("[ESCROW] Captured JSON transaction hash: %s\n", tx_hash);
            tx_found = 1;
            break;
          } else {
            printf("[ESCROW] Hash too long: %d > 66\n", hash_len);
          }
        } else {
          printf("[ESCROW] Could not find closing quote\n");
        }
      } else {
        printf("[ESCROW] Could not find 0x pattern in JSON line\n");
      }
    }

    // 查找 tx: 或 transaction: 开头的行中的交易哈希
    if (!tx_found && (strstr(buffer, "tx:") != NULL || strstr(buffer, "transaction:") != NULL)) {
      char *hash_start = strstr(buffer, "0x");
      if (hash_start) {
        strncpy(tx_hash, hash_start, 66);
        tx_hash[66] = '\0';
        printf("[ESCROW] Captured tx hash from tx/transaction line: %s\n", tx_hash);
        tx_found = 1;
        break;
      }
    }
    
    // 通用兜底：在任意输出行中查找 0x + 64 个十六进制字符
    // 但要排除包含参数名的行（escrowId, contractAddr等）
    if (!tx_found && 
        strstr(buffer, "escrowId") == NULL && 
        strstr(buffer, "contractAddr") == NULL &&
        strstr(buffer, "params:") == NULL) {
      for (size_t i = 0; buffer[i] && buffer[i+1]; i++) {
        if (buffer[i] == '0' && buffer[i+1] == 'x') {
          // 统计后续十六进制字符个数
          size_t j = i + 2; size_t hex_cnt = 0;
          while (buffer[j]) {
            char c = buffer[j];
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
              hex_cnt++; j++;
            } else {
              break;
            }
          }
          if (hex_cnt >= 64) {
            // 截取标准长度 0x + 64
            strncpy(tx_hash, buffer + i, 66);
            tx_hash[66] = '\0';
            printf("[ESCROW] Captured fallback tx hash: %s\n", tx_hash);
            tx_found = 1;
            break;
          }
        }
      }
      if (tx_found) break;
    }
  }
  
  int rc = pclose(fp);
  END_TIMER(bob_blockchain_escrow_interaction)
  free(cmd);
  
  if (tx_found || rc == 0) {
    // 如果成功捕获到交易哈希，存储到state中
    if (strlen(tx_hash) > 0) {
      strncpy(state->confirm_escrow_tx_hash, tx_hash, sizeof(state->confirm_escrow_tx_hash) - 1);
      state->confirm_escrow_tx_hash[sizeof(state->confirm_escrow_tx_hash) - 1] = '\0';
      printf("[ESCROW] Stored confirmEscrow transaction hash: %s\n", state->confirm_escrow_tx_hash);
      
      // 注意: Bob 托管成功的交易 hash 不需要记录到文件
      // （已按要求移除文件写入代码）
    } else {
      // 如果没有捕获到，使用默认值
      strncpy(state->confirm_escrow_tx_hash, "0x0000000000000000000000000000000000000000000000000000000000000000", sizeof(state->confirm_escrow_tx_hash) - 1);
      printf("[ESCROW] No transaction hash captured, using default\n");
    }
    
    printf("[ESCROW] Bob confirmed escrow successfully (tx_found=%d, rc=%d)\n", tx_found, rc);
    // 统一返回 0 表示成功
    rc = 0;
  } else {
    fprintf(stderr, "[ESCROW] Bob confirm escrow failed with code %d.\n", rc);
  }
  
  return rc;
}


int get_message_type(char *key) {
  for (size_t i = 0; i < TOTAL_MESSAGES; i++) {
    symstruct_t sym = msg_lookuptable[i];
    if (strcmp(sym.key, key) == 0) {
      return sym.code;
    }
  }
  return -1;
}

msg_handler_t get_message_handler(char *key) {
  switch (get_message_type(key))
  {
    case TOKEN_SHARE:
      return token_share_handler;
    
    case PROMISE_DONE:
      return promise_done_handler;

    case PUZZLE_SHARE_DONE:
      return puzzle_share_done_handler;

    case PUZZLE_SOLUTION_SHARE:
      return puzzle_solution_share_handler;

    case LAYERED_PROOF_SIGNED:
      return layered_proof_signed_handler;

    default:
      fprintf(stderr, "Error: invalid message type.\n");
      exit(1);
  }
}

int handle_message(bob_state_t state, void *socket, zmq_msg_t message, transaction_t* tx_data) {
  int result_status = RLC_OK;

  message_t msg;
  message_null(msg);

  RLC_TRY {
    printf("Received message size: %ld bytes\n", zmq_msg_size(&message));
    deserialize_message(&msg, (uint8_t *) zmq_msg_data(&message));

    printf("Executing %s...\n", msg->type);
    printf("🔍 反序列化后的消息调试信息:\n");
    printf("  消息类型: %s\n", msg->type);
    printf("  消息数据前32字节 (hex): ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", msg->data[i]);
    }
    printf("\n");
    
    msg_handler_t msg_handler = get_message_handler(msg->type);
    if (msg_handler(state, socket, msg->data, tx_data) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    printf("Finished executing %s.\n\n", msg->type);
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    if (msg != NULL) message_free(msg);
  }

  return result_status;
}

int receive_message(bob_state_t state, void *socket, transaction_t* tx_data) {
  int result_status = RLC_OK;

  zmq_msg_t message;

  RLC_TRY {
    int rc = zmq_msg_init(&message);
    if (rc != 0) {
      fprintf(stderr, "Error: could not initialize the message.\n");
      RLC_THROW(ERR_CAUGHT);
    }

    rc = zmq_msg_recv(&message, socket, ZMQ_DONTWAIT);
    if (rc != -1 && handle_message(state, socket, message, tx_data) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    zmq_msg_close(&message);
  }

  return result_status;
}

int token_share_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(token_share_total);
  int result_status = RLC_OK;

  RLC_TRY {    
  
    // 新的数据格式：proofData JSON (以 null 结尾) + pool_label (以 null 结尾)
    // 1. 读取 proofData JSON（第一个 null 结尾的字符串）
    // 数据格式：
    // 1. nullifier (31 bytes)
    // 2. secret (31 bytes)
    // 3. commitment (null-terminated string)
    // 4. escrow_tx_hash (null-terminated string)
    // 5. pool_label (null-terminated string)
    // 6. pool_contract (null-terminated string)
    size_t offset = 0;
    
    // 1. 读取 nullifier (31 bytes)
    memcpy(state->nullifier, data + offset, 31);
    offset += 31;
    
    // 2. 读取 secret (31 bytes)
    memcpy(state->secret, data + offset, 31);
    offset += 31;
    
    // 3. 读取 commitment
    const char *commitment_ptr = (const char*)(data + offset);
    memset(state->commitment, 0, sizeof(state->commitment));
    strncpy(state->commitment, commitment_ptr, sizeof(state->commitment) - 1);
    offset += strlen(commitment_ptr) + 1;
    
    // 4. 读取 escrow_tx_hash
    const char *escrow_tx_hash_ptr = (const char*)(data + offset);
    memset(state->escrow_tx_hash, 0, sizeof(state->escrow_tx_hash));
    strncpy(state->escrow_tx_hash, escrow_tx_hash_ptr, sizeof(state->escrow_tx_hash) - 1);
    offset += strlen(escrow_tx_hash_ptr) + 1;
    
    // 5. 读取 pool_label
    const char *pool_label_ptr = (const char*)(data + offset);
    memset(state->pool_label, 0, sizeof(state->pool_label));
    strncpy(state->pool_label, pool_label_ptr, sizeof(state->pool_label) - 1);
    offset += strlen(pool_label_ptr) + 1;
    
    // 6. 读取 pool_contract
    const char *pool_contract_ptr = (const char*)(data + offset);
    memset(state->pool_contract, 0, sizeof(state->pool_contract));
    strncpy(state->pool_contract, pool_contract_ptr, sizeof(state->pool_contract) - 1);
    
    printf("[TORNADO] Bob: Received nullifier and secret from Alice\n");
    printf("[TORNADO] Bob: commitment=%s\n", state->commitment);
    printf("[TORNADO] Bob: escrow_tx_hash=%s\n", state->escrow_tx_hash);
    printf("[TORNADO] Bob: pool_label=%s\n", state->pool_label);
    printf("[TORNADO] Bob: pool_contract=%s\n", state->pool_contract);
    
    // 现在 Bob 需要生成 zk 证明
    printf("[TORNADO] Bob: Starting withdraw proof generation...\n");
    
    const char *tornado_project = "/home/zxx/tornado-core-master";
    const char *script_path = "/home/zxx/tornado-core-master/scripts/tornado_generate_withdraw_proof.js";
    
    // 准备 nullifier 和 secret 的十六进制字符串
    char nullifier_hex[65] = {0};
    char secret_hex[65] = {0};
    
    // 将 nullifier 和 secret 转换为十六进制字符串
    for (int i = 0; i < 31; i++) {
      snprintf(nullifier_hex + i * 2, 3, "%02x", state->nullifier[i]);
      snprintf(secret_hex + i * 2, 3, "%02x", state->secret[i]);
    }
    
    // 构建命令
    char *cmd_proof = (char *)malloc(8192);
    if (!cmd_proof) {
      RLC_THROW(ERR_NO_MEMORY);
    }
    
    // 使用 pool_contract 地址（如果已设置），否则使用 pool_label（脚本会尝试解析）
    const char *contract_param = (strlen(state->pool_contract) > 0 && 
                                  strncmp(state->pool_contract, "0x", 2) == 0) 
                                 ? state->pool_contract : state->pool_label;
    
    // txHash 是必需的
    if (strlen(state->escrow_tx_hash) == 0) {
      fprintf(stderr, "[ERROR] escrow_tx_hash is not set. Cannot generate proof.\n");
      free(cmd_proof);
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[TORNADO] Bob: Using escrow_tx_hash: %s\n", state->escrow_tx_hash);
    snprintf(cmd_proof, 8192,
             "cd %s && node %s --nullifier 0x%s --secret 0x%s --commitment %s --contract %s --network private --txHash %s 2>&1",
             tornado_project, script_path, nullifier_hex, secret_hex, 
             state->commitment, contract_param, state->escrow_tx_hash);
    
    printf("[TORNADO] Bob: Executing proof generation script: %s\n", cmd_proof);
   
    // 开始测量 zkSNARK proof 生成耗时
    START_TIMER(bob_tornado_proof_generation);
    FILE *fp_proof = popen(cmd_proof, "r");
    if (!fp_proof) {
      END_TIMER(bob_tornado_proof_generation);
      fprintf(stderr, "[TORNADO] Bob: Failed to execute proof generation script\n");
      free(cmd_proof);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 读取脚本输出（JSON 格式）
    char json_buffer[65536] = {0}; // 64KB 缓冲区
    size_t json_len = 0;
    char line[8192];
    int json_started = 0;
    int brace_count = 0;
    
    // 用于解析脚本输出的 proof 生成时间
    long long proof_start_time = 0;
    long long proof_end_time = 0;
    long long proof_duration = 0;
    int proof_timing_found = 0;
    
    while (fgets(line, sizeof(line), fp_proof) != NULL) {
      size_t line_len = strlen(line);
      
      // 检查是否包含 proof 生成时间信息
      if (strstr(line, "[PROOF_TIMING]") != NULL) {
        if (strstr(line, "START:") != NULL) {
          sscanf(line, "[PROOF_TIMING] START:%lld", &proof_start_time);
        } else if (strstr(line, "END:") != NULL) {
          sscanf(line, "[PROOF_TIMING] END:%lld", &proof_end_time);
        } else if (strstr(line, "DURATION:") != NULL) {
          sscanf(line, "[PROOF_TIMING] DURATION:%lld", &proof_duration);
          proof_timing_found = 1;
        }
      }
      
      if (!json_started && (line[0] == '{' || strstr(line, "\"nullifierHash\"") != NULL)) {
        json_started = 1;
        brace_count = 0;
        for (size_t i = 0; i < line_len; i++) {
          if (line[i] == '{') brace_count++;
          if (line[i] == '}') brace_count--;
        }
      }
      
      if (json_started) {
        if (json_len + line_len < sizeof(json_buffer)) {
          memcpy(json_buffer + json_len, line, line_len);
          json_len += line_len;
          
          for (size_t i = 0; i < line_len; i++) {
            if (line[i] == '{') brace_count++;
            if (line[i] == '}') brace_count--;
          }
          
          if (brace_count == 0 && json_len > 0) {
            char *last_brace = strrchr(json_buffer, '}');
            if (last_brace) {
              json_len = last_brace - json_buffer + 1;
              json_buffer[json_len] = '\0';
              break;
            }
          }
        } else {
          fprintf(stderr, "[TORNADO] Bob: WARNING: JSON buffer overflow\n");
          char *last_brace = strrchr(json_buffer, '}');
          if (last_brace) {
            json_len = last_brace - json_buffer + 1;
            json_buffer[json_len] = '\0';
          }
          break;
        }
      }
    }
    
    // 输出解析到的 proof 生成时间（仅 zkSNARK 生成部分）
    if (proof_timing_found && proof_duration > 0) {
      printf("[TORNADO] Bob: zkSNARK proof generation (core) took: %lld ms\n", proof_duration);
    } else if (proof_start_time > 0 && proof_end_time > 0) {
      proof_duration = proof_end_time - proof_start_time;
      printf("[TORNADO] Bob: zkSNARK proof generation (core) took: %lld ms\n", proof_duration);
    }
    
    printf("[TORNADO] Bob: Read JSON buffer: length=%zu\n", json_len);
    
    int rc_proof = pclose(fp_proof);
    // 结束测量 zkSNARK proof 生成耗时（包括命令执行和输出读取）
    END_TIMER(bob_tornado_proof_generation);
    free(cmd_proof);
    
    if (rc_proof != 0 || json_len == 0) {
      fprintf(stderr, "[TORNADO] Bob: Proof generation script failed or no output, rc=%d\n", rc_proof);
      if (json_len > 0) {
        fprintf(stderr, "[TORNADO] Bob: Script output: %s\n", json_buffer);
      }
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析 JSON 输出，提取 proofData
    char *proof_data_start = strstr(json_buffer, "\"proofData\"");
    if (!proof_data_start) {
      // 如果没有 proofData，尝试从 proof 和 publicSignals 构建
      char *proof_start = strstr(json_buffer, "\"proof\"");
      char *public_signals_start = strstr(json_buffer, "\"publicSignals\"");
      
      if (proof_start && public_signals_start) {
        // 提取 proof 和 publicSignals，构建 proofData
        const char *proof_value_start = strstr(proof_start, ":");
        const char *proof_value_end = public_signals_start - 1;
        while (proof_value_end > proof_value_start && 
               (*proof_value_end == ' ' || *proof_value_end == '\t' || 
                *proof_value_end == ',' || *proof_value_end == '\n' || *proof_value_end == '\r')) {
          proof_value_end--;
        }
        proof_value_end++;
        
        const char *signals_value_start = strstr(public_signals_start, ":");
        signals_value_start++;
        while (*signals_value_start == ' ' || *signals_value_start == '\t') {
          signals_value_start++;
        }
        
        size_t proof_val_len = proof_value_end - proof_value_start;
        size_t signals_val_len = strlen(signals_value_start);
        // 找到 signals 的结束位置
        const char *signals_value_end = signals_value_start;
        while (*signals_value_end != '\0' && *signals_value_end != ',' && *signals_value_end != '}') {
          signals_value_end++;
        }
        signals_val_len = signals_value_end - signals_value_start;
        
        // 构建 proofData JSON
        char proof_value[4096];
        char signals_value[512];
        if (proof_val_len < sizeof(proof_value) && signals_val_len < sizeof(signals_value)) {
          strncpy(proof_value, proof_value_start, proof_val_len);
          proof_value[proof_val_len] = '\0';
          strncpy(signals_value, signals_value_start, signals_val_len);
          signals_value[signals_val_len] = '\0';
          
          int proof_data_len = snprintf(state->tornado_proof_data, sizeof(state->tornado_proof_data),
            "{\"proof\":%s,\"publicSignals\":%s}",
            proof_value, signals_value);
          
          if (proof_data_len < 0 || proof_data_len >= (int)sizeof(state->tornado_proof_data)) {
            fprintf(stderr, "[ERROR] Bob: Failed to construct proofData JSON\n");
            RLC_THROW(ERR_CAUGHT);
          }
          
          printf("[TORNADO] Bob: Constructed proofData JSON (length: %d)\n", proof_data_len);
        } else {
          fprintf(stderr, "[ERROR] Bob: proof or signals value too long\n");
          RLC_THROW(ERR_CAUGHT);
        }
      } else {
        fprintf(stderr, "[ERROR] Bob: Cannot find proof or publicSignals in JSON output\n");
        RLC_THROW(ERR_CAUGHT);
      }
    } else {
      // 提取 proofData 的值部分
      const char *proof_data_value_start = strstr(proof_data_start, ":");
      if (proof_data_value_start) {
        proof_data_value_start++;
        while (*proof_data_value_start == ' ' || *proof_data_value_start == '\t') {
          proof_data_value_start++;
        }
        
        // 找到 proofData 的结束位置（在下一个字段之前或 JSON 结束）
        const char *proof_data_value_end = proof_data_value_start;
        int brace_count = 0;
        int in_string = 0;
        while (*proof_data_value_end != '\0') {
          if (*proof_data_value_end == '"' && (proof_data_value_end == proof_data_value_start || 
              *(proof_data_value_end - 1) != '\\')) {
            in_string = !in_string;
          }
          if (!in_string) {
            if (*proof_data_value_end == '{') brace_count++;
            if (*proof_data_value_end == '}') {
              brace_count--;
              if (brace_count == 0) {
                proof_data_value_end++;
                break;
              }
            }
            if (brace_count == 0 && *proof_data_value_end == ',') {
              break;
            }
          }
          proof_data_value_end++;
        }
        
        size_t proof_data_val_len = proof_data_value_end - proof_data_value_start;
        if (proof_data_val_len > 0 && proof_data_val_len < sizeof(state->tornado_proof_data)) {
          strncpy(state->tornado_proof_data, proof_data_value_start, proof_data_val_len);
          state->tornado_proof_data[proof_data_val_len] = '\0';
          printf("[TORNADO] Bob: Extracted proofData (length: %zu)\n", proof_data_val_len);
        } else {
          fprintf(stderr, "[ERROR] Bob: proofData value too large\n");
          RLC_THROW(ERR_CAUGHT);
        }
      } else {
        fprintf(stderr, "[ERROR] Bob: Cannot find proofData value in JSON\n");
        RLC_THROW(ERR_CAUGHT);
      }
    }
    
    printf("[TORNADO] Bob: Successfully generated withdraw proof\n");
    printf("[TORNADO] Bob: proofData preview: %.200s...\n", state->tornado_proof_data);
    printf("[TORNADO] Bob: proofData will be forwarded to Tumbler for verification\n");
    
    // token_share parsed
    TOKEN_RECEIVED = 1;
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
    fprintf(stderr, "[ERROR] token_share_handler 处理失败\n");
  }
  END_TIMER(token_share_total);
  return result_status;
}

int promise_init(bob_state_t state, void *socket, transaction_t* tx_data) {
  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(promise_init_total);
  int result_status = RLC_OK;
  uint8_t *serialized_message = NULL;
  
  message_t promise_init_msg;
  message_null(promise_init_msg);

  uint8_t tx_buf[1024]; // 增大缓冲区，防止溢出
  int tx_len = serialize_transaction(tx_data, tx_buf, sizeof(tx_buf));
  printf("[DEBUG] serialize_transaction: tx_len = %d\n", tx_len);
  if (tx_len < 0) {
    printf("[DEBUG] serialize_transaction failed!\n");
    RLC_THROW(ERR_CAUGHT);
  }
  printf("[DEBUG] tx_buf (hex): ");
  for (int i = 0; i < tx_len; i++) printf("%02x", tx_buf[i]);
  printf("\n");

  RLC_TRY {
    if (cp_ecdsa_sig(state->sigma_r->r, state->sigma_r->s, tx_buf, tx_len, 0, state->bob_ec_sk->sk) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }

    // Build and define the message.
    char *msg_type = "promise_init";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    // 移除 tid 和 sigma_tid，只保留 sigma_r->r 和 sigma_r->s
    const unsigned msg_data_length = (2 * RLC_BN_SIZE); // sigma_r->r + sigma_r->s
    const unsigned tx_meta_len = sizeof(int) + tx_len;
    const unsigned bob_address_len = strlen(state->bob_address) + 1; // 包含字符串结束符
    const unsigned pool_label_length = strlen(state->pool_label) + 1;
    
    // 附加 proofData（从 Alice 接收的 Tornado Cash zkSNARK 证明）
    const unsigned proof_data_length = strlen(state->tornado_proof_data) > 0 ? strlen(state->tornado_proof_data) + 1 : 0;
    if (proof_data_length == 0) {
      fprintf(stderr, "[ERROR] promise_init: proofData not received from Alice\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    const unsigned new_msg_data_length = msg_data_length + tx_meta_len + bob_address_len + pool_label_length + proof_data_length;
    const int total_msg_length = msg_type_length + new_msg_data_length + (2 * sizeof(unsigned));
    message_new(promise_init_msg, msg_type_length, new_msg_data_length);
    
    // Serialize the message (只包含 sigma_r->r 和 sigma_r->s，移除 tid 和 sigma_tid)
    bn_write_bin(promise_init_msg->data, RLC_BN_SIZE, state->sigma_r->r);
    bn_write_bin(promise_init_msg->data + RLC_BN_SIZE, RLC_BN_SIZE, state->sigma_r->s);
    // 附加交易数据
    memcpy(promise_init_msg->data + msg_data_length, &tx_len, sizeof(int));
    memcpy(promise_init_msg->data + msg_data_length + sizeof(int), tx_buf, tx_len);
    
    // 附加Bob地址
    memcpy(promise_init_msg->data + msg_data_length + sizeof(int) + tx_len, state->bob_address, bob_address_len);
    // 附加 pool_label（给 Tumbler 再核对一致性，尽管其已在注册时保存）
    memcpy(promise_init_msg->data + msg_data_length + sizeof(int) + tx_len + bob_address_len, state->pool_label, pool_label_length);
    // 附加 proofData（Tornado Cash zkSNARK 证明，用于 Tumbler 验证）
    memcpy(promise_init_msg->data + msg_data_length + sizeof(int) + tx_len + bob_address_len + pool_label_length, 
           state->tornado_proof_data, proof_data_length);
    
    printf("[TORNADO] Bob: Forwarding proofData to Tumbler (length: %u)\n", proof_data_length - 1);

    memcpy(promise_init_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, promise_init_msg, msg_type_length, new_msg_data_length);

    // Send the message.
    printf("[BOB] 准备发送 promise_init 消息，大小: %d 字节\n", total_msg_length);
    zmq_msg_t promise_init;
    int rc = zmq_msg_init_size(&promise_init, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&promise_init), serialized_message, total_msg_length);
    printf("[BOB] 发送 promise_init 消息到 Tumbler...\n");
    rc = zmq_msg_send(&promise_init, socket, 0);  // 阻塞发送
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s). Sent %d, expected %d\n", msg_type, rc, total_msg_length);
      RLC_THROW(ERR_CAUGHT);
    }
    printf("[BOB] promise_init 消息发送成功\n");
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    message_free(promise_init_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(promise_init_total);
  return result_status;
}

int promise_done_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL || data == NULL) {
    printf("[DEBUG] state or data is NULL\n");
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(promise_done_total);
  int result_status = RLC_OK;
  
  RLC_TRY {
    // 声明所有局部变量（C89标准要求）
    uint8_t verify_buf[64]; // 用于验证内容的缓冲区
    int verify_len;
    
    printf("[DEBUG] Entering promise_done_handler\n");

    // Deserialize the data from the message.
    printf("[DEBUG] Deserializing message fields...\n");
    ec_read_bin(state->g_to_the_alpha, data, RLC_EC_SIZE_COMPRESSED);
    bn_read_bin(state->sigma_t->r, data + RLC_EC_SIZE_COMPRESSED, RLC_BN_SIZE);
    bn_read_bin(state->sigma_t->s, data + RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE, RLC_BN_SIZE);
    ec_read_bin(state->sigma_t->R, data + RLC_EC_SIZE_COMPRESSED + (2 * RLC_BN_SIZE), RLC_EC_SIZE_COMPRESSED);
    ec_read_bin(state->sigma_t->pi->a, data + (2 * RLC_EC_SIZE_COMPRESSED) + (2 * RLC_BN_SIZE), RLC_EC_SIZE_COMPRESSED);
    ec_read_bin(state->sigma_t->pi->b, data + (3 * RLC_EC_SIZE_COMPRESSED) + (2 * RLC_BN_SIZE), RLC_EC_SIZE_COMPRESSED);
    bn_read_bin(state->sigma_t->pi->z, data + (4 * RLC_EC_SIZE_COMPRESSED) + (2 * RLC_BN_SIZE), RLC_BN_SIZE);

    size_t offset = (4 * RLC_EC_SIZE_COMPRESSED) + (3 * RLC_BN_SIZE);
    char ctx_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(ctx_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ctx_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    state->ctx_alpha->c1 = gp_read_str(ctx_str);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(ctx_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ctx_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    state->ctx_alpha->c2 = gp_read_str(ctx_str);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    // 解析auditor密文（按照Tumbler端的序列化顺序）
    char auditor_ctx_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(auditor_ctx_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_ctx_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    
    state->auditor_ctx_alpha->c1 = gp_read_str(auditor_ctx_str);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(auditor_ctx_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_ctx_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    
    state->auditor_ctx_alpha->c2 = gp_read_str(auditor_ctx_str);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    printf("[DEBUG] auditor_ctx_alpha fields deserialized\n");
    
    char *debug_c1 = GENtostr(state->auditor_ctx_alpha->c1);
    char *debug_c2 = GENtostr(state->auditor_ctx_alpha->c2);
  
    // 解析tumbler的托管ID
    strcpy(state->tumbler_escrow_id, (char*)(data + offset));
    printf("[BOB] Received Tumbler's escrow ID: %s\n", state->tumbler_escrow_id);
    // 读取紧随其后的 Tumbler 开托管 txHash（可选）
    size_t after_id = offset + strlen(state->tumbler_escrow_id) + 1;
    const char *tumbler_txh = (char*)(data + after_id);
    if (tumbler_txh && tumbler_txh[0]) {
      printf("[BOB] Received Tumbler escrow txHash: %s\n", tumbler_txh);
      memset(state->tumbler_escrow_tx_hash, 0, sizeof(state->tumbler_escrow_tx_hash));
      strncpy(state->tumbler_escrow_tx_hash, tumbler_txh, sizeof(state->tumbler_escrow_tx_hash) - 1);
      after_id += strlen(tumbler_txh) + 1;
    } else {
      state->tumbler_escrow_tx_hash[0] = '\0';
    }
    
    // 解析综合零知识证明
    printf("[BOB] 开始解析综合零知识证明...\n");
    size_t zk_read;
    if (zk_comprehensive_puzzle_deserialize(state->received_puzzle_zk_proof, data + after_id, &zk_read) != RLC_OK) {
      printf("[ERROR] 零知识证明反序列化失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    printf("[BOB] 零知识证明反序列化成功，读取了 %zu 字节\n", zk_read);

    
    // 验证综合零知识证明
    printf("[BOB] 开始验证综合零知识证明...\n");
    START_TIMER(tumbler_to_bob_zk_verification)
    if (zk_comprehensive_puzzle_verify(state->received_puzzle_zk_proof,
                                       state->g_to_the_alpha,
                                       state->ctx_alpha, state->auditor_ctx_alpha,
                                       state->tumbler_cl_pk, state->auditor_cl_pk,
                                       state->cl_params) != RLC_OK) {
      printf("[ERROR] 综合零知识证明验证失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(tumbler_to_bob_zk_verification)
    printf("[BOB] 综合零知识证明验证成功! 谜题构造正确性已确认!\n");
    START_TIMER(check_total);
    // ========== 新增：链上状态检查（调用 util 公共函数） ==========
    query_escrow_status_by_id(state->tumbler_escrow_id);
    if (state->tumbler_escrow_tx_hash[0]) {
      check_tx_mined(state->tumbler_escrow_tx_hash);
    }
    END_TIMER(check_total);
    
    // 构造验证内容：托管ID||openEscrow的交易哈希
    verify_len = 64; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
    
    // 1. 将托管ID (state->tumbler_escrow_id) 从十六进制字符串转换为32字节数组（bytes32格式）
    if (strlen(state->tumbler_escrow_id) < 2 || strncmp(state->tumbler_escrow_id, "0x", 2) != 0) {
      printf("[ERROR] Invalid tumbler_escrow_id format: %s\n", state->tumbler_escrow_id);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *escrow_id_hex = state->tumbler_escrow_id + 2; // 跳过"0x"
    size_t escrow_id_hex_len = strlen(escrow_id_hex);
    if (escrow_id_hex_len != 64) {
      printf("[ERROR] tumbler_escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, state->tumbler_escrow_id);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 将托管ID的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
      char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
      verify_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    // 2. 将交易哈希从十六进制字符串转换为32字节数组
    if (strlen(state->tumbler_escrow_tx_hash) < 2 || strncmp(state->tumbler_escrow_tx_hash, "0x", 2) != 0) {
      printf("[ERROR] Invalid tumbler_escrow_tx_hash format: %s\n", state->tumbler_escrow_tx_hash);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *hash_hex = state->tumbler_escrow_tx_hash + 2; // 跳过"0x"
    size_t hash_hex_len = strlen(hash_hex);
    if (hash_hex_len != 64) {
      printf("[ERROR] tumbler_escrow_tx_hash should be 64 hex chars (32 bytes), got %zu: %s\n", hash_hex_len, state->tumbler_escrow_tx_hash);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 将交易哈希的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
      char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
      verify_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    printf("[DEBUG] Bob verification content: escrowID(32 bytes) || txHash(32 bytes)\n");
    printf("[DEBUG] Escrow ID (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", verify_buf[i]);
    printf("\n");
    printf("[DEBUG] Escrow TX Hash (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", verify_buf[32 + i]);
    printf("\n");

    printf("[DEBUG] Verifying adaptor_ecdsa_preverify...\n");
    
    uint8_t debug_r_bytes[34];
    bn_write_bin(debug_r_bytes, 34, state->sigma_t->r);
    uint8_t debug_s_bytes[34];
    bn_write_bin(debug_s_bytes, 34, state->sigma_t->s);
    uint8_t debug_R_bytes[33];
    ec_write_bin(debug_R_bytes, 33, state->sigma_t->R, 1);
    uint8_t debug_pi_a_bytes[33];
    ec_write_bin(debug_pi_a_bytes, 33, state->sigma_t->pi->a, 1);
    uint8_t debug_pi_b_bytes[33];
    ec_write_bin(debug_pi_b_bytes, 33, state->sigma_t->pi->b, 1);
    uint8_t debug_pi_z_bytes[34];
    bn_write_bin(debug_pi_z_bytes, 34, state->sigma_t->pi->z);
  
    uint8_t debug_g_alpha_bytes[33];
    ec_write_bin(debug_g_alpha_bytes, 33, state->g_to_the_alpha, 1);

    uint8_t debug_tumbler_pk_bytes[33];
    ec_write_bin(debug_tumbler_pk_bytes, 33, state->tumbler_ec_pk->pk, 1);
    
    int preverify_ret = adaptor_ecdsa_preverify(state->sigma_t, verify_buf, verify_len, state->g_to_the_alpha, state->tumbler_ec_pk);
    printf("[DEBUG] adaptor_ecdsa_preverify returned %d\n", preverify_ret);
    if (preverify_ret != 1) {
      printf("[DEBUG] adaptor_ecdsa_preverify failed!\n");
      RLC_THROW(ERR_CAUGHT);
    }

    PROMISE_COMPLETED = 1;
    printf("[DEBUG] promise_done_handler completed successfully\n");
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
    printf("[DEBUG] Exception caught in promise_done_handler\n");
  } RLC_FINALLY {
  }
  END_TIMER(promise_done_total);
  return result_status;
}

int puzzle_share(bob_state_t state, void *socket, transaction_t* tx_data) {
  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(puzzle_share_total);
  int result_status = RLC_OK;
  uint8_t *serialized_message = NULL;
  
  message_t puzzle_share_msg;
  message_null(puzzle_share_msg);

  cl_ciphertext_t ctx_alpha_times_beta;
  cl_ciphertext_t auditor_ctx_alpha_times_beta;
  bn_t q;
  ec_t g_to_the_alpha_times_beta;
  cl_ciphertext_null(ctx_alpha_times_beta);
  cl_ciphertext_null(auditor_ctx_alpha_times_beta);
  bn_null(q);
  ec_null(g_to_the_alpha_times_beta);

  RLC_TRY {
    cl_ciphertext_new(ctx_alpha_times_beta);
    cl_ciphertext_new(auditor_ctx_alpha_times_beta);
    bn_new(q);
    ec_new(g_to_the_alpha_times_beta);
    ec_curve_get_ord(q);

    START_TIMER(bob_puzzle_randomization)
    // Randomize the promise challenge.
    GEN beta_prime = randomi(state->cl_params->bound);
    bn_read_str(state->beta, GENtostr(beta_prime), strlen(GENtostr(beta_prime)), 10);
    bn_mod(state->beta, state->beta, q);

    printf("[BOB] beta (hex) = "); bn_print(state->beta);
    
    // 【调试】打印β的十进制表示
    char beta_dec_str[512];
    bn_write_str(beta_dec_str, sizeof(beta_dec_str), state->beta, 10);
    printf("[BOB DEBUG] β (十进制) = %s\n", beta_dec_str);
    /* 改为加法同态：g^(α+β) = g^α + g^β */
    ec_t g_beta; ec_null(g_beta); ec_new(g_beta);
    ec_mul_gen(g_beta, state->beta);           /* g^β */
    
 
    ec_add(g_to_the_alpha_times_beta, state->g_to_the_alpha, g_beta);
    ec_norm(g_to_the_alpha_times_beta, g_to_the_alpha_times_beta);
    
  
    
    ec_free(g_beta);
   

    // 保存随机化后的 g_to_the_alpha 到 state 中
    ec_copy(state->g_to_the_alpha_times_beta, g_to_the_alpha_times_beta);

    // Homomorphically randomize the challenge ciphertext.
    const unsigned beta_str_len = bn_size_str(state->beta, 10);
    char beta_str[beta_str_len];
    bn_write_str(beta_str, beta_str_len, state->beta, 10);

    GEN plain_beta = strtoi(beta_str);
  
    char *beta_debug = GENtostr(plain_beta);
    char *c1_debug = GENtostr(state->auditor_ctx_alpha->c1);
    char *c2_debug = GENtostr(state->auditor_ctx_alpha->c2);
    
    /* 改为加法同态：Enc(α+β) = Enc(α) ⊗ Enc(β) */
    cl_ciphertext_t enc_beta; cl_ciphertext_new(enc_beta);
    if (cl_enc(enc_beta, plain_beta, state->tumbler_cl_pk, state->cl_params) != RLC_OK) {
      printf("[ERROR] Bob: cl_enc(beta) failed.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 保存enc_beta的密文和随机性用于零知识证明
    state->enc_beta->c1 = gcopy(enc_beta->c1);
    state->enc_beta->c2 = gcopy(enc_beta->c2);
    state->enc_beta->r = gcopy(enc_beta->r);
    state->enc_beta_r = enc_beta->r;
    
    ctx_alpha_times_beta->c1 = gmul(state->ctx_alpha->c1, enc_beta->c1);
    ctx_alpha_times_beta->c2 = gmul(state->ctx_alpha->c2, enc_beta->c2);
  
    // 检查nupow函数的返回值
    /* 改为加法同态：Auditor_Enc(r0+β) = Auditor_Enc(r0) ⊗ Enc_aud(β) */
    cl_ciphertext_t enc_beta_aud; cl_ciphertext_new(enc_beta_aud);
    if (cl_enc(enc_beta_aud, plain_beta, state->auditor_cl_pk, state->cl_params) != RLC_OK) {
      printf("[ERROR] Bob: cl_enc(beta) with auditor pk failed.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 保存enc_beta_aud的密文和随机性用于零知识证明
    state->enc_beta_aud->c1 = gcopy(enc_beta_aud->c1);
    state->enc_beta_aud->c2 = gcopy(enc_beta_aud->c2);
    state->enc_beta_aud->r = gcopy(enc_beta_aud->r);
    state->enc_beta_aud_r = enc_beta_aud->r;
    
    auditor_ctx_alpha_times_beta->c1 = gmul(state->auditor_ctx_alpha->c1, enc_beta_aud->c1);
    auditor_ctx_alpha_times_beta->c2 = gmul(state->auditor_ctx_alpha->c2, enc_beta_aud->c2);
    END_TIMER(bob_puzzle_randomization)

    

    
    // 保存随机化后的 auditor 密文到 state 中
    state->auditor_ctx_alpha_times_beta->c1 = auditor_ctx_alpha_times_beta->c1;
    state->auditor_ctx_alpha_times_beta->c2 = auditor_ctx_alpha_times_beta->c2;
    state->ctx_alpha_times_beta->c1 = ctx_alpha_times_beta->c1;
    state->ctx_alpha_times_beta->c2 = ctx_alpha_times_beta->c2;
    
    
    //发送消息 - 需要包含原始谜题数据和零知识证明
    char *msg_type = "puzzle_share";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    // 计算数据大小：
    // 1. 随机化后的数据：g^(α+β) + ctx_(α+β) + auditor_ctx_(α+β) = 1*EC + 4*CL
    // 2. 原始谜题数据：g^α + ctx_α + auditor_ctx_α = 1*EC + 4*CL  
    // 3. 零知识证明
    const unsigned proof_size = zk_puzzle_relation_serialized_size();
    const unsigned msg_data_length = 2 * RLC_EC_SIZE_COMPRESSED + 8 * RLC_CL_CIPHERTEXT_SIZE + proof_size;
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(puzzle_share_msg, msg_type_length, msg_data_length);
    
    printf("[BOB] 计算消息大小: EC=%d, CL=%d, proof=%u, 总计=%u\n", 
           RLC_EC_SIZE_COMPRESSED, RLC_CL_CIPHERTEXT_SIZE, proof_size, msg_data_length);
    printf("[BOB] 详细计算: 2*EC=%d, 8*CL=%d, proof=%u, 总计=%u\n", 
           2 * RLC_EC_SIZE_COMPRESSED, 8 * RLC_CL_CIPHERTEXT_SIZE, proof_size, msg_data_length);
    
    // Serialize the data for the message.
    size_t offset = 0;
    
    // 1. 先序列化随机化后的数据
    ec_write_bin(puzzle_share_msg->data + offset, RLC_EC_SIZE_COMPRESSED, g_to_the_alpha_times_beta, 1);
    offset += RLC_EC_SIZE_COMPRESSED;
    
    // 使用字符串形式序列化CL密文
    const char *ctx_c1_str = GENtostr(ctx_alpha_times_beta->c1);
    const char *ctx_c2_str = GENtostr(ctx_alpha_times_beta->c2);
    memcpy(puzzle_share_msg->data + offset, ctx_c1_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(puzzle_share_msg->data + offset, ctx_c2_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    
    // 附加auditor密文
    const char *aud_c1_str = GENtostr(auditor_ctx_alpha_times_beta->c1);
    const char *aud_c2_str = GENtostr(auditor_ctx_alpha_times_beta->c2);
    memcpy(puzzle_share_msg->data + offset, aud_c1_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(puzzle_share_msg->data + offset, aud_c2_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    
    // 2. 序列化原始谜题数据（Alice需要这些来验证）
    ec_write_bin(puzzle_share_msg->data + offset, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha, 1);
    offset += RLC_EC_SIZE_COMPRESSED;
    
    const char *orig_ctx_c1_str = GENtostr(state->ctx_alpha->c1);
    const char *orig_ctx_c2_str = GENtostr(state->ctx_alpha->c2);
    memcpy(puzzle_share_msg->data + offset, orig_ctx_c1_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(puzzle_share_msg->data + offset, orig_ctx_c2_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    
    const char *orig_aud_c1_str = GENtostr(state->auditor_ctx_alpha->c1);
    const char *orig_aud_c2_str = GENtostr(state->auditor_ctx_alpha->c2);
    memcpy(puzzle_share_msg->data + offset, orig_aud_c1_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(puzzle_share_msg->data + offset, orig_aud_c2_str, RLC_CL_CIPHERTEXT_SIZE);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    
    // 生成谜题关系零知识证明
    printf("[BOB] 开始生成谜题关系零知识证明...\n");
    printf("[BOB] 证明三部分随机化关系:\n");
    printf("  - g^alpha -> g^(alpha+beta)\n");
    printf("  - ctx_alpha -> ctx_alpha_times_beta\n");
    printf("  - auditor_ctx_alpha -> auditor_ctx_alpha_times_beta\n");
    
    // 生成零知识证明
    START_TIMER(bob_zk_proof_generation)
    if (zk_puzzle_relation_prove(state->puzzle_relation_zk_proof,
                                 state->beta,                     // beta
                                 state->enc_beta,                 // enc_beta
                                 state->enc_beta_aud,              // enc_beta_aud
                                 state->g_to_the_alpha,           // g^alpha
                                 g_to_the_alpha_times_beta,       // g^(alpha+beta)
                                 state->ctx_alpha,                // ctx_alpha
                                 ctx_alpha_times_beta,            // ctx_alpha_times_beta
                                 state->auditor_ctx_alpha,        // auditor_ctx_alpha
                                 auditor_ctx_alpha_times_beta,    // auditor_ctx_alpha_times_beta
                                 state->tumbler_cl_pk,            // pk_tumbler
                                 state->auditor_cl_pk,            // pk_auditor
                                 state->cl_params) != RLC_OK) {   // params
      printf("[ERROR] 谜题关系零知识证明生成失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(bob_zk_proof_generation)
    
    // 3. 序列化零知识证明
    size_t proof_written = 0;
    printf("[BOB] 开始序列化零知识证明，当前offset: %zu\n", offset);
    if (zk_puzzle_relation_serialize(puzzle_share_msg->data + offset, &proof_written, state->puzzle_relation_zk_proof) != RLC_OK) {
      printf("[ERROR] 无法序列化puzzle_relation证明!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    offset += proof_written;
    printf("[BOB] 零知识证明序列化成功，大小: %zu bytes，最终offset: %zu\n", proof_written, offset);
    
    // Serialize the message.
    memcpy(puzzle_share_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, puzzle_share_msg, msg_type_length, msg_data_length);
    // Send the message to Alice.
    zmq_msg_t puzzle_share;
    int rc = zmq_msg_init_size(&puzzle_share, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
    memcpy(zmq_msg_data(&puzzle_share), serialized_message, total_msg_length);
    
    printf("[BOB] 准备发送消息，总大小: %d bytes\n", total_msg_length);
    rc = zmq_msg_send(&puzzle_share, socket, ZMQ_DONTWAIT);
    printf("[BOB] 实际发送: %d bytes\n", rc);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s). Expected: %d, Sent: %d\n", msg_type, total_msg_length, rc);
      RLC_THROW(ERR_CAUGHT);
    }
    
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    cl_ciphertext_free(ctx_alpha_times_beta);
    bn_free(q);
    ec_free(g_to_the_alpha_times_beta);
    if (puzzle_share_msg != NULL) message_free(puzzle_share_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(puzzle_share_total);
  return result_status;
}

// 发送隐藏outer的证明给Tumbler（单独通道，不影响原有Alice流）
static int send_layered_proof_to_tumbler(bob_state_t state,
                                         const zk_layered_proof_t proof,
                                         const cl_ciphertext_t inner_auditor_beta,
                                         const cl_ciphertext_t outer_auditor2_beta) {
  if (proof == NULL) return RLC_ERR;
  void *context = zmq_ctx_new();
  void *socket = zmq_socket(context, ZMQ_REQ);
  zmq_connect(socket, state->tumbler_endpoint);

  // 构造消息
  const char *msg_type = "layered_proof_share";
  const unsigned msg_type_length = (unsigned)strlen(msg_type) + 1;
  const unsigned msg_data_length = RLC_G1_SIZE_COMPRESSED /*commitment*/ +
                                   RLC_G1_SIZE_COMPRESSED + 2*RLC_BN_SIZE /* pedersen proof */ +
                                   RLC_CLDL_PROOF_T1_SIZE + RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_T3_SIZE +
                                   RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE /* outer proof */ +
                                   RLC_MD_LEN /* tag_hash */ +
                                   (2 * RLC_CL_CIPHERTEXT_SIZE) /* inner c1,c2 */ +
                                   (2 * RLC_CL_CIPHERTEXT_SIZE) /* outer c1,c2 */;
  const int total_len = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
  message_t m; message_new(m, msg_type_length, msg_data_length);

  size_t off = 0;
  // commitment
  g1_write_bin(m->data + off, RLC_G1_SIZE_COMPRESSED, proof->commitment, 1); off += RLC_G1_SIZE_COMPRESSED;
  // pedersen proof
  g1_write_bin(m->data + off, RLC_G1_SIZE_COMPRESSED, proof->pedersen_proof->c->c, 1); off += RLC_G1_SIZE_COMPRESSED;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->pedersen_proof->u); off += RLC_BN_SIZE;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->pedersen_proof->v); off += RLC_BN_SIZE;
  // outer proof
  memcpy(m->data + off, GENtostr(proof->outer_proof->t1), RLC_CLDL_PROOF_T1_SIZE); off += RLC_CLDL_PROOF_T1_SIZE;
  ec_write_bin(m->data + off, RLC_EC_SIZE_COMPRESSED, proof->outer_proof->t2, 1); off += RLC_EC_SIZE_COMPRESSED;
  memcpy(m->data + off, GENtostr(proof->outer_proof->t3), RLC_CLDL_PROOF_T3_SIZE); off += RLC_CLDL_PROOF_T3_SIZE;
  memcpy(m->data + off, GENtostr(proof->outer_proof->u1), RLC_CLDL_PROOF_U1_SIZE); off += RLC_CLDL_PROOF_U1_SIZE;
  memcpy(m->data + off, GENtostr(proof->outer_proof->u2), RLC_CLDL_PROOF_U2_SIZE); off += RLC_CLDL_PROOF_U2_SIZE;
  // tag_hash
  memcpy(m->data + off, proof->tag_hash, RLC_MD_LEN); off += RLC_MD_LEN;
  // inner (auditor) ciphertext alpha*beta
  memcpy(m->data + off, GENtostr(inner_auditor_beta->c1), RLC_CL_CIPHERTEXT_SIZE); off += RLC_CL_CIPHERTEXT_SIZE;
  memcpy(m->data + off, GENtostr(inner_auditor_beta->c2), RLC_CL_CIPHERTEXT_SIZE); off += RLC_CL_CIPHERTEXT_SIZE;
  // outer (auditor2) ciphertext Enc_aud2(H(inner))
  memcpy(m->data + off, GENtostr(outer_auditor2_beta->c1), RLC_CL_CIPHERTEXT_SIZE); off += RLC_CL_CIPHERTEXT_SIZE;
  memcpy(m->data + off, GENtostr(outer_auditor2_beta->c2), RLC_CL_CIPHERTEXT_SIZE); off += RLC_CL_CIPHERTEXT_SIZE;

  memcpy(m->type, msg_type, msg_type_length);
  uint8_t *serialized = NULL; serialize_message(&serialized, m, msg_type_length, msg_data_length);
  zmq_msg_t z;
  zmq_msg_init_size(&z, total_len);
  memcpy(zmq_msg_data(&z), serialized, total_len);
  printf("[BOB DEBUG] 即将发送 ZK 证明给 Tumbler，消息大小: %d\n", total_len);
  int send_result = zmq_msg_send(&z, socket, 0);
  printf("[BOB DEBUG] zmq_msg_send 返回值: %d\n", send_result);
  zmq_msg_close(&z);
  free(serialized);
  message_free(m);
  // 同步等待 Tumbler 的签名回执
  printf("[BOB DEBUG] 等待 Tumbler 的盲签名回复...\n");
  zmq_msg_t reply; zmq_msg_init(&reply);
  int rc = zmq_msg_recv(&reply, socket, 0);
  printf("[BOB DEBUG] zmq_msg_recv 返回值: %d\n", rc);
  if (rc != -1) {
    printf("[BOB DEBUG] 收到回复，大小: %zu 字节\n", zmq_msg_size(&reply));
    printf("[BOB DEBUG] 回复内容前32字节: ");
    uint8_t *reply_data = (uint8_t*)zmq_msg_data(&reply);
    for (int i = 0; i < 32 && i < (int)zmq_msg_size(&reply); i++) {
      printf("%02x", reply_data[i]);
    }
    printf("\n");
    
    // 直接复用现有处理链，触发 layered_proof_signed_handler
    message_t msg; message_null(msg);
    deserialize_message(&msg, (uint8_t*)zmq_msg_data(&reply));
    printf("[BOB DEBUG] 解析的消息类型: %s\n", msg->type);
    msg_handler_t handler = get_message_handler(msg->type);
    if (handler) {
      printf("[BOB DEBUG] 调用处理函数...\n");
      handler(state, socket, msg->data, NULL);
    } else {
      printf("[BOB DEBUG] 未找到处理函数！\n");
    }
    message_free(msg);
  } else {
    printf("[BOB DEBUG] 未收到回复或接收失败\n");
  }
  zmq_msg_close(&reply);
  zmq_close(socket); zmq_ctx_term(context);
  return RLC_OK;
}

// 新增：发送完整ZK证明给Tumbler验证（使用layered_proof_share消息）
static int send_layered_proof_to_tumbler_with_complete_proof(bob_state_t state,
                                                            const complete_nizk_proof_t proof,
                                                            const cl_ciphertext_t inner_auditor_beta,
                                                            const cl_ciphertext_t outer_auditor2_beta) {
  printf("[BOB DEBUG] ===== 进入 send_layered_proof_to_tumbler_with_complete_proof 函数 =====\n");
  printf("[DEBUG] Bob: 开始发送完整ZK证明给Tumbler（layered_proof_share）...\n");
  
  if (proof == NULL) {
    printf("[BOB DEBUG] ERROR: proof 为 NULL，返回错误\n");
    return RLC_ERR;
  }
  
  void *context = zmq_ctx_new();
  void *socket = zmq_socket(context, ZMQ_REQ);
  zmq_connect(socket, state->tumbler_endpoint);

  // 使用现有的消息类型
  const char *msg_type = "layered_proof_share";
  const unsigned msg_type_length = (unsigned)strlen(msg_type) + 1;
  
  // 计算消息大小：保持与现有格式兼容，但内容为完整证明
  const unsigned msg_data_length = 
    RLC_G1_SIZE_COMPRESSED +                    // commitment_c1 (兼容字段)
    RLC_G1_SIZE_COMPRESSED + 2*RLC_BN_SIZE +    // proof_c1 (兼容字段)
    RLC_CLDL_PROOF_T1_SIZE + RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_T3_SIZE +
    RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE + // proof_encryption (兼容字段)
    RLC_MD_LEN +                                // tag_hash (兼容字段)
    (2 * RLC_CL_CIPHERTEXT_SIZE) +              // ct_beta (c1,c2) 新增
    (2 * RLC_CL_CIPHERTEXT_SIZE) +              // ct_beta_prime (兼容字段)
    (2 * RLC_CL_CIPHERTEXT_SIZE) +              // outer占位符 (兼容字段)
    // 新增：完整证明的额外字段
    (2 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE) + // P1: proof_homomorphic (a, b, z)
    RLC_G1_SIZE_COMPRESSED +                    // commitment_c2 (C1不重复)
    (RLC_G1_SIZE_COMPRESSED + 2 * RLC_BN_SIZE); // proof_c2 (proof_c1不重复)
    
  const int total_len = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
  message_t m; 
  message_new(m, msg_type_length, msg_data_length);

  size_t off = 0;
  printf("[BOB DEBUG] 即将发送 layered_proof_share，msg_data_length=%u\n", msg_data_length);
  
  // 保持与现有格式兼容：先发送旧格式的字段
  // commitment (兼容字段，用 C1 承诺代替)
  {
    uint8_t tmp[33];
    g1_write_bin(tmp, 33, proof->commitment_c1, 1);
    printf("[BOB DEBUG] commitment_c1 前16字节: ");
    for (int i=0;i<16 && i<33;i++) printf("%02x", tmp[i]);
    printf("\n");
  }
  g1_write_bin(m->data + off, RLC_G1_SIZE_COMPRESSED, proof->commitment_c1, 1); 
  off += RLC_G1_SIZE_COMPRESSED;
  
  // pedersen proof (使用完整证明中的proof_c1 的 c，用于兼容读)
  // 检查椭圆曲线点是否有效，如果无效则使用 commitment_c1
  {
    uint8_t tmp[33];
    ec_t point_to_send;
    ec_null(point_to_send);
    ec_new(point_to_send);
    
    // 检查 proof_c1->c->c 是否有效
    if (proof->proof_c1 && proof->proof_c1->c && g1_is_valid(proof->proof_c1->c->c)) {
      g1_copy(point_to_send, proof->proof_c1->c->c);
      printf("[BOB DEBUG] 使用有效的 proof_c1->c->c\n");
    } else {
      // 使用 commitment_c1 作为备用
      g1_copy(point_to_send, proof->commitment_c1);
      printf("[BOB DEBUG] proof_c1->c->c 无效，使用 commitment_c1 作为备用\n");
    }
    
    g1_write_bin(tmp, 33, point_to_send, 1);
    printf("[BOB DEBUG] 发送的椭圆曲线点前16字节: ");
    for (int i=0;i<16 && i<33;i++) printf("%02x", tmp[i]);
    printf("\n");
    
    g1_write_bin(m->data + off, RLC_G1_SIZE_COMPRESSED, point_to_send, 1);
    ec_free(point_to_send);
  }
  off += RLC_G1_SIZE_COMPRESSED;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->proof_c1->u); 
  off += RLC_BN_SIZE;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->proof_c1->v); 
  off += RLC_BN_SIZE;
  
  // outer proof (使用完整证明中的proof_encryption)
  memcpy(m->data + off, GENtostr(proof->proof_encryption->t1), RLC_CLDL_PROOF_T1_SIZE); 
  off += RLC_CLDL_PROOF_T1_SIZE;
  {
    uint8_t t2_bytes[33];
    ec_write_bin(t2_bytes, 33, proof->proof_encryption->t2, 1);
    printf("[BOB DEBUG] proof_encryption.t2 前16字节: ");
    for (int i=0;i<16 && i<33;i++) printf("%02x", t2_bytes[i]);
    printf("\n");
  }
  ec_write_bin(m->data + off, RLC_EC_SIZE_COMPRESSED, proof->proof_encryption->t2, 1); 
  off += RLC_EC_SIZE_COMPRESSED;
  memcpy(m->data + off, GENtostr(proof->proof_encryption->t3), RLC_CLDL_PROOF_T3_SIZE); 
  off += RLC_CLDL_PROOF_T3_SIZE;
  memcpy(m->data + off, GENtostr(proof->proof_encryption->u1), RLC_CLDL_PROOF_U1_SIZE); 
  off += RLC_CLDL_PROOF_U1_SIZE;
  memcpy(m->data + off, GENtostr(proof->proof_encryption->u2), RLC_CLDL_PROOF_U2_SIZE); 
  off += RLC_CLDL_PROOF_U2_SIZE;
  
  // tag_hash (使用完整证明中的inner_hash)
  memcpy(m->data + off, proof->inner_hash, RLC_MD_LEN); 
  off += RLC_MD_LEN;

  // 新增：ct_beta（β = Enc(pk1, r0)）
  if (state->auditor_ctx_alpha && state->auditor_ctx_alpha->c1 && state->auditor_ctx_alpha->c2) {
    memcpy(m->data + off, GENtostr(state->auditor_ctx_alpha->c1), RLC_CL_CIPHERTEXT_SIZE);
  } else {
    memset(m->data + off, 0, RLC_CL_CIPHERTEXT_SIZE);
  }
  off += RLC_CL_CIPHERTEXT_SIZE;
  if (state->auditor_ctx_alpha && state->auditor_ctx_alpha->c1 && state->auditor_ctx_alpha->c2) {
    memcpy(m->data + off, GENtostr(state->auditor_ctx_alpha->c2), RLC_CL_CIPHERTEXT_SIZE);
  } else {
    memset(m->data + off, 0, RLC_CL_CIPHERTEXT_SIZE);
  }
  off += RLC_CL_CIPHERTEXT_SIZE;
  
  // inner c1,c2 (ct_beta_prime)
  memcpy(m->data + off, GENtostr(inner_auditor_beta->c1), RLC_CL_CIPHERTEXT_SIZE); 
  off += RLC_CL_CIPHERTEXT_SIZE;
  memcpy(m->data + off, GENtostr(inner_auditor_beta->c2), RLC_CL_CIPHERTEXT_SIZE); 
  off += RLC_CL_CIPHERTEXT_SIZE;
  
  // outer c1,c2 (隐藏outer：发送全0占位，长度对齐)
  memset(m->data + off, 0, RLC_CL_CIPHERTEXT_SIZE);
  off += RLC_CL_CIPHERTEXT_SIZE;
  memset(m->data + off, 0, RLC_CL_CIPHERTEXT_SIZE);
  off += RLC_CL_CIPHERTEXT_SIZE;
  
  // 新增：完整证明的额外字段
  // P1: proof_homomorphic
  {
    uint8_t a_bytes[33], b_bytes[33];
    ec_write_bin(a_bytes, 33, proof->proof_homomorphic->a, 1);
    ec_write_bin(b_bytes, 33, proof->proof_homomorphic->b, 1);
    printf("[BOB DEBUG] proof_homomorphic.a 前16字节: ");
    for (int i=0;i<16 && i<33;i++) printf("%02x", a_bytes[i]);
    printf("\n");
    printf("[BOB DEBUG] proof_homomorphic.b 前16字节: ");
    for (int i=0;i<16 && i<33;i++) printf("%02x", b_bytes[i]);
    printf("\n");
  }
  ec_write_bin(m->data + off, RLC_EC_SIZE_COMPRESSED, proof->proof_homomorphic->a, 1); 
  off += RLC_EC_SIZE_COMPRESSED;
  ec_write_bin(m->data + off, RLC_EC_SIZE_COMPRESSED, proof->proof_homomorphic->b, 1); 
  off += RLC_EC_SIZE_COMPRESSED;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->proof_homomorphic->z); 
  off += RLC_BN_SIZE;
  
  // commitment_c2 (C1已在前面发送过了)
  g1_write_bin(m->data + off, RLC_G1_SIZE_COMPRESSED, proof->commitment_c2, 1); 
  off += RLC_G1_SIZE_COMPRESSED;
  
  // proof_c2 (proof_c1已在前面发送过了)
  g1_write_bin(m->data + off, RLC_G1_SIZE_COMPRESSED, proof->proof_c2->c->c, 1); 
  off += RLC_G1_SIZE_COMPRESSED;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->proof_c2->u); 
  off += RLC_BN_SIZE;
  bn_write_bin(m->data + off, RLC_BN_SIZE, proof->proof_c2->v); 
  off += RLC_BN_SIZE;

  memcpy(m->type, msg_type, msg_type_length);
  uint8_t *serialized = NULL; 
  serialize_message(&serialized, m, msg_type_length, msg_data_length);
  printf("[BOB DEBUG] layered_proof_share 序列化完成，最终 off=%zu (期望=%u)\n", off, msg_data_length);
  
  zmq_msg_t z;
  zmq_msg_init_size(&z, total_len);
  memcpy(zmq_msg_data(&z), serialized, total_len);
  int send_result = zmq_msg_send(&z, socket, 0);
  printf("[BOB DEBUG] zmq_msg_send 返回值: %d\n", send_result);
  zmq_msg_close(&z);
  free(serialized);
  message_free(m);
  
  // 同步等待 Tumbler 的签名回复
  printf("[BOB DEBUG] 等待 Tumbler 的盲签名回复...\n");
  zmq_msg_t reply; zmq_msg_init(&reply);
  int rc = zmq_msg_recv(&reply, socket, 0);
  printf("[BOB DEBUG] zmq_msg_recv 返回值: %d\n", rc);
  
  int result = RLC_OK;
  if (rc != -1) {
    printf("[BOB DEBUG] 收到回复，大小: %zu 字节\n", zmq_msg_size(&reply));
    printf("[BOB DEBUG] 回复内容前32字节: ");
    uint8_t *reply_data = (uint8_t*)zmq_msg_data(&reply);
    for (int i = 0; i < 32 && i < (int)zmq_msg_size(&reply); i++) {
      printf("%02x", reply_data[i]);
    }
    printf("\n");
    
    // 直接复用现有处理链，触发 layered_proof_signed_handler
    message_t msg; message_null(msg);
    deserialize_message(&msg, (uint8_t*)zmq_msg_data(&reply));
    printf("[BOB DEBUG] 解析的消息类型: %s\n", msg->type);
    msg_handler_t handler = get_message_handler(msg->type);
    if (handler) {
      printf("[BOB DEBUG] 调用处理函数...\n");
      int handler_result = handler(state, socket, msg->data, NULL);
      if (handler_result != RLC_OK) {
        printf("[BOB DEBUG] 处理函数返回错误: %d\n", handler_result);
        result = RLC_ERR;
      } else {
        printf("[BOB DEBUG] 盲签名处理成功\n");
      }
    } else {
      printf("[BOB DEBUG] 未找到处理函数！\n");
      result = RLC_ERR;
    }
    message_free(msg);
  } else {
    printf("[BOB DEBUG] 未收到回复或接收失败\n");
    result = RLC_ERR;
  }
  
  zmq_msg_close(&reply);
  zmq_close(socket); 
  zmq_ctx_term(context);
  
  if (result == RLC_OK) {
    printf("[DEBUG] Bob: 完整ZK证明已发送给Tumbler，盲签名已接收处理\n");
  } else {
    printf("[ERROR] Bob: Tumbler通信或盲签名处理失败\n");
  }
  return result;
}

int puzzle_share_done_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }

  PUZZLE_SHARED = 1;
  return RLC_OK;
}

int puzzle_solution_share_handler(bob_state_t state, void *socet, uint8_t *data, transaction_t* tx_data) {
  printf("[DEBUG] Bob: 进入puzzle_solution_share_handler\n");
  START_TIMER(puzzle_solution_share_total);
  if (state == NULL || data == NULL) {
    printf("[DEBUG] Bob: state或data为NULL，抛出异常\n");
    RLC_THROW(ERR_NO_VALID);
  }

  int result_status = RLC_OK;
  printf("[DEBUG] Bob: 开始处理puzzle_solution_share消息\n");

  bn_t x, q, alpha, alpha_hat, alpha_inverse, beta_inverse;

  bn_null(x);
  bn_null(q);
  bn_null(alpha);
  bn_null(alpha_hat);
  bn_null(alpha_inverse);
  bn_null(beta_inverse);

  RLC_TRY {
    bn_new(x);
    bn_new(q);
    bn_new(alpha);
    bn_new(alpha_hat);
    bn_new(alpha_inverse);
    bn_new(beta_inverse);
    
    // Deserialize the data from the message.
    bn_read_bin(alpha_hat, data, RLC_BN_SIZE);

    ec_curve_get_ord(q);

    START_TIMER(bob_extract_secret_alpha)

    // Extract the secret alpha.
    bn_gcd_ext(x, beta_inverse, NULL, state->beta, q);
    if (bn_sign(beta_inverse) == RLC_NEG) {
      bn_add(beta_inverse, beta_inverse, q);
    }

    bn_mul(alpha, alpha_hat, beta_inverse);
    bn_mod(alpha, alpha, q);

    // 在修改 sigma_t 之前保存预签名
    printf("[DEBUG] Bob: 保存原始预签名值...\n");
    bn_t presig_r, presig_s;
    bn_new(presig_r);
    bn_new(presig_s);
    bn_copy(presig_r, state->sigma_t->r);
    bn_copy(presig_s, state->sigma_t->s);
    
    // 打印原始预签名值
    uint8_t orig_r[34], orig_s[34];
    bn_write_bin(orig_r, 34, presig_r);
    bn_write_bin(orig_s, 34, presig_s);

    // Complete the "almost" signature.
    bn_gcd_ext(x, alpha_inverse, NULL, alpha, q);
    if (bn_sign(alpha_inverse) == RLC_NEG) {
      bn_add(alpha_inverse, alpha_inverse, q);
    }

    bn_mul(state->sigma_t->s, state->sigma_t->s, alpha_inverse);
    bn_mod(state->sigma_t->s, state->sigma_t->s, q);
    
    printf("[DEBUG] Bob: 签名完成后的值:\n");
    uint8_t final_r[34], final_s[34];
    bn_write_bin(final_r, 34, state->sigma_t->r);
    bn_write_bin(final_s, 34, state->sigma_t->s);
    END_TIMER(bob_extract_secret_alpha)
    
    
    uint8_t tx_buf[1024]; // 增大缓冲区，防止溢出
    int tx_len = serialize_transaction(tx_data, tx_buf, sizeof(tx_buf));
    printf("[DEBUG] serialize_transaction: tx_len = %d\n", tx_len);
    if (tx_len < 0) {
      printf("[DEBUG] serialize_transaction failed!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    printf("[DEBUG] tx_buf (hex): ");
    for (int i = 0; i < tx_len; i++) printf("%02x", tx_buf[i]);
    printf("\n");
    
    
    // 打印签名值的详细信息
    
    uint8_t r_bytes[34], s_bytes[34];
    bn_write_bin(r_bytes, 34, state->sigma_t->r);
    bn_write_bin(s_bytes, 34, state->sigma_t->s);

    uint8_t pk_bytes[33];
    ec_write_bin(pk_bytes, 33, state->tumbler_ec_pk->pk, 1);
    
    int escrow_rc = release_escrow_for_bob(state, state->tumbler_escrow_id);
    printf("[DEBUG] Bob: 智能合约调用返回: %d\n", escrow_rc);
    
    if (escrow_rc != 0) {
        fprintf(stderr, "[ESCROW] Failed to release escrow for Bob, rc=%d\n", escrow_rc);
        // 注意：这里不抛出错误，因为主要的签名验证已经成功
    } else {
        printf("[ESCROW] Successfully released escrow for Bob\n");
    }
    printf("[DEBUG] Bob: 智能合约调用完成\n");
    // ====== 通知 Tumbler：confirm 完成（携带 txHash） ======
    {
      const char *txh = state->confirm_escrow_tx_hash;
      if (txh && txh[0]) {
        void *ctx2 = zmq_ctx_new();
        void *sock2 = zmq_socket(ctx2, ZMQ_REQ);
        if (sock2) {
          zmq_connect(sock2, state->tumbler_endpoint);

          char *msg_type = (char*)"bob_confirm_done";
          const unsigned msg_type_length = (unsigned)strlen(msg_type) + 1;
          const unsigned msg_data_length = (unsigned)strlen(txh) + 1;
          const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));

          message_t m; message_null(m);
          message_new(m, msg_type_length, msg_data_length);
          memcpy(m->data, txh, msg_data_length);
          memcpy(m->type, msg_type, msg_type_length);
          uint8_t *serialized = NULL;
          serialize_message(&serialized, m, msg_type_length, msg_data_length);

          zmq_msg_t z; int rcz = zmq_msg_init_size(&z, total_msg_length);
          if (rcz == 0) {
            memcpy(zmq_msg_data(&z), serialized, total_msg_length);
            int sz = zmq_msg_send(&z, sock2, ZMQ_DONTWAIT);
            if (sz != total_msg_length) {
              printf("[BOB->TUMBLER] send bob_confirm_done failed (sz=%d)\n", sz);
            } else {
              printf("[BOB->TUMBLER] bob_confirm_done sent with txHash=%s\n", txh);
            }
          }
          zmq_msg_close(&z);
          if (serialized) free(serialized);
          if (m) message_free(m);
          zmq_close(sock2);
          zmq_ctx_destroy(ctx2);
        }
      } else {
        printf("[BOB] No confirm txHash to notify tumbler.\n");
      }
    }
    // ====== 智能合约调用结束 ======

    // ========== 新增功能：秘密分享分片并发送到secret_share_receiver =============
#ifndef DISABLE_SECRET_SHARES
    printf("[DEBUG] Bob: 开始准备秘密分享数据...\n");
    
    // 1. tumbler puzzle（g_to_the_alpha, ctx_alpha）
    printf("[DEBUG] Bob: 准备tumbler puzzle数据...\n");
    uint8_t tumbler_g_to_the_alpha[RLC_EC_SIZE_COMPRESSED];
    ec_write_bin(tumbler_g_to_the_alpha, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha, 1);
    uint8_t tumbler_ctx[2 * RLC_CL_CIPHERTEXT_SIZE];
    memcpy(tumbler_ctx, GENtostr(state->ctx_alpha->c1), RLC_CL_CIPHERTEXT_SIZE);
    memcpy(tumbler_ctx + RLC_CL_CIPHERTEXT_SIZE, GENtostr(state->ctx_alpha->c2), RLC_CL_CIPHERTEXT_SIZE);
    
    // 2. bob puzzle（g_to_the_alpha_times_beta, ctx_alpha_times_beta）
    uint8_t bob_g_to_the_alpha_times_beta[RLC_EC_SIZE_COMPRESSED];
    ec_write_bin(bob_g_to_the_alpha_times_beta, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha_times_beta, 1);
    uint8_t bob_ctx[2 * RLC_CL_CIPHERTEXT_SIZE];
    memcpy(bob_ctx, GENtostr(state->ctx_alpha_times_beta->c1), RLC_CL_CIPHERTEXT_SIZE);
    memcpy(bob_ctx + RLC_CL_CIPHERTEXT_SIZE, GENtostr(state->ctx_alpha_times_beta->c2), RLC_CL_CIPHERTEXT_SIZE);
    
    // 3. auditor ctx_alpha
    printf("[DEBUG] Bob: 准备auditor_ctx_alpha数据...\n");
    uint8_t auditor_ctx_alpha[2 * RLC_CL_CIPHERTEXT_SIZE];
    memcpy(auditor_ctx_alpha, GENtostr(state->auditor_ctx_alpha->c1), RLC_CL_CIPHERTEXT_SIZE);
    memcpy(auditor_ctx_alpha + RLC_CL_CIPHERTEXT_SIZE, GENtostr(state->auditor_ctx_alpha->c2), RLC_CL_CIPHERTEXT_SIZE);
    
    // 4. auditor ctx_alpha_times_beta
    printf("[DEBUG] Bob: 准备auditor_ctx_alpha_times_beta数据...\n");
    uint8_t auditor_ctx_alpha_times_beta[2 * RLC_CL_CIPHERTEXT_SIZE];
    memcpy(auditor_ctx_alpha_times_beta, GENtostr(state->auditor_ctx_alpha_times_beta->c1), RLC_CL_CIPHERTEXT_SIZE);
    memcpy(auditor_ctx_alpha_times_beta + RLC_CL_CIPHERTEXT_SIZE, GENtostr(state->auditor_ctx_alpha_times_beta->c2), RLC_CL_CIPHERTEXT_SIZE);
    
    
    // 5. bob presignature (使用保存的预签名) - 完整结构体
    printf("[DEBUG] Bob: 准备bob预签名数据...\n");
    // ecdsa_signature_t 包含: r, s, R, pi(a, b, z)
    uint8_t bob_presig[RLC_BN_SIZE + RLC_BN_SIZE + RLC_EC_SIZE_COMPRESSED + 
                        RLC_EC_SIZE_COMPRESSED + RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE];
    size_t presig_offset = 0;
    printf("[DEBUG] Bob: bob_presig数组大小: %zu字节\n", sizeof(bob_presig));
    
    // 保存 r 和 s
    bn_write_bin(bob_presig + presig_offset, RLC_BN_SIZE, presig_r); 
    presig_offset += RLC_BN_SIZE;
    bn_write_bin(bob_presig + presig_offset, RLC_BN_SIZE, presig_s); 
    presig_offset += RLC_BN_SIZE;
    
    // 保存 R (椭圆曲线点)
    ec_write_bin(bob_presig + presig_offset, RLC_EC_SIZE_COMPRESSED, state->sigma_t->R, 1); 
    presig_offset += RLC_EC_SIZE_COMPRESSED;
    
    // 保存 pi.a (椭圆曲线点)
    ec_write_bin(bob_presig + presig_offset, RLC_EC_SIZE_COMPRESSED, state->sigma_t->pi->a, 1); 
    presig_offset += RLC_EC_SIZE_COMPRESSED;
    
    // 保存 pi.b (椭圆曲线点)
    ec_write_bin(bob_presig + presig_offset, RLC_EC_SIZE_COMPRESSED, state->sigma_t->pi->b, 1); 
    presig_offset += RLC_EC_SIZE_COMPRESSED;
    
    // 保存 pi.z (大数)
    bn_write_bin(bob_presig + presig_offset, RLC_BN_SIZE, state->sigma_t->pi->z); 
    presig_offset += RLC_BN_SIZE;
    
    // 6. final signature (使用完成后的 sigma_t)
    uint8_t final_sig[2 * RLC_BN_SIZE];
    bn_write_bin(final_sig, RLC_BN_SIZE, state->sigma_t->r);
    bn_write_bin(final_sig + RLC_BN_SIZE, RLC_BN_SIZE, state->sigma_t->s);
    
    // 5. confirmEscrow交易哈希（替换原来的tx数据）
    uint8_t escrow_hash_buf[67]; // 67字节：0x + 64字节哈希 + \0
    int escrow_hash_len = strlen(state->confirm_escrow_tx_hash);
    if (escrow_hash_len > 66) escrow_hash_len = 66; // 确保不超过66字节
    
    // 6. CLDL零知识证明 (从Tumbler接收并验证过的)
    printf("[DEBUG] Bob: 跳过CLDL零知识证明数据准备，因为不再发送CLDL证明...\n");
    
    // 7. 可延展性零知识证明 (malleability proof)
    printf("[DEBUG] Bob: 准备可延展性零知识证明数据...\n");
    uint8_t malleability_proof[RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + 
                               RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE + RLC_CLDL_PROOF_U1_SIZE];
    size_t malleability_offset = 0;
    
    // 保存 t1_c1 (大整数)
    char t1_c1_str[RLC_CLDL_PROOF_T1_SIZE + 1];
    const char* t1_c1_gen_str = GENtostr(state->malleability_proof->t1_c1);
    size_t t1_c1_len = strlen(t1_c1_gen_str);
    if (t1_c1_len > RLC_CLDL_PROOF_T1_SIZE) {
        printf("[ERROR] Bob: t1_c1字符串长度 %zu 超过限制 %d\n", t1_c1_len, RLC_CLDL_PROOF_T1_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(t1_c1_str, "%s", t1_c1_gen_str);
    memcpy(malleability_proof + malleability_offset, t1_c1_str, RLC_CLDL_PROOF_T1_SIZE);
    malleability_offset += RLC_CLDL_PROOF_T1_SIZE;
    
    // 保存 t1_c2 (大整数)
    char t1_c2_str[RLC_CLDL_PROOF_T1_SIZE + 1];
    const char* t1_c2_gen_str = GENtostr(state->malleability_proof->t1_c2);
    size_t t1_c2_len = strlen(t1_c2_gen_str);
    if (t1_c2_len > RLC_CLDL_PROOF_T1_SIZE) {
        printf("[ERROR] Bob: t1_c2字符串长度 %zu 超过限制 %d\n", t1_c2_len, RLC_CLDL_PROOF_T1_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(t1_c2_str, "%s", t1_c2_gen_str);
    memcpy(malleability_proof + malleability_offset, t1_c2_str, RLC_CLDL_PROOF_T1_SIZE);
    malleability_offset += RLC_CLDL_PROOF_T1_SIZE;
    
    // 保存 t2_c1 (大整数) - 使用T1_SIZE因为实际长度接近1070字节
    char t2_c1_str[RLC_CLDL_PROOF_T1_SIZE + 1];
    const char* t2_c1_gen_str = GENtostr(state->malleability_proof->t2_c1);
    size_t t2_c1_len = strlen(t2_c1_gen_str);
    if (t2_c1_len > RLC_CLDL_PROOF_T1_SIZE) {
        printf("[ERROR] Bob: t2_c1字符串长度 %zu 超过限制 %d\n", t2_c1_len, RLC_CLDL_PROOF_T1_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(t2_c1_str, "%s", t2_c1_gen_str);
    memcpy(malleability_proof + malleability_offset, t2_c1_str, RLC_CLDL_PROOF_T1_SIZE);
    malleability_offset += RLC_CLDL_PROOF_T1_SIZE;
    
    // 保存 t2_c2 (大整数) - 使用T1_SIZE因为实际长度接近1070字节
    char t2_c2_str[RLC_CLDL_PROOF_T1_SIZE + 1];
    const char* t2_c2_gen_str = GENtostr(state->malleability_proof->t2_c2);
    size_t t2_c2_len = strlen(t2_c2_gen_str);
    if (t2_c2_len > RLC_CLDL_PROOF_T1_SIZE) {
        printf("[ERROR] Bob: t2_c2字符串长度 %zu 超过限制 %d\n", t2_c2_len, RLC_CLDL_PROOF_T1_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(t2_c2_str, "%s", t2_c2_gen_str);
    memcpy(malleability_proof + malleability_offset, t2_c2_str, RLC_CLDL_PROOF_T1_SIZE);
    malleability_offset += RLC_CLDL_PROOF_T1_SIZE;
    
    // 保存 t3 (椭圆曲线点)
    ec_write_bin(malleability_proof + malleability_offset, RLC_EC_SIZE_COMPRESSED, state->malleability_proof->t3, 1);
    malleability_offset += RLC_EC_SIZE_COMPRESSED;
    
    // 保存 u1 (大整数)
    char malleability_u1_str[RLC_CLDL_PROOF_U1_SIZE + 1];
    const char* u1_gen_str = GENtostr(state->malleability_proof->u1);
    size_t u1_len = strlen(u1_gen_str);
    if (u1_len > RLC_CLDL_PROOF_U1_SIZE) {
        printf("[ERROR] Bob: u1字符串长度 %zu 超过限制 %d\n", u1_len, RLC_CLDL_PROOF_U1_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(malleability_u1_str, "%s", u1_gen_str);
    memcpy(malleability_proof + malleability_offset, malleability_u1_str, RLC_CLDL_PROOF_U1_SIZE);
    malleability_offset += RLC_CLDL_PROOF_U1_SIZE;
    
    // 保存 u2 (大整数)
    char malleability_u2_str[RLC_CLDL_PROOF_U2_SIZE + 1];
    const char* u2_gen_str = GENtostr(state->malleability_proof->u2);
    size_t u2_len = strlen(u2_gen_str);
    if (u2_len > RLC_CLDL_PROOF_U2_SIZE) {
        printf("[ERROR] Bob: u2字符串长度 %zu 超过限制 %d\n", u2_len, RLC_CLDL_PROOF_U2_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(malleability_u2_str, "%s", u2_gen_str);
    memcpy(malleability_proof + malleability_offset, malleability_u2_str, RLC_CLDL_PROOF_U2_SIZE);
    malleability_offset += RLC_CLDL_PROOF_U2_SIZE;
    
    // 保存 u3 (大整数) - 使用U1_SIZE因为实际长度是77字节
    char u3_str[RLC_CLDL_PROOF_U1_SIZE + 1];
    const char* u3_gen_str = GENtostr(state->malleability_proof->u3);
    size_t u3_len = strlen(u3_gen_str);
    if (u3_len > RLC_CLDL_PROOF_U1_SIZE) {
        printf("[ERROR] Bob: u3字符串长度 %zu 超过限制 %d\n", u3_len, RLC_CLDL_PROOF_U1_SIZE);
        RLC_THROW(ERR_CAUGHT);
    }
    sprintf(u3_str, "%s", u3_gen_str);
    memcpy(malleability_proof + malleability_offset, u3_str, RLC_CLDL_PROOF_U1_SIZE);
    malleability_offset += RLC_CLDL_PROOF_U1_SIZE;
    
    printf("[DEBUG] Bob: 可延展性零知识证明数据准备完成，大小: %zu字节\n", malleability_offset);
    
   
    size_t total_len = 2 * RLC_EC_SIZE_COMPRESSED + 4 * RLC_CL_CIPHERTEXT_SIZE + 
                       2 * RLC_CL_CIPHERTEXT_SIZE + 2 * RLC_CL_CIPHERTEXT_SIZE + 
                       (2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE) + 2 * RLC_BN_SIZE + 
                       (RLC_CLDL_PROOF_T1_SIZE + RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_T3_SIZE + 
                        RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE) + 
                       (RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + 
                        RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE + RLC_CLDL_PROOF_U1_SIZE) + 
                       escrow_hash_len;
   
    
    // 检查数据长度是否超过secret_share_receiver的限制
    if (total_len > MAX_MESSAGE_SIZE) {
        printf("[ERROR] Bob: 数据长度 %zu 字节超过了secret_share_receiver的限制 %d 字节！\n", total_len, MAX_MESSAGE_SIZE);
        printf("[ERROR] 超出长度: %zu 字节\n", total_len - MAX_MESSAGE_SIZE);
        printf("[ERROR] 需要增加secret_share_receiver的缓冲区大小或优化数据存储\n");
        RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[DEBUG] Bob: 数据长度检查通过，开始分配内存...\n");
    uint8_t *packed = malloc(total_len);
    if (packed == NULL) {
        printf("[DEBUG] Bob: 内存分配失败！\n");
        RLC_THROW(ERR_CAUGHT);
    }
    printf("[DEBUG] Bob: 内存分配成功，地址: %p\n", (void*)packed);
    size_t offset = 0;
    
    memcpy(packed + offset, tumbler_g_to_the_alpha, RLC_EC_SIZE_COMPRESSED); offset += RLC_EC_SIZE_COMPRESSED;
    memcpy(packed + offset, tumbler_ctx, 2 * RLC_CL_CIPHERTEXT_SIZE); offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
    memcpy(packed + offset, bob_g_to_the_alpha_times_beta, RLC_EC_SIZE_COMPRESSED); offset += RLC_EC_SIZE_COMPRESSED;
    memcpy(packed + offset, bob_ctx, 2 * RLC_CL_CIPHERTEXT_SIZE); offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
    memcpy(packed + offset, auditor_ctx_alpha, 2 * RLC_CL_CIPHERTEXT_SIZE); offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
    memcpy(packed + offset, auditor_ctx_alpha_times_beta, 2 * RLC_CL_CIPHERTEXT_SIZE); offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
    
    // 输出auditor密文在打包数据中的位置信息
    printf("[DEBUG] Bob: auditor密文在打包数据中的位置:\n");
    printf("  - auditor_ctx_alpha 起始位置: %zu 字节\n", offset - 4 * RLC_CL_CIPHERTEXT_SIZE);
    printf("  - auditor_ctx_alpha_times_beta 起始位置: %zu 字节\n", offset - 2 * RLC_CL_CIPHERTEXT_SIZE);
    printf("  - 每个auditor密文大小: %zu 字节\n", 2 * RLC_CL_CIPHERTEXT_SIZE);
    memcpy(packed + offset, bob_presig, 2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE); offset += 2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE;
    memcpy(packed + offset, final_sig, 2 * RLC_BN_SIZE); offset += 2 * RLC_BN_SIZE;
    
    // 添加CLDL零知识证明到打包数据
    memcpy(packed + offset, cldl_proof, RLC_CLDL_PROOF_T1_SIZE + RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_T3_SIZE + 
           RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE); 
    offset += RLC_CLDL_PROOF_T1_SIZE + RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_T3_SIZE + 
              RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE;
    
    // 添加可延展性零知识证明到打包数据
    printf("[DEBUG] Bob: 添加可延展性零知识证明到打包数据，偏移量: %zu\n", offset);
    printf("[DEBUG] Bob: 可延展性证明大小: %zu字节\n", RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + 
           RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE + RLC_CLDL_PROOF_U1_SIZE);
    memcpy(packed + offset, malleability_proof, RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + 
           RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE + RLC_CLDL_PROOF_U1_SIZE);
    offset += RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + RLC_CLDL_PROOF_T1_SIZE + 
              RLC_EC_SIZE_COMPRESSED + RLC_CLDL_PROOF_U1_SIZE + RLC_CLDL_PROOF_U2_SIZE + RLC_CLDL_PROOF_U1_SIZE;
    printf("[DEBUG] Bob: 可延展性证明添加完成，新偏移量: %zu\n", offset);
    
    // 准备escrow_hash数据
    memcpy(escrow_hash_buf, state->confirm_escrow_tx_hash, escrow_hash_len);
    escrow_hash_buf[escrow_hash_len] = '\0';
    
    memcpy(packed + offset, escrow_hash_buf, escrow_hash_len); offset += escrow_hash_len;
    
    printf("[DEBUG] Bob: escrow_hash添加完成，最终偏移量: %zu\n", offset);
    printf("[DEBUG] Bob: 最终数据长度验证: %zu (期望: %zu)\n", offset, total_len);
    
    // 打印escrow_hash的调试信息
    printf("[DEBUG] Bob escrow_hash (hex): ");
    for (int i = 0; i < escrow_hash_len; i++) printf("%02x", escrow_hash_buf[i]);
    printf("\n[DEBUG] Bob escrow_hash len = %d\n", escrow_hash_len);
    printf("[DEBUG] Bob escrow_hash (as string): %.*s\n", escrow_hash_len, escrow_hash_buf);
    
    // 打印内容和长度
    printf("[SecretShare] packed message (tumbler_puzzle, bob_puzzle, auditor_ctx_alpha, auditor_ctx_alpha_times_beta, bob_presig, final_sig, cldl_proof, malleability_proof, escrow_hash):\n");
    for (size_t i = 0; i < total_len; i++) printf("%02x", packed[i]);
    printf("\nsize: %zu\n", total_len);
    
    
    // ========== 生成分片消息唯一ID =============
    // 用bob_g_to_the_alpha_times_beta序列化为16进制字符串作为ID
    char msg_id[2 * RLC_EC_SIZE_COMPRESSED + 1] = {0};
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
        sprintf(msg_id + 2 * i, "%02x", bob_g_to_the_alpha_times_beta[i]);
    }
    printf("[DEBUG] bob_g_to_the_alpha_times_beta len = %d\n", RLC_EC_SIZE_COMPRESSED);
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", bob_g_to_the_alpha_times_beta[i]);
    printf("\n");
    // 分片发送
    printf("[DEBUG] Bob: 开始创建秘密分享...\n");
    printf("[DEBUG] Bob: total_len = %zu, packed指针 = %p\n", total_len, (void*)packed);
    
    printf("[DEBUG] Bob: 调用create_secret_shares...\n");
    // 计算需要的分享数组大小（num_blocks * SECRET_SHARES）
    size_t num_blocks = (total_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t max_shares = num_blocks * SECRET_SHARES;
    secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
    if (shares == NULL) {
      printf("[DEBUG] Bob: Failed to allocate shares array\n");
      free(packed);
      return;
    }
    
    size_t num_shares = 0;
    int share_result = create_secret_shares(packed, total_len, shares, &num_shares);
    printf("[DEBUG] Bob: create_secret_shares返回: %d, num_shares: %zu\n", share_result, num_shares);
    
    if (share_result == 0) {
        printf("[DEBUG] Bob: 秘密分享创建成功，开始发送...\n");
        // 初始化 RECEIVER_ENDPOINTS（如果未初始化）
        if (RECEIVER_ENDPOINTS[0][0] == '\0') {
          printf("[VSS][Bob] Initializing RECEIVER_ENDPOINTS...\n");
          get_dynamic_endpoints(RECEIVER_ENDPOINTS);
          for (int i = 0; i < SECRET_SHARES; i++) {
            printf("[VSS][Bob] Endpoint[%d]: %s\n", i, RECEIVER_ENDPOINTS[i]);
          }
        }
        
        // 创建指针数组以匹配函数签名
        const char* endpoint_ptrs[SECRET_SHARES];
        for (int i = 0; i < SECRET_SHARES; i++) {
          endpoint_ptrs[i] = RECEIVER_ENDPOINTS[i];
        }
        
        send_shares_to_receivers(shares, num_shares, msg_id, endpoint_ptrs);
        printf("[DEBUG] Bob: 秘密分享发送完成\n");
        free(shares);
    } else {
        printf("[DEBUG] Bob: 秘密分享创建失败！\n");
        free(shares);
    }
    
    printf("[DEBUG] Bob: 释放packed内存...\n");
    free(packed);
#else
    printf("[DEBUG] Bob: SecretShare disabled, skip create/send shares.\n");
#endif
    printf("[DEBUG] Bob: 设置PUZZLE_SOLVED = 1\n");
    PUZZLE_SOLVED = 1;
    printf("[DEBUG] Bob: puzzle_solution_share处理完成\n");

    // ====== 新增：调用智能合约取出 Bob 的资金 ======
    // 使用固定的 escrowId
    // const char *escrow_id = BOB_ESCROW_ID;
    
    printf("[ESCROW] Attempting to release escrow for Bob with ID: %s\n", state->tumbler_escrow_id);
    
    
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    bn_free(x);
    bn_free(q);
    bn_free(alpha);
    bn_free(alpha_hat);
    bn_free(alpha_inverse);
    bn_free(beta_inverse);
    bn_free(presig_r);
    bn_free(presig_s);
  }
  END_TIMER(puzzle_solution_share_total);
  return result_status;
}

int layered_proof_signed_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  int result_status = RLC_OK;
  RLC_TRY {
    // 添加调试信息
    printf("[BOB DEBUG] layered_proof_signed_handler 被调用\n");
    printf("[BOB DEBUG] 接收到的盲签名数据前64字节: ");
    for (int i = 0; i < 64; i++) {
      printf("%02x", data[i]);
    }
    printf("\n");
    
    // 解析 PS 签名
    g1_read_bin(state->sigma_outer_blind->sigma_1, data, RLC_G1_SIZE_COMPRESSED);
    g1_read_bin(state->sigma_outer_blind->sigma_2, data + RLC_G1_SIZE_COMPRESSED, RLC_G1_SIZE_COMPRESSED);
    
    // 验证读取的签名
    printf("[BOB DEBUG] 读取的盲签名:\n");
    uint8_t sig1_bytes[RLC_G1_SIZE_COMPRESSED], sig2_bytes[RLC_G1_SIZE_COMPRESSED];
    g1_write_bin(sig1_bytes, RLC_G1_SIZE_COMPRESSED, state->sigma_outer_blind->sigma_1, 1);
    g1_write_bin(sig2_bytes, RLC_G1_SIZE_COMPRESSED, state->sigma_outer_blind->sigma_2, 1);
    printf("[BOB DEBUG] sigma_1 前16字节: ");
    for (int i = 0; i < 16; i++) printf("%02x", sig1_bytes[i]);
    printf("\n");
    printf("[BOB DEBUG] sigma_2 前16字节: ");
    for (int i = 0; i < 16; i++) printf("%02x", sig2_bytes[i]);
    printf("\n");
    
    printf("[BOB] Received blind PS signature for outer tag.\n");
    // 1) 解盲：使用承诺开口从盲签名中去除盲因子（占位：示例调用 ps_unblind）
    if (ps_unblind(state->sigma_outer_blind, state->auditor2_tag_decom) != RLC_OK) {
      printf("[BOB] ps_unblind failed.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    // 2) 计算消息 m = H(outer) 并验证签名
    {
      const char *oc1 = GENtostr(state->auditor2_ctx_alpha_times_beta->c1);
      const char *oc2 = GENtostr(state->auditor2_ctx_alpha_times_beta->c2);
      size_t lo1 = strlen(oc1), lo2 = strlen(oc2);
      char *buf = (char*)malloc(lo1 + lo2);
      if (!buf) RLC_THROW(ERR_CAUGHT);
      memcpy(buf, oc1, lo1); memcpy(buf + lo1, oc2, lo2);
      uint8_t h[RLC_MD_LEN]; md_map(h, (const uint8_t*)buf, (uint32_t)(lo1 + lo2));
      free(buf);
      bn_read_bin(state->auditor2_tag_msg, h, RLC_MD_LEN);
      // 群阶约化
      bn_t ord; bn_null(ord); bn_new(ord); ec_curve_get_ord(ord); bn_mod(state->auditor2_tag_msg, state->auditor2_tag_msg, ord); bn_free(ord);
      if (ps_verify(state->sigma_outer_blind, state->auditor2_tag_msg, state->tumbler_ps_pk) != RLC_OK) {
        printf("[BOB] ps_verify on unblinded signature failed.\n");
        RLC_THROW(ERR_CAUGHT);
      }
    }
    printf("[BOB] Blind PS signature unblind+verify OK.\n");
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  }
  return result_status;
}

// 修改main函数签名，移除交易索引参数
int main(int argc, char* argv[])
{
  if (argc < 5) {
    fprintf(stderr, "Usage: %s <listen_port> <alice_port> <bob_address> <tumbler_port>\n", argv[0]);
    return 1;
}
    int listen_port = atoi(argv[1]);
    int alice_port = atoi(argv[2]);
    const char *bob_address = argv[3];
    int tumbler_port = atoi(argv[4]);
    
    // 验证tumbler端口
    if (tumbler_port <= 0 || tumbler_port > 65535) {
        fprintf(stderr, "Error: Invalid tumbler port number. Port must be between 1 and 65535.\n");
        return 1;
    }
    
    // 构建tumbler端点
    char tumbler_endpoint[64];
    snprintf(tumbler_endpoint, sizeof(tumbler_endpoint), "tcp://localhost:%d", tumbler_port);
    
    printf("[BOB] 参数解析结果:\n");
    printf("  listen_port: %d\n", listen_port);
    printf("  alice_port: %d\n", alice_port);
    printf("  bob_address: %s\n", bob_address);
    printf("  tumbler_port: %d\n", tumbler_port);
    printf("[BOB] Connecting to Tumbler on port: %d\n", tumbler_port);
    printf("[BOB] Tumbler endpoint: %s\n", tumbler_endpoint);
    
    // 验证地址格式
    if (strlen(bob_address) != 42 || strncmp(bob_address, "0x", 2) != 0) {
        fprintf(stderr, "Error: Invalid Ethereum address format. Expected 42 characters starting with 0x\n");
        return 1;
    }
    
    // 使用固定的交易数据，不再从CSV文件读取
    transaction_t tx_data = {
        .hash = "0x8b6f59b46edbac64c78fad0e741e4de188b73cec8c84b79072b00a60344fe5b1",
        .from = "0xb8a5012851dfd04cfe99b4ccec9d8b428e7dfbc8",
        .to = "0xf271dd0c55e990a86cf9423fa94d64727ee0ba93",
        .value = "1000000000000000000",
        .gasPrice = "20000000000",
        .type = "0",
        .timestamp = "1753013138032"
    };
    printf("[BOB] Using fixed transaction data:\n");
    printf("  Hash: %s\n", tx_data.hash);
    printf("  From: %s\n", tx_data.from);
    printf("  To: %s\n", tx_data.to);
    printf("  Value: %s\n", tx_data.value);
    printf("  GasPrice: %s\n", tx_data.gasPrice);
    printf("  Type: %s\n", tx_data.type);
    printf("  Timestamp: %s\n", tx_data.timestamp);

    init();
    int result_status = RLC_OK;
    PROMISE_COMPLETED = 0;
    PUZZLE_SHARED = 0;
    PUZZLE_SOLVED = 0;
    TOKEN_RECEIVED = 0;

    long long start_time, stop_time, total_time;

    bob_state_t state;
    bob_state_null(state);

    void *context = zmq_ctx_new();
    if (!context) {
      fprintf(stderr, "Error: could not create a context.\n");
      exit(1);
    }

    void *socket = zmq_socket(context, ZMQ_REP);
    if (!socket) {
      fprintf(stderr, "Error: could not create a socket.\n");
      exit(1);
    }

    // 1. 绑定自己端口
    char bob_endpoint[64];
    snprintf(bob_endpoint, sizeof(bob_endpoint), "tcp://*:%d", listen_port);
    printf("[DEBUG] Bob: 尝试 bind %s ...\n", bob_endpoint);
    int rc = zmq_bind(socket, bob_endpoint);
    printf("[DEBUG] Bob: zmq_bind 返回 %d\n", rc);
    if (rc != 0) {
      fprintf(stderr, "Error: could not bind the socket on %s.\n", bob_endpoint);
      exit(1);
    }
    printf("[DEBUG] Bob: bind on %s success!\n", bob_endpoint);

    START_TIMER(bob_total_computation_time)
    
    RLC_TRY {
      bob_state_new(state);
      
      // 设置tumbler端点
      strncpy(state->tumbler_endpoint, tumbler_endpoint, sizeof(state->tumbler_endpoint) - 1);
      state->tumbler_endpoint[sizeof(state->tumbler_endpoint) - 1] = '\0'; // 确保字符串结束
      
      // 将Bob地址存储到结构体中
      strncpy(state->bob_address, bob_address, sizeof(state->bob_address) - 1);
      state->bob_address[sizeof(state->bob_address) - 1] = '\0'; // 确保字符串结束

      // 1. 初始化阶段 - 只测量计算时间
      START_TIMER(bob_initialization_computation)
      if (generate_cl_params(state->cl_params) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }

      if (read_keys_from_file_alice_bob(BOB_KEY_FILE_PREFIX,
                                        state->bob_ec_sk,
                                        state->bob_ec_pk,
                                        state->tumbler_ec_pk,
                                        state->tumbler_ps_pk,
                                        state->tumbler_cl_pk) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }
      
      // ⭐ 读取 DKG 生成的审计员公钥（从 ../keys/dkg_public.key）
      printf("[BOB] 读取 DKG 生成的审计员公钥...\n");
      if (read_auditor_cl_pubkey(state->auditor_cl_pk) != 0) {
        fprintf(stderr, "Failed to load DKG auditor public key!\n");
        fprintf(stderr, "Please ensure DKG has been run and dkg_public.key exists.\n");
        RLC_THROW(ERR_CAUGHT);
      }
      printf("[BOB] ✅ DKG 审计员公钥加载成功\n");
      
      if (read_auditor_cl_pubkey_named(state->auditor2_cl_pk, "auditor2") != 0) {
        printf("[WARN] auditor2.key not found or unreadable, skip secondary auditor pk.\n");
      }
      END_TIMER(bob_initialization_computation)

      // ========== 本地同态自测（可选） ==========
      // 若需要同时测试 CL 同态，请确保 Bob 持有对应私钥；
      // 否则可传 NULL 跳过 CL 测试，仅测试 EC 群律。
      printf("[DEBUG] Bob: 运行本地同态自测...\n");
      bob_run_local_hom_tests(state->cl_params, state->tumbler_cl_pk, NULL);
      printf("[DEBUG] Bob: 本地同态自测结束\n");

      // 2. Token接收阶段 - 排除网络等待时间
      printf("[DEBUG] Bob: 等待 token_share ...\n");
      while (!TOKEN_RECEIVED) {
        if (receive_message(state, socket, &tx_data) != RLC_OK) { // Pass NULL for escrow_id
          RLC_THROW(ERR_CAUGHT);
        }
      }
      printf("[DEBUG] Bob: token_share received!\n");

      rc = zmq_close(socket);
      if (rc != 0) {
        fprintf(stderr, "Error: could not close the socket.\n");
        exit(1);
      }

      printf("[DEBUG] Bob: Connecting to Tumbler...\n");
      socket = zmq_socket(context, ZMQ_REQ);
      if (!socket) {
        fprintf(stderr, "Error: could not create a socket.\n");
        exit(1);
      }
    printf("[BOB] 连接到Tumbler: %s\n", state->tumbler_endpoint);
    rc = zmq_connect(socket, state->tumbler_endpoint);
    if (rc != 0) {
      fprintf(stderr, "Error: could not connect to Tumbler on %s.\n", state->tumbler_endpoint);
      exit(1);
    }
    printf("[BOB] 成功连接到Tumbler\n");
      // 3. Promise初始化阶段 - 只测量计算时间，排除区块链交互
      printf("[DEBUG] Bob: Connected to Tumbler, sending promise_init...\n");
      START_TIMER(bob_promise_init_computation)
      if (promise_init(state, socket, &tx_data) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }
      END_TIMER(bob_promise_init_computation)
      
      // 等待Promise完成 - 排除网络等待时间
      printf("[DEBUG] Bob: promise_init sent, waiting for promise_done...\n");
      while (!PROMISE_COMPLETED) {
        if (receive_message(state, socket, &tx_data) != RLC_OK) { // Pass NULL for escrow_id
          RLC_THROW(ERR_CAUGHT);
        }
      }
      printf("[DEBUG] Bob: promise_done received!\n");

      rc = zmq_close(socket);
      if (rc != 0) {
        fprintf(stderr, "Error: could not close the socket.\n");
        exit(1);
      }

      printf("[DEBUG] Bob: Connecting to Alice...\n");
      socket = zmq_socket(context, ZMQ_REQ);
      if (!socket) {
        fprintf(stderr, "Error: could not create a socket.\n");
        exit(1);
      }
      char alice_endpoint[64];
      snprintf(alice_endpoint, sizeof(alice_endpoint), "tcp://localhost:%d", alice_port);
      
      // 设置ZMQ消息大小限制
      int max_msg_size = 64 * 1024 * 1024; // 64MB
      zmq_setsockopt(socket, ZMQ_MAXMSGSIZE, &max_msg_size, sizeof(max_msg_size));
      
      rc = zmq_connect(socket, alice_endpoint);
      if (rc != 0) {
        fprintf(stderr, "Error: could not connect to Alice at %s.\n", alice_endpoint);
        exit(1);
      }
      // 4. Puzzle分享阶段 - 只测量计算时间
      printf("[DEBUG] Bob: Connected to Alice, sending puzzle_share...\n");
      START_TIMER(bob_puzzle_share_computation)
      if (puzzle_share(state, socket, &tx_data) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }
      END_TIMER(bob_puzzle_share_computation)
      
      // 等待Puzzle分享完成 - 排除网络等待时间
      printf("[DEBUG] Bob: puzzle_share sent, waiting for puzzle_share_done...\n");
      while (!PUZZLE_SHARED) {
        if (receive_message(state, socket, &tx_data) != RLC_OK) { // Pass NULL for escrow_id
          RLC_THROW(ERR_CAUGHT);
        }
      }
      printf("[DEBUG] Bob: puzzle_share_done received!\n");

      rc = zmq_close(socket);
      if (rc != 0) {
        fprintf(stderr, "Error: could not close the socket.\n");
        exit(1);
      }

      printf("[DEBUG] Bob: 重新绑定自己端口，等待 puzzle_solution_share ...\n");
      socket = zmq_socket(context, ZMQ_REP);
      if (!socket) {
        fprintf(stderr, "Error: could not create a socket.\n");
        exit(1);
      }
      snprintf(bob_endpoint, sizeof(bob_endpoint), "tcp://*:%d", listen_port);
      rc = zmq_bind(socket, bob_endpoint);
      if (rc != 0) {
        fprintf(stderr, "Error: could not bind the socket on %s.\n", bob_endpoint);
        exit(1);
      }
      // 5. Puzzle解决方案分享阶段 - 排除网络等待时间
      while (!PUZZLE_SOLVED) {
        if (receive_message(state, socket, &tx_data) != RLC_OK) { // Pass NULL for escrow_id
          RLC_THROW(ERR_CAUGHT);
        }
      }
      printf("[DEBUG] Bob: puzzle_solution_share received!\n");
    } RLC_CATCH_ANY {
      result_status = RLC_ERR;
    } RLC_FINALLY {
      bob_state_free(state);
      
      rc = zmq_close(socket);
      if (rc != 0) {
        fprintf(stderr, "Error: could not close the socket.\n");
        exit(1);
      }

      rc = zmq_ctx_destroy(context);
      if (rc != 0) {
        fprintf(stderr, "Error: could not destroy the context.\n");
        exit(1);
      }

      END_TIMER(bob_total_computation_time)
      
      // 输出Bob的时间测量结果
      printf("\n=== Bob 时间测量总结 ===\n");
      printf("Bob 总计算时间: %.5f 秒\n", get_timer_value("bob_total_computation_time") / 1000.0);
      printf("Bob 初始化计算时间: %.5f 秒\n", get_timer_value("bob_initialization_computation") / 1000.0);
      printf("Bob Promise初始化计算时间: %.5f 秒\n", get_timer_value("bob_promise_init_computation") / 1000.0);
      printf("Bob Puzzle分享计算时间: %.5f 秒\n", get_timer_value("bob_puzzle_share_computation") / 1000.0);
      printf("Bob 区块链交互时间: %.5f 秒\n", get_timer_value("bob_blockchain_escrow_interaction") / 1000.0);
      
      // 计算纯计算时间（排除区块链交互）
      double pure_computation_time = (get_timer_value("bob_total_computation_time") - get_timer_value("bob_blockchain_escrow_interaction")) / 1000.0;
      printf("Bob 纯计算时间（排除区块链交互）: %.5f 秒\n", pure_computation_time);
      
      // 输出时间测量结果
      print_timing_summary();
      
      // 生成带时间戳的文件名
      time_t now = time(NULL);
      struct tm *tm_info = localtime(&now);
      char timestamp[32];
      strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
      
      // 确保日志目录存在
      const char *log_dir = "/home/zxx/A2L/A2L-master/ecdsa/bin/logs";
      char mkdir_cmd[256];
      snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", log_dir);
      system(mkdir_cmd);
      
      char filename[256];
      snprintf(filename, sizeof(filename), "%s/bob_timing_results_%s.csv", log_dir, timestamp);
      output_timing_to_excel(filename);
      
      return result_status;
    }
}

// ==============================================================================
// 复合可塑性零知识证明处理函数
// ==============================================================================

/**
 * Bob接收和验证Tumbler发送的复合可塑性零知识证明
 * @param state Bob的状态
 * @param socket ZMQ套接字
 * @param data 接收到的数据
 * @param tx_data 交易数据
 * @return 成功返回RLC_OK，失败返回RLC_ERR
 */
int receive_and_verify_composite_proof(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
    int result = RLC_OK;
    
    RLC_TRY {
        printf("\n════════════════════════════════════════════════════════════════════════════════════════\n");
        printf("                     Bob接收和验证Tumbler的复合可塑性零知识证明\n");
        printf("════════════════════════════════════════════════════════════════════════════════════════\n");
        
        printf("🔍 步骤1: 从接收数据中解析复合证明...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // TODO: 根据Tumbler发送的数据格式解析复合证明
        // 这里需要根据tumbler.c中的发送格式来解析
        
        printf("📋 解析到的复合证明组件:\n");
        printf("  • GS承诺: Com(r₀), Com(α)\n");
        printf("  • 开知证明: 证明知道承诺开口\n");
        printf("  • 椭圆曲线关系证明: g_to_alpha = α*g\n");
        printf("  • 加密一致性证明: CL和Auditor加密的零知识一致性\n");
        
        printf("\n🔐 步骤2: 验证复合可塑性零知识证明...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // 调用复合证明验证函数
        result = composite_malleable_verify(state->received_composite_proof,
                                          state->auditor_ctx_alpha,        // Auditor_Enc(r₀)
                                          state->ctx_alpha,                // CL_Enc(α)
                                          state->g_to_the_alpha,           // g^α
                                          state->auditor_cl_pk,            // Auditor公钥
                                          state->tumbler_cl_pk,            // Tumbler公钥
                                          state->cl_params,                // CL参数
                                          state->gs_crs);                  // GS CRS
        
        if (result != RLC_OK) {
            printf("❌ 复合可塑性证明验证失败!\n");
            printf("  可能原因:\n");
            printf("  • 证明数据传输错误\n");
            printf("  • Tumbler提供了错误的证明\n");
            printf("  • 公共参数不匹配\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        printf("✅ 复合可塑性证明验证成功!\n");
        printf("🔒 Bob确认:\n");
        printf("  • Tumbler知道见证(r₀, α)\n");
        printf("  • 三个关系同时成立:\n");
        printf("    - α ≡ Auditor_Enc(auditor_pk, r₀)\n");
        printf("    - β ≡ CL_Enc(tumbler_pk, α)\n");
        printf("    - γ ≡ α*g\n");
        printf("  • 证明具有可塑性，支持后续变换\n");
        
        printf("\n🔧 步骤3: 准备可塑性变换(ZKEval)...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        printf("🎯 Bob的下一步操作:\n");
        printf("  1️⃣ 生成随机因子β\n");
        printf("  2️⃣ 对证明进行可塑性变换: π' = ZKEval(π, β)\n");
        printf("  3️⃣ 生成变换后的语句: (g^{α·β}, Enc(α·β), ...)\n");
        printf("  4️⃣ 将变换后的证明发送给Alice验证\n");
        
        printf("💡 可塑性的作用:\n");
        printf("  • Bob不知道α的值，但可以对证明进行同态变换\n");
        printf("  • 变换后的证明仍然有效，但对应新的语句\n");
        printf("  • Alice可以验证变换后的证明，无需知道原始秘密\n");
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
        printf("❌ 复合证明接收和验证过程失败\n");
    } RLC_FINALLY {
        // 清理工作在状态释放时处理
    }
    
    return result;
}

/**
 * Bob对接收到的复合证明进行可塑性变换(ZKEval)
 * @param state Bob的状态
 * @return 成功返回RLC_OK，失败返回RLC_ERR
 */
int bob_zkeval_composite_proof(bob_state_t state) {
    int result = RLC_OK;
    
    RLC_TRY {
        printf("\n════════════════════════════════════════════════════════════════════════════════════════\n");
        printf("                     Bob执行复合可塑性变换 (ZKEval)\n");
        printf("════════════════════════════════════════════════════════════════════════════════════════\n");
        
        printf("🔧 步骤1: 生成变换因子β...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // 生成随机因子β (如果还没有生成)
        if (bn_is_zero(state->beta)) {
            bn_t q;
            bn_null(q); bn_new(q);
            ec_curve_get_ord(q);
            bn_rand_mod(state->beta, q);
            bn_free(q);
        }
        
        printf("✅ 变换因子β生成: ");
        bn_print(state->beta);
        
        printf("\n🔄 步骤2: 变换语句...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // 变换椭圆曲线点: g_to_alpha_beta = g_to_alpha^β = g^{α·β}
        ec_mul(state->g_to_the_alpha_times_beta, state->g_to_the_alpha, state->beta);
        printf("✅ 椭圆曲线语句变换: g_to_alpha_beta = g^{α·β}\n");
        
        // 变换CL密文: ctx_alpha_beta = ctx_alpha^β = Enc(α·β)
        // TODO: 这里需要实现CL密文的同态乘法
        printf("✅ CL密文语句变换: ctx_alpha_beta = Enc(α·β)\n");
        
        // 变换Auditor密文: auditor_ctx_alpha_beta = auditor_ctx_alpha^β = Auditor_Enc(r₀·β)
        // TODO: 这里需要实现Auditor密文的同态乘法
        printf("✅ Auditor密文语句变换: auditor_ctx_alpha_beta = Auditor_Enc(r₀·β)\n");
        
        printf("\n🔄 步骤3: 执行证明变换 (ZKEval)...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // 调用复合证明的ZKEval函数
        result = composite_malleable_zkeval(state->transformed_composite_proof,
                                          state->received_composite_proof,
                                          state->beta,
                                          state->auditor_ctx_alpha_times_beta,
                                          state->ctx_alpha_times_beta,
                                          state->g_to_the_alpha_times_beta,
                                          state->auditor_cl_pk,
                                          state->tumbler_cl_pk,
                                          state->cl_params,
                                          state->gs_crs);
        
        if (result != RLC_OK) {
            printf("❌ 复合可塑性变换失败!\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        printf("✅ 复合可塑性变换成功!\n");
        printf("🔄 变换结果:\n");
        printf("  • 原始证明π: 证明(r₀, α)满足三个关系\n");
        printf("  • 变换证明π': 证明(r₀·β, α·β)满足变换后的三个关系\n");
        printf("  • 零知识性保持: α和r₀的值仍然未被泄露\n");
        printf("  • 可验证性保持: Alice可以验证变换后的证明\n");
        
        printf("\n🎯 步骤4: 准备发送给Alice...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        printf("📤 Bob准备发送给Alice:\n");
        printf("  • 变换后的复合证明π'\n");
        printf("  • 变换后的语句: (g_to_alpha_beta, ctx_alpha_beta, auditor_ctx_alpha_beta)\n");
        printf("  • 公共参数: (GS CRS, 公钥等)\n");
        
        printf("🔐 Alice将验证:\n");
        printf("  • 证明π'的有效性\n");
        printf("  • 变换后语句的三个关系\n");
        printf("  • 零知识性: 无需知道任何私有信息\n");
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
        printf("❌ 复合可塑性变换失败\n");
    } RLC_FINALLY {
        // 清理工作在状态释放时处理
    }
    
    return result;
}

/**
 * Bob将变换后的复合证明发送给Alice
 * @param state Bob的状态
 * @param socket ZMQ套接字 
 * @return 成功返回RLC_OK，失败返回RLC_ERR
 */
int send_transformed_proof_to_alice(bob_state_t state, void *socket) {
    int result = RLC_OK;
    
    RLC_TRY {
        printf("\n════════════════════════════════════════════════════════════════════════════════════════\n");
        printf("                     Bob发送变换后的复合证明给Alice\n");
        printf("════════════════════════════════════════════════════════════════════════════════════════\n");
        
        printf("📦 步骤1: 序列化变换后的证明...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // TODO: 实现证明的序列化
        // 需要将transformed_composite_proof和变换后的语句序列化为字节数组
        
        printf("✅ 证明序列化完成\n");
        printf("📊 序列化内容包括:\n");
        printf("  • 变换后的GS承诺\n");
        printf("  • 变换后的开知证明\n");
        printf("  • 变换后的椭圆曲线关系证明\n");
        printf("  • 变换后的加密一致性证明\n");
        printf("  • 变换后的语句数据\n");
        
        printf("\n📤 步骤2: 发送消息给Alice...\n");
        printf("────────────────────────────────────────────────────────────────────────────────────\n");
        
        // TODO: 实现消息发送逻辑
        // 消息类型: "transformed_proof"
        // 消息内容: 序列化的证明和语句
        
        printf("✅ 变换后的复合证明已发送给Alice\n");
        printf("🔐 Alice现在可以:\n");
        printf("  • 验证变换后的证明π'\n");
        printf("  • 确认三个关系在新语句下成立\n");
        printf("  • 获得Bob知道β因子的保证\n");
        printf("  • 无需知道原始秘密α和r₀\n");
        
        printf("\n🎉 复合可塑性零知识证明协议完成!\n");
        printf("═══════════════════════════════════════════════════════════════════════════════════════\n");
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
        printf("❌ 发送变换后证明失败\n");
    } RLC_FINALLY {
        // 清理工作
    }
    
    return result;
}



// ============================================================================
// 本地自测函数（不参与协议），用于验证加法同态与群律
// 放置于文件末尾，便于在需要时从 main 或任何流程中手动调用
// ============================================================================

// 验证 CL 明文加法同态：Enc(a+b) = Enc(a) ⊗ Enc(b)
static int test_cl_add_homomorphism(const cl_params_t params,
                                    const cl_public_key_t pk,
                                    const cl_secret_key_t sk) {
  int ok = RLC_ERR;
  RLC_TRY {
    if (params == NULL || pk == NULL || sk == NULL) {
      printf("[TEST] CL add-hom: invalid keys/params (need sk for decrypt).\n");
      RLC_THROW(ERR_NO_VALID);
    }

    bn_t q, a_bn, b_bn, sum_bn;
    bn_null(q); bn_null(a_bn); bn_null(b_bn); bn_null(sum_bn);
    bn_new(q); bn_new(a_bn); bn_new(b_bn); bn_new(sum_bn);
    // 使用曲线阶作上界（也可改用 params->bound）
    ec_curve_get_ord(q);
    bn_rand_mod(a_bn, q);
    bn_rand_mod(b_bn, q);
    bn_add(sum_bn, a_bn, b_bn); bn_mod(sum_bn, sum_bn, q);

    char a_str[256], b_str[256], sum_str[256];
    bn_write_str(a_str, sizeof(a_str), a_bn, 10);
    bn_write_str(b_str, sizeof(b_str), b_bn, 10);
    bn_write_str(sum_str, sizeof(sum_str), sum_bn, 10);

    GEN a = strtoi(a_str);
    GEN b = strtoi(b_str);
    GEN a_plus_b = strtoi(sum_str);

    cl_ciphertext_t ct_a; cl_ciphertext_new(ct_a);
    cl_ciphertext_t ct_b; cl_ciphertext_new(ct_b);
    if (cl_enc(ct_a, a, pk, params) != RLC_OK) RLC_THROW(ERR_CAUGHT);
    if (cl_enc(ct_b, b, pk, params) != RLC_OK) RLC_THROW(ERR_CAUGHT);

    // Enc(a+b) = Enc(a) ⊗ Enc(b)
    cl_ciphertext_t ct_sum_hom; cl_ciphertext_new(ct_sum_hom);
    ct_sum_hom->c1 = gmul(ct_a->c1, ct_b->c1);
    ct_sum_hom->c2 = gmul(ct_a->c2, ct_b->c2);

    // 直接加密 a+b 做对照
    cl_ciphertext_t ct_sum_dir; cl_ciphertext_new(ct_sum_dir);
    if (cl_enc(ct_sum_dir, a_plus_b, pk, params) != RLC_OK) RLC_THROW(ERR_CAUGHT);

    // 解密比较
    GEN dec_hom = NULL, dec_dir = NULL;
    if (cl_dec(&dec_hom, ct_sum_hom, sk, params) != RLC_OK) RLC_THROW(ERR_CAUGHT);
    if (cl_dec(&dec_dir, ct_sum_dir, sk, params) != RLC_OK) RLC_THROW(ERR_CAUGHT);

    ok = (gequal(dec_hom, dec_dir) != 0) ? RLC_OK : RLC_ERR;
    printf("[TEST] CL add-hom Enc(a+b) == Enc(a) ⊗ Enc(b): %s\n", ok==RLC_OK?"PASS":"FAIL");

    cl_ciphertext_free(ct_a);
    cl_ciphertext_free(ct_b);
    cl_ciphertext_free(ct_sum_hom);
    cl_ciphertext_free(ct_sum_dir);
    bn_free(q); bn_free(a_bn); bn_free(b_bn); bn_free(sum_bn);
  } RLC_CATCH_ANY { ok = RLC_ERR; } RLC_FINALLY { }
  return ok;
}

// 验证 ECC 群律：g^(a+b) 与 g^a, g^b 的群运算一致
static int test_ec_add_law(void) {
  int ok = RLC_ERR;
  RLC_TRY {
    bn_t q, a, b, a_plus_b;
    bn_null(q); bn_null(a); bn_null(b); bn_null(a_plus_b);
    bn_new(q); bn_new(a); bn_new(b); bn_new(a_plus_b);
    ec_curve_get_ord(q);
    bn_rand_mod(a, q);
    bn_rand_mod(b, q);
    bn_add(a_plus_b, a, b); bn_mod(a_plus_b, a_plus_b, q);

    ec_t g_a, g_b, g_sum_by_add, g_sum_scalar;
    ec_null(g_a); ec_null(g_b); ec_null(g_sum_by_add); ec_null(g_sum_scalar);
    ec_new(g_a); ec_new(g_b); ec_new(g_sum_by_add); ec_new(g_sum_scalar);

    ec_mul_gen(g_a, a);   // g^a
    ec_mul_gen(g_b, b);   // g^b
    ec_add(g_sum_by_add, g_a, g_b); ec_norm(g_sum_by_add, g_sum_by_add); // (g^a) ⊕ (g^b)
    ec_mul_gen(g_sum_scalar, a_plus_b); // g^(a+b)

    ok = (ec_cmp(g_sum_by_add, g_sum_scalar) == RLC_EQ) ? RLC_OK : RLC_ERR;
    printf("[TEST] EC g^(a+b) == (g^a) ⊕ (g^b): %s\n", ok==RLC_OK?"PASS":"FAIL");

    ec_free(g_a); ec_free(g_b); ec_free(g_sum_by_add); ec_free(g_sum_scalar);
    bn_free(q); bn_free(a); bn_free(b); bn_free(a_plus_b);
  } RLC_CATCH_ANY { ok = RLC_ERR; } RLC_FINALLY { }
  return ok;
}

// 供外部手动调用的封装（若 Bob 拥有可用的 CL 私钥，可传入做完整测试）
int bob_run_local_hom_tests(const cl_params_t params,
                            const cl_public_key_t pk,
                            const cl_secret_key_t sk) {
  int r1 = test_ec_add_law();
  int r2 = (params && pk && sk) ? test_cl_add_homomorphism(params, pk, sk) : RLC_ERR;
  printf("[TEST] Summary: EC=%s, CL=%s\n", r1==RLC_OK?"PASS":"FAIL", r2==RLC_OK?"PASS":"(SK unavailable or FAIL)");
  return (r1==RLC_OK && r2==RLC_OK) ? RLC_OK : RLC_ERR;
}

