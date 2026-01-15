#include "vrf_generator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// RELIC 库初始化标志（用于避免重复初始化）
static int relic_initialized = 0;

// 打印十六进制
void print_hex(const char *label, const unsigned char *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    vrf_generator_state_t state = NULL;
    const char *key_file = NULL;
    const char *message = NULL;

    // 解析命令行参数
    if (argc > 1) {
        key_file = argv[1];
    }
    if (argc > 2) {
        message = argv[2];
    } else {
        message = "sample_message";
    }

    printf("=== VRF 生成器测试 ===\n\n");

    // 初始化 RELIC 库（如果尚未初始化）
    // 注意：vrf_generate_ecdsa_keypair_relic 内部也会初始化，但这里提前初始化更安全
    if (!relic_initialized) {
        if (core_init() != RLC_OK) {
            fprintf(stderr, "错误: RELIC 库初始化失败\n");
            return 1;
        }
        ep_param_set_any();
        ep_param_set(SECG_K256);  // 设置 secp256k1 曲线
        relic_initialized = 1;
        printf("RELIC 库已初始化\n");
    }

    // 初始化 VRF 生成器
    printf("1. 初始化 VRF 生成器...\n");
    if (key_file && strlen(key_file) > 0) {
        printf("   从文件加载密钥: %s\n", key_file);
    } else {
        printf("   生成新密钥对\n");
    }

    if (vrf_generator_init(&state, key_file) != 1) {
        fprintf(stderr, "错误: VRF 生成器初始化失败\n");
        return 1;
    }
    printf("   ✓ 初始化成功\n\n");

    // 显示公钥
    printf("2. 公钥信息:\n");
    print_hex("   序列化公钥", state->pk_serialized, 33);
    printf("\n");

    // 如果指定了密钥文件但没有保存，保存密钥
    if (key_file && strlen(key_file) > 0) {
        // 尝试从文件加载，如果失败则保存
        vrf_generator_state_t test_state = NULL;
        if (vrf_generator_init(&test_state, key_file) != 1) {
            printf("3. 保存密钥对到文件: %s\n", key_file);
            if (vrf_generator_save_keys(state, key_file) == 1) {
                printf("   ✓ 密钥保存成功\n\n");
            } else {
                printf("   ✗ 密钥保存失败\n\n");
            }
        } else {
            vrf_generator_free(test_state);
        }
    }

    // 生成 VRF 证明和随机数
    printf("4. 生成 VRF 证明和随机数:\n");
    printf("   消息: \"%s\"\n", message);

    unsigned char proof[81];
    unsigned char output[32];

    if (vrf_generator_prove(state, message, strlen(message), proof, output) != 1) {
        fprintf(stderr, "错误: VRF 证明生成失败\n");
        vrf_generator_free(state);
        return 1;
    }

    printf("   ✓ 证明生成成功\n");
    print_hex("   证明", proof, 81);
    print_hex("   随机数输出", output, 32);
    printf("\n");

    // 转换为不同格式
    printf("5. 随机数转换:\n");
    uint64_t range_value = vrf_random_to_range(output, 100);
    double double_value = vrf_random_to_double(output);
    printf("   转换为范围 [0, 100]: %llu\n", (unsigned long long)range_value);
    printf("   转换为浮点数 [0.0, 1.0]: %.10f\n", double_value);
    printf("\n");

    // 验证证明
    printf("6. 验证 VRF 证明:\n");
    unsigned char verify_output[32];
    if (vrf_generator_verify(proof, state->pk_serialized, message, strlen(message), verify_output) == 1) {
        printf("   ✓ 验证成功\n");
        print_hex("   验证得到的随机数", verify_output, 32);
        
        // 检查是否与原始输出相同
        if (memcmp(output, verify_output, 32) == 0) {
            printf("   ✓ 随机数匹配\n");
        } else {
            printf("   ✗ 随机数不匹配（这不应该发生）\n");
        }
    } else {
        printf("   ✗ 验证失败\n");
        printf("\n=== 调试信息：验证失败原因分析 ===\n");
        printf("这通常意味着证明生成或验证过程中存在问题\n");
        printf("请检查以下内容:\n");
        printf("  1. 公钥是否正确: ");
        for (int i = 0; i < 33; i++) {
            printf("%02x", state->pk_serialized[i]);
        }
        printf("\n");
        printf("  2. 消息是否正确: \"%s\"\n", message);
        printf("  3. 证明长度: 81 字节\n");
        printf("===============================\n");
    }
    printf("\n");

    // 测试错误验证（错误的消息）
    printf("7. 测试错误验证（错误的消息）:\n");
    const char *wrong_message = "wrong_message";
    if (vrf_generator_verify(proof, state->pk_serialized, wrong_message, strlen(wrong_message), verify_output) == 1) {
        printf("   ✗ 错误：不应该验证成功\n");
    } else {
        printf("   ✓ 正确拒绝错误消息\n");
    }
    printf("\n");

    // 输出合约验证所需的完整数据
    printf("=======================================================\n");
    printf("      合约验证所需数据（完整格式）\n");
    printf("=======================================================\n");
    printf("\n");
    
    printf("1. 公钥 (Public Key, 33 bytes):\n");
    printf("   Hex: ");
    for (int i = 0; i < 33; i++) {
        printf("%02x", state->pk_serialized[i]);
    }
    printf("\n");
    printf("   Solidity: 0x");
    for (int i = 0; i < 33; i++) {
        printf("%02x", state->pk_serialized[i]);
    }
    printf("\n\n");
    
    printf("2. 证明 (Proof, 81 bytes):\n");
    printf("   Hex: ");
    for (int i = 0; i < 81; i++) {
        printf("%02x", proof[i]);
    }
    printf("\n");
    printf("   Solidity: 0x");
    for (int i = 0; i < 81; i++) {
        printf("%02x", proof[i]);
    }
    printf("\n\n");
    
    printf("3. 消息 (Message):\n");
    printf("   Text: \"%s\"\n", message);
    printf("   Length: %zu bytes\n", strlen(message));
    printf("   Hex: ");
    for (size_t i = 0; i < strlen(message); i++) {
        printf("%02x", (unsigned char)message[i]);
    }
    printf("\n");
    printf("   Solidity (UTF-8): 0x");
    for (size_t i = 0; i < strlen(message); i++) {
        printf("%02x", (unsigned char)message[i]);
    }
    printf("\n\n");
    
    printf("4. 随机数 (Random Output, 32 bytes):\n");
    printf("   Hex: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", output[i]);
    }
    printf("\n");
    printf("   Solidity (bytes32): 0x");
    for (int i = 0; i < 32; i++) {
        printf("%02x", output[i]);
    }
    printf("\n\n");
    
    printf("5. 证明结构详解 (Proof Structure):\n");
    printf("   [0-32]:  Gamma_x (Point X coordinate, 33 bytes compressed)\n");
    printf("            0x");
    for (int i = 0; i < 33; i++) {
        printf("%02x", proof[i]);
    }
    printf("\n");
    printf("   [33-48]: c (Challenge, 16 bytes)\n");
    printf("            0x");
    for (int i = 33; i < 49; i++) {
        printf("%02x", proof[i]);
    }
    printf("\n");
    printf("   [49-80]: s (Response, 32 bytes)\n");
    printf("            0x");
    for (int i = 49; i < 81; i++) {
        printf("%02x", proof[i]);
    }
    printf("\n\n");
    
    printf("6. Solidity 合约调用示例:\n");
    printf("   await committeeRotation.submitVRFRandomWithProof(\n");
    printf("       round,\n");
    printf("       \"0x");
    for (int i = 0; i < 32; i++) {
        printf("%02x", output[i]);
    }
    printf("\", // random\n");
    printf("       \"0x");
    for (int i = 0; i < 81; i++) {
        printf("%02x", proof[i]);
    }
    printf("\", // proof\n");
    printf("       \"0x");
    for (int i = 0; i < 33; i++) {
        printf("%02x", state->pk_serialized[i]);
    }
    printf("\", // publicKey\n");
    printf("       \"0x");
    for (size_t i = 0; i < strlen(message); i++) {
        printf("%02x", (unsigned char)message[i]);
    }
    printf("\"  // message\n");
    printf("   );\n\n");
    
    printf("7. 自动合约验证命令:\n");
    printf("   cd /home/zxx/Config/truffleProject/truffletest && \\\n");
    printf("   truffle exec scripts/verify_vrf_with_contract.js ");
    for (int i = 0; i < 81; i++) {
        printf("%02x", proof[i]);
    }
    printf(" ");
    for (int i = 0; i < 33; i++) {
        printf("%02x", state->pk_serialized[i]);
    }
    printf(" ");
    for (size_t i = 0; i < strlen(message); i++) {
        printf("%02x", (unsigned char)message[i]);
    }
    printf(" ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", output[i]);
    }
    printf("\n\n");
    
    printf("=======================================================\n");
    printf("\n");
    
    // 询问是否执行合约验证
    printf("是否要自动执行合约验证? (y/n): ");
    char choice;
    if (scanf(" %c", &choice) == 1 && (choice == 'y' || choice == 'Y')) {
        printf("\n正在调用合约验证...\n\n");
        
        // 构造命令
        char command[4096];
        int pos = 0;
        
        pos += sprintf(command + pos, "cd /home/zxx/Config/truffleProject/truffletest && ");
        pos += sprintf(command + pos, "truffle exec scripts/verify_vrf_with_contract.js ");
        
        // proof
        for (int i = 0; i < 81; i++) {
            pos += sprintf(command + pos, "%02x", proof[i]);
        }
        pos += sprintf(command + pos, " ");
        
        // publicKey
        for (int i = 0; i < 33; i++) {
            pos += sprintf(command + pos, "%02x", state->pk_serialized[i]);
        }
        pos += sprintf(command + pos, " ");
        
        // message
        for (size_t i = 0; i < strlen(message); i++) {
            pos += sprintf(command + pos, "%02x", (unsigned char)message[i]);
        }
        pos += sprintf(command + pos, " ");
        
        // random
        for (int i = 0; i < 32; i++) {
            pos += sprintf(command + pos, "%02x", output[i]);
        }
        
        // 执行命令
        int result = system(command);
        
        if (result == 0) {
            printf("\n✅ 合约验证完成\n");
        } else {
            printf("\n❌ 合约验证失败 (退出码: %d)\n", result);
        }
    } else {
        printf("跳过合约验证\n");
    }
    printf("\n");

    // 清理
    vrf_generator_free(state);

    printf("=== 测试完成 ===\n");
    return 0;
}

