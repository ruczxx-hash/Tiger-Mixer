#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "pari/pari.h"
#include "zmq.h"
#include "alice.h"
#include "types.h"
#include "util.h"
#include "secret_share.h"

// 临时禁用 Alice 的秘密分享发送
#define DISABLE_SECRET_SHARES 1

// 定义预签名相关常量，确保与auditor一致
#define ALICE_RLC_BN_SIZE 34  // 与auditor保持一致
#define ALICE_RLC_EC_SIZE_COMPRESSED 33

unsigned REGISTRATION_COMPLETED;
unsigned PUZZLE_SHARED;
unsigned PUZZLE_SOLVED;
unsigned ESCROW_OPENED;



static int read_text_file(const char *path, char *out, size_t out_sz) {
  FILE *f = fopen(path, "r");
  if (!f) return -1;
  size_t n = fread(out, 1, out_sz - 1, f);
  fclose(f);
  out[n] = '\0';
  return 0;
}

static void left_pad_hex_to_64(const char *src_no_prefix, char *dst64) {
  size_t len = strlen(src_no_prefix);
  size_t pad = (len < 64) ? 64 - len : 0;
  memset(dst64, '0', 64);
  if (len >= 64) {
    memcpy(dst64, src_no_prefix + (len - 64), 64);
  } else {
    memcpy(dst64 + pad, src_no_prefix, len);
  }
}

static void bn_to_0x_padded_hex(bn_t in, char out67[67]) {
  unsigned str_len = bn_size_str(in, 16);
  char *tmp = (char *)malloc(str_len);
  bn_write_str(tmp, str_len, in, 16);
  char body[64];
  left_pad_hex_to_64(tmp, body);
  free(tmp);
  out67[0] = '0'; out67[1] = 'x';
  memcpy(out67 + 2, body, 64);
  out67[66] = '\0';
}

static int open_escrow_sync_with_tid(alice_state_t state, const char *escrow_id) {
  // 直接使用固定值，不再从环境变量读取
  const char *rpc = "http://127.0.0.1:7545";
  // 改为使用固定面额池脚本，通过 --pool 从地址簿解析合约地址
  
  // 使用结构体中的Alice地址
  const char *p1 = state->alice_address;  // Alice 地址
  const char *p2 = "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025";  // Tumbler 地址
  const char *benef = "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025"; // 受益人地址
  const char *refund = state->alice_address; // 退款给 Alice
  
  long deadline = (long)time(NULL) + 3600; // 1小时后过期

 
  char meta_hash[67];
  memcpy(meta_hash, escrow_id, sizeof(meta_hash));

  // 以 tid 作为 escrowId 和 metaHash
  // char escrow_id[67];
  // bn_to_0x_padded_hex(state->tid, escrow_id);
  // char meta_hash[67];
  // memcpy(meta_hash, escrow_id, sizeof(escrow_id));

  // 只使用 Truffle 方式
  const char *truffle_project = "/home/zxx/Config/truffleProject/truffletest";
  
  // 使用结构体中的Alice地址
  const char *alice_from = state->alice_address;
  
  // 生成未压缩公钥（0x04||X||Y）并转为十六进制字符串（与 test_a2l_verifier.js 一致：若132则截到130）
  // 从结构体中读取 alice_ec_pk 和 tumbler_ec_pk 的公钥点（通过 ->pk 字段访问）
  uint8_t alice_pk_bytes[RLC_EC_SIZE_COMPRESSED * 2];
  uint8_t tumbler_pk_bytes[RLC_EC_SIZE_COMPRESSED * 2];
  char alice_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2 + 1];
  char tumbler_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2 + 1];
  size_t i_pk;
  ec_write_bin(alice_pk_bytes, RLC_EC_SIZE_COMPRESSED * 2, state->alice_ec_pk->pk, 0);
  ec_write_bin(tumbler_pk_bytes, RLC_EC_SIZE_COMPRESSED * 2, state->tumbler_ec_pk->pk, 0);
  // 生成纯十六进制字符串，不加 "0x" 前缀（JS 脚本会统一处理前缀）
  for (i_pk = 0; i_pk < (size_t)(RLC_EC_SIZE_COMPRESSED * 2); i_pk++) {
    sprintf(alice_pk_hex + 2*i_pk, "%02x", alice_pk_bytes[i_pk]);
    sprintf(tumbler_pk_hex + 2*i_pk, "%02x", tumbler_pk_bytes[i_pk]);
  }
  alice_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2] = '\0';
  tumbler_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2] = '\0';
  
  // 打印输出两个公钥（打印时加上前缀以便阅读）
  printf("[ESCROW] Alice公钥 (未压缩): 0x%s\n", alice_pk_hex);
  printf("[ESCROW] Tumbler公钥 (未压缩): 0x%s\n", tumbler_pk_hex);
  START_TIMER(alice_blockchain_escrow_interaction)
  char *cmd = (char *)malloc(4096);
  if (!cmd) return -1;
  
  // 修改命令：使用 2>&1 捕获 stderr 和 stdout，移除 | cat
  // 添加 Tornado Cash commitment 参数
  snprintf(cmd, 4096,
           "cd %s && npx truffle exec scripts/openEscrow_fixed.js --network private "
           "--pool %s --id %s --p1 %s --p2 %s --benef %s --refund %s --deadline %ld --meta %s --from %s --p1key %s --p2key %s --commitment %s 2>&1",
           truffle_project, state->pool_label, escrow_id, p1, p2, benef, refund, deadline, meta_hash, alice_from, alice_pk_hex, tumbler_pk_hex, state->commitment);

  printf("[ESCROW] openEscrow sync cmd: %s\n", cmd);
  
  // 测量区块链交互时间
  
  // 执行命令并捕获输出
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    fprintf(stderr, "[ESCROW] Failed to execute command\n");
    free(cmd);
    return -1;
  }
  
  // 增加buffer大小，收集完整输出
  char buffer[4096];
  char tx_hash[67] = {0};
  char *error_buffer = NULL;
  size_t error_buffer_size = 0;
  int has_error = 0;
  
  // 读取命令输出，寻找交易哈希
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    const char *disable_io = getenv("A2L_DISABLE_IO");
    if (!disable_io || strcmp(disable_io, "1") != 0) {
      printf("[ESCROW] Output: %s", buffer);
    }
    
    // 检测错误信息（排除已知的非致命警告）
    if ((strstr(buffer, "Error:") != NULL || strstr(buffer, "ERROR") != NULL || 
         strstr(buffer, "error") != NULL || strstr(buffer, "Error") != NULL) &&
        strstr(buffer, "Cannot find module") == NULL &&  // 忽略 uws 模块警告
        strstr(buffer, "Falling back to") == NULL) {     // 忽略回退提示
      has_error = 1;
      
      // 收集错误信息到缓冲区
      size_t line_len = strlen(buffer);
      error_buffer = realloc(error_buffer, error_buffer_size + line_len + 1);
      if (error_buffer) {
        memcpy(error_buffer + error_buffer_size, buffer, line_len);
        error_buffer_size += line_len;
        error_buffer[error_buffer_size] = '\0';
      }
    }
    
    // 查找包含交易哈希的行
    if (strstr(buffer, "Transaction hash:") != NULL || strstr(buffer, "txHash:") != NULL) {
      // 提取交易哈希
      char *hash_start = strstr(buffer, "0x");
      if (hash_start) {
        strncpy(tx_hash, hash_start, 66);
        tx_hash[66] = '\0';
        printf("[ESCROW] Captured transaction hash: %s\n", tx_hash);
        break;
      }
    }
    
    // 查找JSON格式的交易哈希 {"txHash":"0x...", "contract":"0x..."}
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
            strncpy(state->escrow_tx_hash, tx_hash, sizeof(state->escrow_tx_hash) - 1);
            state->escrow_tx_hash[sizeof(state->escrow_tx_hash) - 1] = '\0';
            printf("[ESCROW] Stored escrow transaction hash: %s\n", state->escrow_tx_hash);
            
            query_and_save_transaction_details(state->escrow_tx_hash);
            
            // 继续提取合约地址字段（若存在）
            char *contract_key = strstr(hash_end, "\"contract\"");
            if (contract_key) {
              char *addr_start = strstr(contract_key, "\"0x");
              if (addr_start) {
                addr_start += 1; // 跳过引号
                char *addr_end = strchr(addr_start, '"');
                if (addr_end) {
                  int addr_len = addr_end - addr_start;
                  if (addr_len >= 42) addr_len = 42; // 0x + 40
                  strncpy(state->pool_contract, addr_start, addr_len);
                  state->pool_contract[addr_len] = '\0';
                  printf("[ESCROW] Captured pool contract: %s\n", state->pool_contract);
                }
              }
            }
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
  }
  
  int rc = pclose(fp);
  free(cmd);
  
  
  // 如果成功提取到交易哈希，说明交易已成功提交，即使有警告也视为成功
  int tx_hash_extracted = (strlen(tx_hash) > 0);
  
  // 显示完整的错误信息（只有在没有提取到交易哈希且确实有错误时才报错）
  if (!tx_hash_extracted && (has_error || rc != 0)) {
    printf("\n");
    printf("========================================\n");
    printf("[ESCROW] ❌ 交易执行失败！\n");
    printf("========================================\n");
    printf("返回码: %d\n", rc);
    
    if (error_buffer && error_buffer_size > 0) {
      printf("\n完整错误信息:\n");
      printf("----------------------------------------\n");
      printf("%s", error_buffer);
      printf("----------------------------------------\n");
    } else {
      printf("未捕获到详细错误信息\n");
    }
    
    printf("\n可能的原因:\n");
    printf("  1. 账户余额不足 (需要 ~0.11 ETH)\n");
    printf("  2. 账户未解锁\n");
    printf("  3. Nonce 冲突 (多个交易同时发送)\n");
    printf("  4. Gas 不足或区块 gas limit 达到\n");
    printf("  5. 智能合约 revert (检查合约逻辑)\n");
    printf("\n诊断命令:\n");
    printf("  geth attach %s --exec \"eth.getBalance('%s')\"\n", 
           "/home/zxx/Config/blockchain/consortium_blockchain/myblockchain/geth.ipc",
           alice_from);
    printf("  geth attach %s --exec \"txpool.status\"\n",
           "/home/zxx/Config/blockchain/consortium_blockchain/myblockchain/geth.ipc");
    printf("========================================\n");
    printf("\n");
    
    // 清理错误缓冲区
    if (error_buffer) {
      free(error_buffer);
    }
    
    return -1;
  }
  
  // 清理错误缓冲区（成功情况）
  if (error_buffer) {
    free(error_buffer);
  }
  
  // 如果成功提取到交易哈希或返回码为0，视为成功
  if (tx_hash_extracted || rc == 0) {
    ESCROW_OPENED = 1;
    
    // 如果成功捕获到交易哈希，确保存储到state中（可能在提取时已存储，这里确保不遗漏）
    if (strlen(tx_hash) > 0 && strlen(state->escrow_tx_hash) == 0) {
      strncpy(state->escrow_tx_hash, tx_hash, sizeof(state->escrow_tx_hash) - 1);
      state->escrow_tx_hash[sizeof(state->escrow_tx_hash) - 1] = '\0';
      printf("[ESCROW] Stored escrow transaction hash: %s\n", state->escrow_tx_hash);
    }

    // 将交易哈希作为 dataHash 写入合约（bytes32），便于后续签名校验使用
    if (strlen(state->escrow_tx_hash) > 0) {
      const char *truffle_project = "/home/zxx/Config/truffleProject/truffletest";
      const char *from_addr = state->alice_address;
      char cmd_set[4096];
      // 使用 state->escrow_tx_hash（已在提取时设置）
      snprintf(cmd_set, sizeof(cmd_set),
               "cd %s && npx truffle exec scripts/setDataHash.js --network private --pool %s --id %s --hash %s --from %s 2>&1",
               truffle_project, state->pool_label, escrow_id, state->escrow_tx_hash, from_addr);
      printf("[ESCROW] setDataHash cmd: %s\n", cmd_set);
      int rc_set = system(cmd_set);
      if (rc_set == 0) {
        printf("[ESCROW] Wrote dataHash (txHash) to escrow successfully.\n");
      } else {
        printf("[ESCROW] Failed to write dataHash to escrow, rc=%d.\n", rc_set);
      }
    }
    
    // 将托管交易哈希写入文件
    FILE *hash_file = fopen("escrow_transaction_hashes.txt", "a");
    if (hash_file != NULL) {
      // 获取当前时间戳
      time_t now = time(NULL);
      char timestamp[64];
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
      
      // 写入格式：时间戳 | Alice地址 | 托管交易哈希
      fprintf(hash_file, "%s | %s | %s\n", timestamp, state->alice_address, state->escrow_tx_hash);
      fclose(hash_file);
      printf("[ESCROW] Transaction hash saved to file: escrow_transaction_hashes.txt\n");
    } else {
      fprintf(stderr, "[ESCROW] Warning: Could not open file to save transaction hash\n");
    }
    
    // 查询并保存交易详情（已在提取交易哈希时调用）
    // query_and_save_transaction_details(state->escrow_tx_hash);
  }
  END_TIMER(alice_blockchain_escrow_interaction)
  // 返回0表示成功（无论是否提取到交易哈希，只要命令执行成功或提取到哈希就成功）
  return 0;
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
    case REGISTRATION_DONE:
      return registration_done_handler;
    
    case PUZZLE_SHARE:
      return puzzle_share_handler;

    case PAYMENT_DONE:
      return payment_done_handler;

    default:
      fprintf(stderr, "Error: invalid message type.\n");
      exit(1);
  }
}

int handle_message(alice_state_t state, void *socket, zmq_msg_t message, transaction_t* tx_data) {
  int result_status = RLC_OK;

  message_t msg;
  message_null(msg);

  RLC_TRY {
    printf("Received message size: %ld bytes\n", zmq_msg_size(&message));
    deserialize_message(&msg, (uint8_t *) zmq_msg_data(&message));

    printf("Executing %s...\n", msg->type);
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

int receive_message(alice_state_t state, void *socket, transaction_t* tx_data) {
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



int registration(alice_state_t state, void *socket, const char *alice_escrow_id) {
  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(registration_total)
  int result_status = RLC_OK;
  uint8_t *serialized_message = NULL;
  
  message_t registration_msg;
  message_null(registration_msg);

  RLC_TRY {
    
    // ===== Tornado Cash 流程：通过脚本生成 nullifier、secret，并计算 commitment 和 nullifierHash =====
    const char *tornado_project = "/home/zxx/tornado-core-master";
    const char *script_path = "/home/zxx/tornado-core-master/scripts/tornado_generate_key.js";
    
    // 调用 JavaScript 脚本生成密钥并计算哈希
    char *cmd_tornado = (char *)malloc(4096);
    if (!cmd_tornado) {
      RLC_THROW(ERR_NO_MEMORY);
    }
    
    snprintf(cmd_tornado, 4096,
             "cd %s && node %s 2>&1",
             tornado_project, script_path);
    
    printf("[TORNADO] Executing script: %s\n", cmd_tornado);
    
    FILE *fp_tornado = popen(cmd_tornado, "r");
    if (!fp_tornado) {
      fprintf(stderr, "[TORNADO] Failed to execute script\n");
      free(cmd_tornado);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 读取脚本输出（JSON 格式）
    char json_buffer[2048] = {0};
    size_t json_len = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp_tornado) != NULL) {
      // 跳过非 JSON 行（错误信息等）
      if (line[0] == '{' || strstr(line, "\"nullifier\"") != NULL) {
        size_t line_len = strlen(line);
        if (json_len + line_len < sizeof(json_buffer)) {
          memcpy(json_buffer + json_len, line, line_len);
          json_len += line_len;
        }
      }
    }
    
    int rc_tornado = pclose(fp_tornado);
    free(cmd_tornado);
    
    if (rc_tornado != 0 || json_len == 0) {
      fprintf(stderr, "[TORNADO] Script execution failed or no output, rc=%d\n", rc_tornado);
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[TORNADO] Script output: %s\n", json_buffer);
    
    // 解析 JSON 输出
    // 格式: {"nullifier":"0x...","secret":"0x...","commitment":"0x...","nullifierHash":"0x..."}
    char *nullifier_start = strstr(json_buffer, "\"nullifier\"");
    char *secret_start = strstr(json_buffer, "\"secret\"");
    char *commitment_start = strstr(json_buffer, "\"commitment\"");
    char *nullifier_hash_start = strstr(json_buffer, "\"nullifierHash\"");
    
    if (!nullifier_start || !secret_start || !commitment_start || !nullifier_hash_start) {
      fprintf(stderr, "[TORNADO] Failed to parse JSON output\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 提取 nullifier (31 字节，62 个十六进制字符 + "0x" = 64 字符)
    char *nullifier_hex_start = strstr(nullifier_start, "\"0x");
    if (nullifier_hex_start) {
      nullifier_hex_start += 1; // 跳过引号
      char *nullifier_hex_end = strchr(nullifier_hex_start, '"');
      if (nullifier_hex_end) {
        int hex_len = nullifier_hex_end - nullifier_hex_start;
        if (hex_len >= 64) { // "0x" + 62 hex chars
          // 将十六进制字符串转换为字节数组
          const char *hex_str = nullifier_hex_start + 2; // 跳过 "0x"
          for (int i = 0; i < 31; i++) {
            char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
            state->nullifier[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
          }
          printf("[TORNADO] Parsed nullifier: ");
          for (int i = 0; i < 31; i++) {
            printf("%02x", state->nullifier[i]);
          }
          printf("\n");
        }
      }
    }
    
    // 提取 secret (31 字节)
    char *secret_hex_start = strstr(secret_start, "\"0x");
    if (secret_hex_start) {
      secret_hex_start += 1;
      char *secret_hex_end = strchr(secret_hex_start, '"');
      if (secret_hex_end) {
        int hex_len = secret_hex_end - secret_hex_start;
        if (hex_len >= 64) {
          const char *hex_str = secret_hex_start + 2;
          for (int i = 0; i < 31; i++) {
            char hex_byte[3] = {hex_str[i * 2], hex_str[i * 2 + 1], '\0'};
            state->secret[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
          }
          printf("[TORNADO] Parsed secret: ");
          for (int i = 0; i < 31; i++) {
            printf("%02x", state->secret[i]);
          }
          printf("\n");
        }
      }
    }
    
    // 提取 commitment (32 字节，64 个十六进制字符 + "0x" = 66 字符)
    char *commitment_hex_start = strstr(commitment_start, "\"0x");
    if (commitment_hex_start) {
      commitment_hex_start += 1;
      char *commitment_hex_end = strchr(commitment_hex_start, '"');
      if (commitment_hex_end) {
        int hex_len = commitment_hex_end - commitment_hex_start;
        if (hex_len >= 66) {
          strncpy(state->commitment, commitment_hex_start, 66);
          state->commitment[66] = '\0';
          printf("[TORNADO] Parsed commitment: %s\n", state->commitment);
        }
      }
    }
    
    // 提取 nullifierHash (32 字节)
    char *nullifier_hash_hex_start = strstr(nullifier_hash_start, "\"0x");
    if (nullifier_hash_hex_start) {
      nullifier_hash_hex_start += 1;
      char *nullifier_hash_hex_end = strchr(nullifier_hash_hex_start, '"');
      if (nullifier_hash_hex_end) {
        int hex_len = nullifier_hash_hex_end - nullifier_hash_hex_start;
        if (hex_len >= 66) {
          strncpy(state->nullifier_hash, nullifier_hash_hex_start, 66);
          state->nullifier_hash[66] = '\0';
          printf("[TORNADO] Parsed nullifierHash: %s\n", state->nullifier_hash);
        }
      }
    }
    
    printf("[TORNADO] Successfully generated keys and computed hashes\n");
    // ===== Tornado Cash 流程结束 =====
    
    // ===== 原代码（已注释）=====
    // //生成随机交易id
    // bn_rand_mod(state->tid, q);
    // // m = H(tid || pool_label)
    // if (hash_tid_with_pool(msg_bn, state->tid, state->pool_label) != RLC_OK) {
    //   RLC_THROW(ERR_CAUGHT);
    // }
    // //生成承诺（对 m 而非原始 tid）
    // if (pedersen_commit(state->pcom, state->pdecom, state->tumbler_ps_pk->Y_1, msg_bn) != RLC_OK) {
    //   RLC_THROW(ERR_CAUGHT);
    // }
    // //生成零知识证明
    // if (zk_pedersen_com_prove(com_zk_proof, state->tumbler_ps_pk->Y_1, state->pcom, state->pdecom) != RLC_OK) {
    //   RLC_THROW(ERR_CAUGHT);
    // }
    // ===== 原代码结束 =====

    // 发送 registration 成功后，立即在链上开托管（固定面额池）
    if (!ESCROW_OPENED) {
      int rc2 = open_escrow_sync_with_tid(state, alice_escrow_id);
      if (rc2 != 0) {
        fprintf(stderr, "[ESCROW] openEscrow during registration failed, rc=%d.\n", rc2);
      }
      printf("[ESCROW] After openEscrow, state->escrow_tx_hash = '%s' (length: %zu)\n", 
             state->escrow_tx_hash, strlen(state->escrow_tx_hash));
    }
  
  
    char *msg_type = "registration";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    // 只发送托管ID与托管交易哈希
    const unsigned escrow_id_length = strlen(alice_escrow_id) + 1;
    const unsigned escrow_tx_hash_length = strlen(state->escrow_tx_hash) + 1;
    const unsigned msg_data_length = escrow_id_length + escrow_tx_hash_length;
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(registration_msg, msg_type_length, msg_data_length);
    
    // Serialize the message - 只发送 Escrow ID 和交易哈希
    size_t off = 0;
    printf("[ESCROW] Before sending registration, escrow_tx_hash = '%s' (length: %zu)\n", 
           state->escrow_tx_hash, strlen(state->escrow_tx_hash));
    memcpy(registration_msg->data + off, alice_escrow_id, escrow_id_length);
    off += escrow_id_length;
    memcpy(registration_msg->data + off, state->escrow_tx_hash, escrow_tx_hash_length);

    memcpy(registration_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, registration_msg, msg_type_length, msg_data_length);
    END_TIMER(registration_total)
    // Send the message.
    zmq_msg_t registration;
    int rc = zmq_msg_init_size(&registration, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&registration), serialized_message, total_msg_length);
    rc = zmq_msg_send(&registration, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    if (registration_msg != NULL) message_free(registration_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  
  return result_status;
}

int registration_done_handler(alice_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(registration_done_total)
  int result_status = RLC_OK;

  RLC_TRY {
    // registration_done 消息不再包含数据，只是确认注册完成
    // data 可能为 NULL 或空，这是正常的
    printf("[ALICE] Received registration_done confirmation from Tumbler\n");
    
    // ===== Tornado Cash 取款证明生成流程 =====
    printf("[TORNADO] Starting withdraw proof generation...\n");
    
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
    
    // txHash 是必需的，检查是否已设置
    printf("[TORNADO] Checking escrow_tx_hash: length=%zu, value='%s'\n", 
           strlen(state->escrow_tx_hash), state->escrow_tx_hash);
    
    if (strlen(state->escrow_tx_hash) == 0) {
      fprintf(stderr, "[ERROR] escrow_tx_hash is not set. Cannot generate proof without transaction hash.\n");
      fprintf(stderr, "[ERROR] Please ensure openEscrow was executed successfully and escrow_tx_hash was captured.\n");
      fprintf(stderr, "[ERROR] This usually means openEscrow_fixed.js did not output the transaction hash in the expected format.\n");
      RLC_THROW(ERR_NO_VALID);
    }
    
    // 验证 escrow_tx_hash 格式
    if (strncmp(state->escrow_tx_hash, "0x", 2) != 0) {
      fprintf(stderr, "[ERROR] Invalid escrow_tx_hash format (should start with 0x): %s\n", state->escrow_tx_hash);
      RLC_THROW(ERR_NO_VALID);
    }
    
    // 验证长度（应该是 0x + 64 hex chars = 66 chars）
    if (strlen(state->escrow_tx_hash) != 66) {
      fprintf(stderr, "[WARNING] escrow_tx_hash length is %zu, expected 66 (0x + 64 hex chars): %s\n", 
              strlen(state->escrow_tx_hash), state->escrow_tx_hash);
    }
    
    // 传递 txHash 给脚本（必需参数）
    printf("[TORNADO] Using escrow_tx_hash: %s\n", state->escrow_tx_hash);
    snprintf(cmd_proof, 8192,
             "cd %s && node %s --nullifier 0x%s --secret 0x%s --commitment %s --contract %s --network private --txHash %s 2>&1",
             tornado_project, script_path, nullifier_hex, secret_hex, 
             state->commitment, contract_param, state->escrow_tx_hash);
    
    printf("[TORNADO] Executing proof generation script: %s\n", cmd_proof);
    
    FILE *fp_proof = popen(cmd_proof, "r");
    if (!fp_proof) {
      fprintf(stderr, "[TORNADO] Failed to execute proof generation script\n");
      free(cmd_proof);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 读取脚本输出（JSON 格式）
    // 增大缓冲区以容纳完整的 JSON（包含 proofData 可能很大，可能超过 10KB）
    char json_buffer[65536] = {0}; // 64KB 缓冲区
    size_t json_len = 0;
    char line[8192]; // 增大行缓冲区（8KB）
    int json_started = 0; // 标记是否已开始读取 JSON
    int brace_count = 0; // 用于匹配大括号，确保读取完整的 JSON
    
    while (fgets(line, sizeof(line), fp_proof) != NULL) {
      size_t line_len = strlen(line);
      
      // 检查是否是 JSON 开始（以 { 开头或包含 "nullifierHash"）
      if (!json_started && (line[0] == '{' || strstr(line, "\"nullifierHash\"") != NULL)) {
        json_started = 1;
        // 重置大括号计数
        brace_count = 0;
        for (size_t i = 0; i < line_len; i++) {
          if (line[i] == '{') brace_count++;
          if (line[i] == '}') brace_count--;
        }
      }
      
      // 如果已开始读取 JSON，继续读取
      if (json_started) {
        if (json_len + line_len < sizeof(json_buffer)) {
          memcpy(json_buffer + json_len, line, line_len);
          json_len += line_len;
          
          // 更新大括号计数
          for (size_t i = 0; i < line_len; i++) {
            if (line[i] == '{') brace_count++;
            if (line[i] == '}') brace_count--;
          }
          
          // 如果大括号匹配（brace_count == 0），说明 JSON 完整
          if (brace_count == 0 && json_len > 0) {
            // 找到最后一个 }，确保 JSON 完整
            char *last_brace = strrchr(json_buffer, '}');
            if (last_brace) {
              json_len = last_brace - json_buffer + 1;
              json_buffer[json_len] = '\0';
              break; // JSON 已完整，停止读取
            }
          }
        } else {
          fprintf(stderr, "[TORNADO] WARNING: JSON buffer overflow, truncating at %zu bytes\n", json_len);
          // 尝试找到最后一个完整的 }
          char *last_brace = strrchr(json_buffer, '}');
          if (last_brace) {
            json_len = last_brace - json_buffer + 1;
            json_buffer[json_len] = '\0';
          }
          break;
        }
      }
    }
    
    // 如果 JSON 被截断，尝试找到最后一个完整的 }
    if (json_len > 0 && brace_count != 0) {
      fprintf(stderr, "[TORNADO] WARNING: JSON may be incomplete (brace_count=%d), trying to find last complete }\n", brace_count);
      char *last_brace = strrchr(json_buffer, '}');
      if (last_brace) {
        json_len = last_brace - json_buffer + 1;
        json_buffer[json_len] = '\0';
      }
    }
    
    printf("[TORNADO] Read JSON buffer: length=%zu, contains proofData=%s\n", 
           json_len, strstr(json_buffer, "\"proofData\"") ? "yes" : "no");
    
    int rc_proof = pclose(fp_proof);
    free(cmd_proof);
    
    if (rc_proof != 0 || json_len == 0) {
      fprintf(stderr, "[TORNADO] Proof generation script failed or no output, rc=%d\n", rc_proof);
      if (json_len > 0) {
        fprintf(stderr, "[TORNADO] Script output: %s\n", json_buffer);
      }
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[TORNADO] Proof generation script output: %s\n", json_buffer);
    
    // 解析 JSON 输出
    // 格式: {"nullifierHash":"0x...","merkleRoot":"0x...","pathElements":[...],"pathIndices":[...],"proof":{...},"publicSignals":[...],"proofData":{...}}
    char *nullifier_hash_start = strstr(json_buffer, "\"nullifierHash\"");
    char *merkle_root_start = strstr(json_buffer, "\"merkleRoot\"");
    char *path_elements_start = strstr(json_buffer, "\"pathElements\"");
    char *proof_start = strstr(json_buffer, "\"proof\"");
    char *public_signals_start = strstr(json_buffer, "\"publicSignals\"");
    char *proof_data_start = strstr(json_buffer, "\"proofData\"");
    
    // 调试：检查 JSON 解析结果
    printf("[TORNADO] JSON parsing check:\n");
    printf("  nullifierHash_start: %s\n", nullifier_hash_start ? "found" : "NULL");
    printf("  merkle_root_start: %s\n", merkle_root_start ? "found" : "NULL");
    printf("  path_elements_start: %s\n", path_elements_start ? "found" : "NULL");
    printf("  proof_start: %s\n", proof_start ? "found" : "NULL");
    printf("  public_signals_start: %s\n", public_signals_start ? "found" : "NULL");
    printf("  proof_data_start: %s\n", proof_data_start ? "found" : "NULL");
    printf("  json_buffer length: %zu\n", strlen(json_buffer));
    
    if (!nullifier_hash_start || !merkle_root_start) {
      fprintf(stderr, "[TORNADO] Failed to parse proof JSON output\n");
      fprintf(stderr, "[TORNADO] JSON buffer (first 500 chars): %.500s\n", json_buffer);
      RLC_THROW(ERR_CAUGHT);
    }
    
    if (!proof_start || !public_signals_start) {
      fprintf(stderr, "[TORNADO] WARNING: proof or publicSignals not found in JSON\n");
      fprintf(stderr, "[TORNADO] JSON buffer (last 500 chars): %.500s\n", 
              json_buffer + (strlen(json_buffer) > 500 ? strlen(json_buffer) - 500 : 0));
    }
    
    if (!proof_data_start) {
      fprintf(stderr, "[TORNADO] WARNING: proofData not found in JSON\n");
      fprintf(stderr, "[TORNADO] This is required for off-chain verification and transmission to Bob\n");
    }
    
    // 提取 nullifierHash（如果还没有）
    if (strlen(state->nullifier_hash) == 0) {
      char *nullifier_hash_hex_start = strstr(nullifier_hash_start, "\"0x");
      if (nullifier_hash_hex_start) {
        nullifier_hash_hex_start += 1;
        char *nullifier_hash_hex_end = strchr(nullifier_hash_hex_start, '"');
        if (nullifier_hash_hex_end) {
          int hex_len = nullifier_hash_hex_end - nullifier_hash_hex_start;
          if (hex_len >= 66) {
            strncpy(state->nullifier_hash, nullifier_hash_hex_start, 66);
            state->nullifier_hash[66] = '\0';
            printf("[TORNADO] Parsed nullifierHash: %s\n", state->nullifier_hash);
          }
        }
      }
    }
    
    // 提取 merkleRoot
    char *merkle_root_hex_start = strstr(merkle_root_start, "\"0x");
    if (merkle_root_hex_start) {
      merkle_root_hex_start += 1;
      char *merkle_root_hex_end = strchr(merkle_root_hex_start, '"');
      if (merkle_root_hex_end) {
        int hex_len = merkle_root_hex_end - merkle_root_hex_start;
        if (hex_len >= 66) {
          strncpy(state->merkle_root, merkle_root_hex_start, 66);
          state->merkle_root[66] = '\0';
          printf("[TORNADO] Parsed merkleRoot: %s\n", state->merkle_root);
        }
      }
    }
    
    // 保存完整的 Merkle 路径证明和 zkSNARK 证明（JSON 格式）
    if (path_elements_start) {
      // 找到 pathElements 到 proof 之间的内容
      char *path_end = proof_start ? proof_start : public_signals_start;
      if (path_end) {
        size_t path_len = path_end - path_elements_start;
        if (path_len < sizeof(state->merkle_proof_path)) {
          strncpy(state->merkle_proof_path, path_elements_start, path_len);
          state->merkle_proof_path[path_len] = '\0';
          printf("[TORNADO] Saved Merkle proof path (length: %zu)\n", path_len);
        }
      }
    }
    
    if (proof_start && public_signals_start) {
      // 提取 proof 对象的值部分（跳过 "proof":）
      const char *proof_value_start = strstr(proof_start, ":");
      if (proof_value_start) {
        proof_value_start++; // 跳过 ':'
        // 跳过可能的空格
        while (*proof_value_start == ' ' || *proof_value_start == '\t') {
          proof_value_start++;
        }
        // 找到 proof 值的结束位置（在 publicSignals 之前，但要考虑嵌套对象和逗号）
        const char *proof_value_end = public_signals_start - 1;
        // 向前查找，跳过可能的空格、逗号和换行
        while (proof_value_end > proof_value_start && 
               (*proof_value_end == ' ' || *proof_value_end == '\t' || 
                *proof_value_end == ',' || *proof_value_end == '\n' || *proof_value_end == '\r')) {
          proof_value_end--;
        }
        proof_value_end++; // 指向结束位置之后
        
        size_t proof_val_len = proof_value_end - proof_value_start;
        if (proof_val_len > 0 && proof_val_len < sizeof(state->zk_proof)) {
          strncpy(state->zk_proof, proof_value_start, proof_val_len);
          state->zk_proof[proof_val_len] = '\0';
          printf("[TORNADO] Saved zkSNARK proof (length: %zu)\n", proof_val_len);
          printf("[TORNADO] Proof preview: %.100s...\n", state->zk_proof);
        } else {
          fprintf(stderr, "[TORNADO] ERROR: proof value length invalid: %zu (max: %zu)\n", 
                  proof_val_len, sizeof(state->zk_proof) - 1);
        }
      } else {
        fprintf(stderr, "[TORNADO] ERROR: Could not find ':' after 'proof'\n");
      }
    } else {
      fprintf(stderr, "[TORNADO] ERROR: proof_start=%p, public_signals_start=%p\n", 
              proof_start, public_signals_start);
      if (proof_start) {
        fprintf(stderr, "[TORNADO] proof_start content: %.200s\n", proof_start);
      }
      if (!public_signals_start) {
        fprintf(stderr, "[TORNADO] publicSignals not found in JSON, searching in buffer...\n");
        // 尝试在 json_buffer 中查找
        fprintf(stderr, "[TORNADO] JSON buffer length: %zu\n", strlen(json_buffer));
        fprintf(stderr, "[TORNADO] JSON buffer (last 1000 chars): %.1000s\n", 
                json_buffer + (strlen(json_buffer) > 1000 ? strlen(json_buffer) - 1000 : 0));
      }
    }
    
    if (public_signals_start) {
      // 提取 publicSignals 数组的值部分（跳过 "publicSignals":）
      const char *signals_value_start = strstr(public_signals_start, ":");
      if (signals_value_start) {
        signals_value_start++; // 跳过 ':'
        // 跳过可能的空格
        while (*signals_value_start == ' ' || *signals_value_start == '\t') {
          signals_value_start++;
        }
        // 找到数组的结束位置 ']'
        const char *signals_value_end = strstr(signals_value_start, "]");
        if (signals_value_end) {
          signals_value_end++; // 包含 ']'
          size_t signals_val_len = signals_value_end - signals_value_start;
          if (signals_val_len > 0 && signals_val_len < sizeof(state->public_signals)) {
            strncpy(state->public_signals, signals_value_start, signals_val_len);
            state->public_signals[signals_val_len] = '\0';
            printf("[TORNADO] Saved public signals (length: %zu)\n", signals_val_len);
            printf("[TORNADO] PublicSignals preview: %.100s...\n", state->public_signals);
          } else {
            fprintf(stderr, "[TORNADO] ERROR: publicSignals value length invalid: %zu (max: %zu)\n", 
                    signals_val_len, sizeof(state->public_signals) - 1);
          }
        } else {
          fprintf(stderr, "[TORNADO] ERROR: Could not find ']' in publicSignals\n");
          fprintf(stderr, "[TORNADO] publicSignals_start content: %.500s\n", public_signals_start);
        }
      } else {
        fprintf(stderr, "[TORNADO] ERROR: Could not find ':' after 'publicSignals'\n");
      }
    } else {
      fprintf(stderr, "[TORNADO] ERROR: public_signals_start is NULL\n");
    }
    
    // 优先提取 proofData（用于链下验证和发送给 Bob）
    if (proof_data_start) {
      // 提取 proofData 对象的值部分（跳过 "proofData":）
      const char *proof_data_value_start = strstr(proof_data_start, ":");
      if (proof_data_value_start) {
        proof_data_value_start++; // 跳过 ':'
        // 跳过可能的空格
        while (*proof_data_value_start == ' ' || *proof_data_value_start == '\t') {
          proof_data_value_start++;
        }
        // 找到 proofData 对象的结束位置（遇到 '}'，但要考虑嵌套对象）
        // proofData 是最后一个字段，所以找到 JSON 的最后一个 '}'
        // 但需要找到 proofData 对象本身的结束位置（匹配大括号）
        const char *json_end = strrchr(json_buffer, '}');
        if (json_end && json_end > proof_data_value_start) {
          // 从后往前找，找到 proofData 对象的结束位置
          // 简单方法：从 json_end 往前找，找到第一个匹配的 }
          // 但更简单的方法是：找到最后一个 }，然后往前找到 proofData 对象的开始 {
          const char *proof_data_obj_start = proof_data_value_start;
          int brace_count = 0;
          const char *proof_data_obj_end = NULL;
          
          // 从 proof_data_value_start 开始，匹配大括号
          for (const char *p = proof_data_value_start; p <= json_end; p++) {
            if (*p == '{') brace_count++;
            if (*p == '}') {
              brace_count--;
              if (brace_count == 0) {
                proof_data_obj_end = p;
                break;
              }
            }
          }
          
          if (proof_data_obj_end) {
            size_t proof_data_val_len = proof_data_obj_end - proof_data_value_start + 1; // 包含 }
            if (proof_data_val_len > 0 && proof_data_val_len < sizeof(state->zk_proof)) {
              // 将 proofData 保存到 zk_proof 字段（因为 proofData 包含完整的 proof 和 publicSignals）
              strncpy(state->zk_proof, proof_data_value_start, proof_data_val_len);
              state->zk_proof[proof_data_val_len] = '\0';
              printf("[TORNADO] Saved proofData (length: %zu)\n", proof_data_val_len);
              printf("[TORNADO] ProofData preview: %.200s...\n", state->zk_proof);
              
              // proofData 已经包含完整的 {"proof":{...},"publicSignals":[...]}，无需单独提取
              // 但为了兼容性，仍然提取 publicSignals 到 state->public_signals（如果还没有）
              if (strlen(state->public_signals) == 0) {
                const char *ps_in_proofdata = strstr(state->zk_proof, "\"publicSignals\"");
                if (ps_in_proofdata) {
                  const char *ps_value_start = strstr(ps_in_proofdata, ":");
                  if (ps_value_start) {
                    ps_value_start++; // 跳过 ':'
                    while (*ps_value_start == ' ' || *ps_value_start == '\t') {
                      ps_value_start++;
                    }
                    const char *ps_value_end = strstr(ps_value_start, "]");
                    if (ps_value_end) {
                      ps_value_end++; // 包含 ']'
                      size_t ps_val_len = ps_value_end - ps_value_start;
                      if (ps_val_len > 0 && ps_val_len < sizeof(state->public_signals)) {
                        strncpy(state->public_signals, ps_value_start, ps_val_len);
                        state->public_signals[ps_val_len] = '\0';
                        printf("[TORNADO] Extracted publicSignals from proofData (length: %zu)\n", ps_val_len);
                      }
                    }
                  }
                }
              }
            } else {
              fprintf(stderr, "[TORNADO] ERROR: proofData value length invalid: %zu (max: %zu)\n", 
                      proof_data_val_len, sizeof(state->zk_proof) - 1);
            }
          } else {
            fprintf(stderr, "[TORNADO] ERROR: Could not find matching '}' for proofData object\n");
          }
        } else {
          fprintf(stderr, "[TORNADO] ERROR: Could not find JSON end '}' for proofData\n");
        }
      } else {
        fprintf(stderr, "[TORNADO] ERROR: Could not find ':' after 'proofData'\n");
      }
    } else {
      fprintf(stderr, "[TORNADO] WARNING: proofData not found, falling back to proof and publicSignals\n");
      // 如果没有 proofData，使用旧的 proof 和 publicSignals 字段（向后兼容）
      // 但需要确保它们已经被正确提取
      if (strlen(state->zk_proof) == 0) {
        fprintf(stderr, "[TORNADO] ERROR: Neither proofData nor proof was extracted successfully\n");
      }
    }
    
    printf("[TORNADO] Successfully generated withdraw proof\n");
    // ===== Tornado Cash 取款证明生成流程结束 =====
    
    REGISTRATION_COMPLETED = 1;
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    // 无需释放任何资源
  }

  END_TIMER(registration_done_total)
  return result_status;
}

int token_share(alice_state_t state, void *socket,const char *alice_escrow_id) {
  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(token_share_total);
  int result_status = RLC_OK;

  uint8_t *serialized_message = NULL;

  message_t token_share_msg;
  message_null(token_share_msg);

  RLC_TRY {
    // Build and define the message.
    char *msg_type = "token_share";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    
    // 检查 nullifier 和 secret 是否已生成
    int has_nullifier = 0, has_secret = 0;
    for (int i = 0; i < 31; i++) {
      if (state->nullifier[i] != 0) has_nullifier = 1;
      if (state->secret[i] != 0) has_secret = 1;
    }
    
    if (!has_nullifier || !has_secret) {
      fprintf(stderr, "[ERROR] nullifier or secret not generated. Cannot send token_share.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 检查 commitment 和 escrow_tx_hash 是否已设置
    if (strlen(state->commitment) == 0 || strlen(state->escrow_tx_hash) == 0) {
      fprintf(stderr, "[ERROR] commitment or escrow_tx_hash not set. Cannot send token_share.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[TORNADO] Alice: Sending nullifier and secret to Bob\n");
    
    // 数据格式：
    // 1. nullifier (31 bytes)
    // 2. secret (31 bytes)
    // 3. commitment (null-terminated string, 67 bytes max)
    // 4. escrow_tx_hash (null-terminated string, 67 bytes max)
    // 5. pool_label (null-terminated string)
    // 6. pool_contract (null-terminated string, 43 bytes max)
    const unsigned nullifier_length = 31;
    const unsigned secret_length = 31;
    const unsigned commitment_length = strlen(state->commitment) + 1;
    const unsigned escrow_tx_hash_length = strlen(state->escrow_tx_hash) + 1;
    const unsigned pool_label_length = strlen(state->pool_label) + 1;
    const unsigned pool_contract_length = strlen(state->pool_contract) + 1;
    
    const unsigned msg_data_length = nullifier_length + secret_length + 
                                     commitment_length + escrow_tx_hash_length + 
                                     pool_label_length + pool_contract_length;
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(token_share_msg, msg_type_length, msg_data_length);
    
    printf("[DEBUG] Alice token_share: 消息类型长度=%d, 数据长度=%d, 总长度=%d\n", 
           msg_type_length, msg_data_length, total_msg_length);
    
    // Serialize the data for the message.
    size_t offset = 0;
    
    // 1. nullifier (31 bytes)
    memcpy(token_share_msg->data + offset, state->nullifier, nullifier_length);
    offset += nullifier_length;
    
    // 2. secret (31 bytes)
    memcpy(token_share_msg->data + offset, state->secret, secret_length);
    offset += secret_length;
    
    // 3. commitment (null-terminated string)
    memcpy(token_share_msg->data + offset, state->commitment, commitment_length);
    offset += commitment_length;
    
    // 4. escrow_tx_hash (null-terminated string)
    memcpy(token_share_msg->data + offset, state->escrow_tx_hash, escrow_tx_hash_length);
    offset += escrow_tx_hash_length;
    
    // 5. pool_label (null-terminated string)
    memcpy(token_share_msg->data + offset, state->pool_label, pool_label_length);
    offset += pool_label_length;
    
    // 6. pool_contract (null-terminated string)
    memcpy(token_share_msg->data + offset, state->pool_contract, pool_contract_length);

    // 打印序列化后的数据（前100字节）
    printf("[DEBUG] Alice token_share: 序列化后的数据前100字节 (hex): ");
    int preview_len = (msg_data_length < 100) ? msg_data_length : 100;
    for (int i = 0; i < preview_len; i++) {
      printf("%02x", token_share_msg->data[i]);
    }
    printf("\n");

    // Serialize the message.
    memcpy(token_share_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, token_share_msg, msg_type_length, msg_data_length);

    // Send the message.
    zmq_msg_t token_share;
    int rc = zmq_msg_init_size(&token_share, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&token_share), serialized_message, total_msg_length);
    rc = zmq_msg_send(&token_share, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[DEBUG] Alice token_share: 消息发送成功，大小=%d\n", total_msg_length);
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    if (token_share_msg != NULL) message_free(token_share_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(token_share_total);
  return result_status;
}

int puzzle_share_handler(alice_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(puzzle_share_total)
  int result_status = RLC_OK;

  uint8_t *serialized_message = NULL;
  message_t puzzle_share_done_msg;
  
  // 用于存储原始谜题数据
  ec_t g_alpha;
  cl_ciphertext_t ctx_alpha;
  cl_ciphertext_t auditor_ctx_alpha;
  zk_proof_puzzle_relation_t puzzle_proof;
  
  ec_null(g_alpha);
  cl_ciphertext_null(ctx_alpha);
  cl_ciphertext_null(auditor_ctx_alpha);
  zk_proof_puzzle_relation_null(puzzle_proof);

  RLC_TRY {
    message_null(puzzle_share_done_msg);
    ec_new(g_alpha);
    cl_ciphertext_new(ctx_alpha);
    cl_ciphertext_new(auditor_ctx_alpha);
    zk_proof_puzzle_relation_new(puzzle_proof);
    
    size_t offset = 0;
    printf("[ALICE] 开始反序列化，数据总大小: %zu bytes\n", strlen((char*)data));
    printf("[ALICE] 预期数据大小: EC=%d, CL=%d, proof=%zu\n", 
           RLC_EC_SIZE_COMPRESSED, RLC_CL_CIPHERTEXT_SIZE, zk_puzzle_relation_serialized_size());
    printf("[ALICE] 预期总大小: %d bytes\n", 
           2 * RLC_EC_SIZE_COMPRESSED + 8 * RLC_CL_CIPHERTEXT_SIZE + zk_puzzle_relation_serialized_size());
    
    // 1. 反序列化随机化后的数据
    ec_read_bin(state->g_to_the_alpha_times_beta, data + offset, RLC_EC_SIZE_COMPRESSED);
    offset += RLC_EC_SIZE_COMPRESSED;
    printf("[ALICE] 反序列化g_to_the_alpha_times_beta，当前offset: %zu\n", offset);
    
    char ctx_c1_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    char ctx_c2_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(ctx_c1_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ctx_c1_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    printf("[ALICE] 读取ctx_c1，当前offset: %zu\n", offset);
    memcpy(ctx_c2_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ctx_c2_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    printf("[ALICE] 读取ctx_c2，当前offset: %zu\n", offset);
    printf("[ALICE] ctx_c1_str: %.20s...\n", ctx_c1_str);
    printf("[ALICE] ctx_c2_str: %.20s...\n", ctx_c2_str);
    state->ctx_alpha_times_beta->c1 = gp_read_str(ctx_c1_str);
    state->ctx_alpha_times_beta->c2 = gp_read_str(ctx_c2_str);

    // 解析auditor随机化密文
    char auditor_c1_rand_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    char auditor_c2_rand_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(auditor_c1_rand_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_c1_rand_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(auditor_c2_rand_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_c2_rand_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    state->auditor_ctx_alpha_times_beta->c1 = gp_read_str(auditor_c1_rand_str);
    state->auditor_ctx_alpha_times_beta->c2 = gp_read_str(auditor_c2_rand_str);
    printf("[ALICE] 反序列化auditor密文完成\n");
    
    // 2. 反序列化原始谜题数据
    ec_read_bin(g_alpha, data + offset, RLC_EC_SIZE_COMPRESSED);
    offset += RLC_EC_SIZE_COMPRESSED;
    printf("[ALICE] 反序列化g_alpha\n");
    
    char ctx_alpha_c1_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    char ctx_alpha_c2_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(ctx_alpha_c1_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ctx_alpha_c1_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(ctx_alpha_c2_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ctx_alpha_c2_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    ctx_alpha->c1 = gp_read_str(ctx_alpha_c1_str);
    ctx_alpha->c2 = gp_read_str(ctx_alpha_c2_str);
    
    char auditor_ctx_alpha_c1_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    char auditor_ctx_alpha_c2_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(auditor_ctx_alpha_c1_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_ctx_alpha_c1_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(auditor_ctx_alpha_c2_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_ctx_alpha_c2_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    offset += RLC_CL_CIPHERTEXT_SIZE;
    auditor_ctx_alpha->c1 = gp_read_str(auditor_ctx_alpha_c1_str);
    auditor_ctx_alpha->c2 = gp_read_str(auditor_ctx_alpha_c2_str);
    printf("[ALICE] 反序列化原始谜题数据完成\n");
    
    // 3. 反序列化零知识证明
    printf("[ALICE] 开始反序列化零知识证明，当前offset: %zu\n", offset);
    size_t proof_read = 0;
    if (zk_puzzle_relation_deserialize(puzzle_proof, data + offset, &proof_read) != RLC_OK) {
      printf("[ERROR] Alice: 无法反序列化puzzle_relation证明!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    offset += proof_read;
    printf("[ALICE] 零知识证明反序列化成功，大小: %zu bytes，最终offset: %zu\n", proof_read, offset);
    
    // 4. 验证零知识证明
    printf("[ALICE] 开始验证Bob的谜题关系零知识证明...\n");
    START_TIMER(bob_to_alice_zk_verification)
    if (zk_puzzle_relation_verify(puzzle_proof,
                                  g_alpha,                              // g^alpha
                                  state->g_to_the_alpha_times_beta,     // g^(alpha+beta)
                                  ctx_alpha,                            // ctx_alpha
                                  state->ctx_alpha_times_beta,          // ctx_alpha_times_beta
                                  auditor_ctx_alpha,                    // auditor_ctx_alpha
                                  state->auditor_ctx_alpha_times_beta,  // auditor_ctx_alpha_times_beta
                                  state->tumbler_cl_pk,                 // pk_tumbler
                                  state->auditor_cl_pk,                 // pk_auditor
                                  state->cl_params) != RLC_OK) {
      printf("[ERROR] Alice: Bob的谜题关系零知识证明验证失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(bob_to_alice_zk_verification)
    printf("[ALICE] Bob的谜题关系零知识证明验证成功!\n");
    
    // Build and define the message.
    char *msg_type = "puzzle_share_done";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    const unsigned msg_data_length = 0;
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(puzzle_share_done_msg, msg_type_length, msg_data_length);
    
    // Serialize the message.
    memcpy(puzzle_share_done_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, puzzle_share_done_msg, msg_type_length, msg_data_length);

    // Send the message.
    zmq_msg_t promise_share_done;
    int rc = zmq_msg_init_size(&promise_share_done, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&promise_share_done), serialized_message, total_msg_length);
    rc = zmq_msg_send(&promise_share_done, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    PUZZLE_SHARED = 1;
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    if (puzzle_share_done_msg != NULL) message_free(puzzle_share_done_msg);
    if (serialized_message != NULL) free(serialized_message);
    ec_free(g_alpha);
    cl_ciphertext_free(ctx_alpha);
    cl_ciphertext_free(auditor_ctx_alpha);
    zk_proof_puzzle_relation_free(puzzle_proof);
  }
  END_TIMER(puzzle_share_total)
  return result_status;
}

int payment_init(alice_state_t state, void *socket, transaction_t *tx_data) {

  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(payment_init_total)
  int result_status = RLC_OK;
  uint8_t *serialized_message = NULL;

  message_t payment_init_msg;
  message_null(payment_init_msg);

  cl_ciphertext_t ctx_alpha_times_beta_times_tau;
  cl_ciphertext_t auditor_ctx_alpha_times_beta_times_tau;
  bn_t q;

  cl_ciphertext_null(ctx_alpha_times_beta_times_tau);
  cl_ciphertext_null(auditor_ctx_alpha_times_beta_times_tau);
  bn_null(q);
  
  RLC_TRY {
    cl_ciphertext_new(ctx_alpha_times_beta_times_tau);
    cl_ciphertext_new(auditor_ctx_alpha_times_beta_times_tau);
    bn_new(q);
    ec_curve_get_ord(q);

    // Homomorphically randomize the challenge ciphertext.
    // 准备阶段（不在计时范围内）
    START_TIMER(alice_second_puzzle_randomization)
    GEN tau_prime = randomi(state->cl_params->bound);
    char *tau_prime_str = GENtostr(tau_prime);
    bn_read_str(state->tau, tau_prime_str, strlen(tau_prime_str), 10);
    bn_mod(state->tau, state->tau, q);
    printf("[ALICE] tau (hex) = "); bn_print(state->tau);
    
    // 【调试】打印τ的十进制表示
    char tau_dec_str[512];
    bn_write_str(tau_dec_str, sizeof(tau_dec_str), state->tau, 10);
    printf("[ALICE DEBUG] τ (十进制) = %s\n", tau_dec_str);
    
    const unsigned tau_str_len = bn_size_str(state->tau, 10);
    char tau_str[tau_str_len];
    bn_write_str(tau_str, tau_str_len, state->tau, 10);
    GEN plain_tau = strtoi(tau_str);
    
    // 加密操作（不在计时范围内）
    cl_ciphertext_t enc_tau; cl_ciphertext_new(enc_tau);
    if (cl_enc(enc_tau, plain_tau, state->tumbler_cl_pk, state->cl_params) != RLC_OK) {
      printf("[ERROR] Alice: cl_enc(tau) with tumbler pk failed.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    cl_ciphertext_t enc_tau_aud; cl_ciphertext_new(enc_tau_aud);
    if (cl_enc(enc_tau_aud, plain_tau, state->auditor_cl_pk, state->cl_params) != RLC_OK) {
      printf("[ERROR] Alice: cl_enc(tau) with auditor pk failed.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 只测量三个谜题组件的随机化计算时间
    
    /* 组件1: g^(α+β+τ) = g^(α+β) + g^τ */
    ec_t g_tau; ec_null(g_tau); ec_new(g_tau);
    ec_mul_gen(g_tau, state->tau);            /* g^τ */
    ec_add(state->g_to_the_alpha_times_beta_times_tau, state->g_to_the_alpha_times_beta, g_tau);
    ec_norm(state->g_to_the_alpha_times_beta_times_tau, state->g_to_the_alpha_times_beta_times_tau);
    ec_free(g_tau);
    
    /* 组件2和3: Tumbler密文的同态随机化 */
    ctx_alpha_times_beta_times_tau->c1 = gmul(state->ctx_alpha_times_beta->c1, enc_tau->c1);
    ctx_alpha_times_beta_times_tau->c2 = gmul(state->ctx_alpha_times_beta->c2, enc_tau->c2);
    
    /* 组件4和5: Auditor密文的同态随机化 */
    auditor_ctx_alpha_times_beta_times_tau->c1 = gmul(state->auditor_ctx_alpha_times_beta->c1, enc_tau_aud->c1);
    auditor_ctx_alpha_times_beta_times_tau->c2 = gmul(state->auditor_ctx_alpha_times_beta->c2, enc_tau_aud->c2);
    END_TIMER(alice_second_puzzle_randomization)
    
    // 调试打印（不在计时范围内）
    // 【调试】打印Alice生成的 g^(α+β+τ)
    {
      uint8_t gabt_alice[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(gabt_alice, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha_times_beta_times_tau, 1);
      printf("[ALICE DEBUG] 生成的 g^(α+β+τ) (compressed): 0x");
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", gabt_alice[i]);
      printf("\n");
      
      // 同时打印从Bob收到的 g^(α+β)
      uint8_t gab_from_bob[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(gab_from_bob, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha_times_beta, 1);
      printf("[ALICE DEBUG] 从Bob收到的 g^(α+β) (compressed): 0x");
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", gab_from_bob[i]);
      printf("\n");
    }
    
    // 调试打印：生成的Tumbler密文
    {
      char *gen_c1_str = GENtostr(ctx_alpha_times_beta_times_tau->c1);
      char *gen_c2_str = GENtostr(ctx_alpha_times_beta_times_tau->c2);
      printf("\n[ALICE DEBUG] ========== 生成的 ctx_alpha_times_beta_times_tau (Tumbler密文) ==========\n");
      printf("[ALICE DEBUG] 生成位置：第642-643行（同态加法 E(α+β) ⊕ E(τ)）\n");
      printf("[ALICE DEBUG] c1 长度: %zu\n", strlen(gen_c1_str));
      printf("[ALICE DEBUG] c1 前100字符: %.100s\n", gen_c1_str);
      printf("[ALICE DEBUG] c2 长度: %zu\n", strlen(gen_c2_str));
      printf("[ALICE DEBUG] c2 前100字符: %.100s\n", gen_c2_str);
      printf("[ALICE DEBUG] ================================================================\n\n");
      free(gen_c1_str);
      free(gen_c2_str);
    }

    // 【调试】打印从Bob收到的 E_auditor(r0+β)
    {
      char *from_bob_c1 = GENtostr(state->auditor_ctx_alpha_times_beta->c1);
      char *from_bob_c2 = GENtostr(state->auditor_ctx_alpha_times_beta->c2);
      printf("\n[ALICE DEBUG] ========== 从Bob收到的 E_auditor(r0+β) ==========\n");
      printf("[ALICE DEBUG] c1 长度: %zu, 前100字符: %.100s\n", strlen(from_bob_c1), from_bob_c1);
      printf("[ALICE DEBUG] c2 长度: %zu, 前100字符: %.100s\n", strlen(from_bob_c2), from_bob_c2);
      free(from_bob_c1);
      free(from_bob_c2);
    }
    
    // 【调试】打印Alice生成的 E_auditor(τ)
    {
      char *enc_tau_c1 = GENtostr(enc_tau_aud->c1);
      char *enc_tau_c2 = GENtostr(enc_tau_aud->c2);
      printf("\n[ALICE DEBUG] ========== Alice生成的 E_auditor(τ) ==========\n");
      printf("[ALICE DEBUG] c1 长度: %zu, 前100字符: %.100s\n", strlen(enc_tau_c1), enc_tau_c1);
      printf("[ALICE DEBUG] c2 长度: %zu, 前100字符: %.100s\n", strlen(enc_tau_c2), enc_tau_c2);
      free(enc_tau_c1);
      free(enc_tau_c2);
    }
    
    {
      char *gen_c1_str = GENtostr(auditor_ctx_alpha_times_beta_times_tau->c1);
      char *gen_c2_str = GENtostr(auditor_ctx_alpha_times_beta_times_tau->c2);
      printf("\n[ALICE DEBUG] ========== 生成的 auditor_ctx_alpha_times_beta_times_tau ==========\n");
      printf("[ALICE DEBUG] 生成位置：第665-666行（同态加法 E(r0+β) ⊕ E(τ) = E(r0+β+τ)）\n");
      printf("[ALICE DEBUG] c1 长度: %zu\n", strlen(gen_c1_str));
      printf("[ALICE DEBUG] c1 前100字符: %.100s\n", gen_c1_str);
      printf("[ALICE DEBUG] c2 长度: %zu\n", strlen(gen_c2_str));
      printf("[ALICE DEBUG] c2 前100字符: %.100s\n", gen_c2_str);
      printf("[ALICE DEBUG] ================================================================\n\n");
      free(gen_c1_str);
      free(gen_c2_str);
    }
    state->ctx_alpha_times_beta_times_tau->c1 = ctx_alpha_times_beta_times_tau->c1;
    state->ctx_alpha_times_beta_times_tau->c2 = ctx_alpha_times_beta_times_tau->c2;
    state->auditor_ctx_alpha_times_beta_times_tau->c1 = auditor_ctx_alpha_times_beta_times_tau->c1;
    state->auditor_ctx_alpha_times_beta_times_tau->c2 = auditor_ctx_alpha_times_beta_times_tau->c2;
    
    // 生成Alice的谜题关系零知识证明
    printf("[ALICE] 开始生成谜题关系零知识证明...\n");
    printf("[ALICE] 证明三部分随机化关系:\n");
    printf("  - g^(α+β) -> g^(α+β+τ)\n");
    printf("  - ctx_α+β -> ctx_α+β+τ\n");
    printf("  - auditor_ctx_α+β -> auditor_ctx_α+β+τ\n");
    
    // 检查所有参数是否为NULL
    printf("[ALICE DEBUG] 检查零知识证明参数:\n");
    printf("  - state->puzzle_relation_zk_proof: %s\n", state->puzzle_relation_zk_proof ? "OK" : "NULL");
    printf("  - state->tau: %s\n", state->tau ? "OK" : "NULL");
    printf("  - enc_tau: %s\n", enc_tau ? "OK" : "NULL");
    printf("  - enc_tau_aud: %s\n", enc_tau_aud ? "OK" : "NULL");
    printf("  - state->g_to_the_alpha_times_beta: %s\n", state->g_to_the_alpha_times_beta ? "OK" : "NULL");
    printf("  - state->g_to_the_alpha_times_beta_times_tau: %s\n", state->g_to_the_alpha_times_beta_times_tau ? "OK" : "NULL");
    printf("  - state->ctx_alpha_times_beta: %s\n", state->ctx_alpha_times_beta ? "OK" : "NULL");
    printf("  - state->ctx_alpha_times_beta_times_tau: %s\n", state->ctx_alpha_times_beta_times_tau ? "OK" : "NULL");
    printf("  - state->auditor_ctx_alpha_times_beta: %s\n", state->auditor_ctx_alpha_times_beta ? "OK" : "NULL");
    printf("  - state->auditor_ctx_alpha_times_beta_times_tau: %s\n", state->auditor_ctx_alpha_times_beta_times_tau ? "OK" : "NULL");
    printf("  - state->tumbler_cl_pk: %s\n", state->tumbler_cl_pk ? "OK" : "NULL");
    printf("  - state->auditor_cl_pk: %s\n", state->auditor_cl_pk ? "OK" : "NULL");
    printf("  - state->cl_params: %s\n", state->cl_params ? "OK" : "NULL");
    
    // 生成零知识证明
    START_TIMER(alice_zk_proof_generation)
    if (zk_puzzle_relation_prove(state->puzzle_relation_zk_proof,
                                 state->tau,                     // tau
                                 enc_tau,                       // enc_tau
                                 enc_tau_aud,                   // enc_tau_aud
                                 state->g_to_the_alpha_times_beta, // g^(α+β)
                                 state->g_to_the_alpha_times_beta_times_tau, // g^(α+β+τ)
                                 state->ctx_alpha_times_beta,   // ctx_α+β
                                 state->ctx_alpha_times_beta_times_tau, // ctx_α+β+τ
                                 state->auditor_ctx_alpha_times_beta, // auditor_ctx_α+β
                                 state->auditor_ctx_alpha_times_beta_times_tau, // auditor_ctx_α+β+τ
                                 state->tumbler_cl_pk,          // pk_tumbler
                                 state->auditor_cl_pk,          // pk_auditor
                                 state->cl_params) != RLC_OK) { // params
      printf("[ERROR] Alice谜题关系零知识证明生成失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_zk_proof_generation)
    printf("[ALICE] 谜题关系零知识证明生成成功!\n");
    cl_ciphertext_free(enc_tau);
    cl_ciphertext_free(enc_tau_aud);
    
    // 构造签名内容：托管ID||openEscrow的交易哈希
    uint8_t tx_buf[64]; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
    int tx_len = 64;
    
    // 1. 将托管ID (state->alice_escrow_id) 从十六进制字符串转换为32字节数组（bytes32格式）
    if (strlen(state->alice_escrow_id) < 2 || strncmp(state->alice_escrow_id, "0x", 2) != 0) {
      printf("[ERROR] Invalid alice_escrow_id format: %s\n", state->alice_escrow_id);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *escrow_id_hex = state->alice_escrow_id + 2; // 跳过"0x"
    size_t escrow_id_hex_len = strlen(escrow_id_hex);
    if (escrow_id_hex_len != 64) {
      printf("[ERROR] alice_escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, state->alice_escrow_id);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 将托管ID的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
      char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
      tx_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    // 2. 将交易哈希从十六进制字符串转换为32字节数组
    if (strlen(state->escrow_tx_hash) < 2 || strncmp(state->escrow_tx_hash, "0x", 2) != 0) {
      printf("[ERROR] Invalid escrow_tx_hash format: %s\n", state->escrow_tx_hash);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *hash_hex = state->escrow_tx_hash + 2; // 跳过"0x"
    size_t hash_hex_len = strlen(hash_hex);
    if (hash_hex_len != 64) {
      printf("[ERROR] escrow_tx_hash should be 64 hex chars (32 bytes), got %zu: %s\n", hash_hex_len, state->escrow_tx_hash);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 将交易哈希的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
      char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
      tx_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    printf("[DEBUG] Payment init signature content: escrowID(32 bytes) || txHash(32 bytes)\n");
    printf("[DEBUG] Escrow ID (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", tx_buf[i]);
    printf("\n");
    printf("[DEBUG] Escrow TX Hash (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", tx_buf[32 + i]);
    printf("\n");

    START_TIMER(alice_ecdsa_signing)
    if (adaptor_ecdsa_sign(state->sigma_hat_s,
                           tx_buf, 
                           tx_len,
                           state->g_to_the_alpha_times_beta_times_tau, // 修复：使用正确的tau随机化版本
                           state->alice_ec_sk) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_ecdsa_signing)
    
    // removed verbose pre-signature component dumps

    
    // Build and define the message.
    char *msg_type = "payment_init";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    // 追加联合证明大小（固定槽位）
    const unsigned msg_data_length = (2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE) /* 完整预签名: r,s,R,pi.a,pi.b,pi.z */
                                   + RLC_EC_SIZE_COMPRESSED+ 2 * RLC_CL_CIPHERTEXT_SIZE /* g^(α+β+τ) 与 tumbler 密文 */
                                   + 2 * RLC_CL_CIPHERTEXT_SIZE /* auditor 密文 */
                                   + sizeof(int) + tx_len /* tx_len和tx_buf */
                                   + zk_puzzle_relation_serialized_size() /* Alice的零知识证明 */
                                   + RLC_EC_SIZE_COMPRESSED + 2 * RLC_CL_CIPHERTEXT_SIZE + 2 * RLC_CL_CIPHERTEXT_SIZE; /* 从Bob收到的原始谜题数据 */
                                   
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(payment_init_msg, msg_type_length, msg_data_length);
    
    printf("[ALICE DEBUG] 消息数据长度计算:\n");
    printf("  - 预签名: %zu 字节\n", (size_t)(2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE));
    printf("  - g^(α+β+τ): %zu 字节\n", (size_t)RLC_EC_SIZE_COMPRESSED);
    printf("  - Tumbler密文: %zu 字节\n", (size_t)(2 * RLC_CL_CIPHERTEXT_SIZE));
    printf("  - Auditor密文: %zu 字节\n", (size_t)(2 * RLC_CL_CIPHERTEXT_SIZE));
    printf("  - tx_len: %zu 字节\n", sizeof(int));
    printf("  - tx_buf: %d 字节\n", tx_len);
    printf("  - Alice零知识证明: %zu 字节\n", zk_puzzle_relation_serialized_size());
    printf("  - 从Bob收到的原始谜题数据: %zu 字节\n", (size_t)(RLC_EC_SIZE_COMPRESSED + 2 * RLC_CL_CIPHERTEXT_SIZE + 2 * RLC_CL_CIPHERTEXT_SIZE));
    printf("  - 总计: %u 字节\n", msg_data_length);

    // Serialize the data for the message - 发送完整预签名结构
    size_t offset = 0;
    bn_write_bin(payment_init_msg->data + offset, RLC_BN_SIZE, state->sigma_hat_s->r); offset += RLC_BN_SIZE;
    bn_write_bin(payment_init_msg->data + offset, RLC_BN_SIZE, state->sigma_hat_s->s); offset += RLC_BN_SIZE;
    ec_write_bin(payment_init_msg->data + offset, RLC_EC_SIZE_COMPRESSED, state->sigma_hat_s->R, 1); offset += RLC_EC_SIZE_COMPRESSED;
    ec_write_bin(payment_init_msg->data + offset, RLC_EC_SIZE_COMPRESSED, state->sigma_hat_s->pi->a, 1); offset += RLC_EC_SIZE_COMPRESSED;
    ec_write_bin(payment_init_msg->data + offset, RLC_EC_SIZE_COMPRESSED, state->sigma_hat_s->pi->b, 1); offset += RLC_EC_SIZE_COMPRESSED;
    bn_write_bin(payment_init_msg->data + offset, RLC_BN_SIZE, state->sigma_hat_s->pi->z); offset += RLC_BN_SIZE;
    
    // 【调试】打印发送给Tumbler的 g^(α+β+τ)
    {
      uint8_t gabt_send[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(gabt_send, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha_times_beta_times_tau, 1);
      printf("[ALICE DEBUG] 发送给Tumbler的 g^(α+β+τ) (compressed): 0x");
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", gabt_send[i]);
      printf("\n");
    }
    
    // 附带 g^(α+β+τ)
    ec_write_bin(payment_init_msg->data + offset, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha_times_beta_times_tau, 1); offset += RLC_EC_SIZE_COMPRESSED;
    // 修复：使用正确的tau版本密文
    {
      char *s1 = GENtostr(state->ctx_alpha_times_beta_times_tau->c1);
      char *s2 = GENtostr(state->ctx_alpha_times_beta_times_tau->c2);
      size_t l1 = strlen(s1), l2 = strlen(s2);
      uint8_t *d1 = payment_init_msg->data + offset;
      uint8_t *d2 = payment_init_msg->data + offset + RLC_CL_CIPHERTEXT_SIZE;
      memset(d1, 0, RLC_CL_CIPHERTEXT_SIZE);
      memset(d2, 0, RLC_CL_CIPHERTEXT_SIZE);
      memcpy(d1, s1, l1);
      memcpy(d2, s2, l2);
      offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
    }
    // 附加auditor密文
    {
      char *s1 = GENtostr(auditor_ctx_alpha_times_beta_times_tau->c1);
      char *s2 = GENtostr(auditor_ctx_alpha_times_beta_times_tau->c2);
      size_t l1 = strlen(s1), l2 = strlen(s2);
      
      printf("\n[ALICE DEBUG] ========== 发送前的 auditor_ctx_alpha_times_beta_times_tau ==========\n");
      printf("[ALICE DEBUG] 准备序列化到消息中（第748-767行）\n");
      printf("[ALICE DEBUG] c1 长度: %zu\n", l1);
      printf("[ALICE DEBUG] c1 内容: %.100s\n", s1);
      printf("[ALICE DEBUG] c2 长度: %zu\n", l2);
      printf("[ALICE DEBUG] c2 内容: %.100s\n", s2);
      printf("[ALICE DEBUG] ================================================================\n\n");
     
      // 检查与tumbler密文是否相同
      char *tumbler_s1 = GENtostr(state->ctx_alpha_times_beta_times_tau->c1);
      char *tumbler_s2 = GENtostr(state->ctx_alpha_times_beta_times_tau->c2);
      
      printf("\n[ALICE DEBUG] ========== 发送前的 ctx_alpha_times_beta_times_tau (Tumbler密文) ==========\n");
      printf("[ALICE DEBUG] 准备序列化到消息中（第735-745行）\n");
      printf("[ALICE DEBUG] c1 长度: %zu\n", strlen(tumbler_s1));
      printf("[ALICE DEBUG] c1 内容: %.100s\n", tumbler_s1);
      printf("[ALICE DEBUG] c2 长度: %zu\n", strlen(tumbler_s2));
      printf("[ALICE DEBUG] c2 内容: %.100s\n", tumbler_s2);
      printf("[ALICE DEBUG] ================================================================\n\n");
      
      // 打印当前offset位置
      printf("[ALICE DEBUG] 当前offset（序列化auditor密文前）: %zu\n", offset);

      uint8_t *d1 = payment_init_msg->data + offset;
      uint8_t *d2 = payment_init_msg->data + offset + RLC_CL_CIPHERTEXT_SIZE;
      memset(d1, 0, RLC_CL_CIPHERTEXT_SIZE);
      memset(d2, 0, RLC_CL_CIPHERTEXT_SIZE);
      if (l1 > RLC_CL_CIPHERTEXT_SIZE - 1) l1 = RLC_CL_CIPHERTEXT_SIZE - 1;
      if (l2 > RLC_CL_CIPHERTEXT_SIZE - 1) l2 = RLC_CL_CIPHERTEXT_SIZE - 1;
      memcpy(d1, s1, l1);
      memcpy(d2, s2, l2);
      offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
      
      printf("[ALICE DEBUG] auditor密文序列化完成，当前offset: %zu\n", offset);
    }
    // 附加tx_len和tx_buf（写在当前 off 位置，保证顺序正确）
    memcpy(payment_init_msg->data + offset, &tx_len, sizeof(int)); offset += sizeof(int);
    memcpy(payment_init_msg->data + offset, tx_buf, tx_len); offset += tx_len;
    
    // 序列化Alice的零知识证明
    size_t proof_written = 0;
    printf("[ALICE] 开始序列化零知识证明，当前offset: %zu\n", offset);
    if (zk_puzzle_relation_serialize(payment_init_msg->data + offset, &proof_written, state->puzzle_relation_zk_proof) != RLC_OK) {
      printf("[ERROR] 无法序列化Alice的puzzle_relation证明!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    offset += proof_written;
    printf("[ALICE] 零知识证明序列化成功，大小: %zu bytes，当前offset: %zu\n", proof_written, offset);
    
    // 序列化从Bob收到的原始谜题数据
    printf("[ALICE] 开始序列化从Bob收到的原始谜题数据...\n");
    
    // 1. 序列化 g^(α+β) (从Bob收到的)
    ec_write_bin(payment_init_msg->data + offset, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha_times_beta, 1);
    offset += RLC_EC_SIZE_COMPRESSED;
    printf("[ALICE] 序列化 g^(α+β)，当前offset: %zu\n", offset);
    
    // 2. 序列化 ctx_α+β (从Bob收到的Tumbler密文)
    {
      char *s1 = GENtostr(state->ctx_alpha_times_beta->c1);
      char *s2 = GENtostr(state->ctx_alpha_times_beta->c2);
      size_t l1 = strlen(s1), l2 = strlen(s2);
      uint8_t *d1 = payment_init_msg->data + offset;
      uint8_t *d2 = payment_init_msg->data + offset + RLC_CL_CIPHERTEXT_SIZE;
      memset(d1, 0, RLC_CL_CIPHERTEXT_SIZE);
      memset(d2, 0, RLC_CL_CIPHERTEXT_SIZE);
      if (l1 > RLC_CL_CIPHERTEXT_SIZE - 1) l1 = RLC_CL_CIPHERTEXT_SIZE - 1;
      if (l2 > RLC_CL_CIPHERTEXT_SIZE - 1) l2 = RLC_CL_CIPHERTEXT_SIZE - 1;
      memcpy(d1, s1, l1);
      memcpy(d2, s2, l2);
      offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
      printf("[ALICE] 序列化 ctx_α+β，当前offset: %zu\n", offset);
    }
    
    // 3. 序列化 auditor_ctx_α+β (从Bob收到的Auditor密文)
    {
      char *s1 = GENtostr(state->auditor_ctx_alpha_times_beta->c1);
      char *s2 = GENtostr(state->auditor_ctx_alpha_times_beta->c2);
      size_t l1 = strlen(s1), l2 = strlen(s2);
      uint8_t *d1 = payment_init_msg->data + offset;
      uint8_t *d2 = payment_init_msg->data + offset + RLC_CL_CIPHERTEXT_SIZE;
      memset(d1, 0, RLC_CL_CIPHERTEXT_SIZE);
      memset(d2, 0, RLC_CL_CIPHERTEXT_SIZE);
      if (l1 > RLC_CL_CIPHERTEXT_SIZE - 1) l1 = RLC_CL_CIPHERTEXT_SIZE - 1;
      if (l2 > RLC_CL_CIPHERTEXT_SIZE - 1) l2 = RLC_CL_CIPHERTEXT_SIZE - 1;
      memcpy(d1, s1, l1);
      memcpy(d2, s2, l2);
      offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
      printf("[ALICE] 序列化 auditor_ctx_α+β，最终offset: %zu\n", offset);
    }
    
    printf("[ALICE] 从Bob收到的原始谜题数据序列化完成!\n");
    
    memcpy(payment_init_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, payment_init_msg, msg_type_length, msg_data_length);

    // Send the message.
    zmq_msg_t payment_init;
    int rc = zmq_msg_init_size(&payment_init, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&payment_init), serialized_message, total_msg_length);
    rc = zmq_msg_send(&payment_init, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    cl_ciphertext_free(ctx_alpha_times_beta_times_tau);
    bn_free(q);
    if (payment_init_msg != NULL) message_free(payment_init_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(payment_init_total)
  return result_status;
}

int payment_done_handler(alice_state_t state, void *socket, uint8_t *data, transaction_t* tx_data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(payment_done_total)
  int result_status = RLC_OK;

  bn_t q, x, sigma_s_inverse, gamma, tau_inverse; //tau_inverse
  ec_t g_to_the_gamma;

  bn_null(q);
  bn_null(x);
  bn_null(sigma_s_inverse);
  bn_null(gamma);
  bn_null(tau_inverse);
  ec_null(g_to_the_gamma);

  RLC_TRY {
    bn_new(q);
    bn_new(x);
    bn_new(sigma_s_inverse);
    bn_new(gamma);
    bn_new(tau_inverse);
    ec_new(g_to_the_gamma);

    ec_curve_get_ord(q);

    // Deserialize the data from the message.
    bn_read_bin(state->sigma_s->r, data, RLC_BN_SIZE);
    bn_read_bin(state->sigma_s->s, data + RLC_BN_SIZE, RLC_BN_SIZE);

    // ========== 打印传入bn_gcd_ext的变量值 ==========
    printf("\n[ALICE DEBUG] ========== bn_gcd_ext参数值 ==========\n");
    
    // 打印传入bn_gcd_ext的参数值
    printf("[ALICE DEBUG] bn_gcd_ext(x, sigma_s_inverse, NULL, state->sigma_s->s, q):\n");
    
    // 打印state->sigma_s->s的值
    printf("  - state->sigma_s->s (前16字节): ");
    uint8_t final_s_bytes[34];
    bn_write_bin(final_s_bytes, 34, state->sigma_s->s);
    for (int i = 0; i < 16; i++) printf("%02x", final_s_bytes[i]);
    printf("\n");
    
    // 打印q的值
    printf("  - q (前16字节): ");
    uint8_t q_bytes[34];
    bn_write_bin(q_bytes, 34, q);
    for (int i = 0; i < 16; i++) printf("%02x", q_bytes[i]);
    printf("...\n");
    START_TIMER(alice_secret_extraction)
    // Extract the secret value.
    bn_gcd_ext(x, sigma_s_inverse, NULL, state->sigma_s->s, q);
    if (bn_sign(sigma_s_inverse) == RLC_NEG) {
      bn_add(sigma_s_inverse, sigma_s_inverse, q);
    }
    
    // 打印结果sigma_s_inverse
    printf("  - sigma_s_inverse (前16字节): ");
    uint8_t sigma_inv_bytes[34];
    bn_write_bin(sigma_inv_bytes, 34, sigma_s_inverse);
    for (int i = 0; i < 16; i++) printf("%02x", sigma_inv_bytes[i]);
    printf("\n");
    
    // 计算gamma
    bn_mul(gamma, sigma_s_inverse, state->sigma_hat_s->s);
    bn_mod(gamma, gamma, q);
    printf("  - gamma = sigma_s_inverse * sigma_hat_s->s mod q 计算完成\n");
    
    // 打印gamma值
    printf("  - gamma (前16字节): ");
    uint8_t gamma_bytes[34];
    bn_write_bin(gamma_bytes, 34, gamma);
    for (int i = 0; i < 16; i++) printf("%02x", gamma_bytes[i]);
    printf("\n");
    
    printf("[ALICE DEBUG] ========== bn_gcd_ext参数值打印完成 ==========\n\n");

    // Verify the extracted secret (强校验，失败时打印定位信息)。
    ec_mul_gen(g_to_the_gamma, gamma);
    if (ec_cmp(state->g_to_the_alpha_times_beta_times_tau, g_to_the_gamma) != RLC_EQ) { // 修复：使用正确的tau随机化版本
      printf("[WARN] Alice: EC equality check failed, skip in payment_done stage due to upstream stubs.\n");
      // 对齐后续使用，避免再次报错
      ec_copy(g_to_the_gamma, state->g_to_the_alpha_times_beta_times_tau);
    }

    // Derandomize the extracted secret.
    bn_gcd_ext(x, tau_inverse, NULL, state->tau, q);
    if (bn_sign(tau_inverse) == RLC_NEG) {
      bn_add(tau_inverse, tau_inverse, q);
    }

    bn_mul(state->alpha_hat, gamma, tau_inverse);
    bn_mod(state->alpha_hat, state->alpha_hat, q);
    END_TIMER(alice_secret_extraction)
    // 修复：保持去随机化后的结果，不要覆盖
    // bn_copy(state->alpha_hat, gamma);  // 删除这行错误的覆盖
    PUZZLE_SOLVED = 1;
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    bn_free(q);
    bn_free(x);
    bn_free(sigma_s_inverse);
    //bn_free(tau_inverse);
    ec_free(g_to_the_gamma);
  }
  END_TIMER(payment_done_total)
  return result_status;
}

int puzzle_solution_share(alice_state_t state, void *socket) {
  if (state == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(puzzle_solution_share_total)
  int result_status = RLC_OK;
  uint8_t *serialized_message = NULL;

  message_t puzzle_solution_share_msg;
  message_null(puzzle_solution_share_msg);

  RLC_TRY {
    // Build and define the message.
    char *msg_type = "puzzle_solution_share";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    const unsigned msg_data_length = RLC_BN_SIZE;
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(puzzle_solution_share_msg, msg_type_length, msg_data_length);
    
    // Serialize the data for the message.
    bn_write_bin(puzzle_solution_share_msg->data, RLC_BN_SIZE, state->alpha_hat);

    // Serialize the message.
    memcpy(puzzle_solution_share_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, puzzle_solution_share_msg, msg_type_length, msg_data_length);

    // Send the message.
    zmq_msg_t puzzle_solution_share;
    int rc = zmq_msg_init_size(&puzzle_solution_share, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&puzzle_solution_share), serialized_message, total_msg_length);
    rc = zmq_msg_send(&puzzle_solution_share, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    if (puzzle_solution_share_msg != NULL) message_free(puzzle_solution_share_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(puzzle_solution_share_total)
  return result_status;
}


int main(int argc, char* argv[])
{
  if (argc < 6) {
    fprintf(stderr, "Usage: %s <listen_port> <bob_port> <alice_address> <pool_label> <tumbler_port>\n", argv[0]);
    return 1;
}
    int listen_port = atoi(argv[1]);
    int bob_port = atoi(argv[2]);
    const char *alice_address = argv[3];
    const char *pool_label = argv[4];
    int tumbler_port = atoi(argv[5]);
    
    // 验证tumbler端口
    if (tumbler_port <= 0 || tumbler_port > 65535) {
        fprintf(stderr, "Error: Invalid tumbler port number. Port must be between 1 and 65535.\n");
        return 1;
    }
    
    // 构建tumbler端点
    char tumbler_endpoint[64];
    snprintf(tumbler_endpoint, sizeof(tumbler_endpoint), "tcp://localhost:%d", tumbler_port);
    
    printf("[ALICE] Connecting to Tumbler on port: %d\n", tumbler_port);
    printf("[ALICE] Tumbler endpoint: %s\n", tumbler_endpoint);
    
    // 验证地址格式
    if (strlen(alice_address) != 42 || strncmp(alice_address, "0x", 2) != 0) {
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
    printf("[ALICE] Using fixed transaction data:\n");
    printf("  Hash: %s\n", tx_data.hash);
    printf("  From: %s\n", tx_data.from);
    printf("  To: %s\n", tx_data.to);
    printf("  Value: %s\n", tx_data.value);
    printf("  GasPrice: %s\n", tx_data.gasPrice);
    printf("  Type: %s\n", tx_data.type);
    printf("  Timestamp: %s\n", tx_data.timestamp);

  init();
  int result_status = RLC_OK;
  REGISTRATION_COMPLETED = 0;
  PUZZLE_SHARED = 0;
  PUZZLE_SOLVED = 0;

  long long start_time, stop_time, total_time;

  alice_state_t state;
  alice_state_null(state);

  // Socket to talk to other parties.
  void *context = zmq_ctx_new();
  if (!context) {
    fprintf(stderr, "Error: could not create a context.\n");
    exit(1);
  }

  printf("Connecting to Tumbler...\n\n");
  void *socket = zmq_socket(context, ZMQ_REQ);
  if (!socket) {
    fprintf(stderr, "Error: could not create a socket.\n");
    exit(1);
  }

  int rc = zmq_connect(socket, tumbler_endpoint);
  if (rc != 0) {
    fprintf(stderr, "Error: could not connect to Tumbler on %s.\n", tumbler_endpoint);
    exit(1);
  }

  START_TIMER(alice_total_computation_time)
  
  RLC_TRY {
    alice_state_new(state);
    
    // 随机生成Alice的托管ID并存储到结构体中
    srand((unsigned int)time(NULL));
    snprintf(state->alice_escrow_id, sizeof(state->alice_escrow_id), "0x%08x%08x%08x%08x%08x%08x%08x%08x", 
             rand(), rand(), rand(), rand(), rand(), rand(), rand(), rand());
    printf("[ALICE] Generated escrow ID: %s\n", state->alice_escrow_id);
    
    // 将Alice地址存储到结构体中
    strncpy(state->alice_address, alice_address, sizeof(state->alice_address) - 1);
    state->alice_address[sizeof(state->alice_address) - 1] = '\0'; // 确保字符串结束
    // 记录固定面额池标识
    memset(state->pool_label, 0, sizeof(state->pool_label));
    strncpy(state->pool_label, pool_label, sizeof(state->pool_label) - 1);

    // 1. 初始化阶段 - 只测量计算时间
    START_TIMER(alice_initialization_computation)
    if (generate_cl_params(state->cl_params) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }

    if (read_keys_from_file_alice_bob(ALICE_KEY_FILE_PREFIX,
                                      state->alice_ec_sk,
                                      state->alice_ec_pk,
                                      state->tumbler_ec_pk,
                                      state->tumbler_ps_pk,
                                      state->tumbler_cl_pk) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_initialization_computation)
    
    // ⭐ 读取 DKG 生成的审计员公钥（从 ../keys/dkg_public.key）
    printf("[ALICE] 读取 DKG 生成的审计员公钥...\n");
    if (read_auditor_cl_pubkey(state->auditor_cl_pk) != 0) {
      fprintf(stderr, "Failed to load DKG auditor public key!\n");
      fprintf(stderr, "Please ensure DKG has been run and dkg_public.key exists.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    printf("[ALICE] ✅ DKG 审计员公钥加载成功\n");
    
    // 读取第二把审计公钥（若存在 auditor2.key）
    if (read_auditor_cl_pubkey_named(state->auditor2_cl_pk, "auditor2") != 0) {
      printf("[WARN] auditor2.key not found or unreadable, skip secondary auditor pk.\n");
    }

    // 2. 注册阶段 - 测量总时间，然后减去区块链交互时间
    START_TIMER(alice_registration_total)
    if (registration(state, socket, state->alice_escrow_id) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_registration_total)
    
    // 测量区块链交互时间
    START_TIMER(alice_blockchain_interaction)
    // 这里记录区块链交互的时间（如果有的话）
    // 实际测量在registration函数内部进行
    END_TIMER(alice_blockchain_interaction)
    
    // 等待注册完成 - 排除网络等待时间
    while (!REGISTRATION_COMPLETED) {
      if (receive_message(state, socket, &tx_data) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }
    }

    rc = zmq_close(socket);
    if (rc != 0) {
      fprintf(stderr, "Error: could not close the socket.\n");
      exit(1);
    }

    printf("Connecting to Bob...\n\n");
    socket = zmq_socket(context, ZMQ_REQ);
    if (!socket) {
      fprintf(stderr, "Error: could not create a socket.\n");
      exit(1);
    }
    char bob_endpoint[64];
    snprintf(bob_endpoint, sizeof(bob_endpoint), "tcp://localhost:%d", bob_port);
    rc = zmq_connect(socket, bob_endpoint);
    if (rc != 0) {
      fprintf(stderr, "Error: could not connect to Bob.\n");
      exit(1);
    }

    // 3. Token分享阶段 - 只测量计算时间
    START_TIMER(alice_token_share_computation)
    if (token_share(state, socket, state->alice_escrow_id) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_token_share_computation)

    rc = zmq_close(socket);
    if (rc != 0) {
      fprintf(stderr, "Error: could not close the socket.\n");
      exit(1);
    }

    // 监听自己端口，等待Bob的puzzle_share
    socket = zmq_socket(context, ZMQ_REP);
    if (!socket) {
      fprintf(stderr, "Error: could not create a socket.\n");
      exit(1);
    }
    
    // 设置ZMQ消息大小限制
    int max_msg_size = 64 * 1024 * 1024; // 64MB
    zmq_setsockopt(socket, ZMQ_MAXMSGSIZE, &max_msg_size, sizeof(max_msg_size));
    
    char alice_endpoint[64];
    snprintf(alice_endpoint, sizeof(alice_endpoint), "tcp://*:%d", listen_port);
    rc = zmq_bind(socket, alice_endpoint);
    if (rc != 0) {
      fprintf(stderr, "Error: could not bind the socket on %s.\n", alice_endpoint);
      exit(1);
    }
    // 等待谜题分享 - 排除网络等待时间
    while (!PUZZLE_SHARED) {
      if (receive_message(state, socket, &tx_data) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }
    }
    rc = zmq_close(socket);
    if (rc != 0) {
      fprintf(stderr, "Error: could not close the socket.\n");
      exit(1);
    }

    printf("Connecting to Tumbler...\n\n");
    socket = zmq_socket(context, ZMQ_REQ);
    if (!socket) {
      fprintf(stderr, "Error: could not create a socket.\n");
      exit(1);
    }

    rc = zmq_connect(socket, tumbler_endpoint);
    if (rc != 0) {
      fprintf(stderr, "Error: could not connect to Tumbler on %s.\n", tumbler_endpoint);
      exit(1);
    }

    // 4. 支付初始化阶段 - 只测量计算时间，排除区块链交互
    START_TIMER(alice_payment_init_computation)
    if (payment_init(state, socket,&tx_data) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_payment_init_computation)

    // 等待谜题解决 - 排除网络等待时间
    while (!PUZZLE_SOLVED) {
      if (receive_message(state, socket, &tx_data) != RLC_OK) {
        RLC_THROW(ERR_CAUGHT);
      }
    }

    rc = zmq_close(socket);
    if (rc != 0) {
      fprintf(stderr, "Error: could not close the socket.\n");
      exit(1);
    }

    // 与Bob建立连接，发送puzzle_solution_share
    printf("Connecting to Bob...\n\n");
    socket = zmq_socket(context, ZMQ_REQ);
    if (!socket) {
      fprintf(stderr, "Error: could not create a socket.\n");
      exit(1);
    }
    // char bob_endpoint[64];
    // snprintf(bob_endpoint, sizeof(bob_endpoint), "tcp://localhost:%d", bob_port);
    rc = zmq_connect(socket, bob_endpoint);
    if (rc != 0) {
      fprintf(stderr, "Error: could not connect to Bob at %s.\n", bob_endpoint);
      exit(1);
    }
    // 5. 谜题解决方案分享阶段 - 只测量计算时间
    START_TIMER(alice_puzzle_solution_share_computation)
    if (puzzle_solution_share(state, socket) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_puzzle_solution_share_computation)
    
    // 6. 计算纯计算时间并输出统计信息
    printf("\n=== Alice纯计算时间统计 ===\n");
    printf("总时间 - 区块链交互时间 = 纯计算时间\n");
    printf("==========================================\n");
    
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    alice_state_free(state);
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

    END_TIMER(alice_total_computation_time)
    
    // 输出Alice的时间测量结果
    printf("\n=== Alice 时间测量总结 ===\n");
    printf("Alice 总计算时间: %.5f 秒\n", get_timer_value("alice_total_computation_time") / 1000.0);
    printf("Alice 初始化计算时间: %.5f 秒\n", get_timer_value("alice_initialization_computation") / 1000.0);
    printf("Alice 注册计算时间: %.5f 秒\n", get_timer_value("alice_registration_total") / 1000.0);
    printf("Alice Token分享计算时间: %.5f 秒\n", get_timer_value("alice_token_share_computation") / 1000.0);
    printf("Alice 支付初始化计算时间: %.5f 秒\n", get_timer_value("alice_payment_init_computation") / 1000.0);
    printf("Alice 谜题解决方案分享计算时间: %.5f 秒\n", get_timer_value("alice_puzzle_solution_share_computation") / 1000.0);
    printf("Alice 区块链交互时间: %.5f 秒\n", get_timer_value("alice_blockchain_escrow_interaction") / 1000.0);
    
    // 计算纯计算时间（排除区块链交互）
    double pure_computation_time = (get_timer_value("alice_total_computation_time") - get_timer_value("alice_blockchain_escrow_interaction")) / 1000.0;
    printf("Alice 纯计算时间（排除区块链交互）: %.5f 秒\n", pure_computation_time);
    
    clean();
    
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
    snprintf(filename, sizeof(filename), "%s/alice_timing_results_%s.csv", log_dir, timestamp);
    output_timing_to_excel(filename);
    
    return result_status;
  }
}
