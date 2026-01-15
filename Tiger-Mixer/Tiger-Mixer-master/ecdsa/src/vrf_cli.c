#include "vrf_generator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// 声明调试验证函数（定义在 vrf_generator_debug.c，如果文件不存在则禁用此功能）
// extern int vrf_generator_verify_debug(
//     const unsigned char proof[81],
//     const unsigned char pk[33],
//     const void *msg,
//     size_t msglen,
//     unsigned char output[32]
// );
#define VRF_DEBUG_VERIFY_DISABLED 1  // 禁用调试验证功能

// JSON 格式输出 VRF 结果
void print_json_result(
    const unsigned char proof[81],
    const unsigned char random[32],
    const unsigned char pk[33],
    const char *message
) {
    printf("{\n");
    printf("  \"message\": \"%s\",\n", message);
    
    // 公钥（十六进制）
    printf("  \"publicKey\": \"");
    for (int i = 0; i < 33; i++) {
        printf("%02x", pk[i]);
    }
    printf("\",\n");
    
    // 证明（十六进制）
    printf("  \"proof\": \"");
    for (int i = 0; i < 81; i++) {
        printf("%02x", proof[i]);
    }
    printf("\",\n");
    
    // 随机数（十六进制）
    printf("  \"random\": \"");
    for (int i = 0; i < 32; i++) {
        printf("%02x", random[i]);
    }
    printf("\",\n");
    
    // 随机数（uint256，用于合约）
    printf("  \"randomUint256\": \"0x");
    for (int i = 0; i < 32; i++) {
        printf("%02x", random[i]);
    }
    printf("\"\n");
    printf("}\n");
}

int main(int argc, char *argv[]) {
    vrf_generator_state_t state = NULL;
    const char *key_file = NULL;
    const char *message = NULL;

    // 解析命令行参数
    // 用法: vrf_cli [key_file] message [--debug-verify]
    // 注意: 如果 key_file 为空字符串 ""，则生成新密钥（与 vrf_test 行为一致）
    if (argc < 2) {
        fprintf(stderr, "用法: %s [key_file] <message> [--debug-verify]\n", argv[0]);
        fprintf(stderr, "  或: %s <message> [--debug-verify]  (自动生成新密钥)\n", argv[0]);
        return 1;
    }

    int debug_verify = 0;
    int message_index = 1;
    
    // 检查是否有 --debug-verify 参数
    if (argc >= 3 && strcmp(argv[argc - 1], "--debug-verify") == 0) {
        debug_verify = 1;
        argc--;  // 临时减少 argc，方便后续处理
    }
    
    if (argc == 2) {
        // 只有消息，自动生成密钥
        message = argv[1];
    } else if (argc == 3) {
        // 密钥文件和消息
        key_file = argv[1];
        message = argv[2];
        // 特殊处理：如果 key_file 为空字符串，则不使用密钥文件
        if (strlen(key_file) == 0) {
            key_file = NULL;
        }
    } else {
        fprintf(stderr, "错误: 参数过多\n");
        return 1;
    }

    // 初始化 RELIC 库（与 vrf_test.c 完全相同的顺序）
    if (core_init() != RLC_OK) {
        fprintf(stderr, "错误: RELIC 库初始化失败\n");
        return 1;
    }
    ep_param_set_any();
    ep_param_set(SECG_K256);  // 确保使用 secp256k1 曲线

    // 初始化 VRF 生成器
    if (vrf_generator_init(&state, key_file) != 1) {
        fprintf(stderr, "错误: VRF 生成器初始化失败\n");
        core_clean();
        return 1;
    }

    // 生成 VRF 证明和随机数
    unsigned char proof[81];
    unsigned char output[32];

    if (vrf_generator_prove(state, message, strlen(message), proof, output) != 1) {
        fprintf(stderr, "错误: VRF 证明生成失败\n");
        vrf_generator_free(state);
        core_clean();
        return 1;
    }

    // 输出 JSON 格式结果
    print_json_result(proof, output, state->pk_serialized, message);

    // 如果指定了 --debug-verify，则执行调试验证
    #ifndef VRF_DEBUG_VERIFY_DISABLED
    if (debug_verify) {
        printf("\n=== 执行验证调试 ===\n");
        unsigned char verify_output[32];
        int verify_result = vrf_generator_verify_debug(
            proof,
            state->pk_serialized,
            message,
            strlen(message),
            verify_output
        );
        if (verify_result == 1) {
            printf("\n✅ 验证成功\n");
        } else {
            printf("\n❌ 验证失败\n");
        }
    }
    #else
    if (debug_verify) {
        printf("\n警告: 调试验证功能已禁用（vrf_generator_debug.c 不存在）\n");
    }
    #endif

    // 清理
    vrf_generator_free(state);
    core_clean();

    return 0;
}

