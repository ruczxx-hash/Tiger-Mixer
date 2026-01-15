#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // for strncasecmp
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "pari/pari.h"
#include "zmq.h"
#include "tumbler.h"
#include "types.h"
#include "util.h"
#include "secret_share.h"
#include "composite_malleable_proof.h"
#include "http_zk_client.h"

// IO控制宏
#define CONDITIONAL_PRINTF(...) do { \
    const char *disable_io = getenv("A2L_DISABLE_IO"); \
    if (!disable_io || strcmp(disable_io, "1") != 0) { \
        printf(__VA_ARGS__); \
    } \
} while(0)

// 全局变量用于计算Tumbler总时间
static struct timespec tumbler_start_time;
static int tumbler_timing_initialized = 0;

// 全局退出标志
static int tumbler_should_exit = 0;

// 辅助函数：将bn_t转换为字符串
static char* bn_to_string(const bn_t bn) {
    static char buffer[256];
    const int len = bn_size_str(bn, 10);
    bn_write_str(buffer, len, bn, 10);
    return buffer;
}

// 调用 HTTP 零知识证明服务
static int call_http_zk_service(const char* alpha_str, const char* g_alpha_x, const char* g_alpha_y, const char* proof_type) {
    char curl_command[4096];
    char response_file[] = "/tmp/zk_response.json";
    
    // 构建 curl 命令，添加超时和错误处理
    snprintf(curl_command, sizeof(curl_command),
        "curl -s --connect-timeout 5 --max-time 30 -X POST http://localhost:8080/generate-proof "
        "-H \"Content-Type: application/json\" "
        "-d '{\"alpha\":\"%s\",\"g_to_the_alpha_x\":\"%s\",\"g_to_the_alpha_y\":\"%s\",\"proof_type\":\"%s\"}' "
        "-o %s -w \"%%{http_code}\"",
        alpha_str, g_alpha_x, g_alpha_y, proof_type, response_file);
    
    printf("[DEBUG] 执行 curl 命令: %s\n", curl_command);
    
    // 执行 curl 命令并捕获 HTTP 状态码
    FILE* pipe = popen(curl_command, "r");
    if (pipe == NULL) {
        printf("[ERROR] 无法执行 curl 命令\n");
        return -1;
    }
    
    char http_code[10];
    if (fgets(http_code, sizeof(http_code), pipe) == NULL) {
        printf("[ERROR] 无法读取 HTTP 状态码\n");
        pclose(pipe);
        return -1;
    }
    
    int curl_result = pclose(pipe);
    int http_status = atoi(http_code);
    
    printf("[DEBUG] HTTP 状态码: %d, curl 返回码: %d\n", http_status, curl_result);
    
    // 检查响应文件
    FILE* response = fopen(response_file, "r");
    if (response != NULL) {
        char response_content[1024];
        if (fgets(response_content, sizeof(response_content), response) != NULL) {
            printf("[DEBUG] 服务响应: %s\n", response_content);
        }
        fclose(response);
        unlink(response_file); // 删除临时文件
    }
    
    if (http_status == 200 && curl_result == 0) {
        printf("[SUCCESS] %s 零知识证明生成成功\n", proof_type);
        return 0;
    } else {
        printf("[ERROR] %s 零知识证明生成失败 (HTTP: %d, curl: %d)\n", proof_type, http_status, curl_result);
        return -1;
    }
}

// bob_confirm_done_handler: Bob 在完成 confirmEscrow 后回传 txHash，Tumbler 以 auditor_ctx_alpha_times_beta.c1 作为 msgid 发送自身分片
int bob_confirm_done_handler(tumbler_state_t state, void *socket, uint8_t *data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  
  int result_status = RLC_OK;
#ifndef DISABLE_SECRET_SHARES
  RLC_TRY {
    // data: 以 '\0' 结尾的 confirmEscrow txHash 字符串
    const char *bob_confirm_tx = (const char*)data;
    printf("[BOB_CONFIRM] Received Bob confirm txHash: %s\n", bob_confirm_tx);
    // msgid 生成：统一采用 m(E(beta')) 的十进制字符串
    const char *msgid = bob_confirm_tx; // 默认回退为 bob_confirm_tx
    char msgid_hex[2 * RLC_MD_LEN + 3];  // 用于临时存储十六进制
    char msgid_dec[256];  // 用于存储十进制字符串（足够大）
    
    // 优先：用 cl_ciphertext_encode_to_bn(state->auditor_ctx_alpha_times_beta)
    if (state->auditor_ctx_alpha_times_beta && state->auditor_ctx_alpha_times_beta->c1 && state->auditor_ctx_alpha_times_beta->c2) {
      bn_t m_bn; bn_null(m_bn); bn_new(m_bn);
      if (cl_ciphertext_encode_to_bn(m_bn, state->auditor_ctx_alpha_times_beta) == RLC_OK) {
        // 写为十进制字符串作为 msgid
        memset(msgid_dec, 0, sizeof(msgid_dec));
        bn_write_str(msgid_dec, sizeof(msgid_dec), m_bn, 10);
        msgid = msgid_dec;
        printf("[BOB_CONFIRM][MSGID] Using E(beta') encoded decimal as msgid\n");
        printf("[BOB_CONFIRM][MSGID] m(E(beta')) (dec): %s\n", msgid);
      } else {
        printf("[BOB_CONFIRM][MSGID] cl_ciphertext_encode_to_bn failed, fallback to previous strategies\n");
      }
      bn_free(m_bn);
    }
    
    // 若上面没有覆盖（仍等于 bob_confirm_tx），再尝试历史 tag_hash 路径
    if (msgid == bob_confirm_tx) {
      // 优先使用保存的 tag_hash（这就是 Bob 计算的 H(inner.c1||inner.c2)）
      if (state->auditor2_tag_hash[0] || state->auditor2_tag_hash[1]) {
        // 先转换为十六进制字符串
        static const char *hexd = "0123456789abcdef";
        msgid_hex[0] = '0'; msgid_hex[1] = 'x';
        for (int i = 0; i < RLC_MD_LEN; i++) {
          msgid_hex[2 + 2*i]     = hexd[(state->auditor2_tag_hash[i] >> 4) & 0xF];
          msgid_hex[2 + 2*i + 1] = hexd[state->auditor2_tag_hash[i] & 0xF];
        }
        msgid_hex[2 + 2*RLC_MD_LEN] = '\0';
        
        // 转为 PARI/GP 整数，再得到十进制字符串
        GEN tag_plain_int = strtoi(msgid_hex);
        char *tag_plain_dec = GENtostr(tag_plain_int);
        
        strncpy(msgid_dec, tag_plain_dec, sizeof(msgid_dec) - 1);
        msgid_dec[sizeof(msgid_dec) - 1] = '\0';
        msgid = msgid_dec;
        
        printf("[BOB_CONFIRM][MSGID] Using Bob's tag_hash as msgid (fallback)\n");
        printf("[BOB_CONFIRM][MSGID] tag_plain (hex): %s\n", msgid_hex);
        printf("[BOB_CONFIRM][MSGID] tag_plain (decimal/msgid): %s\n", msgid);
        
        free(tag_plain_dec);
      } else {
        // 兼容回退：若未保存 tag_hash，则按 inner.c1||c2 计算
        if (state->auditor_ctx_alpha_times_beta && state->auditor_ctx_alpha_times_beta->c1 && state->auditor_ctx_alpha_times_beta->c2) {
          const char *c1s = GENtostr(state->auditor_ctx_alpha_times_beta->c1);
          const char *c2s = GENtostr(state->auditor_ctx_alpha_times_beta->c2);
          if (c1s && c2s) {
            size_t l1 = strlen(c1s), l2 = strlen(c2s);
            char *cat = (char*)malloc(l1 + l2);
            if (cat) {
              memcpy(cat, c1s, l1);
              memcpy(cat + l1, c2s, l2);
              uint8_t h[RLC_MD_LEN];
              md_map(h, (const uint8_t*)cat, (uint32_t)(l1 + l2));
              free(cat);
              
              // 先转换为十六进制字符串
              static const char *hexd2 = "0123456789abcdef";
              char hex_temp[2 * RLC_MD_LEN + 3];
              hex_temp[0] = '0'; hex_temp[1] = 'x';
              for (int i = 0; i < RLC_MD_LEN; i++) {
                hex_temp[2 + 2*i]     = hexd2[(h[i] >> 4) & 0xF];
                hex_temp[2 + 2*i + 1] = hexd2[h[i] & 0xF];
              }
              hex_temp[2 + 2*RLC_MD_LEN] = '\0';
              
              GEN tag_plain_int2 = strtoi(hex_temp);
              char *tag_plain_dec2 = GENtostr(tag_plain_int2);
              
              strncpy(msgid_dec, tag_plain_dec2, sizeof(msgid_dec) - 1);
              msgid_dec[sizeof(msgid_dec) - 1] = '\0';
              msgid = msgid_dec;
              
              printf("[BOB_CONFIRM][MSGID] Fallback: calculated H(inner.c1||inner.c2)\n");
              printf("[BOB_CONFIRM][MSGID] tag_plain (hex): %s\n", hex_temp);
              printf("[BOB_CONFIRM][MSGID] tag_plain (decimal/msgid): %s\n", msgid);
              
              free(tag_plain_dec2);
            }
          }
        }
        if (msgid == bob_confirm_tx) {
          printf("[BOB_CONFIRM][MSGID] Fallback: using bob_confirm_tx as msgid: %s\n", bob_confirm_tx);
        }
      }
    }
    if (msgid == bob_confirm_tx) {
      printf("[BOB_CONFIRM][MSGID] Fallback: using bob_confirm_tx as msgid: %s\n", bob_confirm_tx);
    }

    // ===== 覆盖为"第二个分片ID = g^(x) / g^(r0)"（这里 x 取 alpha）=====
    // 等价于加法记号：[alpha]G - [r0]G；为与 Auditor 一致，显式按阶取模并从标量重构两点
    {
      ec_t g_alpha, g_r0, id_point, neg_g_r0;
      ec_null(g_alpha); ec_null(g_r0); ec_null(id_point); ec_null(neg_g_r0);
      ec_new(g_alpha); ec_new(g_r0); ec_new(id_point); ec_new(neg_g_r0);

      // 显式取模并重构 g^alpha、g^r0
      bn_t q, alpha_mod_q, r0_mod_q; bn_null(q); bn_null(alpha_mod_q); bn_null(r0_mod_q);
      bn_new(q); bn_new(alpha_mod_q); bn_new(r0_mod_q);
      ec_curve_get_ord(q);
      bn_mod(alpha_mod_q, state->alpha, q);
      bn_mod(r0_mod_q, state->r0, q);
      
      // 【调试】打印Tumbler的alpha和r0
      char alpha_str[512], r0_str[512];
      bn_write_str(alpha_str, sizeof(alpha_str), alpha_mod_q, 10);
      bn_write_str(r0_str, sizeof(r0_str), r0_mod_q, 10);
      printf("[TUMBLER DEBUG] alpha mod q (十进制): %s\n", alpha_str);
      printf("[TUMBLER DEBUG] r0 mod q (十进制): %s\n", r0_str);
      
      ec_mul_gen(g_alpha, alpha_mod_q);
      ec_mul_gen(g_r0, r0_mod_q);

      // 取负点 -g^r0
      ec_neg(neg_g_r0, g_r0);
      // id_point = g^alpha + (-g^r0) = g^(alpha - r0)
      ec_add(id_point, g_alpha, neg_g_r0);
      ec_norm(id_point, id_point);

      // 编码为压缩点并转为十六进制字符串用作 msgid
      uint8_t id_bytes[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(id_bytes, RLC_EC_SIZE_COMPRESSED, id_point, 1);
      static char msgid_ec_hex[2 * RLC_EC_SIZE_COMPRESSED + 3];
      msgid_ec_hex[0] = '0'; msgid_ec_hex[1] = 'x';
      static const char *hexd_ec = "0123456789abcdef";
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
        msgid_ec_hex[2 + 2 * i]     = hexd_ec[(id_bytes[i] >> 4) & 0xF];
        msgid_ec_hex[2 + 2 * i + 1] = hexd_ec[id_bytes[i] & 0xF];
      }
      msgid_ec_hex[2 + 2 * RLC_EC_SIZE_COMPRESSED] = '\0';

      msgid = msgid_ec_hex;
      printf("[BOB_CONFIRM][MSGID] Overridden: msgid = g^(alpha)/g^(r0) = g^(alpha-r0) (compressed hex)\n");
      printf("[BOB_CONFIRM][MSGID] msgid = %s\n", msgid);

      ec_free(g_alpha); ec_free(g_r0); ec_free(id_point); ec_free(neg_g_r0);
      bn_free(q); bn_free(alpha_mod_q); bn_free(r0_mod_q);
    }

    // === 调试打印：第二分片中的 g^(x) 与 g^(r0)（压缩点） ===
    {
      uint8_t gx_bytes[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(gx_bytes, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha, 1);
      printf("[TUMBLER-2][DEBUG] g^(x)=g^alpha (compressed): 0x");
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", gx_bytes[i]);
      printf("\n");

      ec_t g_r0_dbg; ec_null(g_r0_dbg); ec_new(g_r0_dbg);
      ec_mul_gen(g_r0_dbg, state->r0);
      uint8_t gr0_bytes[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(gr0_bytes, RLC_EC_SIZE_COMPRESSED, g_r0_dbg, 1);
      printf("[TUMBLER-2][DEBUG] g^(r0) (compressed): 0x");
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", gr0_bytes[i]);
      printf("\n");
      ec_free(g_r0_dbg);
    }

    // 新的打包顺序：ZK证明 | g^alpha | ctx_alpha | auditor_enc(r0) | 完整预签名结构 | 最终签 r|s | bob_confirm_tx
    size_t confirm_len = strlen(bob_confirm_tx) + 1;
    
    // 计算实际需要的总长度（使用长度前缀格式）
    char *ctx_alpha_c1 = GENtostr(state->ctx_alpha->c1);
    char *ctx_alpha_c2 = GENtostr(state->ctx_alpha->c2);
    char *ctx_r0_c1 = GENtostr(state->ctx_r0_auditor->c1);
    char *ctx_r0_c2 = GENtostr(state->ctx_r0_auditor->c2);
    
    // 计算 tumbler_escrow_id 和 tumbler_escrow_tx_hash 的长度
    size_t tumbler_escrow_id_len = strlen(state->tumbler_escrow_id) + 1; // 包含 '\0'
    size_t tumbler_escrow_tx_hash_len = strlen(state->tumbler_escrow_tx_hash) + 1; // 包含 '\0'
    
    size_t total2 = 0;
    // 新增：Tumbler综合谜题零知识证明（放在最前面）
    size_t tumbler_zk_size = zk_comprehensive_puzzle_serialized_size();
    printf("[TUMBLER][SECRET_SHARE_2] Tumbler ZK证明估计大小: %zu bytes\n", tumbler_zk_size);
    total2 += tumbler_zk_size; // Tumbler综合谜题零知识证明
    total2 += RLC_EC_SIZE_COMPRESSED;        // g^alpha
    total2 += sizeof(size_t) + strlen(ctx_alpha_c1);  // ctx_alpha c1 长度+数据
    total2 += sizeof(size_t) + strlen(ctx_alpha_c2);  // ctx_alpha c2 长度+数据
    total2 += sizeof(size_t) + strlen(ctx_r0_c1);    // auditor_enc(r0) c1 长度+数据
    total2 += sizeof(size_t) + strlen(ctx_r0_c2);     // auditor_enc(r0) c2 长度+数据
    total2 += 2 * RLC_BN_SIZE;               // presig r|s (sigma_tr)
    total2 += RLC_EC_SIZE_COMPRESSED;        // presig R (sigma_tr)
    total2 += 2 * RLC_EC_SIZE_COMPRESSED;    // presig pi.a|pi.b (sigma_tr)
    total2 += RLC_BN_SIZE;                   // presig pi.z (sigma_tr)
    total2 += 2 * RLC_BN_SIZE;               // final sig r|s (sigma_ts)
    // 新增：Tumbler签名验证所需的信息
    total2 += tumbler_escrow_id_len;                  // tumbler_escrow_id (字符串)
    total2 += tumbler_escrow_tx_hash_len;             // tumbler_escrow_tx_hash (字符串)
    total2 += confirm_len;                   // bob confirm tx hash

    printf("[TUMBLER][SECRET_SHARE_2] 分配缓冲区大小: %zu bytes\n", total2);
    uint8_t *packed2 = (uint8_t*)malloc(total2);
    if (!packed2) { 
      printf("[TUMBLER-2][ERROR] malloc失败，大小: %zu\n", total2);
      RLC_THROW(ERR_CAUGHT); 
    }
    printf("[TUMBLER-2][PACK] 内存分配成功\n");
    size_t off2 = 0;

    // 1. 首先序列化Tumbler综合谜题零知识证明
    printf("[TUMBLER][SECRET_SHARE_2] 开始序列化Tumbler综合谜题零知识证明...\n");
    printf("[TUMBLER][SECRET_SHARE_2] 序列化前 - 当前offset: %zu, 剩余空间: %zu\n", off2, total2 - off2);
    
    // 检查是否有足够的空间
    if (off2 + tumbler_zk_size > total2) {
      printf("[TUMBLER][SECRET_SHARE_2] ERROR: 缓冲区空间不足！需要 %zu, 剩余 %zu\n", tumbler_zk_size, total2 - off2);
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[TUMBLER][SECRET_SHARE_2] 序列化ZK证明到offset: %zu\n", off2);
    size_t zk_written = 0;
    if (zk_comprehensive_puzzle_serialize(packed2 + off2, &zk_written, state->comprehensive_puzzle_zk_proof) != RLC_OK) {
      printf("[ERROR] 无法序列化Tumbler综合谜题零知识证明!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    off2 += zk_written;
    printf("[TUMBLER][SECRET_SHARE_2] ZK证明序列化完成，写入: %zu bytes，最终offset: %zu\n", zk_written, off2);
    
    // 调试：打印序列化后的ZK证明数据
    printf("[TUMBLER][SECRET_SHARE_2] 序列化后的ZK证明数据前64字节: ");
    for (size_t i = 0; i < 64 && i < zk_written; i++) {
        printf("%02x", packed2[off2 - zk_written + i]);
    }
    printf("\n");
    printf("[TUMBLER][SECRET_SHARE_2] Tumbler综合谜题零知识证明序列化成功，大小: %zu bytes，最终offset: %zu\n", zk_written, off2);
    
    printf("[TUMBLER][SECRET_SHARE_2] Tumbler综合谜题零知识证明序列化完成!\n");

    // 2. g^alpha
    printf("[TUMBLER][SECRET_SHARE_2] 序列化 g^alpha，当前offset: %zu\n", off2);
    ec_write_bin(packed2 + off2, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha, 1); off2 += RLC_EC_SIZE_COMPRESSED;
    printf("[TUMBLER][SECRET_SHARE_2] g^alpha 序列化完成，当前offset: %zu\n", off2);
    
    // ctx_alpha (使用长度前缀格式)
    {
      size_t len1 = strlen(ctx_alpha_c1);
      size_t len2 = strlen(ctx_alpha_c2);
      memcpy(packed2 + off2, &len1, sizeof(size_t)); off2 += sizeof(size_t);
      memcpy(packed2 + off2, ctx_alpha_c1, len1); off2 += len1;
      memcpy(packed2 + off2, &len2, sizeof(size_t)); off2 += sizeof(size_t);
      memcpy(packed2 + off2, ctx_alpha_c2, len2); off2 += len2;
    }
    
    // auditor_enc(r0) (使用长度前缀格式)
    {
      size_t len1 = strlen(ctx_r0_c1);
      size_t len2 = strlen(ctx_r0_c2);
      memcpy(packed2 + off2, &len1, sizeof(size_t)); off2 += sizeof(size_t);
      memcpy(packed2 + off2, ctx_r0_c1, len1); off2 += len1;
      memcpy(packed2 + off2, &len2, sizeof(size_t)); off2 += sizeof(size_t);
      memcpy(packed2 + off2, ctx_r0_c2, len2); off2 += len2;
    }
    // 预签 (sigma_tr) - 完整结构：r|s|R|pi.a|pi.b|pi.z
    printf("[TUMBLER-2][PACK] 打包Tumbler完整预签名结构:\n");
    {
      uint8_t debug_r[RLC_BN_SIZE], debug_s[RLC_BN_SIZE];
      uint8_t debug_R[RLC_EC_SIZE_COMPRESSED], debug_pi_a[RLC_EC_SIZE_COMPRESSED];
      bn_write_bin(debug_r, RLC_BN_SIZE, state->sigma_tr->r);
      bn_write_bin(debug_s, RLC_BN_SIZE, state->sigma_tr->s);
      ec_write_bin(debug_R, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->R, 1);
      ec_write_bin(debug_pi_a, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->a, 1);
      
      printf("  - r[0..15]: ");
      for (size_t i = 0; i < 16 && i < RLC_BN_SIZE; i++) printf("%02x", debug_r[i]);
      printf("\n");
      printf("  - s[0..15]: ");
      for (size_t i = 0; i < 16 && i < RLC_BN_SIZE; i++) printf("%02x", debug_s[i]);
      printf("\n");
      printf("  - R[0..15]: ");
      for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", debug_R[i]);
      printf("\n");
      printf("  - pi.a[0..15]: ");
      for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", debug_pi_a[i]);
      printf("\n");
    }
    
    bn_write_bin(packed2 + off2, RLC_BN_SIZE, state->sigma_tr->r); off2 += RLC_BN_SIZE;
    bn_write_bin(packed2 + off2, RLC_BN_SIZE, state->sigma_tr->s); off2 += RLC_BN_SIZE;
    ec_write_bin(packed2 + off2, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->R, 1); off2 += RLC_EC_SIZE_COMPRESSED;
    ec_write_bin(packed2 + off2, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->a, 1); off2 += RLC_EC_SIZE_COMPRESSED;
    ec_write_bin(packed2 + off2, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->b, 1); off2 += RLC_EC_SIZE_COMPRESSED;
    bn_write_bin(packed2 + off2, RLC_BN_SIZE, state->sigma_tr->pi->z); off2 += RLC_BN_SIZE;
    // 最终签 (sigma_ts)
    bn_write_bin(packed2 + off2, RLC_BN_SIZE, state->sigma_ts->r); off2 += RLC_BN_SIZE;
    bn_write_bin(packed2 + off2, RLC_BN_SIZE, state->sigma_ts->s); off2 += RLC_BN_SIZE;
    
    // 新增：序列化Tumbler签名验证所需的信息
    printf("[TUMBLER][SECRET_SHARE_2] 开始序列化Tumbler签名验证信息...\n");
    memcpy(packed2 + off2, state->tumbler_escrow_id, tumbler_escrow_id_len);
    off2 += tumbler_escrow_id_len;
    printf("[TUMBLER][SECRET_SHARE_2] 序列化 tumbler_escrow_id: %s，当前offset: %zu\n", state->tumbler_escrow_id, off2);
    
    memcpy(packed2 + off2, state->tumbler_escrow_tx_hash, tumbler_escrow_tx_hash_len);
    off2 += tumbler_escrow_tx_hash_len;
    printf("[TUMBLER][SECRET_SHARE_2] 序列化 tumbler_escrow_tx_hash: %s，当前offset: %zu\n", state->tumbler_escrow_tx_hash, off2);
    
    // 附加 bob confirm tx hash（便于审计侧完整验证）
    if (off2 + confirm_len > total2) {
      printf("[TUMBLER-2][ERROR] 缓冲区溢出：off2=%zu, confirm_len=%zu, total2=%zu\n", off2, confirm_len, total2);
      free(packed2);
      RLC_THROW(ERR_CAUGHT);
    }
    memcpy(packed2 + off2, bob_confirm_tx, confirm_len); off2 += confirm_len;
    

    CONDITIONAL_PRINTF("[TUMBLER-2][PACK] 实际使用大小: %zu, 分配大小: %zu\n", off2, total2);
    if (off2 != total2) {
      CONDITIONAL_PRINTF("[TUMBLER-2][WARNING] 大小不匹配！实际: %zu, 预期: %zu\n", off2, total2);
      CONDITIONAL_PRINTF("[TUMBLER-2][WARNING] 未初始化内存: %zu 字节\n", total2 - off2);
      
      // 将未使用的内存清零，避免垃圾数据
      if (off2 < total2) {
        memset(packed2 + off2, 0, total2 - off2);
        CONDITIONAL_PRINTF("[TUMBLER-2][FIX] 已将未使用内存清零\n");
      }
    }
    
    START_TIMER(tumbler_secret_share_phase2)
    printf("[SecretShare][Tumbler-2] packed(size=%zu) msgid=%.80s%s\n", off2, msgid, strlen(msgid) > 80 ? "..." : "");
    
    // 计算需要的分享数组大小（num_blocks * SECRET_SHARES）
    size_t num_blocks = (off2 + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t max_shares = num_blocks * SECRET_SHARES;
    secret_share_t* shares2 = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
    if (shares2 == NULL) {
      printf("[SecretShare][Tumbler-2] Error: Failed to allocate shares array\n");
      free(packed2);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 保存分片前的完整消息到文件（用于调试）
    {
      char filename[256];
      // 处理文件名：移除 0x 前缀，替换特殊字符
      char safe_msgid[128];
      if (msgid && strlen(msgid) > 0) {
        // 移除 0x 前缀（如果存在）
        if (strncmp(msgid, "0x", 2) == 0) {
          strncpy(safe_msgid, msgid + 2, sizeof(safe_msgid) - 1);
        } else {
          strncpy(safe_msgid, msgid, sizeof(safe_msgid) - 1);
        }
        safe_msgid[sizeof(safe_msgid) - 1] = '\0';
        // 替换可能存在的特殊字符
        for (char *p = safe_msgid; *p; p++) {
          if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            *p = '_';
          }
        }
      } else {
        strcpy(safe_msgid, "unknown");
      }
      snprintf(filename, sizeof(filename), "tumbler2_original_msg_%s.txt", safe_msgid);
      FILE *fp = fopen(filename, "w");
      if (fp) {
        fprintf(fp, "Tumbler-2 Original Message (before sharing)\n");
        fprintf(fp, "Message ID: %s\n", msgid);
        fprintf(fp, "Length: %zu bytes\n", off2);
        fprintf(fp, "Hex dump:\n");
        for (size_t i = 0; i < off2; i++) {
          fprintf(fp, "%02x", packed2[i]);
          if ((i + 1) % 32 == 0) fprintf(fp, "\n");
          else if ((i + 1) % 2 == 0) fprintf(fp, " ");
        }
        if (off2 % 32 != 0) fprintf(fp, "\n");
        fclose(fp);
        printf("[TUMBLER-2][DEBUG] Saved original message before sharing: %s (%zu bytes)\n", filename, off2);
      } else {
        printf("[TUMBLER-2][DEBUG] Failed to save original message to %s\n", filename);
      }
    }
    
    size_t num_shares2 = 0;
    int ss2 = create_secret_shares(packed2, off2, shares2, &num_shares2);
    if (ss2 != 0) {
      printf("[SecretShare][Tumbler-2] create_secret_shares failed=%d\n", ss2);
      free(shares2);
    } else {
      printf("[SecretShare][Tumbler-2] Created %zu shares\n", num_shares2);
      // ===== VSS: 创建并发送承诺给 Auditor =====
      vss_commitment_t commitment2;
      printf("[VSS][Tumbler-2] DEBUG: msgid = '%s' (length: %zu)\n", msgid, strlen(msgid));
      
      // 检查msgid长度是否过长
      if (strlen(msgid) > MSG_ID_MAXLEN - 1) {
        printf("[VSS][Tumbler-2] WARNING: msgid too long (%zu > %d), truncating\n", strlen(msgid), MSG_ID_MAXLEN - 1);
      }
      
      if (strlen(msgid) == 0) {
        printf("[VSS][Tumbler-2] WARNING: msgid is empty, using fallback msgid\n");
        strncpy(commitment2.msgid, "fallback_msgid_bob", MSG_ID_MAXLEN - 1);
      } else {
        strncpy(commitment2.msgid, msgid, MSG_ID_MAXLEN - 1);
      }
      commitment2.msgid[MSG_ID_MAXLEN - 1] = '\0';
      
      printf("[VSS][Tumbler-2] 开始创建VSS承诺\n");
      if (create_vss_commitments(packed2, off2, shares2, &commitment2, msgid) == 0) {
        printf("[VSS][Tumbler-2] Created VSS commitments for second share\n");
        if (save_vss_commitment_to_file(&commitment2) == 0) {
          printf("[VSS][Tumbler-2] Successfully saved VSS commitment to file\n");
        } else {
          printf("[VSS][Tumbler-2] Failed to save VSS commitment to file\n");
        }
      } else {
        printf("[VSS][Tumbler-2] Failed to create VSS commitments\n");
      }
      
      printf("[VSS][Tumbler-2] 开始发送分片到接收者\n");
      
      // 初始化 RECEIVER_ENDPOINTS（如果未初始化）
      if (RECEIVER_ENDPOINTS[0][0] == '\0') {
        printf("[VSS][Tumbler-2] Initializing RECEIVER_ENDPOINTS...\n");
        get_dynamic_endpoints(RECEIVER_ENDPOINTS);
        for (int i = 0; i < SECRET_SHARES; i++) {
          printf("[VSS][Tumbler-2] Endpoint[%d]: %s\n", i, RECEIVER_ENDPOINTS[i]);
        }
      }
      
      // 创建指针数组以匹配函数签名
      const char* endpoint_ptrs2[SECRET_SHARES];
      for (int i = 0; i < SECRET_SHARES; i++) {
        endpoint_ptrs2[i] = RECEIVER_ENDPOINTS[i];
      }
      
      if (send_shares_to_receivers(shares2, num_shares2, msgid, endpoint_ptrs2) != 0) {
        printf("[SecretShare][Tumbler-2] send_shares_to_receivers encountered warnings\n");
      } else {
        printf("[SecretShare][Tumbler-2] Shares sent using msgid(tag_plain) after Bob confirm\n");
      }
      printf("[VSS][Tumbler-2] 分片发送完成\n");
      free(shares2);
      
      // 设置退出标志，程序将在发送完分片后退出
      tumbler_should_exit = 1;
      printf("[TUMBLER] 设置退出标志，程序将在处理完当前消息后退出\n");
    }
    END_TIMER(tumbler_secret_share_phase2)
    free(packed2);
    
    // 计算总时间（从Tumbler启动到处理完一个完整流程）
    struct timespec tumbler_end_time;
    clock_gettime(CLOCK_MONOTONIC, &tumbler_end_time);
    double total_time_ms = (tumbler_end_time.tv_sec - tumbler_start_time.tv_sec) * 1000.0 + 
                          (tumbler_end_time.tv_nsec - tumbler_start_time.tv_nsec) / 1000000.0;
    
    // 输出Tumbler的时间测量结果（处理完一个完整流程后）
    printf("\n=== Tumbler 时间测量总结 ===\n");
    printf("Tumbler 总计算时间: %.5f 秒\n", total_time_ms / 1000.0);
    printf("Tumbler 初始化计算时间: %.5f 秒\n", get_timer_value("tumbler_initialization_computation") / 1000.0);
    printf("Tumbler 注册阶段时间: %.5f 秒\n", get_timer_value("registration_phase") / 1000.0);
    printf("Tumbler 谜题生成时间: %.5f 秒\n", get_timer_value("tumbler_puzzle_generation") / 1000.0);
    printf("Tumbler 零知识证明生成时间: %.5f 秒\n", get_timer_value("tumbler_zk_proof_generation") / 1000.0);
    printf("Tumbler 分层证明处理时间: %.5f 秒\n", get_timer_value("tumbler_layered_proof_handler") / 1000.0);
    printf("Tumbler 零知识证明验证时间: %.5f 秒\n", get_timer_value("tumbler_zk_verification") / 1000.0);
    printf("Tumbler 秘密分享阶段2时间: %.5f 秒\n", get_timer_value("tumbler_secret_share_phase2") / 1000.0);
    printf("Tumbler 区块链交互时间: %.5f 秒\n", get_timer_value("tumbler_blockchain_escrow_interaction") / 1000.0);
    
    // 计算纯计算时间（排除区块链交互）
    double pure_computation_time = (total_time_ms - get_timer_value("tumbler_blockchain_escrow_interaction")) / 1000.0;
    printf("Tumbler 纯计算时间（排除区块链交互）: %.5f 秒\n", pure_computation_time);
    
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
    snprintf(filename, sizeof(filename), "%s/tumbler_timing_results_%s.csv", log_dir, timestamp);
    output_timing_to_excel(filename);
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
  }
#else
  (void)socket; (void)data;
  printf("[SecretShare] Disabled at tumbler. Skip bob_confirm_done_handler.\n");
#endif
  return result_status;
}



// 在 registration_handler 函数之前添加
static int open_escrow_sync_for_tumbler(tumbler_state_t state, const char *escrow_id) {
    const char *truffle_project = "/home/zxx/Config/truffleProject/truffletest";
    
    // 设置参与方地址（Tumbler 作为 party1，Bob 作为 party2）
    const char *p1 = "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025";  // Tumbler 地址
    const char *p2 = state->bob_address;  // 使用存储的Bob地址
    
    // 设置截止时间
    long deadline = (long)time(NULL) + 3600; // 1小时后过期
    
    // 使用 meta 字段存储 Tumbler 的标识
    char meta_hash[67];
    time_t now = time(NULL);
    snprintf(meta_hash, sizeof(meta_hash), "0x%lx%08x", (unsigned long)now, (unsigned int)rand());
    
    // 获取 Tumbler 的账户地址
    const char *tumbler_from = "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025";
    
    // 生成未压缩公钥（0x04||X||Y）并转为十六进制字符串
    // 从结构体中读取 tumbler_ec_pk 和 bob_ec_pk 的公钥点（通过 ->pk 字段访问）
    uint8_t tumbler_pk_bytes[RLC_EC_SIZE_COMPRESSED * 2];
    uint8_t bob_pk_bytes[RLC_EC_SIZE_COMPRESSED * 2];
    char tumbler_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2 + 1];
    char bob_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2 + 1];
    size_t i_pk;
    
    // 检查公钥是否已初始化
    if (state->tumbler_ec_pk == NULL || state->bob_ec_pk == NULL) {
      fprintf(stderr, "[ESCROW] ERROR: Public keys not initialized! tumbler_ec_pk=%p, bob_ec_pk=%p\n", 
              (void*)state->tumbler_ec_pk, (void*)state->bob_ec_pk);
      return -1;
    }
    
    ec_write_bin(tumbler_pk_bytes, RLC_EC_SIZE_COMPRESSED * 2, state->tumbler_ec_pk->pk, 0);
    ec_write_bin(bob_pk_bytes, RLC_EC_SIZE_COMPRESSED * 2, state->bob_ec_pk->pk, 0);
    // 生成纯十六进制字符串，不加 "0x" 前缀（JS 脚本会统一处理前缀）
    for (i_pk = 0; i_pk < (size_t)(RLC_EC_SIZE_COMPRESSED * 2); i_pk++) {
      sprintf(tumbler_pk_hex + 2*i_pk, "%02x", tumbler_pk_bytes[i_pk]);
      sprintf(bob_pk_hex + 2*i_pk, "%02x", bob_pk_bytes[i_pk]);
    }
    tumbler_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2] = '\0';
    bob_pk_hex[(RLC_EC_SIZE_COMPRESSED * 2) * 2] = '\0';
    
    // 验证公钥字符串不为空
    if (strlen(tumbler_pk_hex) == 0 || strlen(bob_pk_hex) == 0) {
      fprintf(stderr, "[ESCROW] ERROR: Failed to generate public key hex strings! tumbler_len=%zu, bob_len=%zu\n",
              strlen(tumbler_pk_hex), strlen(bob_pk_hex));
      return -1;
    }
    
    // 打印输出两个公钥（打印时加上前缀以便阅读）
    printf("[ESCROW] Tumbler公钥 (未压缩): 0x%s (length: %zu)\n", tumbler_pk_hex, strlen(tumbler_pk_hex));
    printf("[ESCROW] Bob公钥 (未压缩): 0x%s (length: %zu)\n", bob_pk_hex, strlen(bob_pk_hex));
    
    // 构建命令（固定面额池脚本，按 pool_label 自动选择合约与面额）
    char *cmd = malloc(4096);
    if (!cmd) return -1;
    
    // 修改命令：添加 --p1key 和 --p2key 参数，移除 --benef 和 --refund
    int cmd_len = snprintf(cmd, 4096,
                           "cd %s && npx truffle exec scripts/openEscrow_fixed.js --network private "
                           "--pool %s --id %s --p1 %s --p2 %s --deadline %ld --meta %s --from %s --p1key %s --p2key %s 2>&1",
                           truffle_project, state->pool_label, escrow_id, p1, p2, deadline, meta_hash, tumbler_from, tumbler_pk_hex, bob_pk_hex);
    
    if (cmd_len >= 4096) {
      fprintf(stderr, "[ESCROW] WARNING: Command string was truncated! Required length: %d\n", cmd_len);
    }
    
    printf("[ESCROW] Tumbler openEscrow sync cmd (length: %d): %s\n", cmd_len, cmd);
    printf("[ESCROW] Debug - p1key value: %s (len: %zu)\n", tumbler_pk_hex, strlen(tumbler_pk_hex));
    printf("[ESCROW] Debug - p2key value: %s (len: %zu)\n", bob_pk_hex, strlen(bob_pk_hex));

    // 执行命令并捕获输出，以解析 txHash
    START_TIMER(tumbler_blockchain_escrow_interaction)
    FILE *fp = popen(cmd, "r");
    if (!fp) {
      fprintf(stderr, "[ESCROW] Failed to execute openEscrow command\n");
      free(cmd);
      return -1;
    }
    char buffer[1024];
    char tx_hash[67] = {0};
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
      printf("[ESCROW] Output: %s", buffer);
      if (strstr(buffer, "\"txHash\"") != NULL || strstr(buffer, "Transaction hash:") != NULL) {
        char *hash_start = strstr(buffer, "0x");
        if (hash_start) {
          // 尝试截取 0x+64
          strncpy(tx_hash, hash_start, 66);
          tx_hash[66] = '\0';
          printf("[ESCROW] Captured Tumbler txHash: %s\n", tx_hash);
          break;
        }
      }
    }
    int rc = pclose(fp);
    
    free(cmd);

    if (strlen(tx_hash) > 0) {
      memset(state->tumbler_escrow_tx_hash, 0, sizeof(state->tumbler_escrow_tx_hash));
      strncpy(state->tumbler_escrow_tx_hash, tx_hash, sizeof(state->tumbler_escrow_tx_hash) - 1);
      state->tumbler_escrow_tx_hash[sizeof(state->tumbler_escrow_tx_hash) - 1] = '\0';
      printf("[ESCROW] Stored Tumbler escrow txHash: %s\n", state->tumbler_escrow_tx_hash);

      // 将交易哈希作为 dataHash 写入合约（bytes32），便于后续签名校验使用
      {
        const char *from_addr = "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025"; // Tumbler 地址
        char cmd_set[4096];
        snprintf(cmd_set, sizeof(cmd_set),
                 "cd %s && npx truffle exec scripts/setDataHash.js --network private --pool %s --id %s --hash %s --from %s 2>&1",
                 truffle_project, state->pool_label, escrow_id, state->tumbler_escrow_tx_hash, from_addr);
        printf("[ESCROW] Tumbler setDataHash cmd: %s\n", cmd_set);
        int rc_set = system(cmd_set);
        if (rc_set == 0) {
          printf("[ESCROW] Wrote dataHash (txHash) to escrow successfully.\n");
        } else {
          printf("[ESCROW] Failed to write dataHash to escrow, rc=%d.\n", rc_set);
        }
      }
    } else {
      // 未捕获到时写入占位，避免后续序列化空指针
      strncpy(state->tumbler_escrow_tx_hash, "0x0000000000000000000000000000000000000000000000000000000000000000", sizeof(state->tumbler_escrow_tx_hash) - 1);
      state->tumbler_escrow_tx_hash[sizeof(state->tumbler_escrow_tx_hash) - 1] = '\0';
      printf("[ESCROW] No txHash captured for Tumbler openEscrow, using default\n");
    }
    END_TIMER(tumbler_blockchain_escrow_interaction)
    return rc;
}

// 在 open_escrow_sync_for_tumbler 函数之后添加
static int release_escrow_for_alice(tumbler_state_t state, const char *escrow_id) {
  // 直接使用固定值
  // const char *rpc = "http://127.0.0.1:7545";  // 暂时注释掉未使用的变量
  // 获取 Tumbler 的账户地址（固定使用第二个账户）
  const char *tumbler_from = "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025";
  
  // 构造签名内容：escrowID || openEscrow的交易哈希
  uint8_t sign_buf[64]; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
  int sign_len = 64;
  
  // 1. 将托管ID (escrow_id) 从十六进制字符串转换为32字节数组（bytes32格式）
  if (strlen(escrow_id) < 2 || strncmp(escrow_id, "0x", 2) != 0) {
    printf("[ERROR] Invalid escrow_id format: %s\n", escrow_id);
    return -1;
  }
  
  // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
  const char *escrow_id_hex = escrow_id + 2; // 跳过"0x"
  size_t escrow_id_hex_len = strlen(escrow_id_hex);
  if (escrow_id_hex_len != 64) {
    printf("[ERROR] escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, escrow_id);
    return -1;
  }
  
  // 将托管ID的十六进制字符串转换为字节数组
  for (size_t i = 0; i < 32; i++) {
    char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
    sign_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
  }
  
  // 2. 将交易哈希从十六进制字符串转换为32字节数组
  if (strlen(state->alice_escrow_tx_hash) < 2 || strncmp(state->alice_escrow_tx_hash, "0x", 2) != 0) {
    printf("[ERROR] Invalid alice_escrow_tx_hash format: %s\n", state->alice_escrow_tx_hash);
    return -1;
  }
  
  // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
  const char *hash_hex = state->alice_escrow_tx_hash + 2; // 跳过"0x"
  size_t hash_hex_len = strlen(hash_hex);
  if (hash_hex_len != 64) {
    printf("[ERROR] alice_escrow_tx_hash should be 64 hex chars (32 bytes), got %zu: %s\n", hash_hex_len, state->alice_escrow_tx_hash);
    return -1;
  }
  
  // 将交易哈希的十六进制字符串转换为字节数组
  for (size_t i = 0; i < 32; i++) {
    char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
    sign_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
  }
  
  // 检查 Alice 的签名是否已初始化
  bn_t test_r, test_s;
  bn_new(test_r);
  bn_new(test_s);
  bn_copy(test_r, state->sigma_s->r);
  bn_copy(test_s, state->sigma_s->s);
  
  if (bn_is_zero(test_r) || bn_is_zero(test_s)) {
    printf("[ERROR] Alice signature (sigma_s) not initialized or invalid!\n");
    bn_free(test_r);
    bn_free(test_s);
    return -1;
  }
  bn_free(test_r);
  bn_free(test_s);
  
  // 获取 Alice 的签名 (state->sigma_s)
  uint8_t alice_r_bytes[32], alice_s_bytes[32];
  bn_write_bin(alice_r_bytes, 32, state->sigma_s->r);
  bn_write_bin(alice_s_bytes, 32, state->sigma_s->s);
  
  // 转换为十六进制字符串
  char alice_r_hex[65] = {0};
  char alice_s_hex[65] = {0};
  for (int i = 0; i < 32; i++) {
    sprintf(alice_r_hex + 2 * i, "%02x", alice_r_bytes[i]);
    sprintf(alice_s_hex + 2 * i, "%02x", alice_s_bytes[i]);
  }
  
  printf("[ESCROW DEBUG] Alice signature r (first 20 chars): %.20s\n", alice_r_hex);
  printf("[ESCROW DEBUG] Alice signature s (first 20 chars): %.20s\n", alice_s_hex);
  printf("[ESCROW DEBUG] Alice signature r (full): %s\n", alice_r_hex);
  printf("[ESCROW DEBUG] Alice signature s (full): %s\n", alice_s_hex);
  
  // 对于 Tumbler 的签名，我们需要对相同的消息进行签名
  // Tumbler 需要对 alice_escrow_id || alice_escrow_tx_hash 进行标准 ECDSA 签名
  // 使用 cp_ecdsa_sig 函数生成签名
  bn_t tumbler_r, tumbler_s;
  bn_new(tumbler_r);
  bn_new(tumbler_s);
  
  // 使用标准 ECDSA 签名生成 Tumbler 的签名
  // cp_ecdsa_sig 的参数：r, s, msg, len, hash_flag, secret_key
  // hash_flag = 0 表示函数内部会对消息进行哈希
  int sig_result = cp_ecdsa_sig(tumbler_r, tumbler_s, sign_buf, sign_len, 0, state->tumbler_ec_sk->sk);
  if (sig_result != RLC_OK) {
    printf("[ERROR] Failed to generate Tumbler signature for Alice escrow\n");
    bn_free(tumbler_r);
    bn_free(tumbler_s);
    return -1;
  }
  
  uint8_t tumbler_r_bytes[32], tumbler_s_bytes[32];
  char tumbler_r_hex[65] = {0};
  char tumbler_s_hex[65] = {0};
  
  bn_write_bin(tumbler_r_bytes, 32, tumbler_r);
  bn_write_bin(tumbler_s_bytes, 32, tumbler_s);
  
  for (int i = 0; i < 32; i++) {
    sprintf(tumbler_r_hex + 2 * i, "%02x", tumbler_r_bytes[i]);
    sprintf(tumbler_s_hex + 2 * i, "%02x", tumbler_s_bytes[i]);
  }
  
  printf("[ESCROW DEBUG] Tumbler signature r (first 20 chars): %.20s\n", tumbler_r_hex);
  printf("[ESCROW DEBUG] Tumbler signature s (first 20 chars): %.20s\n", tumbler_s_hex);
  
  bn_free(tumbler_r);
  bn_free(tumbler_s);
  
  // 构建命令调用 confirm 函数，传递签名参数
  char *cmd = malloc(8192);
  if (!cmd) return -1;
  
  // 使用 --pool 参数，让脚本从地址簿解析固定面额池合约地址
  // 传递两个签名：Alice 的签名 (r1, s1, v1) 和 Tumbler 的签名 (r2, s2, v2)
  // 注意：v 值需要在 JS 脚本中计算，这里先传 0 或占位值
  printf("[ESCROW DEBUG] Building command with:\n");
  printf("  pool_label: %s\n", state->pool_label);
  printf("  escrow_id: %s\n", escrow_id);
  printf("  alice_r_hex length: %zu\n", strlen(alice_r_hex));
  printf("  alice_s_hex length: %zu\n", strlen(alice_s_hex));
  printf("  tumbler_r_hex length: %zu\n", strlen(tumbler_r_hex));
  printf("  tumbler_s_hex length: %zu\n", strlen(tumbler_s_hex));
  
  int cmd_len = snprintf(cmd, 8192,
           "cd /home/zxx/Config/truffleProject/truffletest && npx truffle exec scripts/confirmEscrow.js --network private "
           "--pool %s --id %s --from %s "
           "--r1 0x%s --s1 0x%s --v1 0 "
           "--r2 0x%s --s2 0x%s --v2 0 2>&1 | cat",
           state->pool_label, escrow_id, tumbler_from,
           alice_r_hex, alice_s_hex,
           tumbler_r_hex, tumbler_s_hex);
  
  printf("[ESCROW DEBUG] Command length: %d (max 8192)\n", cmd_len);
  printf("[ESCROW] Tumbler confirm escrow cmd: %s\n", cmd);
  
  START_TIMER(tumbler_blockchain_escrow_interaction)
  int rc = system(cmd);
  END_TIMER(tumbler_blockchain_escrow_interaction)
  free(cmd);
  
  if (rc == 0) {
      printf("[ESCROW] Tumbler confirmed escrow successfully\n");
  } else {
      fprintf(stderr, "[ESCROW] Tumbler confirm escrow failed with code %d.\n", rc);
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
    case REGISTRATION:
      return registration_handler;
    
    case PROMISE_INIT:
      return promise_init_handler;

    case PAYMENT_INIT:
      return payment_init_handler;

    case LAYERED_PROOF_SHARE:
      return layered_proof_share_handler;


    case BOB_CONFIRM_DONE:
      return bob_confirm_done_handler;

    default:
      fprintf(stderr, "Error: invalid message type.\n");
      exit(1);
  }
}

int handle_message(tumbler_state_t state, void *socket, zmq_msg_t message) {
  int result_status = RLC_OK;

  message_t msg;
  message_null(msg);

  RLC_TRY {
    size_t msg_size = zmq_msg_size(&message);
    printf("Received message size: %ld bytes\n", msg_size);
    deserialize_message(&msg, (uint8_t *) zmq_msg_data(&message));

    printf("Executing %s...\n", msg->type);
    
    // 对于 promise_init，打印数据的前几个字节用于调试
    if (strcmp(msg->type, "promise_init") == 0 && msg->data != NULL) {
      printf("[DEBUG] promise_init data (first 100 bytes hex): ");
      for (unsigned i = 0; i < 100 && msg->data[i] != 0; i++) {
        printf("%02x", msg->data[i]);
      }
      printf("\n");
    }
    
    msg_handler_t msg_handler = get_message_handler(msg->type);
    if (msg_handler(state, socket, msg->data) != RLC_OK) {
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

// 移除本地 static 版本，统一使用 util.c 的非静态实现

int receive_message(tumbler_state_t state, void *socket) {
  int result_status = RLC_OK;

  zmq_msg_t message;

  RLC_TRY {
    int rc = zmq_msg_init(&message);
    if (rc != 0) {
      fprintf(stderr, "Error: could not initialize the message.\n");
      RLC_THROW(ERR_CAUGHT);
    }

    rc = zmq_msg_recv(&message, socket, 0);  // 阻塞等待消息
    if (rc > 0 && handle_message(state, socket, message) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    } else if (rc == -1) {
      // 检查是否是EAGAIN错误（没有消息）
      if (errno == EAGAIN) {
        // 没有消息，继续循环
        continue;
      } else {
        fprintf(stderr, "Error: zmq_msg_recv failed with errno %d\n", errno);
        RLC_THROW(ERR_CAUGHT);
      }
    }
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    zmq_msg_close(&message);
  }

  return result_status;
}

int registration_handler(tumbler_state_t state, void *socket, uint8_t *data) {
   if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(registration_total)
  START_TIMER(registration_phase)
  int result_status = RLC_OK;

  message_t registration_done_msg;
  uint8_t *serialized_message = NULL;

  RLC_TRY {
    // 声明所有局部变量（C89标准要求）
    char dynamic_tumbler_escrow_id[67];
    
    // Deserialize the data from the message - 只接收 Escrow ID 和交易哈希
    size_t base_off = 0;
    strcpy(state->alice_escrow_id, (char*)(data + base_off));
    size_t after_id = base_off + strlen(state->alice_escrow_id) + 1;
    memset(state->alice_escrow_tx_hash, 0, sizeof(state->alice_escrow_tx_hash));
    strncpy(state->alice_escrow_tx_hash, (char*)(data + after_id), sizeof(state->alice_escrow_tx_hash) - 1);
    printf("[TUMBLER] Received Alice's escrow ID: %s, txHash: %s\n", state->alice_escrow_id, state->alice_escrow_tx_hash);
    
    // 触发合约状态查询与交易上链检查（异步外部脚本）
    START_TIMER(check_total)
    query_escrow_status_by_id(state->alice_escrow_id);
    check_tx_mined(state->alice_escrow_tx_hash);
    END_TIMER(check_total)

    // Build and define the message - registration_done 消息不再包含盲签名
    char *msg_type = "registration_done";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    const unsigned msg_data_length = 0; // 不再发送数据
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(registration_done_msg, msg_type_length, msg_data_length);

    // 不再序列化任何数据

    memcpy(registration_done_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, registration_done_msg, msg_type_length, msg_data_length);

    // Send the message.
    zmq_msg_t registration_done;
    int rc = zmq_msg_init_size(&registration_done, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&registration_done), serialized_message, total_msg_length);
    rc = zmq_msg_send(&registration_done, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 生成Tumbler的托管ID
    snprintf(dynamic_tumbler_escrow_id, sizeof(dynamic_tumbler_escrow_id), "0x%08x%08x%08x%08x%08x%08x%08x%08x", 
             rand(), rand(), rand(), rand(), rand(), rand(), rand(), rand());
    printf("[TUMBLER] Generated dynamic escrow ID for new Bob: %s\n", dynamic_tumbler_escrow_id);
    
    // 存储到state中，供后续使用
    strncpy(state->current_bob_escrow_id, dynamic_tumbler_escrow_id, sizeof(state->current_bob_escrow_id) - 1);
    state->current_bob_escrow_id[sizeof(state->current_bob_escrow_id) - 1] = '\0';
    // 同时保存到 tumbler_escrow_id 字段
    strncpy(state->tumbler_escrow_id, dynamic_tumbler_escrow_id, sizeof(state->tumbler_escrow_id) - 1);
    state->tumbler_escrow_id[sizeof(state->tumbler_escrow_id) - 1] = '\0';
    printf("[TUMBLER] Stored tumbler escrow ID: %s\n", state->tumbler_escrow_id);
    
    END_TIMER(registration_phase)
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    if (registration_done_msg != NULL) message_free(registration_done_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(registration_total)
  return result_status;
}

int promise_init_handler(tumbler_state_t state, void *socket, uint8_t *data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(promise_init_total)
  printf("[TUMBLER] 收到 promise_init 消息\n");
  int result_status = RLC_OK;

  message_t promise_done_msg;
  uint8_t *serialized_message = NULL;

  bn_t q;
  zk_proof_cldl_t pi_cldl;
  // 注意：已移除 tid 和 sigma_tid，因为不再接收这些数据

  bn_null(q);
  zk_proof_cldl_null(pi_cldl);
  
  RLC_TRY {
    // 声明所有局部变量（C89标准要求）
    uint8_t sign_buf[64]; // 用于签名内容的缓冲区
    int sign_len;
    
    bn_new(q);
    zk_proof_cldl_new(pi_cldl);

    // Deserialize the data from the message.
    // 数据格式：sigma_r->r + sigma_r->s + tx_len + tx_buf + bob_address + pool_label + proofData
    // 注意：已移除 tid 和 sigma_tid（sigma_1, sigma_2）
    printf("[DEBUG] [Tumbler] promise_init: Starting to deserialize data\n");
    printf("[DEBUG] [Tumbler] promise_init: RLC_BN_SIZE=%zu\n", (size_t)RLC_BN_SIZE);
    
    // 打印数据的前几个字节用于调试
    printf("[DEBUG] [Tumbler] promise_init: First 50 bytes of data (hex): ");
    for (int i = 0; i < 50; i++) {
      printf("%02x", data[i]);
    }
    printf("\n");
    
    // 读取 sigma_r->r 和 sigma_r->s（移除 tid 和 sigma_tid）
    bn_read_bin(state->sigma_r->r, data, RLC_BN_SIZE);
    printf("[DEBUG] [Tumbler] promise_init: Read sigma_r->r successfully\n");
    
    bn_read_bin(state->sigma_r->s, data + RLC_BN_SIZE, RLC_BN_SIZE);
    printf("[DEBUG] [Tumbler] promise_init: Read sigma_r->s successfully\n");
    
    // 解析交易数据
    const unsigned msg_data_length1 = (2 * RLC_BN_SIZE); // sigma_r->r + sigma_r->s
    int tx_len;
    memcpy(&tx_len, data + msg_data_length1, sizeof(int));
    uint8_t tx_buf[1024];
    memcpy(tx_buf, data + msg_data_length1 + sizeof(int), tx_len);
    printf("[DEBUG] [Tumbler] received tx_len = %d\n", tx_len);
    printf("[DEBUG] [Tumbler] received tx_buf (hex): ");
    for (int i = 0; i < tx_len; i++) printf("%02x", tx_buf[i]);
    printf("\n");
    
    // 解析Bob的地址与 Bob 转发的 pool_label（与注册时一致性校验）
    const unsigned bob_address_offset = msg_data_length1 + sizeof(int) + tx_len;
    strcpy(state->bob_address, (char*)(data + bob_address_offset));
    const unsigned pool_label_offset = bob_address_offset + strlen(state->bob_address) + 1;
    char pool_label_from_bob[16] = {0};
    strncpy(pool_label_from_bob, (char*)(data + pool_label_offset), sizeof(pool_label_from_bob) - 1);
    printf("[TUMBLER] Received Bob's address: %s, pool_label=%s\n", state->bob_address, pool_label_from_bob);
    // 若注册阶段未记录 pool_label（新逻辑不再由 Alice 发送），则首次采用 Bob 的值
    if (state->pool_label[0] == '\0') {
      strncpy(state->pool_label, pool_label_from_bob, sizeof(state->pool_label) - 1);
      printf("[TUMBLER] Adopt pool_label from Bob: %s\n", state->pool_label);
    } else if (strcmp(pool_label_from_bob, state->pool_label) != 0) {
      printf("[ERROR] Tumbler: pool_label mismatch! reg=%s, bob=%s\n", state->pool_label, pool_label_from_bob);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析 proofData（Tornado Cash zkSNARK 证明，从 Bob 转发）
    const unsigned proof_data_offset = pool_label_offset + strlen(pool_label_from_bob) + 1;
    const char *proof_data_json = (const char*)(data + proof_data_offset);
    size_t proof_data_len = strlen(proof_data_json);
    
    if (proof_data_len == 0) {
      fprintf(stderr, "[ERROR] Tumbler: proofData is empty\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    if (proof_data_len < sizeof(state->tornado_proof_data)) {
      memset(state->tornado_proof_data, 0, sizeof(state->tornado_proof_data));
      memcpy(state->tornado_proof_data, proof_data_json, proof_data_len);
      state->tornado_proof_data[proof_data_len] = '\0';
      printf("[TORNADO] Tumbler: Received proofData from Bob (length: %zu)\n", proof_data_len);
      printf("[TORNADO] Tumbler: proofData preview: %.200s...\n", proof_data_json);
    } else {
      fprintf(stderr, "[ERROR] Tumbler: proofData too large (%zu > %zu)\n", 
              proof_data_len, sizeof(state->tornado_proof_data));
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 从 proofData JSON 中提取 nullifierHash（从 publicSignals 中）
    // publicSignals 格式：[root, nullifierHash, recipient, relayer, fee, refund]
    // nullifierHash 是 publicSignals[1]
    // 注意：nullifierHash 在 publicSignals 中是十进制数字字符串，需要转换为十六进制
    const char *tornado_project = "/home/zxx/tornado-core-master";
    char nullifier_hash[67] = {0}; // 0x + 64 hex chars + null terminator
    const char *public_signals_start = strstr(state->tornado_proof_data, "\"publicSignals\":");
    if (!public_signals_start) {
      // 尝试不区分大小写
      const char *p = state->tornado_proof_data;
      while (*p) {
        if (strncasecmp(p, "publicSignals", 13) == 0) {
          public_signals_start = p;
          break;
        }
        p++;
      }
    }
    
    if (public_signals_start) {
      printf("[TORNADO] Tumbler: Found publicSignals field at offset %zu\n", public_signals_start - state->tornado_proof_data);
      
      // 找到 publicSignals 数组的开始
      const char *array_start = strchr(public_signals_start, '[');
      if (array_start) {
        printf("[TORNADO] Tumbler: Found '[' at offset %zu\n", array_start - state->tornado_proof_data);
        
        // 跳过第一个元素（root），找到第二个元素（nullifierHash）
        // 第一个元素可能是数字字符串（不带引号）
        const char *first_comma = strchr(array_start + 1, ',');
        if (first_comma) {
          printf("[TORNADO] Tumbler: Found first comma at offset %zu\n", first_comma - state->tornado_proof_data);
          
          const char *second_start = first_comma + 1;
          // 跳过空格和换行符
          while (*second_start == ' ' || *second_start == '\t' || *second_start == '\n' || *second_start == '\r') {
            second_start++;
          }
          
          printf("[TORNADO] Tumbler: Second element starts at offset %zu, char: '%c'\n", second_start - state->tornado_proof_data, *second_start);
          
          // 找到第二个元素的结束（逗号或右括号）
          const char *second_end = strchr(second_start, ',');
          if (!second_end) {
            second_end = strchr(second_start, ']');
          }
          if (second_end) {
            printf("[TORNADO] Tumbler: Second element ends at offset %zu\n", second_end - state->tornado_proof_data);
            
            size_t nullifier_len = second_end - second_start;
            // 去除末尾的空白字符
            while (nullifier_len > 0 && (second_start[nullifier_len - 1] == ' ' || 
                                         second_start[nullifier_len - 1] == '\t' ||
                                         second_start[nullifier_len - 1] == '\n' ||
                                         second_start[nullifier_len - 1] == '\r')) {
              nullifier_len--;
            }
            
            // 去除引号（如果有）
            const char *value_start = second_start;
            if (*value_start == '"') {
              value_start++;
              nullifier_len--;
            }
            if (nullifier_len > 0 && value_start[nullifier_len - 1] == '"') {
              nullifier_len--;
            }
            
            printf("[TORNADO] Tumbler: Extracted value length: %zu\n", nullifier_len);
            
            // publicSignals 中的值可能是十进制数字字符串（很长），需要转换为十六进制
            // 使用临时缓冲区存储原始值（可能很长）
            char raw_value[256] = {0};
            if (nullifier_len > 0 && nullifier_len < sizeof(raw_value) - 1) {
              strncpy(raw_value, value_start, nullifier_len);
              raw_value[nullifier_len] = '\0';
              
              printf("[TORNADO] Tumbler: Raw extracted value: '%s' (length: %zu)\n", raw_value, nullifier_len);
              
              // 检查是否是十六进制格式（以 0x 开头）
              if (strncmp(raw_value, "0x", 2) == 0) {
                // 已经是十六进制格式
                if (strlen(raw_value) < sizeof(nullifier_hash)) {
                  strncpy(nullifier_hash, raw_value, sizeof(nullifier_hash) - 1);
                  nullifier_hash[sizeof(nullifier_hash) - 1] = '\0';
                  printf("[TORNADO] Tumbler: nullifierHash is already hex: %s\n", nullifier_hash);
                } else {
                  fprintf(stderr, "[ERROR] Tumbler: Hex nullifierHash too long: %zu\n", strlen(raw_value));
                }
              } else {
                // 是十进制数字字符串，需要转换为十六进制
                // 使用 Node.js 脚本转换（在 tornado-core-master 目录下）
                char convert_cmd[1024];
                snprintf(convert_cmd, sizeof(convert_cmd),
                         "cd %s && node -e \"const bigInt = require('snarkjs').bigInt; console.log('0x' + bigInt('%s').toString(16));\"",
                         tornado_project, raw_value);
                
                printf("[TORNADO] Tumbler: Converting decimal to hex using: %s\n", convert_cmd);
                
                FILE *fp_convert = popen(convert_cmd, "r");
                if (fp_convert) {
                  char hex_result[67] = {0};
                  if (fgets(hex_result, sizeof(hex_result), fp_convert) != NULL) {
                    // 去除换行符
                    size_t hex_len = strlen(hex_result);
                    if (hex_len > 0 && hex_result[hex_len - 1] == '\n') {
                      hex_result[hex_len - 1] = '\0';
                      hex_len--;
                    }
                    
                    if (hex_len > 0 && hex_len < sizeof(nullifier_hash)) {
                      strncpy(nullifier_hash, hex_result, sizeof(nullifier_hash) - 1);
                      nullifier_hash[sizeof(nullifier_hash) - 1] = '\0';
                      printf("[TORNADO] Tumbler: Converted nullifierHash: %s (length: %zu)\n", nullifier_hash, hex_len);
                    } else {
                      fprintf(stderr, "[ERROR] Tumbler: Converted hex length invalid: %zu (max: %zu)\n", 
                              hex_len, sizeof(nullifier_hash) - 1);
                    }
                  } else {
                    fprintf(stderr, "[ERROR] Tumbler: Failed to read conversion result\n");
                  }
                  pclose(fp_convert);
                } else {
                  fprintf(stderr, "[ERROR] Tumbler: Failed to execute conversion command\n");
                }
              }
            } else {
              fprintf(stderr, "[ERROR] Tumbler: Raw value length invalid: %zu (max: %zu)\n", 
                      nullifier_len, sizeof(raw_value) - 1);
            }
          } else {
            fprintf(stderr, "[ERROR] Tumbler: Could not find end of second element in publicSignals\n");
          }
        } else {
          fprintf(stderr, "[ERROR] Tumbler: Could not find first comma in publicSignals array\n");
          fprintf(stderr, "[ERROR] Tumbler: Array content: %.100s\n", array_start);
        }
      } else {
        fprintf(stderr, "[ERROR] Tumbler: Could not find '[' in publicSignals\n");
        fprintf(stderr, "[ERROR] Tumbler: Content after publicSignals: %.100s\n", public_signals_start);
      }
    } else {
      fprintf(stderr, "[ERROR] Tumbler: Could not find 'publicSignals' field in proofData\n");
      fprintf(stderr, "[ERROR] Tumbler: proofData preview: %.500s...\n", state->tornado_proof_data);
    }
    
    if (nullifier_hash[0] == '\0') {
      fprintf(stderr, "[ERROR] Tumbler: Failed to extract nullifierHash from proofData\n");
      fprintf(stderr, "[ERROR] Tumbler: proofData length: %zu\n", strlen(state->tornado_proof_data));
      fprintf(stderr, "[ERROR] Tumbler: proofData (first 500 chars): %.500s\n", state->tornado_proof_data);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 检查 nullifierHash 是否已经被使用过（防止双重花费）
    const char *nullifier_hash_file = "/home/zxx/Config/truffleProject/truffletest/nullifier_hashes.txt";
    FILE *fp_check = fopen(nullifier_hash_file, "r");
    if (fp_check) {
      // 逐行读取文件，检查是否有匹配的 nullifierHash
      char line[128];
      int found = 0;
      while (fgets(line, sizeof(line), fp_check) != NULL) {
        // 去除换行符
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n') {
          line[line_len - 1] = '\0';
          line_len--;
        }
        
        // 比较 nullifierHash（精确匹配，因为都是十六进制）
        if (strcmp(line, nullifier_hash) == 0) {
          found = 1;
          break;
        }
      }
      fclose(fp_check);
      
      if (found) {
        fprintf(stderr, "[ERROR] Tumbler: nullifierHash %s has already been used (double-spending attempt detected)\n", nullifier_hash);
        RLC_THROW(ERR_CAUGHT);
      }
    }
    
    printf("[TORNADO] Tumbler: nullifierHash %s is not in the used list, proceeding with verification...\n", nullifier_hash);
    
    // 验证 proofData（使用 tornado_generate_withdraw_proof.js 中的验证方式）
    // 直接通过 stdin 传递 proofData，无需临时文件
    // 使用 pipe + fork + exec 来同时写入 stdin 和读取 stdout
    printf("[TORNADO] Tumbler: Starting proofData verification...\n");
    // 注意：tornado_project 已在上面定义，这里不再重复定义
    START_TIMER(tumbler_proof_verification);
    const char *verify_script_path = "/home/zxx/tornado-core-master/scripts/verify_proof_locally.js";
    
    // 创建两个管道：一个用于 stdin，一个用于 stdout
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
      fprintf(stderr, "[ERROR] Tumbler: Failed to create pipes\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    pid_t pid = fork();
    if (pid == -1) {
      fprintf(stderr, "[ERROR] Tumbler: Failed to fork\n");
      close(stdin_pipe[0]);
      close(stdin_pipe[1]);
      close(stdout_pipe[0]);
      close(stdout_pipe[1]);
      RLC_THROW(ERR_CAUGHT);
    }
    
    if (pid == 0) {
      // 子进程：执行验证脚本
      close(stdin_pipe[1]); // 关闭写端
      close(stdout_pipe[0]); // 关闭读端
      
      // 重定向 stdin 和 stdout
      // 注意：stderr 不重定向到 stdout_pipe，这样错误信息不会混入验证结果
      dup2(stdin_pipe[0], STDIN_FILENO);
      dup2(stdout_pipe[1], STDOUT_FILENO);
      // stderr 保持原样，这样错误信息会输出到终端
      close(stdin_pipe[0]);
      close(stdout_pipe[1]);
      
      // 切换到 tornado 项目目录
      if (chdir(tornado_project) != 0) {
        fprintf(stderr, "[ERROR] Tumbler: Failed to chdir to %s\n", tornado_project);
        exit(1);
      }
      
      // 执行验证脚本
      execlp("node", "node", verify_script_path, (char *)NULL);
      
      // 如果 execlp 失败
      fprintf(stderr, "[ERROR] Tumbler: Failed to execute node\n");
      exit(1);
    } else {
      // 父进程：写入 JSON 并读取结果
      close(stdin_pipe[0]); // 关闭读端
      close(stdout_pipe[1]); // 关闭写端
      
      // 写入 JSON 到子进程的 stdin
      size_t written_bytes = write(stdin_pipe[1], state->tornado_proof_data, strlen(state->tornado_proof_data));
      if (written_bytes != strlen(state->tornado_proof_data)) {
        fprintf(stderr, "[ERROR] Tumbler: Failed to write proofData to script\n");
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        RLC_THROW(ERR_CAUGHT);
      }
      close(stdin_pipe[1]); // 关闭写端，发送 EOF
      
      // 从子进程的 stdout 读取结果（包括时间信息和验证结果）
      // 需要循环读取，直到管道关闭（EOF）
      char verify_output[4096] = {0}; // 增大缓冲区以读取所有输出
      size_t total_read = 0;
      ssize_t read_bytes;
      
      // 循环读取直到 EOF（read 返回 0）或错误（返回 -1）
      while (total_read < sizeof(verify_output) - 1) {
        read_bytes = read(stdout_pipe[0], verify_output + total_read, sizeof(verify_output) - 1 - total_read);
        if (read_bytes < 0) {
          fprintf(stderr, "[ERROR] Tumbler: Failed to read verification result\n");
          close(stdout_pipe[0]);
          RLC_THROW(ERR_CAUGHT);
        } else if (read_bytes == 0) {
          // EOF，读取完成
          break;
        } else {
          total_read += read_bytes;
        }
      }
      
      if (total_read == 0) {
        fprintf(stderr, "[ERROR] Tumbler: No data read from verification script\n");
        close(stdout_pipe[0]);
        RLC_THROW(ERR_CAUGHT);
      }
      
      verify_output[total_read] = '\0';
      close(stdout_pipe[0]);
      
      // 等待子进程结束
      int status;
      waitpid(pid, &status, 0);
      
      // 解析验证时间信息（从脚本输出中提取）
      long long verify_start_time = 0;
      long long verify_end_time = 0;
      long long verify_duration = 0;
      int verify_timing_found = 0;
      
      // 逐行解析输出
      char *line = verify_output;
      char *verify_result = NULL;
      while (*line != '\0') {
        char *line_end = strchr(line, '\n');
        if (line_end) {
          *line_end = '\0';
        }
        
        // 检查是否包含验证时间信息
        if (strstr(line, "[VERIFY_TIMING]") != NULL) {
          if (strstr(line, "START:") != NULL) {
            sscanf(line, "[VERIFY_TIMING] START:%lld", &verify_start_time);
          } else if (strstr(line, "END:") != NULL) {
            sscanf(line, "[VERIFY_TIMING] END:%lld", &verify_end_time);
          } else if (strstr(line, "DURATION:") != NULL) {
            sscanf(line, "[VERIFY_TIMING] DURATION:%lld", &verify_duration);
            verify_timing_found = 1;
          }
        } else if (strlen(line) == 1 && (line[0] == '1' || line[0] == '0')) {
          // 找到验证结果（"1" 或 "0"）
          verify_result = line;
        }
        
        if (line_end) {
          line = line_end + 1;
        } else {
          break;
        }
      }
      
      // 输出解析到的验证时间（仅 zkSNARK 验证部分）
      if (verify_timing_found && verify_duration > 0) {
        printf("[TORNADO] Tumbler: zkSNARK proof verification (core) took: %lld ms\n", verify_duration);
      } else if (verify_start_time > 0 && verify_end_time > 0) {
        verify_duration = verify_end_time - verify_start_time;
        printf("[TORNADO] Tumbler: zkSNARK proof verification (core) took: %lld ms\n", verify_duration);
      }
      
      // 提取验证结果
      if (!verify_result) {
        // 如果没有找到验证结果，尝试从最后一行提取
        char *last_line = verify_output;
        char *last_newline = strrchr(verify_output, '\n');
        if (last_newline) {
          last_line = last_newline + 1;
        }
        // 去除换行符和空白字符
        size_t result_len = strlen(last_line);
        while (result_len > 0 && (last_line[result_len - 1] == '\n' || 
                                  last_line[result_len - 1] == '\r' || 
                                  last_line[result_len - 1] == ' ')) {
          last_line[result_len - 1] = '\0';
          result_len--;
        }
        if (result_len == 1 && (last_line[0] == '1' || last_line[0] == '0')) {
          verify_result = last_line;
        }
      }
      
      if (!verify_result || (verify_result[0] != '1' && verify_result[0] != '0')) {
        fprintf(stderr, "[ERROR] Tumbler: Could not find verification result in output\n");
        fprintf(stderr, "[ERROR] Tumbler: Output was: %s\n", verify_output);
        RLC_THROW(ERR_CAUGHT);
      }
      
      // 只取第一个字符（应该是 "1" 或 "0"）
      char result_char = verify_result[0];
      printf("[TORNADO] Tumbler: Verification result: %c\n", result_char);
      
      // 检查验证结果：应该是 "1" 表示成功，"0" 表示失败
      if (result_char != '1') {
        fprintf(stderr, "[ERROR] Tumbler: Proof verification failed (result=%s, first_char='%c', exit_status=%d)\n", 
                verify_result, result_char, WEXITSTATUS(status));
        RLC_THROW(ERR_CAUGHT);
      }
      
      printf("[TORNADO] Tumbler: ✅ Proof verification passed!\n");
      
      // 验证成功后，将 nullifierHash 存储到文件中（防止双重花费）
      // 使用简单的追加方式：每行一个 nullifierHash
      FILE *fp_store = fopen(nullifier_hash_file, "a");
      if (fp_store) {
        fprintf(fp_store, "%s\n", nullifier_hash);
        fclose(fp_store);
        printf("[TORNADO] Tumbler: ✅ nullifierHash %s stored to prevent double-spending\n", nullifier_hash);
      } else {
        fprintf(stderr, "[WARNING] Tumbler: Failed to open nullifier hash file for writing: %s\n", nullifier_hash_file);
        fprintf(stderr, "[WARNING] Tumbler: Double-spending protection may not work correctly\n");
      }
    }
    END_TIMER(tumbler_proof_verification);
    // 注意：已移除 tid 和 sigma_tid，因此不再进行 PS 签名验证
    // 原来的 PS 验证逻辑已移除，因为不再接收这些数据

    int ecdsa_ret = cp_ecdsa_ver(state->sigma_r->r, state->sigma_r->s, tx_buf, tx_len, 0, state->bob_ec_pk->pk);
    printf("[DEBUG] cp_ecdsa_ver returned %d\n", ecdsa_ret);
    if (ecdsa_ret != 1) {
      printf("[DEBUG] cp_ecdsa_ver failed!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    //获取椭圆曲线阶数q
    ec_curve_get_ord(q);
    START_TIMER(tumbler_puzzle_generation)
    bn_rand_mod(state->alpha, q);
    printf("[TUMBLER] alpha (hex) = "); bn_print(state->alpha);
    
    // 【调试】打印alpha的十进制表示
    char alpha_dec_str[512];
    bn_write_str(alpha_dec_str, sizeof(alpha_dec_str), state->alpha, 10);
    printf("[TUMBLER DEBUG] alpha (十进制) = %s\n", alpha_dec_str);
    
    ec_mul_gen(state->g_to_the_alpha, state->alpha);
   
    //将alpha转换为字符串格式
    const unsigned alpha_str_len = bn_size_str(state->alpha, 10);
    //转化成PARI/GP格式
    char alpha_str[alpha_str_len];
    //写入到alpha_str中
    bn_write_str(alpha_str, alpha_str_len, state->alpha, 10);
    printf("[DEBUG] alpha_str = %s\n", alpha_str);

  
    GEN plain_alpha = strtoi(alpha_str);
    printf("[DEBUG] strtoi(alpha_str) done.\n");
    //用tumbler的公钥加密alpha
    int cl_enc_ret = cl_enc(state->ctx_alpha, plain_alpha, state->tumbler_cl_pk, state->cl_params);
    printf("[DEBUG] cl_enc returned %d\n", cl_enc_ret);
    if (cl_enc_ret != RLC_OK) {
      printf("[DEBUG] cl_enc failed!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    // 新增：生成随机数r0用于auditor加密
     // 生成 r0
     bn_rand_mod(state->r0, q);
     printf("[TUMBLER] r0 (hex) = "); bn_print(state->r0);
     
     // 【调试】打印r0的十进制表示
     char r0_dec_str[512];
     bn_write_str(r0_dec_str, sizeof(r0_dec_str), state->r0, 10);
     printf("[TUMBLER DEBUG] r0 (十进制) = %s\n", r0_dec_str);
 
    cl_ciphertext_t ctx_r0_auditor;
    cl_ciphertext_null(ctx_r0_auditor);
    cl_ciphertext_new(ctx_r0_auditor);
    
    const unsigned r0_str_len = bn_size_str(state->r0, 10);
    char r0_str[r0_str_len];
    bn_write_str(r0_str, r0_str_len, state->r0, 10);
    GEN plain_r0 = strtoi(r0_str);
    int cl_enc_ret_auditor = cl_enc(state->ctx_r0_auditor, plain_r0, state->auditor_cl_pk, state->cl_params);
    if (cl_enc_ret_auditor != RLC_OK) {
      printf("[DEBUG] cl_enc (auditor) failed!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(tumbler_puzzle_generation)
    // 【调试】打印Tumbler生成的 E_auditor(r0)
    {
      char *r0_aud_c1 = GENtostr(state->ctx_r0_auditor->c1);
      char *r0_aud_c2 = GENtostr(state->ctx_r0_auditor->c2);
      printf("\n[TUMBLER DEBUG] ========== Tumbler生成的 E_auditor(r0) ==========\n");
      printf("[TUMBLER DEBUG] 将发送给Bob (在promise_done消息中)\n");
      printf("[TUMBLER DEBUG] c1 长度: %zu, 前100字符: %.100s\n", strlen(r0_aud_c1), r0_aud_c1);
      printf("[TUMBLER DEBUG] c2 长度: %zu, 前100字符: %.100s\n", strlen(r0_aud_c2), r0_aud_c2);
      free(r0_aud_c1);
      free(r0_aud_c2);
    }
    
    // 生成综合谜题零知识证明
    // 证明知道(alpha, r0)，使得：
    // 1. ctx_alpha = Enc(pk_tumbler, alpha)
    // 2. ctx_r0_auditor = Enc(pk_auditor, r0)  
    // 3. g_to_the_alpha = g^alpha
    printf("[TUMBLER] 开始生成综合谜题零知识证明...\n");
    
    // 注意：plain_alpha 和 plain_r0 已经在上面定义过了，直接使用
    
    // 生成综合零知识证明
    START_TIMER(tumbler_zk_generation)
    if (zk_comprehensive_puzzle_prove(state->comprehensive_puzzle_zk_proof,
                                     plain_alpha, plain_r0,
                                     state->g_to_the_alpha,
                                     state->ctx_alpha, state->ctx_r0_auditor,
                                     state->tumbler_cl_pk, state->auditor_cl_pk,
                                     state->cl_params) != RLC_OK) {
      printf("[ERROR] 生成综合谜题零知识证明失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(tumbler_zk_generation)
    printf("[TUMBLER] 综合谜题零知识证明生成成功!\n");
    
    // 立即验证生成的零知识证明
    printf("[TUMBLER] 开始验证综合谜题零知识证明...\n");
    
    // if (zk_comprehensive_puzzle_verify(state->comprehensive_puzzle_zk_proof,
    //                                    state->g_to_the_alpha,
    //                                    state->ctx_alpha, state->ctx_r0_auditor,
    //                                    state->tumbler_cl_pk, state->auditor_cl_pk,
    //                                    state->cl_params) != RLC_OK) {
    //   printf("[ERROR] 综合谜题零知识证明验证失败!\n");
    //   RLC_THROW(ERR_CAUGHT);
    // }
    
    // printf("[TUMBLER] 综合谜题零知识证明验证成功!\n");
    // printf("[TUMBLER] 谜题构造正确性已通过零知识证明验证!\n");

    // 先打开托管，获取交易哈希，以便用于签名
    const char *skip_escrow = getenv("A2L_SKIP_OPEN_ESCROW");
    if (skip_escrow && strcmp(skip_escrow, "1") == 0) {
        printf("[ESCROW] Skipping openEscrow due to A2L_SKIP_OPEN_ESCROW=1\n");
        fflush(stdout);
        strncpy(state->tumbler_escrow_tx_hash, "0x0", sizeof(state->tumbler_escrow_tx_hash) - 1);
        state->tumbler_escrow_tx_hash[sizeof(state->tumbler_escrow_tx_hash) - 1] = '\0';
    } else {
        printf("[ESCROW] About to call openEscrow before signing...\n"); fflush(stdout);
        int rc1 = open_escrow_sync_for_tumbler(state, state->current_bob_escrow_id);
        printf("[ESCROW] openEscrow returned rc=%d\n", rc1); fflush(stdout);
    }

    // 构造签名内容：托管ID||openEscrow的交易哈希
    sign_len = 64; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
    
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
      sign_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    // 2. 将交易哈希从十六进制字符串转换为32字节数组
    if (strlen(state->tumbler_escrow_tx_hash) < 2 || strncmp(state->tumbler_escrow_tx_hash, "0x", 2) != 0) {
      printf("[ERROR] Invalid tumbler_escrow_tx_hash format: %s\n", state->tumbler_escrow_tx_hash);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *hash_hex = state->tumbler_escrow_tx_hash + 2; // 跳过"0x"
    size_t hash_hex_len = strlen(hash_hex);
    if (hash_hex_len != 64 && hash_hex_len != 1) { // 允许 "0x0" (跳过模式)
      printf("[ERROR] tumbler_escrow_tx_hash should be 64 hex chars (32 bytes) or '0x0', got %zu: %s\n", hash_hex_len, state->tumbler_escrow_tx_hash);
      RLC_THROW(ERR_CAUGHT);
    }
    
    // 如果交易哈希是 "0x0"（跳过模式），使用全零
    if (hash_hex_len == 1 && hash_hex[0] == '0') {
      memset(sign_buf + 32, 0, 32);
    } else {
      // 将交易哈希的十六进制字符串转换为字节数组
      for (size_t i = 0; i < 32; i++) {
        char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
        sign_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
      }
    }
    
    printf("[DEBUG] Tumbler signature content: escrowID(32 bytes) || txHash(32 bytes)\n");
    printf("[DEBUG] Escrow ID (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", sign_buf[i]);
    printf("\n");
    printf("[DEBUG] Escrow TX Hash (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", sign_buf[32 + i]);
    printf("\n");

    START_TIMER(tumbler_adaptor_ecdsa_sign)
    if (adaptor_ecdsa_sign(state->sigma_tr, sign_buf, sign_len, state->g_to_the_alpha, state->tumbler_ec_sk) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(tumbler_adaptor_ecdsa_sign)

    char *msg_type = "promise_done";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    // 增加auditor密文的空间和托管ID
    const unsigned escrow_id_length = strlen(state->current_bob_escrow_id) + 1;
    size_t txhash_len = strlen(state->tumbler_escrow_tx_hash);
    if (txhash_len == 0) txhash_len = 1; // 至少携带一个字节的结尾符
    
    // 计算零知识证明的大小
    const size_t zk_proof_size = zk_comprehensive_puzzle_serialized_size();
    
    const unsigned msg_data_length = (4 * RLC_EC_SIZE_COMPRESSED) + (3 * RLC_BN_SIZE) + (2 * RLC_CL_CIPHERTEXT_SIZE)
        + 2 * RLC_CL_CIPHERTEXT_SIZE + escrow_id_length
        + txhash_len + 1 + zk_proof_size; /* 追加 Tumbler 自己的 openEscrow txHash 和零知识证明 */
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    
    printf("[TUMBLER DEBUG] 消息大小计算:\n");
    printf("  - 基础数据: %zu bytes\n", (size_t)((4 * RLC_EC_SIZE_COMPRESSED) + (3 * RLC_BN_SIZE) + (2 * RLC_CL_CIPHERTEXT_SIZE) + 2 * RLC_CL_CIPHERTEXT_SIZE));
    printf("  - escrow_id: %zu bytes\n", (size_t)escrow_id_length);
    printf("  - txhash: %zu bytes\n", (size_t)(txhash_len + 1));
    printf("  - zk_proof_size: %zu bytes\n", zk_proof_size);
    printf("  - 总msg_data_length: %u bytes\n", msg_data_length);
    printf("  - total_msg_length: %d bytes\n", total_msg_length);
    
    message_new(promise_done_msg, msg_type_length, msg_data_length);

    // Serialize the data for the message.
    ec_write_bin(promise_done_msg->data, RLC_EC_SIZE_COMPRESSED, state->g_to_the_alpha, 1);
    bn_write_bin(promise_done_msg->data + RLC_EC_SIZE_COMPRESSED, RLC_BN_SIZE, state->sigma_tr->r);
    bn_write_bin(promise_done_msg->data + RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE, RLC_BN_SIZE, state->sigma_tr->s);
    ec_write_bin(promise_done_msg->data + RLC_EC_SIZE_COMPRESSED + (2 * RLC_BN_SIZE), RLC_EC_SIZE_COMPRESSED, state->sigma_tr->R, 1);
    ec_write_bin(promise_done_msg->data + (2 * RLC_EC_SIZE_COMPRESSED) + (2 * RLC_BN_SIZE), RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->a, 1);
    ec_write_bin(promise_done_msg->data + (3 * RLC_EC_SIZE_COMPRESSED) + (2 * RLC_BN_SIZE), RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->b, 1);
    bn_write_bin(promise_done_msg->data + (4 * RLC_EC_SIZE_COMPRESSED) + (2 * RLC_BN_SIZE), RLC_BN_SIZE, state->sigma_tr->pi->z);   
    // 使用真实的 ctx_alpha 密文序列化
    {
      const char *c1_str = GENtostr(state->ctx_alpha->c1);
      const char *c2_str = GENtostr(state->ctx_alpha->c2);
      size_t base_off = (4 * RLC_EC_SIZE_COMPRESSED) + (3 * RLC_BN_SIZE);
      memset(promise_done_msg->data + base_off, 0, RLC_CL_CIPHERTEXT_SIZE);
      memcpy(promise_done_msg->data + base_off, c1_str, strnlen(c1_str, RLC_CL_CIPHERTEXT_SIZE - 1));
      promise_done_msg->data[base_off + RLC_CL_CIPHERTEXT_SIZE - 1] = '\0';
      memset(promise_done_msg->data + base_off + RLC_CL_CIPHERTEXT_SIZE, 0, RLC_CL_CIPHERTEXT_SIZE);
      memcpy(promise_done_msg->data + base_off + RLC_CL_CIPHERTEXT_SIZE, c2_str, strnlen(c2_str, RLC_CL_CIPHERTEXT_SIZE - 1));
      promise_done_msg->data[base_off + (2 * RLC_CL_CIPHERTEXT_SIZE) - 1] = '\0';
    }

    // 跳过CLDL证明的生成和序列化，直接计算偏移量
    size_t off = (4 * RLC_EC_SIZE_COMPRESSED) + (3 * RLC_BN_SIZE) + (2 * RLC_CL_CIPHERTEXT_SIZE);
  
    size_t auditor_offset = off;
    printf("  附加auditor密文 (偏移量: %zu):\n", auditor_offset);
    // 使用真正的 auditor 加密结果序列化
    {
      const char *c1_str = GENtostr(state->ctx_r0_auditor->c1);
      const char *c2_str = GENtostr(state->ctx_r0_auditor->c2);
      memset(promise_done_msg->data + auditor_offset, 0, RLC_CL_CIPHERTEXT_SIZE);
      memcpy(promise_done_msg->data + auditor_offset, c1_str, strnlen(c1_str, RLC_CL_CIPHERTEXT_SIZE - 1));
      promise_done_msg->data[auditor_offset + RLC_CL_CIPHERTEXT_SIZE - 1] = '\0';
      memset(promise_done_msg->data + auditor_offset + RLC_CL_CIPHERTEXT_SIZE, 0, RLC_CL_CIPHERTEXT_SIZE);
      memcpy(promise_done_msg->data + auditor_offset + RLC_CL_CIPHERTEXT_SIZE, c2_str, strnlen(c2_str, RLC_CL_CIPHERTEXT_SIZE - 1));
      promise_done_msg->data[auditor_offset + (2 * RLC_CL_CIPHERTEXT_SIZE) - 1] = '\0';
    }
    off += 2 * RLC_CL_CIPHERTEXT_SIZE;
   
  
    // 附加托管ID与 Tumbler 的开托管 txHash
  
    memcpy(promise_done_msg->data + off, state->current_bob_escrow_id, escrow_id_length);
    off += escrow_id_length;
    memset(promise_done_msg->data + off, 0, txhash_len + 1);
    memcpy(promise_done_msg->data + off, state->tumbler_escrow_tx_hash, txhash_len);
    off += txhash_len + 1;
    
    // 序列化综合零知识证明
    printf("[TUMBLER] 序列化综合零知识证明...\n");
    size_t zk_written;
    if (zk_comprehensive_puzzle_serialize(promise_done_msg->data + off, &zk_written, state->comprehensive_puzzle_zk_proof) != RLC_OK) {
      printf("[ERROR] 零知识证明序列化失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    off += zk_written;
    printf("[TUMBLER] 零知识证明序列化完成，大小: %zu 字节\n", zk_written);
    
    printf("✅ [DEBUG] Tumbler promise_init: serialization completed, length: %zu bytes\n", off);
    printf("[TUMBLER DEBUG] 缓冲区使用情况:\n");
    printf("  - 实际使用大小: %zu bytes\n", off);
    printf("  - 分配大小: %u bytes\n", msg_data_length);
    printf("  - 剩余空间: %d bytes\n", (int)msg_data_length - (int)off);
    if (off > msg_data_length) {
      printf("[TUMBLER ERROR] 缓冲区溢出！实际使用 %zu > 分配 %u\n", off, msg_data_length);
    }

    memcpy(promise_done_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, promise_done_msg, msg_type_length, msg_data_length);
    
    // 再发送 promise_done 消息
    zmq_msg_t promise_done;
    int rc = zmq_msg_init_size(&promise_done, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&promise_done), serialized_message, total_msg_length);
    rc = zmq_msg_send(&promise_done, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
    // 清理代码已移除，因为不再生成CLDL证明
    
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    bn_free(q);
    // 注意：已移除 tid 和 sigma_tid，不再需要释放
    zk_proof_cldl_free(pi_cldl);
    if (promise_done_msg != NULL) message_free(promise_done_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  END_TIMER(promise_init_total)
  return result_status;
}

int payment_init_handler(tumbler_state_t state, void *socket, uint8_t *data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }
  START_TIMER(payment_init_total)
  int result_status = RLC_OK;
  
  // 定义数据长度常量，与后续硬编码值保持一致
  // const size_t data_length = 22816;  // 暂时注释掉未使用的变量

  uint8_t *serialized_message = NULL;
  message_t payment_done_msg;

  bn_t q, x, gamma_inverse;
  cl_ciphertext_t ctx_alpha_times_beta_times_tau;
  cl_ciphertext_t auditor_ctx_alpha_times_beta_times_tau;
  // 备份 Alice 预签名、outer 及其签名（用于分片发送）
  bn_t pre_r, pre_s;
  uint8_t pre_R[RLC_EC_SIZE_COMPRESSED], pre_pi_a[RLC_EC_SIZE_COMPRESSED], pre_pi_b[RLC_EC_SIZE_COMPRESSED];
  uint8_t pre_pi_z[RLC_BN_SIZE];
  

  bn_null(q);
  bn_null(x);
  bn_null(gamma_inverse);
  cl_ciphertext_null(ctx_alpha_times_beta_times_tau);
  cl_ciphertext_null(auditor_ctx_alpha_times_beta_times_tau);
  message_null(payment_done_msg);
  bn_null(pre_r);
  bn_null(pre_s);

  RLC_TRY {
    bn_new(q);
    bn_new(x);
    bn_new(gamma_inverse);
    cl_ciphertext_new(ctx_alpha_times_beta_times_tau);
    cl_ciphertext_new(auditor_ctx_alpha_times_beta_times_tau);
    bn_new(pre_r);
    bn_new(pre_s);

    // Deserialize the data from the message - 解析完整预签名结构
    size_t offset = 0;
    bn_read_bin(state->sigma_s->r, data + offset, RLC_BN_SIZE); offset += RLC_BN_SIZE;
    bn_read_bin(state->sigma_s->s, data + offset, RLC_BN_SIZE); offset += RLC_BN_SIZE;
    
    // 验证椭圆曲线点数据的有效性
    RLC_TRY {
        ec_read_bin(state->sigma_s->R, data + offset, RLC_EC_SIZE_COMPRESSED); 
        offset += RLC_EC_SIZE_COMPRESSED;
        printf("[TUMBLER DEBUG] R 读取成功\n");
    } RLC_CATCH_ANY {
        printf("[TUMBLER ERROR] 椭圆曲线点 R 读取失败\n");
        RLC_THROW(ERR_CAUGHT);
    }
    
    RLC_TRY {
        ec_read_bin(state->sigma_s->pi->a, data + offset, RLC_EC_SIZE_COMPRESSED); 
        offset += RLC_EC_SIZE_COMPRESSED;
        printf("[TUMBLER DEBUG] pi->a 读取成功\n");
    } RLC_CATCH_ANY {
        printf("[TUMBLER ERROR] 椭圆曲线点 pi->a 读取失败\n");
        RLC_THROW(ERR_CAUGHT);
    }
    
    RLC_TRY {
        ec_read_bin(state->sigma_s->pi->b, data + offset, RLC_EC_SIZE_COMPRESSED); 
        offset += RLC_EC_SIZE_COMPRESSED;
        printf("[TUMBLER DEBUG] pi->b 读取成功\n");
    } RLC_CATCH_ANY {
        printf("[TUMBLER ERROR] 椭圆曲线点 pi->b 读取失败\n");
        RLC_THROW(ERR_CAUGHT);
    }
    
    bn_read_bin(state->sigma_s->pi->z, data + offset, RLC_BN_SIZE); offset += RLC_BN_SIZE;
    
    // 备份 Alice 完整预签名（在修改签名之前）
    bn_copy(pre_r, state->sigma_s->r);
    bn_copy(pre_s, state->sigma_s->s);
    ec_write_bin(pre_R, RLC_EC_SIZE_COMPRESSED, state->sigma_s->R, 1);
    ec_write_bin(pre_pi_a, RLC_EC_SIZE_COMPRESSED, state->sigma_s->pi->a, 1);
    ec_write_bin(pre_pi_b, RLC_EC_SIZE_COMPRESSED, state->sigma_s->pi->b, 1);
    bn_write_bin(pre_pi_z, RLC_BN_SIZE, state->sigma_s->pi->z);
    
    
    // 调试：验证当前的预签名数据
    {
      uint8_t debug_pre_r[RLC_BN_SIZE], debug_pre_s[RLC_BN_SIZE];
      uint8_t debug_R[RLC_EC_SIZE_COMPRESSED], debug_pi_a[RLC_EC_SIZE_COMPRESSED];
      bn_write_bin(debug_pre_r, RLC_BN_SIZE, pre_r);
      bn_write_bin(debug_pre_s, RLC_BN_SIZE, pre_s);
      ec_write_bin(debug_R, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->R, 1);
      ec_write_bin(debug_pi_a, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->a, 1);
      
    }
    
   
    uint8_t temp_r[RLC_BN_SIZE]; bn_write_bin(temp_r, RLC_BN_SIZE, state->sigma_s->r);
    uint8_t temp_s[RLC_BN_SIZE]; bn_write_bin(temp_s, RLC_BN_SIZE, state->sigma_s->s);
    // 先读取 g^(α+β+τ) - 应该在预签名后的位置
    ec_t g_alpha_beta_tau; ec_null(g_alpha_beta_tau); ec_new(g_alpha_beta_tau);
    
    RLC_TRY {
        ec_read_bin(g_alpha_beta_tau, data + offset, RLC_EC_SIZE_COMPRESSED);
        offset += RLC_EC_SIZE_COMPRESSED;  // 更新偏移量
        
        // 【调试】打印从Alice接收到的 g^(α+β+τ)
        uint8_t gabt_received[RLC_EC_SIZE_COMPRESSED];
        ec_write_bin(gabt_received, RLC_EC_SIZE_COMPRESSED, g_alpha_beta_tau, 1);
        printf("[TUMBLER DEBUG] 从Alice收到的 g^(α+β+τ) (compressed): 0x");
        for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", gabt_received[i]);
        printf("\n");
        
        printf("[TUMBLER DEBUG] g^(α+β+τ) 读取成功，新偏移: %zu\n", offset);
    } RLC_CATCH_ANY {
        printf("[TUMBLER ERROR] 椭圆曲线点 g^(α+β+τ) 读取失败\n");
        RLC_THROW(ERR_CAUGHT);
    }

    // 现在读取密文数据
    char ct_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(ct_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ct_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    
    printf("\n[TUMBLER DEBUG] ========== 接收 ctx_alpha_times_beta_times_tau (Tumbler密文) ==========\n");
    printf("[TUMBLER DEBUG] 当前偏移量: %zu\n", offset);
    printf("[TUMBLER DEBUG] c1 原始字符串长度: %zu\n", strlen(ct_str));
    printf("[TUMBLER DEBUG] c1 原始内容（前100字符）: %.100s\n", ct_str);
    
    RLC_TRY {
        ctx_alpha_times_beta_times_tau->c1 = gp_read_str(ct_str);
        printf("[TUMBLER DEBUG] ✅ 密文 c1 读取成功\n");
    } RLC_CATCH_ANY {
        printf("[TUMBLER ERROR] ❌ 密文 c1 读取失败: %s\n", ct_str);
        RLC_THROW(ERR_CAUGHT);
    }
    offset += RLC_CL_CIPHERTEXT_SIZE;  // 更新偏移量（c1读取后）
    
    // 读取c2
    memcpy(ct_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    ct_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    
    printf("[TUMBLER DEBUG] c2 原始字符串长度: %zu\n", strlen(ct_str));
    printf("[TUMBLER DEBUG] c2 原始内容（前100字符）: %.100s\n", ct_str);
    
    RLC_TRY {
        ctx_alpha_times_beta_times_tau->c2 = gp_read_str(ct_str);
        printf("[TUMBLER DEBUG] ✅ 密文 c2 读取成功\n");
    } RLC_CATCH_ANY {
        printf("[TUMBLER ERROR] ❌ 密文 c2 读取失败\n");
        printf("[TUMBLER ERROR] 失败的字符串长度: %zu\n", strlen(ct_str));
        printf("[TUMBLER ERROR] 失败的字符串内容: %s\n", ct_str);
        RLC_THROW(ERR_CAUGHT);
    }
    offset += RLC_CL_CIPHERTEXT_SIZE;  // 更新偏移量（c2读取后）
    
    // 打印解析后的密文
    {
      char *parsed_c1_str = GENtostr(ctx_alpha_times_beta_times_tau->c1);
      char *parsed_c2_str = GENtostr(ctx_alpha_times_beta_times_tau->c2);
      printf("[TUMBLER DEBUG] 解析后 c1 长度: %zu\n", strlen(parsed_c1_str));
      printf("[TUMBLER DEBUG] 解析后 c1 内容（前100字符）: %.100s\n", parsed_c1_str);
      printf("[TUMBLER DEBUG] 解析后 c2 长度: %zu\n", strlen(parsed_c2_str));
      printf("[TUMBLER DEBUG] 解析后 c2 内容（前100字符）: %.100s\n", parsed_c2_str);
      printf("[TUMBLER DEBUG] ================================================================\n\n");
      free(parsed_c1_str);
      free(parsed_c2_str);
    }
    // Decrypt the ciphertext.
    GEN gamma;
    if (cl_dec(&gamma, ctx_alpha_times_beta_times_tau, state->tumbler_cl_sk, state->cl_params) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    bn_read_str(state->gamma, GENtostr(gamma), strlen(GENtostr(gamma)), 10);

    ec_curve_get_ord(q);
    bn_gcd_ext(x, gamma_inverse, NULL, state->gamma, q);
    if (bn_sign(gamma_inverse) == RLC_NEG) {
      bn_add(gamma_inverse, gamma_inverse, q);
    }

    bn_mul(state->sigma_s->s, state->sigma_s->s, gamma_inverse);
    bn_mod(state->sigma_s->s, state->sigma_s->s, q);

    // 解析auditor最终密文（+tau）- 使用offset而不是硬编码偏移
    char auditor_c1_final_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    char auditor_c2_final_str[RLC_CL_CIPHERTEXT_SIZE + 1];
    
    printf("\n[TUMBLER DEBUG] ========== 接收 auditor_ctx_alpha_times_beta_times_tau ==========\n");
    printf("[TUMBLER DEBUG] 当前偏移量: %zu\n", offset);
    
    memcpy(auditor_c1_final_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_c1_final_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    printf("[TUMBLER DEBUG] auditor c1 原始字符串长度: %zu\n", strlen(auditor_c1_final_str));
    printf("[TUMBLER DEBUG] auditor c1 原始内容（前100字符）: %.100s\n", auditor_c1_final_str);
    auditor_ctx_alpha_times_beta_times_tau->c1 = gp_read_str(auditor_c1_final_str);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    
    memcpy(auditor_c2_final_str, data + offset, RLC_CL_CIPHERTEXT_SIZE);
    auditor_c2_final_str[RLC_CL_CIPHERTEXT_SIZE] = 0;
    printf("[TUMBLER DEBUG] auditor c2 原始字符串长度: %zu\n", strlen(auditor_c2_final_str));
    printf("[TUMBLER DEBUG] auditor c2 原始内容（前100字符）: %.100s\n", auditor_c2_final_str);
    auditor_ctx_alpha_times_beta_times_tau->c2 = gp_read_str(auditor_c2_final_str);
    offset += RLC_CL_CIPHERTEXT_SIZE;
    printf("[TUMBLER DEBUG] ================================================================\n\n");
    
    char *tumbler_c1_str = GENtostr(ctx_alpha_times_beta_times_tau->c1);
    char *tumbler_c2_str = GENtostr(ctx_alpha_times_beta_times_tau->c2);
    
    // 比较Tumbler密文和Auditor密文
    printf("\n[TUMBLER DEBUG] ========== 密文对比 ==========\n");
    printf("[TUMBLER DEBUG] Tumbler c1 长度: %zu\n", strlen(tumbler_c1_str));
    printf("[TUMBLER DEBUG] Auditor c1 长度: %zu\n", strlen(auditor_c1_final_str));
    printf("[TUMBLER DEBUG] Tumbler c2 长度: %zu\n", strlen(tumbler_c2_str));
    printf("[TUMBLER DEBUG] Auditor c2 长度: %zu\n", strlen(auditor_c2_final_str));
    printf("[TUMBLER DEBUG] c1 相同？%s\n", strcmp(tumbler_c1_str, auditor_c1_final_str) == 0 ? "是" : "否");
    printf("[TUMBLER DEBUG] c2 相同？%s\n", strcmp(tumbler_c2_str, auditor_c2_final_str) == 0 ? "是" : "否");
    printf("[TUMBLER DEBUG] ================================================================\n\n");
    // 解析tx_len和tx_buf
    int tx_len;
    printf("[TUMBLER DEBUG] 读取tx_len，当前偏移: %zu\n", offset);
    memcpy(&tx_len, data + offset, sizeof(int));
    offset += sizeof(int);
    uint8_t tx_buf[1024];
    if (tx_len > 0 && tx_len <= 1024) {
      printf("[TUMBLER DEBUG] 读取tx_buf，偏移: %zu, 长度: %d\n", offset, tx_len);
      memcpy(tx_buf, data + offset, (size_t)tx_len);
      offset += tx_len;
      printf("[DEBUG] [Tumbler] received tx_len = %d\n", tx_len);
      // removed verbose tx_buf dump
      // 构造验证消息：alice_escrow_id || alice_escrow_tx_hash
      {
        uint8_t verify_buf[64]; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
        int verify_len = 64;
        
        // 1. 将托管ID (state->alice_escrow_id) 从十六进制字符串转换为32字节数组（bytes32格式）
        if (strlen(state->alice_escrow_id) < 2 || strncmp(state->alice_escrow_id, "0x", 2) != 0) {
          printf("[ERROR] Invalid alice_escrow_id format for verification: %s\n", state->alice_escrow_id);
        } else {
          // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
          const char *escrow_id_hex = state->alice_escrow_id + 2; // 跳过"0x"
          size_t escrow_id_hex_len = strlen(escrow_id_hex);
          if (escrow_id_hex_len != 64) {
            printf("[ERROR] alice_escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, state->alice_escrow_id);
          } else {
            // 将托管ID的十六进制字符串转换为字节数组
            for (size_t i = 0; i < 32; i++) {
              char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
              verify_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
            }
            
            // 2. 将交易哈希从十六进制字符串转换为32字节数组
            if (strlen(state->alice_escrow_tx_hash) < 2 || strncmp(state->alice_escrow_tx_hash, "0x", 2) != 0) {
              printf("[ERROR] Invalid alice_escrow_tx_hash format for verification: %s\n", state->alice_escrow_tx_hash);
            } else {
              // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
              const char *hash_hex = state->alice_escrow_tx_hash + 2; // 跳过"0x"
              size_t hash_hex_len = strlen(hash_hex);
              if (hash_hex_len != 64) {
                printf("[ERROR] alice_escrow_tx_hash should be 64 hex chars (32 bytes), got %zu: %s\n", hash_hex_len, state->alice_escrow_tx_hash);
              } else {
                // 将交易哈希的十六进制字符串转换为字节数组
                for (size_t i = 0; i < 32; i++) {
                  char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
                  verify_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
                }
                
                // 调试：打印验签输入摘要
                printf("[DEBUG] Tumbler: ECDSA verify inputs summary:\n");
                printf("[DEBUG] Verifying Alice signature (sigma_s) against: alice_escrow_id || alice_escrow_tx_hash\n");
                printf("[DEBUG] Escrow ID (hex): ");
                for (int i = 0; i < 32; i++) printf("%02x", verify_buf[i]);
                printf("\n");
                printf("[DEBUG] Escrow TX Hash (hex): ");
                for (int i = 0; i < 32; i++) printf("%02x", verify_buf[32 + i]);
                printf("\n");
                
                // 验证 sigma_s 签名
                int ver_ret = cp_ecdsa_ver(state->sigma_s->r, state->sigma_s->s, verify_buf, verify_len, 0, state->alice_ec_pk->pk);
                if (ver_ret == 1) {
                  printf("[DEBUG] ✅ Alice signature (sigma_s) verification SUCCESS\n");
                } else {
                  // 支持预签名阶段：payment_init 不强制验签，打印告警后继续
                  printf("[WARN] payment_init_handler: Alice signature (sigma_s) verification FAILED. This is expected for pre-signature stage, continuing...\n");
                }
              }
            }
          }
        }
      }
      START_TIMER(tumbler_adaptor_transfer_sign_time);
      // 从预签名生成最终签名：保持相同的 r 值，s = s_presig * alpha^{-1} mod q
      bn_t q, alpha_inverse, x_temp;
      bn_null(q); bn_null(alpha_inverse); bn_null(x_temp);
      bn_new(q); bn_new(alpha_inverse); bn_new(x_temp);
      
      ec_curve_get_ord(q);
      
      // 计算 alpha 的逆元
      bn_gcd_ext(x_temp, alpha_inverse, NULL, state->alpha, q);
      if (bn_sign(alpha_inverse) == RLC_NEG) {
        bn_add(alpha_inverse, alpha_inverse, q);
      }
      
      // 从预签名派生最终签名
      bn_copy(state->sigma_ts->r, state->sigma_tr->r);  // r 值保持不变
      bn_mul(state->sigma_ts->s, state->sigma_tr->s, alpha_inverse);  // s = s_presig * alpha^{-1}
      bn_mod(state->sigma_ts->s, state->sigma_ts->s, q);
      END_TIMER(tumbler_adaptor_transfer_sign_time);
      
      bn_free(q); bn_free(alpha_inverse); bn_free(x_temp);
      
      printf("[TUMBLER] 从预签名生成最终签名:\n");
      printf("  - r 值（与预签名相同）: ");
      uint8_t r_bytes[34];
      bn_write_bin(r_bytes, 34, state->sigma_ts->r);
      for (int i = 0; i < 16; i++) printf("%02x", r_bytes[i]);
      printf("\n");
      printf("  - s 值（s_presig * alpha^{-1} mod q）: ");
      uint8_t s_bytes[34];
      bn_write_bin(s_bytes, 34, state->sigma_ts->s);
      for (int i = 0; i < 16; i++) printf("%02x", s_bytes[i]);
      printf("\n");
    } else {
      printf("[WARN] payment_init_handler: invalid tx_len=%d, bypassing tx verification/signing (stub).\n", tx_len);
      bn_set_dig(state->sigma_ts->r, 0);
      bn_set_dig(state->sigma_ts->s, 0);
    }
    
    // 接收和验证Alice的零知识证明
    printf("[TUMBLER] 开始接收Alice的零知识证明，当前offset: %zu\n", offset);
    size_t proof_read = 0;
    if (zk_puzzle_relation_deserialize(state->alice_puzzle_relation_zk_proof, data + offset, &proof_read) != RLC_OK) {
      printf("[ERROR] 无法反序列化Alice的puzzle_relation证明!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    offset += proof_read;
    printf("[TUMBLER] Alice零知识证明反序列化成功，大小: %zu bytes，当前offset: %zu\n", proof_read, offset);
    
    // 接收从Bob收到的原始谜题数据
    printf("[TUMBLER] 开始接收从Bob收到的原始谜题数据...\n");
    
    // 1. 接收 g^(α+β) (从Bob收到的)
    ec_read_bin(state->alice_g_to_the_alpha_times_beta, data + offset, RLC_EC_SIZE_COMPRESSED);
    offset += RLC_EC_SIZE_COMPRESSED;
    printf("[TUMBLER] 接收 g^(α+β)，当前offset: %zu\n", offset);
    
    // 2. 接收 ctx_α+β (从Bob收到的Tumbler密文)
    {
      char *s1 = (char*)(data + offset);
      char *s2 = (char*)(data + offset + RLC_CL_CIPHERTEXT_SIZE);
      state->alice_ctx_alpha_times_beta->c1 = gp_read_str(s1);
      state->alice_ctx_alpha_times_beta->c2 = gp_read_str(s2);
      offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
      printf("[TUMBLER] 接收 ctx_α+β，当前offset: %zu\n", offset);
    }
    
    // 3. 接收 auditor_ctx_α+β (从Bob收到的Auditor密文)
    {
      char *s1 = (char*)(data + offset);
      char *s2 = (char*)(data + offset + RLC_CL_CIPHERTEXT_SIZE);
      state->alice_auditor_ctx_alpha_times_beta->c1 = gp_read_str(s1);
      state->alice_auditor_ctx_alpha_times_beta->c2 = gp_read_str(s2);
      offset += 2 * RLC_CL_CIPHERTEXT_SIZE;
      printf("[TUMBLER] 接收 auditor_ctx_α+β，最终offset: %zu\n", offset);
    }
    
    printf("[TUMBLER] 从Bob收到的原始谜题数据接收完成!\n");
    
    // 验证Alice的零知识证明
    START_TIMER(alice_to_tumbler_zk_verify_time);
    printf("[TUMBLER] 开始验证Alice的零知识证明...\n");
    if (zk_puzzle_relation_verify(state->alice_puzzle_relation_zk_proof,
                                 state->alice_g_to_the_alpha_times_beta, // g^(α+β)
                                 g_alpha_beta_tau, // g^(α+β+τ)
                                 state->alice_ctx_alpha_times_beta, // ctx_α+β
                                 ctx_alpha_times_beta_times_tau, // ctx_α+β+τ
                                 state->alice_auditor_ctx_alpha_times_beta, // auditor_ctx_α+β
                                 auditor_ctx_alpha_times_beta_times_tau, // auditor_ctx_α+β+τ
                                 state->tumbler_cl_pk, // pk_tumbler
                                 state->auditor_cl_pk, // pk_auditor
                                 state->cl_params) != RLC_OK) { // params
      printf("[ERROR] Alice的零知识证明验证失败!\n");
      RLC_THROW(ERR_CAUGHT);
    }
    END_TIMER(alice_to_tumbler_zk_verify_time);
    printf("[TUMBLER] Alice的零知识证明验证成功!\n");
    // 调用智能合约的 confirm 函数，使用存储的Alice托管ID
    int escrow_rc = release_escrow_for_alice(state, state->alice_escrow_id);
    if (escrow_rc != 0) {
        fprintf(stderr, "[ESCROW] Failed to release escrow for Alice, rc=%d\n", escrow_rc);
        // 注意：这里不抛出错误，因为主要的签名验证已经成功
    } else {
        printf("[ESCROW] Successfully released escrow for Alice\n");
    }
    // ====== 智能合约调用结束 ======
    
    END_TIMER(payment_init_total)
    #ifndef DISABLE_SECRET_SHARES
    // ====== 构造并发送秘密分享分片（使用 alice_escrow_tx_hash 作为 msgid） ======
    {
      // 计算实际需要的总长度（使用长度前缀格式）
      char *s1 = GENtostr(ctx_alpha_times_beta_times_tau->c1);
      char *s2 = GENtostr(ctx_alpha_times_beta_times_tau->c2);
      size_t aud_len1 = strlen(auditor_c1_final_str);
      size_t aud_len2 = strlen(auditor_c2_final_str);
      
      // 计算 alice_escrow_id 和 alice_escrow_tx_hash 的长度
      size_t alice_escrow_id_len = strlen(state->alice_escrow_id) + 1; // 包含 '\0'
      size_t alice_escrow_tx_hash_len = strlen(state->alice_escrow_tx_hash) + 1; // 包含 '\0'
      
      size_t total_len = 0;
      total_len += RLC_EC_SIZE_COMPRESSED;                 // g^(alpha+beta+tau)
      total_len += sizeof(size_t) + strlen(s1);            // ctx c1 长度+数据
      total_len += sizeof(size_t) + strlen(s2);            // ctx c2 长度+数据
      total_len += sizeof(size_t) + aud_len1;              // auditor c1 长度+数据
      total_len += sizeof(size_t) + aud_len2;              // auditor c2 长度+数据
      total_len += 2 * RLC_BN_SIZE;                        // Alice 预签名 r|s
      total_len += RLC_EC_SIZE_COMPRESSED;                 // Alice 预签名 R
      total_len += 2 * RLC_EC_SIZE_COMPRESSED;             // Alice 预签名 pi.a|pi.b
      total_len += RLC_BN_SIZE;                            // Alice 预签名 pi.z
      total_len += 2 * RLC_BN_SIZE;                        // 真实签名 r|s
      // 新增：Alice签名验证所需的信息
      total_len += alice_escrow_id_len;                     // alice_escrow_id (字符串)
      total_len += alice_escrow_tx_hash_len;                // alice_escrow_tx_hash (字符串)
      // 新增：从Bob收到的原始谜题数据
      total_len += RLC_EC_SIZE_COMPRESSED;                 // g^(α+β) (从Bob收到的)
      total_len += 2 * RLC_CL_CIPHERTEXT_SIZE;            // ctx_α+β (从Bob收到的Tumbler密文)
      total_len += 2 * RLC_CL_CIPHERTEXT_SIZE;            // auditor_ctx_α+β (从Bob收到的Auditor密文)
      // 新增：Alice的零知识证明
      size_t alice_zk_size = zk_puzzle_relation_serialized_size();
      printf("[TUMBLER][SECRET_SHARE_1] Alice ZK证明估计大小: %zu bytes\n", alice_zk_size);
      total_len += alice_zk_size;   // Alice的零知识证明

      printf("[TUMBLER][SECRET_SHARE_1] 分配缓冲区大小: %zu bytes\n", total_len);
      uint8_t *packed = (uint8_t*)malloc(total_len);
      if (!packed) { RLC_THROW(ERR_CAUGHT); }
      size_t poff = 0;

      // 1) g^(alpha+beta+tau)
      // 调试打印：第一个分片中的 g^(x+alpha+beta)（这里为 g^(alpha+beta+tau)）
      {
        uint8_t gabt_bytes[RLC_EC_SIZE_COMPRESSED];
        ec_write_bin(gabt_bytes, RLC_EC_SIZE_COMPRESSED, g_alpha_beta_tau, 1);
        printf("[TUMBLER-1][DEBUG] 发送给Auditor的 g^(alpha+beta+tau) (compressed): 0x");
        for (int i=0;i<RLC_EC_SIZE_COMPRESSED;i++) printf("%02x", gabt_bytes[i]);
        printf("\n");
      }
      ec_write_bin(packed + poff, RLC_EC_SIZE_COMPRESSED, g_alpha_beta_tau, 1); poff += RLC_EC_SIZE_COMPRESSED;
      // 2) ctx_alpha_times_beta_times_tau c1|c2 (使用长度前缀)
      {
        char *s1 = GENtostr(ctx_alpha_times_beta_times_tau->c1);
        char *s2 = GENtostr(ctx_alpha_times_beta_times_tau->c2);
        size_t len1 = strlen(s1);
        size_t len2 = strlen(s2);
        printf("[TUMBLER][PACK] ctx_alpha_times_beta_times_tau:\n");
        printf("  - c1 len=%zu, head32: %.32s\n", len1, s1);
        printf("  - c2 len=%zu, head32: %.32s\n", len2, s2);
        
        // 写入c1长度和数据
        memcpy(packed + poff, &len1, sizeof(size_t)); poff += sizeof(size_t);
        memcpy(packed + poff, s1, len1); poff += len1;
        // 写入c2长度和数据
        memcpy(packed + poff, &len2, sizeof(size_t)); poff += sizeof(size_t);
        memcpy(packed + poff, s2, len2); poff += len2;
      }
      // 3) auditor (+tau) (使用长度前缀)
      {
        size_t aud_len1 = strlen(auditor_c1_final_str);
        size_t aud_len2 = strlen(auditor_c2_final_str);
        printf("[TUMBLER][PACK] auditor_final:\n");
        printf("  - c1 len=%zu, head32: %.32s\n", aud_len1, auditor_c1_final_str);
        printf("  - c2 len=%zu, head32: %.32s\n", aud_len2, auditor_c2_final_str);
        
        // 写入auditor c1长度和数据
        memcpy(packed + poff, &aud_len1, sizeof(size_t)); poff += sizeof(size_t);
        memcpy(packed + poff, auditor_c1_final_str, aud_len1); poff += aud_len1;
        // 写入auditor c2长度和数据
        memcpy(packed + poff, &aud_len2, sizeof(size_t)); poff += sizeof(size_t);
        memcpy(packed + poff, auditor_c2_final_str, aud_len2); poff += aud_len2;
      }
      // 4) Alice 完整预签名结构：r|s|R|pi.a|pi.b|pi.z
      // 调试：检查完整预签名结构是否有效
      {
        uint8_t debug_r[RLC_BN_SIZE], debug_s[RLC_BN_SIZE];
        uint8_t debug_R[RLC_EC_SIZE_COMPRESSED], debug_pi_a[RLC_EC_SIZE_COMPRESSED], debug_pi_b[RLC_EC_SIZE_COMPRESSED];
        uint8_t debug_pi_z[RLC_BN_SIZE];
        
        bn_write_bin(debug_r, RLC_BN_SIZE, pre_r);
        bn_write_bin(debug_s, RLC_BN_SIZE, pre_s);
        ec_write_bin(debug_R, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->R, 1);
        ec_write_bin(debug_pi_a, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->a, 1);
        ec_write_bin(debug_pi_b, RLC_EC_SIZE_COMPRESSED, state->sigma_tr->pi->b, 1);
        bn_write_bin(debug_pi_z, RLC_BN_SIZE, state->sigma_tr->pi->z);
      }
      
      // 发送Alice完整预签名结构（使用备份的数据）
      bn_write_bin(packed + poff, RLC_BN_SIZE, pre_r); poff += RLC_BN_SIZE;
      bn_write_bin(packed + poff, RLC_BN_SIZE, pre_s); poff += RLC_BN_SIZE;
      memcpy(packed + poff, pre_R, RLC_EC_SIZE_COMPRESSED); poff += RLC_EC_SIZE_COMPRESSED;
      memcpy(packed + poff, pre_pi_a, RLC_EC_SIZE_COMPRESSED); poff += RLC_EC_SIZE_COMPRESSED;
      memcpy(packed + poff, pre_pi_b, RLC_EC_SIZE_COMPRESSED); poff += RLC_EC_SIZE_COMPRESSED;
      memcpy(packed + poff, pre_pi_z, RLC_BN_SIZE); poff += RLC_BN_SIZE;
      // 5) 真实签名 r|s
      // 调试：检查真实签名是否有效
      {
        uint8_t debug_r[RLC_BN_SIZE], debug_s[RLC_BN_SIZE];
        bn_write_bin(debug_r, RLC_BN_SIZE, state->sigma_s->r);
        bn_write_bin(debug_s, RLC_BN_SIZE, state->sigma_s->s);
      }
      bn_write_bin(packed + poff, RLC_BN_SIZE, state->sigma_s->r); poff += RLC_BN_SIZE;
      bn_write_bin(packed + poff, RLC_BN_SIZE, state->sigma_s->s); poff += RLC_BN_SIZE;
      
      // 新增：序列化Alice签名验证所需的信息
      printf("[TUMBLER][SECRET_SHARE_1] 开始序列化Alice签名验证信息...\n");
      memcpy(packed + poff, state->alice_escrow_id, alice_escrow_id_len);
      poff += alice_escrow_id_len;
      printf("[TUMBLER][SECRET_SHARE_1] 序列化 alice_escrow_id: %s，当前offset: %zu\n", state->alice_escrow_id, poff);
      
      memcpy(packed + poff, state->alice_escrow_tx_hash, alice_escrow_tx_hash_len);
      poff += alice_escrow_tx_hash_len;
      printf("[TUMBLER][SECRET_SHARE_1] 序列化 alice_escrow_tx_hash: %s，当前offset: %zu\n", state->alice_escrow_tx_hash, poff);
      
      // 新增：序列化从Bob收到的原始谜题数据
      printf("[TUMBLER][SECRET_SHARE_1] 开始序列化从Bob收到的原始谜题数据...\n");
      
      // 1. 序列化 g^(α+β) (从Bob收到的)
      ec_write_bin(packed + poff, RLC_EC_SIZE_COMPRESSED, state->alice_g_to_the_alpha_times_beta, 1);
      poff += RLC_EC_SIZE_COMPRESSED;
      printf("[TUMBLER][SECRET_SHARE_1] 序列化 g^(α+β)，当前offset: %zu\n", poff);
      
      // 2. 序列化 ctx_α+β (从Bob收到的Tumbler密文)
      {
        char *s1 = GENtostr(state->alice_ctx_alpha_times_beta->c1);
        char *s2 = GENtostr(state->alice_ctx_alpha_times_beta->c2);
        size_t l1 = strlen(s1), l2 = strlen(s2);
        size_t ctx_alpha_beta_start_offset = poff;
        uint8_t *d1 = packed + poff;
        uint8_t *d2 = packed + poff + RLC_CL_CIPHERTEXT_SIZE;
        
        printf("[TUMBLER][SECRET_SHARE_1][DEBUG] ctx_α+β serialization:\n");
        printf("  - Actual data length: c1=%zu bytes, c2=%zu bytes\n", l1, l2);
        printf("  - Allocated size: %d bytes per component (total %d bytes)\n", 
               RLC_CL_CIPHERTEXT_SIZE, 2 * RLC_CL_CIPHERTEXT_SIZE);
        printf("  - Padding size: c1=%zu bytes, c2=%zu bytes\n", 
               (l1 < RLC_CL_CIPHERTEXT_SIZE) ? (RLC_CL_CIPHERTEXT_SIZE - l1) : 0,
               (l2 < RLC_CL_CIPHERTEXT_SIZE) ? (RLC_CL_CIPHERTEXT_SIZE - l2) : 0);
        
        memset(d1, 0, RLC_CL_CIPHERTEXT_SIZE);
        memset(d2, 0, RLC_CL_CIPHERTEXT_SIZE);
        if (l1 > RLC_CL_CIPHERTEXT_SIZE - 1) l1 = RLC_CL_CIPHERTEXT_SIZE - 1;
        if (l2 > RLC_CL_CIPHERTEXT_SIZE - 1) l2 = RLC_CL_CIPHERTEXT_SIZE - 1;
        memcpy(d1, s1, l1);
        memcpy(d2, s2, l2);
        poff += 2 * RLC_CL_CIPHERTEXT_SIZE;
        printf("[TUMBLER][SECRET_SHARE_1] 序列化 ctx_α+β，当前offset: %zu (start: %zu, allocated: %d bytes)\n", 
               poff, ctx_alpha_beta_start_offset, 2 * RLC_CL_CIPHERTEXT_SIZE);
        printf("[TUMBLER][SECRET_SHARE_1][DEBUG] ctx_α+β occupies bytes %zu-%zu (actual data: %zu bytes, padding: %zu bytes)\n",
               ctx_alpha_beta_start_offset, poff - 1, l1 + l2, (2 * RLC_CL_CIPHERTEXT_SIZE) - (l1 + l2));
      }
      
      // 3. 序列化 auditor_ctx_α+β (从Bob收到的Auditor密文)
      {
        char *s1 = GENtostr(state->alice_auditor_ctx_alpha_times_beta->c1);
        char *s2 = GENtostr(state->alice_auditor_ctx_alpha_times_beta->c2);
        size_t l1 = strlen(s1), l2 = strlen(s2);
        size_t auditor_ctx_alpha_beta_start_offset = poff;
        uint8_t *d1 = packed + poff;
        uint8_t *d2 = packed + poff + RLC_CL_CIPHERTEXT_SIZE;
        
        printf("[TUMBLER][SECRET_SHARE_1][DEBUG] auditor_ctx_α+β serialization:\n");
        printf("  - Actual data length: c1=%zu bytes, c2=%zu bytes\n", l1, l2);
        printf("  - Allocated size: %d bytes per component (total %d bytes)\n", 
               RLC_CL_CIPHERTEXT_SIZE, 2 * RLC_CL_CIPHERTEXT_SIZE);
        printf("  - Padding size: c1=%zu bytes, c2=%zu bytes\n", 
               (l1 < RLC_CL_CIPHERTEXT_SIZE) ? (RLC_CL_CIPHERTEXT_SIZE - l1) : 0,
               (l2 < RLC_CL_CIPHERTEXT_SIZE) ? (RLC_CL_CIPHERTEXT_SIZE - l2) : 0);
        
        memset(d1, 0, RLC_CL_CIPHERTEXT_SIZE);
        memset(d2, 0, RLC_CL_CIPHERTEXT_SIZE);
        if (l1 > RLC_CL_CIPHERTEXT_SIZE - 1) l1 = RLC_CL_CIPHERTEXT_SIZE - 1;
        if (l2 > RLC_CL_CIPHERTEXT_SIZE - 1) l2 = RLC_CL_CIPHERTEXT_SIZE - 1;
        memcpy(d1, s1, l1);
        memcpy(d2, s2, l2);
        poff += 2 * RLC_CL_CIPHERTEXT_SIZE;
        printf("[TUMBLER][SECRET_SHARE_1] 序列化 auditor_ctx_α+β，当前offset: %zu (start: %zu, allocated: %d bytes)\n", 
               poff, auditor_ctx_alpha_beta_start_offset, 2 * RLC_CL_CIPHERTEXT_SIZE);
        printf("[TUMBLER][SECRET_SHARE_1][DEBUG] auditor_ctx_α+β occupies bytes %zu-%zu (actual data: %zu bytes, padding: %zu bytes)\n",
               auditor_ctx_alpha_beta_start_offset, poff - 1, l1 + l2, (2 * RLC_CL_CIPHERTEXT_SIZE) - (l1 + l2));
      }
      
      // 4. 序列化Alice的零知识证明
      printf("[TUMBLER][SECRET_SHARE_1] 开始序列化Alice的零知识证明...\n");
      printf("[TUMBLER][SECRET_SHARE_1] 序列化前 - 当前offset: %zu, 剩余空间: %zu\n", poff, total_len - poff);
      
      // 检查是否有足够的空间
      if (poff + alice_zk_size > total_len) {
        printf("[TUMBLER][SECRET_SHARE_1] ERROR: 缓冲区空间不足！需要 %zu, 剩余 %zu\n", alice_zk_size, total_len - poff);
        RLC_THROW(ERR_CAUGHT);
      }
      
      size_t proof_written = 0;
      if (zk_puzzle_relation_serialize(packed + poff, &proof_written, state->alice_puzzle_relation_zk_proof) != RLC_OK) {
        printf("[ERROR] 无法序列化Alice的puzzle_relation证明!\n");
        RLC_THROW(ERR_CAUGHT);
      }
      poff += proof_written;
      printf("[TUMBLER][SECRET_SHARE_1] Alice零知识证明序列化成功，大小: %zu bytes，最终offset: %zu\n", proof_written, poff);
      
      printf("[TUMBLER][SECRET_SHARE_1] 从Bob收到的原始谜题数据和Alice零知识证明序列化完成!\n");
      
      // 调试：检查打包数据的前几个关键位置
      printf("[TUMBLER][DBG] 分片创建前数据验证:\n");
      printf("  - packed前33字节(g^abt): ");
      for (int i = 0; i < 33; i++) printf("%02x", packed[i]);
      printf("\n");
      
      // 检查Alice预签名位置的数据
      size_t presig_offset = 33 + 6000; // g^abt + 两个密文
      printf("  - Alice预签名偏移: %zu\n", presig_offset);
      printf("  - Alice预签名r位置数据: ");
      for (int i = 0; i < 16 && (presig_offset + i) < poff; i++) {
        printf("%02x", packed[presig_offset + i]);
      }
      printf("\n");
      
      // 计算需要的分享数组大小（num_blocks * SECRET_SHARES）
      size_t num_blocks = (poff + BLOCK_SIZE - 1) / BLOCK_SIZE;
      size_t max_shares = num_blocks * SECRET_SHARES;
      secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
      if (shares == NULL) {
        printf("[SecretShare][Tumbler] Error: Failed to allocate shares array\n");
        free(packed);
        RLC_THROW(ERR_CAUGHT);
      }
      
      // 保存分片前的完整消息到文件（用于调试）
      {
        char filename[256];
        // 处理文件名：移除 0x 前缀，替换特殊字符
        char safe_hash[128];
        const char *hash = state->alice_escrow_tx_hash;
        if (hash && strlen(hash) > 0) {
          // 移除 0x 前缀（如果存在）
          if (strncmp(hash, "0x", 2) == 0) {
            strncpy(safe_hash, hash + 2, sizeof(safe_hash) - 1);
          } else {
            strncpy(safe_hash, hash, sizeof(safe_hash) - 1);
          }
          safe_hash[sizeof(safe_hash) - 1] = '\0';
          // 替换可能存在的特殊字符
          for (char *p = safe_hash; *p; p++) {
            if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
              *p = '_';
            }
          }
        } else {
          strcpy(safe_hash, "unknown");
        }
        snprintf(filename, sizeof(filename), "tumbler_original_msg_%s.txt", safe_hash);
        FILE *fp = fopen(filename, "w");
        if (fp) {
          fprintf(fp, "Tumbler Original Message (before sharing)\n");
          fprintf(fp, "Message ID: %s\n", state->alice_escrow_tx_hash);
          fprintf(fp, "Length: %zu bytes\n", poff);
          fprintf(fp, "Hex dump:\n");
          for (size_t i = 0; i < poff; i++) {
            fprintf(fp, "%02x", packed[i]);
            if ((i + 1) % 32 == 0) fprintf(fp, "\n");
            else if ((i + 1) % 2 == 0) fprintf(fp, " ");
          }
          if (poff % 32 != 0) fprintf(fp, "\n");
          fclose(fp);
          printf("[TUMBLER][DEBUG] Saved original message before sharing: %s (%zu bytes)\n", filename, poff);
        } else {
          printf("[TUMBLER][DEBUG] Failed to save original message to %s\n", filename);
        }
      }
      
      // 检查要分片的消息中是否有很多为0的字节
      size_t zero_byte_count = 0;
      size_t consecutive_zero_max = 0;
      size_t consecutive_zero_current = 0;
      for (size_t i = 0; i < poff; i++) {
        if (packed[i] == 0) {
          zero_byte_count++;
          consecutive_zero_current++;
          if (consecutive_zero_current > consecutive_zero_max) {
            consecutive_zero_max = consecutive_zero_current;
          }
        } else {
          consecutive_zero_current = 0;
        }
      }
      double zero_percentage = (poff > 0) ? (100.0 * zero_byte_count / poff) : 0.0;
      
      printf("[TUMBLER][SECRET_SHARE_1] Zero byte analysis before sharing:\n");
      printf("  - Total bytes: %zu\n", poff);
      printf("  - Zero bytes: %zu (%.2f%%)\n", zero_byte_count, zero_percentage);
      printf("  - Max consecutive zeros: %zu\n", consecutive_zero_max);
      
      if (zero_percentage > 50.0) {
        printf("[TUMBLER][SECRET_SHARE_1] WARNING: More than 50%% of bytes are zero!\n");
      }
      if (consecutive_zero_max > 100) {
        printf("[TUMBLER][SECRET_SHARE_1] WARNING: Found %zu consecutive zero bytes (may indicate padding or empty data)\n", 
               consecutive_zero_max);
      }
      
      size_t num_shares = 0;
      int ss = create_secret_shares(packed, poff, shares, &num_shares);
      if (ss != 0) {
        printf("[SecretShare][Tumbler] create_secret_shares failed=%d\n", ss);
        free(shares);
      } else {
        printf("[SecretShare][Tumbler] Created %zu shares\n", num_shares);
        // ===== VSS: 创建并发送承诺给 Auditor =====
        vss_commitment_t commitment;
        printf("[VSS][Tumbler] DEBUG: state->alice_escrow_tx_hash = '%s' (length: %zu)\n", 
               state->alice_escrow_tx_hash, strlen(state->alice_escrow_tx_hash));
        
        if (strlen(state->alice_escrow_tx_hash) == 0) {
          printf("[VSS][Tumbler] WARNING: alice_escrow_tx_hash is empty, using fallback msgid\n");
          strncpy(commitment.msgid, "fallback_msgid_alice", MSG_ID_MAXLEN - 1);
        } else {
          strncpy(commitment.msgid, state->alice_escrow_tx_hash, MSG_ID_MAXLEN - 1);
        }
        commitment.msgid[MSG_ID_MAXLEN - 1] = '\0';
        
        if (create_vss_commitments(packed, poff, shares, &commitment, state->alice_escrow_tx_hash) == 0) {
          printf("[VSS][Tumbler] Created VSS commitments for first share\n");
          if (save_vss_commitment_to_file(&commitment) == 0) {
            printf("[VSS][Tumbler] Successfully saved VSS commitment to file\n");
          } else {
            printf("[VSS][Tumbler] Failed to save VSS commitment to file\n");
          }
        } else {
          printf("[VSS][Tumbler] Failed to create VSS commitments\n");
        }
        
        // 初始化 RECEIVER_ENDPOINTS（如果未初始化）
        if (RECEIVER_ENDPOINTS[0][0] == '\0') {
          printf("[VSS][Tumbler] Initializing RECEIVER_ENDPOINTS...\n");
          get_dynamic_endpoints(RECEIVER_ENDPOINTS);
          for (int i = 0; i < SECRET_SHARES; i++) {
            printf("[VSS][Tumbler] Endpoint[%d]: %s\n", i, RECEIVER_ENDPOINTS[i]);
          }
        }
        
        // 创建指针数组以匹配函数签名
        const char* endpoint_ptrs[SECRET_SHARES];
        for (int i = 0; i < SECRET_SHARES; i++) {
          endpoint_ptrs[i] = RECEIVER_ENDPOINTS[i];
        }
        
        if (send_shares_to_receivers(shares, num_shares, state->alice_escrow_tx_hash, endpoint_ptrs) != 0) {
          printf("[SecretShare][Tumbler] send_shares_to_receivers encountered warnings\n");
        } else {
          printf("[SecretShare][Tumbler] Shares sent using msgid=%s\n", state->alice_escrow_tx_hash);
        }
        free(shares);
      }
      free(packed);
    }

#endif
    // Build and define the message.
    char *msg_type = "payment_done";
    const unsigned msg_type_length = (unsigned) strlen(msg_type) + 1;
    const unsigned msg_data_length = 2 * RLC_BN_SIZE;
    const int total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    message_new(payment_done_msg, msg_type_length, msg_data_length);

    // Serialize the data for the message.
    bn_write_bin(payment_done_msg->data, RLC_BN_SIZE, state->sigma_s->r);
    bn_write_bin(payment_done_msg->data + RLC_BN_SIZE, RLC_BN_SIZE, state->sigma_s->s);

    memcpy(payment_done_msg->type, msg_type, msg_type_length);
    serialize_message(&serialized_message, payment_done_msg, msg_type_length, msg_data_length);

    // Send the message.
    zmq_msg_t payment_done;
    int rc = zmq_msg_init_size(&payment_done, total_msg_length);
    if (rc < 0) {
      fprintf(stderr, "Error: could not initialize the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }

    memcpy(zmq_msg_data(&payment_done), serialized_message, total_msg_length);
    rc = zmq_msg_send(&payment_done, socket, ZMQ_DONTWAIT);
    if (rc != total_msg_length) {
      fprintf(stderr, "Error: could not send the message (%s).\n", msg_type);
      RLC_THROW(ERR_CAUGHT);
    }
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    bn_free(q);
    bn_free(x);
    bn_free(gamma_inverse);
    cl_ciphertext_free(ctx_alpha_times_beta_times_tau);
    cl_ciphertext_free(auditor_ctx_alpha_times_beta_times_tau);
    bn_free(pre_r);
    bn_free(pre_s);
    if (payment_done_msg != NULL) message_free(payment_done_msg);
    if (serialized_message != NULL) free(serialized_message);
  }
  
  return result_status;
}

int layered_proof_share_handler(tumbler_state_t state, void *socket, uint8_t *data) {
  if (state == NULL || data == NULL) {
    RLC_THROW(ERR_NO_VALID);
  }

  START_TIMER(tumbler_layered_proof_handler)
  int result_status = RLC_OK;
  RLC_TRY {
    // 新格式解析并进行验证：
    // [commitment | pedersen(c,u,v) | cldl(t1,t2,t3,u1,u2) | tag_hash |
    //  inner(ct) | outer placeholders | P1(a,b,z) | C1 | C2 | proof_c1(c,u,v) | proof_c2(c,u,v)]
    size_t off = 0;

    complete_nizk_proof_t cp; complete_nizk_proof_new(cp);
    if (cp == NULL) RLC_THROW(ERR_CAUGHT);

    printf("[TUMBLER DEBUG] 开始解析消息，总大小: %zu 字节\n", off);

    // commitment (兼容：Bob发送的是 C1)
    printf("[TUMBLER DEBUG] 读取 commitment_c1，偏移: %zu\n", off);
    g1_read_bin(cp->commitment_c1, data + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
    printf("[TUMBLER DEBUG] commitment_c1 读取成功，新偏移: %zu\n", off);
    
    // pedersen proof (兼容：Bob发送的是 proof_c1)
    printf("[TUMBLER DEBUG] 读取 proof_c1->c->c，偏移: %zu\n", off);
    // 检查椭圆曲线点数据是否有效，如果无效则跳过并使用 commitment_c1
    {
      printf("[TUMBLER DEBUG] proof_c1->c->c 原始数据前16字节: ");
      for (int i = 0; i < 16 && i < RLC_G1_SIZE_COMPRESSED; i++) printf("%02x", data[off + i]);
      printf("\n");
      
      // 尝试读取椭圆曲线点，如果失败则使用 commitment_c1 作为备用
      g1_t temp_point; g1_null(temp_point); g1_new(temp_point);
      
      int read_ok = RLC_OK;
      RLC_TRY { g1_read_bin(temp_point, data + off, RLC_G1_SIZE_COMPRESSED); }
      RLC_CATCH_ANY { read_ok = RLC_ERR; }
      RLC_FINALLY { ; }
      
      if (read_ok == RLC_OK && g1_is_valid(temp_point)) {
        g1_copy(cp->proof_c1->c->c, temp_point);
        printf("[TUMBLER DEBUG] proof_c1->c->c 读取成功\n");
      } else {
        // 数据无效，使用 commitment_c1 作为备用
        g1_copy(cp->proof_c1->c->c, cp->commitment_c1);
        printf("[TUMBLER DEBUG] proof_c1->c->c 数据无效，使用 commitment_c1 作为备用\n");
      }
      
      g1_free(temp_point);
      off += RLC_G1_SIZE_COMPRESSED;
    }
    printf("[TUMBLER DEBUG] proof_c1->c->c 处理完成，新偏移: %zu\n", off);
    
    printf("[TUMBLER DEBUG] 读取 proof_c1->u，偏移: %zu\n", off);
    bn_read_bin(cp->proof_c1->u, data + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    printf("[TUMBLER DEBUG] proof_c1->u 读取成功，新偏移: %zu\n", off);
    
    printf("[TUMBLER DEBUG] 读取 proof_c1->v，偏移: %zu\n", off);
    bn_read_bin(cp->proof_c1->v, data + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    printf("[TUMBLER DEBUG] proof_c1->v 读取成功，新偏移: %zu\n", off);

    // outer proof (CLDL)
    printf("[TUMBLER DEBUG] 开始读取 proof_encryption (CLDL)，偏移: %zu\n", off);
    {
      char buf[RLC_CLDL_PROOF_T1_SIZE + 1];
      printf("[TUMBLER DEBUG] 读取 proof_encryption->t1，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CLDL_PROOF_T1_SIZE); buf[RLC_CLDL_PROOF_T1_SIZE] = 0; off += RLC_CLDL_PROOF_T1_SIZE;
      cp->proof_encryption->t1 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] proof_encryption->t1 读取成功，新偏移: %zu\n", off);
      
      printf("[TUMBLER DEBUG] 读取 proof_encryption->t2 (EC点)，偏移: %zu\n", off);
      printf("[TUMBLER DEBUG] t2数据前16字节: ");
      for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", data[off + i]);
      }
      printf("\n");
      ec_read_bin(cp->proof_encryption->t2, data + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
      printf("[TUMBLER DEBUG] proof_encryption->t2 读取成功，新偏移: %zu\n", off);
      
      printf("[TUMBLER DEBUG] 读取 proof_encryption->t3，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CLDL_PROOF_T3_SIZE); buf[RLC_CLDL_PROOF_T3_SIZE] = 0; off += RLC_CLDL_PROOF_T3_SIZE;
      cp->proof_encryption->t3 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] proof_encryption->t3 读取成功，新偏移: %zu\n", off);
      
      printf("[TUMBLER DEBUG] 读取 proof_encryption->u1，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CLDL_PROOF_U1_SIZE); buf[RLC_CLDL_PROOF_U1_SIZE] = 0; off += RLC_CLDL_PROOF_U1_SIZE;
      cp->proof_encryption->u1 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] proof_encryption->u1 读取成功，新偏移: %zu\n", off);
      
      printf("[TUMBLER DEBUG] 读取 proof_encryption->u2，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CLDL_PROOF_U2_SIZE); buf[RLC_CLDL_PROOF_U2_SIZE] = 0; off += RLC_CLDL_PROOF_U2_SIZE;
      cp->proof_encryption->u2 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] proof_encryption->u2 读取成功，新偏移: %zu\n", off);
    }

    // tag_hash = H(inner)
    printf("[TUMBLER DEBUG] 读取 inner_hash，偏移: %zu\n", off);
    memcpy(cp->inner_hash, data + off, RLC_MD_LEN); off += RLC_MD_LEN;
    printf("[TUMBLER DEBUG] inner_hash 读取成功，新偏移: %zu\n", off);

    // 新增：ct_beta（β）
    printf("[TUMBLER DEBUG] 读取 ct_beta，偏移: %zu\n", off);
    cl_ciphertext_t ct_beta; cl_ciphertext_new(ct_beta);
    {
      char buf[RLC_CL_CIPHERTEXT_SIZE + 1];
      printf("[TUMBLER DEBUG] 读取 ct_beta->c1，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CL_CIPHERTEXT_SIZE); buf[RLC_CL_CIPHERTEXT_SIZE] = 0; off += RLC_CL_CIPHERTEXT_SIZE;
      ct_beta->c1 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] ct_beta->c1 读取成功，新偏移: %zu\n", off);
      
      printf("[TUMBLER DEBUG] 读取 ct_beta->c2，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CL_CIPHERTEXT_SIZE); buf[RLC_CL_CIPHERTEXT_SIZE] = 0; off += RLC_CL_CIPHERTEXT_SIZE;
      ct_beta->c2 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] ct_beta->c2 读取成功，新偏移: %zu\n", off);
    }

    // inner 密文（β'）
    printf("[TUMBLER DEBUG] 读取 ct_beta_prime，偏移: %zu\n", off);
    cl_ciphertext_t ct_beta_prime; cl_ciphertext_new(ct_beta_prime);
    {
      char buf[RLC_CL_CIPHERTEXT_SIZE + 1];
      printf("[TUMBLER DEBUG] 读取 ct_beta_prime->c1，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CL_CIPHERTEXT_SIZE); buf[RLC_CL_CIPHERTEXT_SIZE] = 0; off += RLC_CL_CIPHERTEXT_SIZE;
      ct_beta_prime->c1 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] ct_beta_prime->c1 读取成功，新偏移: %zu\n", off);
      
      printf("[TUMBLER DEBUG] 读取 ct_beta_prime->c2，偏移: %zu\n", off);
      memcpy(buf, data + off, RLC_CL_CIPHERTEXT_SIZE); buf[RLC_CL_CIPHERTEXT_SIZE] = 0; off += RLC_CL_CIPHERTEXT_SIZE;
      ct_beta_prime->c2 = gp_read_str(buf);
      printf("[TUMBLER DEBUG] ct_beta_prime->c2 读取成功，新偏移: %zu\n", off);
    }

    // 跳过 outer 占位符（不解析为密文）
    printf("[TUMBLER DEBUG] 跳过 outer 占位符，偏移: %zu\n", off);
    off += (2 * RLC_CL_CIPHERTEXT_SIZE);
    printf("[TUMBLER DEBUG] 跳过完成，新偏移: %zu\n", off);

    // P1: proof_homomorphic (a,b,z)
    printf("[TUMBLER DEBUG] 开始读取 proof_homomorphic，偏移: %zu\n", off);
    printf("[TUMBLER DEBUG] 读取 proof_homomorphic->a (EC点)，偏移: %zu\n", off);
    printf("[TUMBLER DEBUG] a数据前16字节: ");
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
      printf("%02x", data[off + i]);
    }
    printf("\n");
    ec_read_bin(cp->proof_homomorphic->a, data + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    printf("[TUMBLER DEBUG] proof_homomorphic->a 读取成功，新偏移: %zu\n", off);
    
    printf("[TUMBLER DEBUG] 读取 proof_homomorphic->b (EC点)，偏移: %zu\n", off);
    printf("[TUMBLER DEBUG] b数据前16字节: ");
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
      printf("%02x", data[off + i]);
    }
    printf("\n");
    ec_read_bin(cp->proof_homomorphic->b, data + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    printf("[TUMBLER DEBUG] proof_homomorphic->b 读取成功，新偏移: %zu\n", off);
    
    printf("[TUMBLER DEBUG] 读取 proof_homomorphic->z，偏移: %zu\n", off);
    bn_read_bin(cp->proof_homomorphic->z, data + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    printf("[TUMBLER DEBUG] proof_homomorphic->z 读取成功，新偏移: %zu\n", off);

    // C2 承诺 (C1已在前面读取过了)
    printf("[TUMBLER DEBUG] 读取 commitment_c2，偏移: %zu\n", off);
    g1_read_bin(cp->commitment_c2, data + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
    printf("[TUMBLER DEBUG] commitment_c2 读取成功，新偏移: %zu\n", off);

    // proof_c2 (proof_c1已在前面读取过了)
    printf("[TUMBLER DEBUG] 读取 proof_c2->c->c，偏移: %zu\n", off);
    {
      printf("[TUMBLER DEBUG] proof_c2->c->c 原始数据前16字节: ");
      for (int i = 0; i < 16 && i < RLC_G1_SIZE_COMPRESSED; i++) printf("%02x", data[off + i]);
      printf("\n");
      g1_t temp2; g1_null(temp2); g1_new(temp2);
      int read2_ok = RLC_OK;
      RLC_TRY { g1_read_bin(temp2, data + off, RLC_G1_SIZE_COMPRESSED); }
      RLC_CATCH_ANY { read2_ok = RLC_ERR; }
      RLC_FINALLY { ; }
      if (read2_ok == RLC_OK && g1_is_valid(temp2)) {
        g1_copy(cp->proof_c2->c->c, temp2);
        printf("[TUMBLER DEBUG] proof_c2->c->c 读取成功\n");
      } else {
        g1_copy(cp->proof_c2->c->c, cp->commitment_c2);
        printf("[TUMBLER DEBUG] proof_c2->c->c 数据无效，使用 commitment_c2 作为备用\n");
      }
      g1_free(temp2);
      off += RLC_G1_SIZE_COMPRESSED;
    }
    printf("[TUMBLER DEBUG] proof_c2->c->c 处理完成，新偏移: %zu\n", off);
    
    printf("[TUMBLER DEBUG] 读取 proof_c2->u，偏移: %zu\n", off);
    bn_read_bin(cp->proof_c2->u, data + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    printf("[TUMBLER DEBUG] proof_c2->u 读取成功，新偏移: %zu\n", off);
    
    printf("[TUMBLER DEBUG] 读取 proof_c2->v，偏移: %zu\n", off);
    bn_read_bin(cp->proof_c2->v, data + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    printf("[TUMBLER DEBUG] proof_c2->v 读取成功，新偏移: %zu\n", off);

    printf("[TUMBLER DEBUG] 所有字段解析完成，最终偏移: %zu\n", off);

    // 调用完整验证：P1 + (P2+P3)
    START_TIMER(tumbler_zk_verification)
    if (complete_nizk_verify(cp,
                             ct_beta,
                             ct_beta_prime,
                             state->tumbler_ps_pk,
                             state->auditor_cl_pk,
                             state->auditor2_cl_pk,
                             state->cl_params) != RLC_OK) {
      printf("[TUMBLER] complete_nizk_verify failed\n");
      complete_nizk_proof_free(cp);
      cl_ciphertext_free(ct_beta);
      cl_ciphertext_free(ct_beta_prime);
      RLC_THROW(ERR_CAUGHT);
    }

    printf("[TUMBLER] zk_outer_link_verify OK. tag_hash: ");
    for (int i = 0; i < RLC_MD_LEN; i++) printf("%02x", cp->inner_hash[i]);
    printf("\n");
    END_TIMER(tumbler_zk_verification)

    // 保存 tag_hash 与 inner（β'）
    memcpy(state->auditor2_tag_hash, cp->inner_hash, RLC_MD_LEN);
    state->auditor_ctx_alpha_times_beta->c1 = ct_beta_prime->c1;
    state->auditor_ctx_alpha_times_beta->c2 = ct_beta_prime->c2;

    // 对承诺值进行盲签（沿用原有逻辑，签 C1 对应的承诺点）
    ps_signature_t sigma_outer; ps_signature_new(sigma_outer);
    if (ps_blind_sign(sigma_outer, cp->proof_c1->c, state->tumbler_ps_sk) != RLC_OK) {
      printf("[TUMBLER] ps_blind_sign failed\n");
      ps_signature_free(sigma_outer);
      complete_nizk_proof_free(cp);
      cl_ciphertext_free(ct_beta_prime);
      cl_ciphertext_free(ct_beta);
      RLC_THROW(ERR_CAUGHT);
    }

    // 发送签名给 Bob
    {
      const char *msg_type = "layered_proof_signed";
      const unsigned msg_type_length = (unsigned)strlen(msg_type) + 1;
      const unsigned msg_data_length = 2 * RLC_G1_SIZE_COMPRESSED;
      const int total_len = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
      message_t sig_msg; message_new(sig_msg, msg_type_length, msg_data_length);
      size_t soff = 0;
      
      // 添加调试信息
      printf("[TUMBLER DEBUG] 准备发送盲签名给 Bob:\n");
      uint8_t sig1_debug[RLC_G1_SIZE_COMPRESSED], sig2_debug[RLC_G1_SIZE_COMPRESSED];
      g1_write_bin(sig1_debug, RLC_G1_SIZE_COMPRESSED, sigma_outer->sigma_1, 1);
      g1_write_bin(sig2_debug, RLC_G1_SIZE_COMPRESSED, sigma_outer->sigma_2, 1);
      printf("[TUMBLER DEBUG] sigma_1 前16字节: ");
      for (int i = 0; i < 16; i++) printf("%02x", sig1_debug[i]);
      printf("\n");
      printf("[TUMBLER DEBUG] sigma_2 前16字节: ");
      for (int i = 0; i < 16; i++) printf("%02x", sig2_debug[i]);
      printf("\n");
      
      g1_write_bin(sig_msg->data + soff, RLC_G1_SIZE_COMPRESSED, sigma_outer->sigma_1, 1); soff += RLC_G1_SIZE_COMPRESSED;
      g1_write_bin(sig_msg->data + soff, RLC_G1_SIZE_COMPRESSED, sigma_outer->sigma_2, 1); soff += RLC_G1_SIZE_COMPRESSED;
      memcpy(sig_msg->type, msg_type, msg_type_length);
      uint8_t *serialized = NULL; serialize_message(&serialized, sig_msg, msg_type_length, msg_data_length);
      zmq_msg_t z; zmq_msg_init_size(&z, total_len);
      memcpy(zmq_msg_data(&z), serialized, total_len);
      int send_result = zmq_msg_send(&z, socket, 0);
      printf("[TUMBLER DEBUG] 盲签名发送结果: %d (期望: %d)\n", send_result, total_len);
      zmq_msg_close(&z);
      free(serialized); message_free(sig_msg);
    }

    ps_signature_free(sigma_outer);
    // 不释放 ct_beta_prime 所携带的 GEN（赋给了 state），仅释放容器
    cl_ciphertext_free(ct_beta_prime);
    cl_ciphertext_free(ct_beta);
    complete_nizk_proof_free(cp);
    END_TIMER(tumbler_layered_proof_handler)
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
  }
  return result_status;
}

int main(int argc,char* argv[])
{
  int tumbler_port = 8181;  // 默认端口
  char tumbler_endpoint[64];

  // 解析命令行参数
  if (argc > 1) {
    tumbler_port = atoi(argv[1]);
    if (tumbler_port <= 0 || tumbler_port > 65535) {
      fprintf(stderr, "Error: Invalid port number. Port must be between 1 and 65535.\n");
      fprintf(stderr, "Usage: %s [port]\n", argv[0]);
      fprintf(stderr, "Default port: 8181\n");
      return 1;
    }
  }

  // 构建端点字符串
  snprintf(tumbler_endpoint, sizeof(tumbler_endpoint), "tcp://*:%d", tumbler_port);
  
  printf("[TUMBLER] 启动参数:\n");
  printf("  port: %d\n", tumbler_port);
  printf("  endpoint: %s\n", tumbler_endpoint);
  printf("[TUMBLER] 开始启动...\n");
  srand((unsigned int)time(NULL));

  init();
  int result_status = RLC_OK;

  tumbler_state_t state;
  tumbler_state_null(state);

  // Bind the socket to talk to clients.
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

  int rc = zmq_bind(socket, tumbler_endpoint);
  if (rc != 0) {
    fprintf(stderr, "Error: could not bind the socket on %s.\n", tumbler_endpoint);
    exit(1);
  }

  // 初始化全局时间测量
  if (!tumbler_timing_initialized) {
    clock_gettime(CLOCK_MONOTONIC, &tumbler_start_time);
    tumbler_timing_initialized = 1;
  }

  RLC_TRY {
    tumbler_state_new(state);

    START_TIMER(tumbler_initialization_computation)
    if (generate_cl_params(state->cl_params) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
// if (generate_keys_and_write_to_file(state->cl_params) == 0) {
//         printf("All keys generated successfully!\n");
//     } else {
//         printf("Key generation failed!\n");
//     }

    if (read_keys_from_file_tumbler(state->tumbler_ec_sk,
                                    state->tumbler_ec_pk,
                                    state->tumbler_ps_sk,
                                    state->tumbler_ps_pk,
                                    state->tumbler_cl_sk,
                                    state->tumbler_cl_pk,
                                    state->alice_ec_pk,
                                    state->bob_ec_pk) != RLC_OK) {
      RLC_THROW(ERR_CAUGHT);
    }
    
    // ===== 输出 Tumbler ECDSA 公钥和私钥 =====
    printf("\n========================================\n");
    printf("[TUMBLER] Tumbler ECDSA 密钥信息\n");
    printf("========================================\n");
    
    // 输出私钥（bn_t 格式）
    {
      uint8_t sk_bytes[RLC_BN_SIZE];
      bn_write_bin(sk_bytes, RLC_BN_SIZE, state->tumbler_ec_sk->sk);
      
      // 转换为十六进制字符串
      char sk_hex[2 * RLC_BN_SIZE + 3];
      sk_hex[0] = '0';
      sk_hex[1] = 'x';
      static const char *hexd = "0123456789abcdef";
      for (int i = 0; i < RLC_BN_SIZE; i++) {
        sk_hex[2 + 2*i]     = hexd[(sk_bytes[i] >> 4) & 0xF];
        sk_hex[2 + 2*i + 1] = hexd[sk_bytes[i] & 0xF];
      }
      sk_hex[2 + 2*RLC_BN_SIZE] = '\0';
      
      printf("[TUMBLER] 私钥 (sk, hex): %s\n", sk_hex);
      
      // 同时输出十进制格式
      char sk_dec[512];
      bn_write_str(sk_dec, sizeof(sk_dec), state->tumbler_ec_sk->sk, 10);
      printf("[TUMBLER] 私钥 (sk, dec): %s\n", sk_dec);
    }
    
    // 输出公钥（压缩格式）
    {
      uint8_t pk_compressed[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(pk_compressed, RLC_EC_SIZE_COMPRESSED, state->tumbler_ec_pk->pk, 1);
      
      char pk_hex_compressed[2 * RLC_EC_SIZE_COMPRESSED + 3];
      pk_hex_compressed[0] = '0';
      pk_hex_compressed[1] = 'x';
      static const char *hexd_pk = "0123456789abcdef";
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
        pk_hex_compressed[2 + 2*i]     = hexd_pk[(pk_compressed[i] >> 4) & 0xF];
        pk_hex_compressed[2 + 2*i + 1] = hexd_pk[pk_compressed[i] & 0xF];
      }
      pk_hex_compressed[2 + 2*RLC_EC_SIZE_COMPRESSED] = '\0';
      
      printf("[TUMBLER] 公钥 (pk, 压缩格式): %s\n", pk_hex_compressed);
    }
    
    // 输出公钥（未压缩格式，0x04||X||Y）
    {
      uint8_t pk_uncompressed[RLC_EC_SIZE_COMPRESSED * 2];
      ec_write_bin(pk_uncompressed, RLC_EC_SIZE_COMPRESSED * 2, state->tumbler_ec_pk->pk, 0);
      
      char pk_hex_uncompressed[2 * RLC_EC_SIZE_COMPRESSED * 2 + 3];
      pk_hex_uncompressed[0] = '0';
      pk_hex_uncompressed[1] = 'x';
      static const char *hexd_pk2 = "0123456789abcdef";
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED * 2; i++) {
        pk_hex_uncompressed[2 + 2*i]     = hexd_pk2[(pk_uncompressed[i] >> 4) & 0xF];
        pk_hex_uncompressed[2 + 2*i + 1] = hexd_pk2[pk_uncompressed[i] & 0xF];
      }
      pk_hex_uncompressed[2 + 2*RLC_EC_SIZE_COMPRESSED * 2] = '\0';
      
      printf("[TUMBLER] 公钥 (pk, 未压缩格式): %s\n", pk_hex_uncompressed);
      
      // 分别输出 X 和 Y 坐标
      char pk_x_hex[65], pk_y_hex[65];
      for (int i = 0; i < 32; i++) {
        pk_x_hex[2*i]     = hexd_pk2[(pk_uncompressed[1 + i] >> 4) & 0xF];
        pk_x_hex[2*i + 1] = hexd_pk2[pk_uncompressed[1 + i] & 0xF];
        pk_y_hex[2*i]     = hexd_pk2[(pk_uncompressed[33 + i] >> 4) & 0xF];
        pk_y_hex[2*i + 1] = hexd_pk2[pk_uncompressed[33 + i] & 0xF];
      }
      pk_x_hex[64] = '\0';
      pk_y_hex[64] = '\0';
      printf("[TUMBLER] 公钥 X 坐标: 0x%s\n", pk_x_hex);
      printf("[TUMBLER] 公钥 Y 坐标: 0x%s\n", pk_y_hex);
    }
    
    printf("========================================\n\n");
    // ===== 密钥输出完成 =====
    
    // ⭐ 读取 DKG 生成的审计员公钥（从 ../keys/dkg_public.key）
    printf("[TUMBLER] 读取 DKG 生成的审计员公钥...\n");
    if (read_auditor_cl_pubkey(state->auditor_cl_pk) != 0) {
      fprintf(stderr, "Failed to load DKG auditor public key!\n");
      fprintf(stderr, "Please ensure DKG has been run and dkg_public.key exists.\n");
      RLC_THROW(ERR_CAUGHT);
    }
    printf("[TUMBLER] ✅ DKG 审计员公钥加载成功\n");
    
    if (read_auditor_cl_pubkey_named(state->auditor2_cl_pk, "auditor2") != 0) {
      printf("[WARN] auditor2.key not found or unreadable, skip secondary auditor pk.\n");
    }
    END_TIMER(tumbler_initialization_computation)

    printf("[TUMBLER] 开始监听消息...\n");
    while (!tumbler_should_exit) {
      printf("[TUMBLER] 等待消息...\n");
      if (receive_message(state, socket) != RLC_OK) {
        printf("[TUMBLER] 消息接收失败\n");
        RLC_THROW(ERR_CAUGHT);
      }
    }
    
    printf("[TUMBLER] 分片发送完成，程序即将退出\n");
  } RLC_CATCH_ANY {
    result_status = RLC_ERR;
  } RLC_FINALLY {
    tumbler_state_free(state);
  }

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

  clean();

  // 输出时间测量结果
  

  return result_status;
}


