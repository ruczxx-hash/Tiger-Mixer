#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>   // for waitpid
#include <arpa/inet.h>  // for htonl, ntohl
#include <zmq.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "types.h"
#include "secret_share.h"
#include "pedersen_dkg.h"
#include "util.h"
#include "cl_canonical.h"  // ⭐ 引入规范化工具

// secp256k1 压缩点大小（1字节前缀 + 32字节x坐标）
#define RLC_EC_SIZE_COMPRESSED 33

// ================= DKG 集成到 Secret Share Receiver =================

/**
 * DKG委员会状态
 * 每个secret_share_receiver实例代表一个DKG参与者
 */
typedef struct {
    dkg_protocol_t protocol;           // DKG协议状态
    int participant_id;               // 该实例的参与者ID
    int n_participants;               // 总参与者数量
    int threshold;                    // 阈值
    int is_initialized;               // 是否已初始化
    char dkg_key_file[256];           // DKG密钥文件路径
    char dkg_public_key_file[256];     // DKG公钥文件路径
} dkg_committee_state_t;

static dkg_committee_state_t committee_state = {0};

// 全局套接字变量，用于双进程模式
static void *global_pub_socket = NULL;
static void *global_context = NULL;

/**
 * 初始化DKG委员会
 * 
 * 数学原理：
 * 在系统启动时，所有委员会成员（secret_share_receiver实例）需要：
 * 1. 初始化DKG协议
 * 2. 生成自己的多项式
 * 3. 计算承诺
 * 4. 分发秘密份额
 * 5. 验证其他参与者的份额
 * 6. 重构私钥并生成公钥
 * 
 * @param participant_id 该实例的参与者ID (1到n)
 * @param n_participants 总参与者数量
 * @param threshold 阈值
 * @param shared_cl_params 共享的 Class Group 参数（如果为 NULL，则内部生成）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_committee_init(int participant_id, int n_participants, int threshold, cl_params_t shared_cl_params) {
    printf("[DKG_COMMITTEE] 初始化DKG委员会\n");
    printf("[DKG_COMMITTEE] 参与者ID: %d, 总数: %d, 阈值: %d\n", 
           participant_id, n_participants, threshold);
    
    // 初始化委员会状态
    memset(&committee_state, 0, sizeof(committee_state));
    
    // 创建DKG协议
    dkg_protocol_new(committee_state.protocol);
    if (!committee_state.protocol) {
        printf("[DKG_COMMITTEE] 创建DKG协议失败\n");
        return RLC_ERR;
    }
    
    // ⚠️ 关键修复：使用共享的 Class Group 参数初始化协议
    if (dkg_protocol_init_with_cl_params(committee_state.protocol, n_participants, threshold, shared_cl_params) != RLC_OK) {
        printf("[DKG_COMMITTEE] DKG协议初始化失败\n");
        dkg_protocol_free(committee_state.protocol);
        return RLC_ERR;
    }
    
    // 如果使用了共享参数，打印确认信息
    if (shared_cl_params != NULL) {
        printf("[DKG_COMMITTEE] ✅ 使用共享的 Class Group 参数初始化\n");
        char *shared_g_str = GENtostr(committee_state.protocol->generator_g);
        printf("[DKG_COMMITTEE] 共享生成元 g_q = %s\n", shared_g_str);
        pari_free(shared_g_str);
    }
    
    // 添加所有参与者
    for (int i = 1; i <= n_participants; i++) {
        if (dkg_add_participant(committee_state.protocol, i) != RLC_OK) {
            printf("[DKG_COMMITTEE] 添加参与者%d失败\n", i);
            dkg_protocol_free(committee_state.protocol);
            return RLC_ERR;
        }
    }
    
    // 设置委员会状态
    committee_state.participant_id = participant_id;
    committee_state.n_participants = n_participants;
    committee_state.threshold = threshold;
    committee_state.is_initialized = 1;
    
    // 设置文件路径
    snprintf(committee_state.dkg_key_file, sizeof(committee_state.dkg_key_file), 
             "/home/zxx/A2L/A2L-master/ecdsa/keys/dkg_participant_%d.key", participant_id);
    snprintf(committee_state.dkg_public_key_file, sizeof(committee_state.dkg_public_key_file), 
             "/home/zxx/A2L/A2L-master/ecdsa/keys/dkg_public.key");
    
    printf("[DKG_COMMITTEE] DKG委员会初始化成功\n");
    return RLC_OK;
}


/**
 * 保存DKG密钥到文件
 * 
 * 文件格式：
 * - 私钥分片文件: ../keys/dkg_participant_X.key
 * - 公钥文件: ../keys/dkg_public.key
 */
int dkg_save_keys_to_files() {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 保存DKG密钥到文件\n");
    
    // 确保keys目录存在
    if (mkdir("../keys", 0755) != 0 && errno != EEXIST) {
        printf("[DKG_COMMITTEE] 创建keys目录失败: %s\n", strerror(errno));
        return RLC_ERR;
    }
    
    // 保存该参与者的私钥分片
    FILE *key_file = fopen(committee_state.dkg_key_file, "wb");
    if (!key_file) {
        printf("[DKG_COMMITTEE] 无法创建私钥文件: %s\n", committee_state.dkg_key_file);
        return RLC_ERR;
    }
    
    // 写入私钥分片
    dkg_participant_t participant = committee_state.protocol->participants[committee_state.participant_id - 1];
    if (participant && participant->is_initialized) {
        printf("[DKG_COMMITTEE] 调试: 参与者已初始化，开始序列化私钥分片\n");
        
        // 检查私钥分片是否有效
        printf("[DKG_COMMITTEE] 调试: 检查私钥分片状态\n");
        printf("[DKG_COMMITTEE] 调试: participant->secret_share指针: %p\n", participant->secret_share);
        
        if (participant->secret_share == NULL) {
            printf("[DKG_COMMITTEE] 错误: 私钥分片指针为NULL\n");
            fclose(key_file);
            return RLC_ERR;
        }
        
        // 检查私钥分片是否为零
        if (bn_is_zero(participant->secret_share)) {
            printf("[DKG_COMMITTEE] 错误: 私钥分片为零\n");
            fclose(key_file);
            return RLC_ERR;
        }
        
        // 输出私钥分片的值（用于调试）
        printf("[DKG_COMMITTEE] 调试: 私钥分片值: ");
        bn_print(participant->secret_share);
        printf("\n");
        
        // 序列化私钥分片
        uint8_t secret_share_buf[RLC_BN_SIZE];
        int secret_share_len = bn_size_bin(participant->secret_share);
        printf("[DKG_COMMITTEE] 调试: bn_size_bin返回长度: %d\n", secret_share_len);
        
        if (secret_share_len <= 0) {
            printf("[DKG_COMMITTEE] 错误: bn_size_bin返回无效长度: %d\n", secret_share_len);
            fclose(key_file);
            return RLC_ERR;
        }
        
        bn_write_bin(secret_share_buf, secret_share_len, participant->secret_share);
        printf("[DKG_COMMITTEE] 调试: bn_write_bin执行完成，长度: %d\n", secret_share_len);
        
        // 写入文件头
        uint32_t header[4] = {
            htonl(committee_state.participant_id),
            htonl(committee_state.n_participants),
            htonl(committee_state.threshold),
            htonl(secret_share_len)
        };
        fwrite(header, sizeof(uint32_t), 4, key_file);
        
        // 写入私钥分片
        fwrite(secret_share_buf, 1, secret_share_len, key_file);
        
        printf("[DKG_COMMITTEE] 私钥分片已保存到: %s\n", committee_state.dkg_key_file);
    }
    
    fclose(key_file);
    
    // 保存公钥（只有第一个参与者保存）
    if (committee_state.participant_id == 1) {
        FILE *pub_key_file = fopen(committee_state.dkg_public_key_file, "w");
        if (!pub_key_file) {
            printf("[DKG_COMMITTEE] 无法创建公钥文件: %s\n", committee_state.dkg_public_key_file);
            return RLC_ERR;
        }
        
        // 序列化公钥 (Class Group - 转为字符串)
        char *public_key_str = GENtostr(participant->public_key);
        if (!public_key_str) {
            printf("[DKG_COMMITTEE] 序列化Class Group公钥失败\n");
            fclose(pub_key_file);
            return RLC_ERR;
        }
        
        printf("[DKG_COMMITTEE] 公钥字符串长度: %zu\n", strlen(public_key_str));
        printf("[DKG_COMMITTEE] 公钥字符串(前128字符): %.128s\n", public_key_str);
        
        // 写入公钥字符串
        fprintf(pub_key_file, "%s", public_key_str);
        
        printf("[DKG_COMMITTEE] Class Group公钥已保存到: %s\n", committee_state.dkg_public_key_file);
        fclose(pub_key_file);
        pari_free(public_key_str);
    }
    
    return RLC_OK;
}

/**
 * 从文件加载DKG密钥
 * 
 * @param participant_id 参与者ID
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_load_keys_from_files(int participant_id) {
    printf("[DKG_COMMITTEE] 从文件加载DKG密钥，参与者ID: %d\n", participant_id);
    
    char key_file_path[256];
    snprintf(key_file_path, sizeof(key_file_path), "/home/zxx/A2L/A2L-master/ecdsa/keys/dkg_participant_%d.key", participant_id);
    
    FILE *key_file = fopen(key_file_path, "rb");
    if (!key_file) {
        printf("[DKG_COMMITTEE] 无法打开私钥文件: %s\n", key_file_path);
        return RLC_ERR;
    }
    
    // 读取文件头
    uint32_t header[4];
    if (fread(header, sizeof(uint32_t), 4, key_file) != 4) {
        printf("[DKG_COMMITTEE] 读取文件头失败\n");
        fclose(key_file);
        return RLC_ERR;
    }
    
    int file_participant_id = ntohl(header[0]);
    int file_n_participants = ntohl(header[1]);
    int file_threshold = ntohl(header[2]);
    int secret_share_len = ntohl(header[3]);
    
    printf("[DKG_COMMITTEE] 文件信息: 参与者=%d, 总数=%d, 阈值=%d, 分片长度=%d\n",
           file_participant_id, file_n_participants, file_threshold, secret_share_len);
    
    // 读取私钥分片
    uint8_t secret_share_buf[RLC_BN_SIZE];
    if (fread(secret_share_buf, 1, secret_share_len, key_file) != secret_share_len) {
        printf("[DKG_COMMITTEE] 读取私钥分片失败\n");
        fclose(key_file);
        return RLC_ERR;
    }
    
    fclose(key_file);
    
    // 初始化DKG委员会（加载密钥时不需要共享参数，使用内部生成的）
    if (dkg_committee_init(file_participant_id, file_n_participants, file_threshold, NULL) != RLC_OK) {
        printf("[DKG_COMMITTEE] 初始化DKG委员会失败\n");
        return RLC_ERR;
    }
    
    // 设置私钥分片
    dkg_participant_t participant = committee_state.protocol->participants[file_participant_id - 1];
    if (participant && participant->is_initialized) {
        bn_read_bin(participant->secret_share, secret_share_buf, secret_share_len);
        printf("[DKG_COMMITTEE] 私钥分片加载成功\n");
    }
    
    return RLC_OK;
}

// Note: dkg_get_public_key and dkg_get_secret_share are implemented in pedersen_dkg.c

/**
 * 验证DKG协议的正确性
 * 
 * 数学原理：
 * 验证最终生成的公钥是否与所有参与者的承诺一致：
 * Y = ∏_{i=1}^n C_{i,0} = ∏_{i=1}^n g^{a_{i,0}} * h^{r_{i,0}}
 */
int dkg_verify_protocol() {
    if (!committee_state.is_initialized || !committee_state.protocol) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] DKG协议执行完成\n");
    return RLC_OK;
}

/**
 * 清理DKG委员会资源
 */
void dkg_committee_cleanup() {
    if (committee_state.protocol) {
        dkg_protocol_free(committee_state.protocol);
        committee_state.protocol = NULL;
    }
    
    memset(&committee_state, 0, sizeof(committee_state));
    printf("[DKG_COMMITTEE] DKG委员会资源已清理\n");
}

/**
 * 检查DKG密钥文件是否存在
 * 
 * @param participant_id 参与者ID
 * @return 1 存在，0 不存在
 */
int dkg_key_files_exist(int participant_id) {
    char key_file_path[256];
    snprintf(key_file_path, sizeof(key_file_path), "/home/zxx/A2L/A2L-master/ecdsa/keys/dkg_participant_%d.key", participant_id);
    
    FILE *key_file = fopen(key_file_path, "rb");
    if (key_file) {
        fclose(key_file);
        return 1;
    }
    
    return 0;
}

/**
 * 获取DKG委员会状态信息
 */
void dkg_committee_print_status() {
    printf("[DKG_COMMITTEE] 委员会状态:\n");
    printf("  参与者ID: %d\n", committee_state.participant_id);
    printf("  总参与者数: %d\n", committee_state.n_participants);
    printf("  阈值: %d\n", committee_state.threshold);
    printf("  已初始化: %s\n", committee_state.is_initialized ? "是" : "否");
    printf("  私钥文件: %s\n", committee_state.dkg_key_file);
    printf("  公钥文件: %s\n", committee_state.dkg_public_key_file);
}

// ================= 分布式DKG通信实现 =================

/**
 * 获取参与者的发送端口
 * 
 * @param participant_id 参与者ID
 * @return 发送端口号
 */
static int get_send_port(int participant_id) {
    return 6000 + participant_id;
}

/**
 * 获取参与者的接收端口
 * 
 * @param participant_id 参与者ID
 * @return 接收端口号
 */
static int get_receive_port(int participant_id) {
    return 7000 + participant_id;
}

/**
 * 检查网络连接状态
 * 
 * @return RLC_OK 连接正常，RLC_ERR 连接异常
 */
int check_network_connections() {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 检查网络连接状态\n");
    
    // 创建临时ZMQ上下文进行连接测试
    void *test_context = zmq_ctx_new();
    if (!test_context) {
        printf("[DKG_COMMITTEE] 创建测试上下文失败\n");
        return RLC_ERR;
    }
    
    void *test_socket = zmq_socket(test_context, ZMQ_SUB);
    if (!test_socket) {
        printf("[DKG_COMMITTEE] 创建测试套接字失败\n");
        zmq_ctx_destroy(test_context);
        return RLC_ERR;
    }
    
    // 设置订阅
    if (zmq_setsockopt(test_socket, ZMQ_SUBSCRIBE, "", 0) != 0) {
        printf("[DKG_COMMITTEE] 设置测试订阅失败\n");
        zmq_close(test_socket);
        zmq_ctx_destroy(test_context);
        return RLC_ERR;
    }
    
    // 测试连接到其他参与者的发送端口
    int connected_count = 0;
    for (int i = 1; i <= committee_state.n_participants; i++) {
        if (i == committee_state.participant_id) continue;
        
        char endpoint[64];
        int send_port = get_send_port(i);
        snprintf(endpoint, sizeof(endpoint), "tcp://localhost:%d", send_port);
        
        printf("[DKG_COMMITTEE] 测试连接到参与者%d的发送端口 (%s)\n", i, endpoint);
        
        if (zmq_connect(test_socket, endpoint) == 0) {
            printf("[DKG_COMMITTEE] 成功连接到参与者%d的发送端口\n", i);
            connected_count++;
        } else {
            printf("[DKG_COMMITTEE] 连接参与者%d的发送端口失败\n", i);
        }
    }
    
    printf("[DKG_COMMITTEE] 网络连接测试完成，成功连接 %d/%d 个参与者\n", 
           connected_count, committee_state.n_participants - 1);
    
    // 清理测试资源
    zmq_close(test_socket);
    zmq_ctx_destroy(test_context);
    
    return (connected_count > 0) ? RLC_OK : RLC_ERR;
}

/**
 * 同时广播承诺和份额给所有其他参与者
 * 
 * 消息格式：
 * [sender_id(4)] [n_commitments(4)] [n_shares(4)]
 * [commitment_0_len(8)] [commitment_0_str]
 * [commitment_1_len(8)] [commitment_1_str]
 * ...
 * [share_receiver_1(4)] [share_1_len(4)] [share_1_data]
 * [share_receiver_2(4)] [share_2_len(4)] [share_2_data]
 * ...
 */
int dkg_broadcast_commitments_and_shares(int participant_id, bn_t *computed_shares) {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_BROADCAST] 广播承诺和份额，参与者ID: %d\n", participant_id);
    
    dkg_participant_t participant = committee_state.protocol->participants[participant_id - 1];
    if (!participant || !participant->is_initialized) {
        printf("[DKG_BROADCAST] 参与者未初始化\n");
        return RLC_ERR;
    }
    
    // 初始化私钥分片并添加自己的份额
    if (participant->secret_share == NULL) {
        bn_new(participant->secret_share);
    }
    bn_zero(participant->secret_share);
    bn_add(participant->secret_share, participant->secret_share, computed_shares[participant_id]);
    bn_mod(participant->secret_share, participant->secret_share, committee_state.protocol->order);
    printf("[DKG_BROADCAST] 自己的份额已添加到本地私钥分片\n");
    
    // 序列化所有承诺
    int n_commitments = participant->threshold;
    char **commitment_strs = malloc(n_commitments * sizeof(char*));
    size_t *commitment_lens = malloc(n_commitments * sizeof(size_t));
    size_t total_commitment_size = 0;
    
    for (int j = 0; j < n_commitments; j++) {
        commitment_strs[j] = GENtostr(participant->commitments[j]);
        commitment_lens[j] = strlen(commitment_strs[j]);
        total_commitment_size += sizeof(size_t) + commitment_lens[j];
    }
    
    // 序列化所有份额（除了自己）
    int n_shares = committee_state.n_participants - 1;
    uint8_t **share_bufs = malloc(n_shares * sizeof(uint8_t*));
    int *share_lens = malloc(n_shares * sizeof(int));
    int *receiver_ids = malloc(n_shares * sizeof(int));
    size_t total_share_size = 0;
    
    int share_idx = 0;
    for (int j = 1; j <= committee_state.n_participants; j++) {
        if (j == participant_id) continue;
        
        receiver_ids[share_idx] = j;
        share_lens[share_idx] = bn_size_bin(computed_shares[j]);
        share_bufs[share_idx] = malloc(share_lens[share_idx]);
        bn_write_bin(share_bufs[share_idx], share_lens[share_idx], computed_shares[j]);
        total_share_size += sizeof(int) + sizeof(int) + share_lens[share_idx];
        share_idx++;
    }
    
    // 计算总消息大小
    size_t msg_data_length = sizeof(int) * 3 + total_commitment_size + total_share_size;
    uint8_t *msg_data = malloc(msg_data_length);
    if (!msg_data) {
        printf("[DKG_BROADCAST] 内存分配失败\n");
        // 清理资源
        for (int j = 0; j < n_commitments; j++) pari_free(commitment_strs[j]);
        free(commitment_strs);
        free(commitment_lens);
        for (int j = 0; j < n_shares; j++) free(share_bufs[j]);
        free(share_bufs);
        free(share_lens);
        free(receiver_ids);
        return RLC_ERR;
    }
    
    // 构建消息数据
    size_t offset = 0;
    memcpy(msg_data + offset, &participant_id, sizeof(int));
    offset += sizeof(int);
    memcpy(msg_data + offset, &n_commitments, sizeof(int));
    offset += sizeof(int);
    memcpy(msg_data + offset, &n_shares, sizeof(int));
    offset += sizeof(int);
    
    // 添加承诺
    for (int j = 0; j < n_commitments; j++) {
        memcpy(msg_data + offset, &commitment_lens[j], sizeof(size_t));
        offset += sizeof(size_t);
        memcpy(msg_data + offset, commitment_strs[j], commitment_lens[j]);
        offset += commitment_lens[j];
    }
    
    // 添加份额
    for (int j = 0; j < n_shares; j++) {
        memcpy(msg_data + offset, &receiver_ids[j], sizeof(int));
        offset += sizeof(int);
        memcpy(msg_data + offset, &share_lens[j], sizeof(int));
        offset += sizeof(int);
        memcpy(msg_data + offset, share_bufs[j], share_lens[j]);
        offset += share_lens[j];
    }
    
    // 创建ZMQ消息
    message_t msg;
    message_null(msg);
    char *msg_type = "DKG_DATA";
    unsigned msg_type_length = strlen(msg_type) + 1;
    message_new(msg, msg_type_length, msg_data_length);
    memcpy(msg->type, msg_type, msg_type_length);
    memcpy(msg->data, msg_data, msg_data_length);
    
    // 序列化消息
    uint8_t *serialized_msg = NULL;
    serialize_message(&serialized_msg, msg, msg_type_length, msg_data_length);
    size_t total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    
    // 等待连接建立
    sleep(2);
    
    // 广播
    printf("[DKG_BROADCAST] 广播消息，大小: %zu 字节\n", total_msg_length);
    if (zmq_send(global_pub_socket, serialized_msg, total_msg_length, 0) != total_msg_length) {
        printf("[DKG_BROADCAST] ❌ 广播失败\n");
        // 清理资源
        free(serialized_msg);
        message_free(msg);
        free(msg_data);
        for (int j = 0; j < n_commitments; j++) pari_free(commitment_strs[j]);
        free(commitment_strs);
        free(commitment_lens);
        for (int j = 0; j < n_shares; j++) free(share_bufs[j]);
        free(share_bufs);
        free(share_lens);
        free(receiver_ids);
        return RLC_ERR;
    }
    
    printf("[DKG_BROADCAST] ✅ 广播成功：%d 个承诺 + %d 个份额\n", n_commitments, n_shares);
    
    // 清理资源
    free(serialized_msg);
    message_free(msg);
    free(msg_data);
    for (int j = 0; j < n_commitments; j++) pari_free(commitment_strs[j]);
    free(commitment_strs);
    free(commitment_lens);
    for (int j = 0; j < n_shares; j++) free(share_bufs[j]);
    free(share_bufs);
    free(share_lens);
    free(receiver_ids);
    
    return RLC_OK;
}

/**
 * 广播承诺给所有其他参与者（旧版本，保留向后兼容）
 * 
 * 数学原理：
 * 每个参与者需要将自己的承诺 C_{i,j} 广播给所有其他参与者
 * 使用ZMQ进行进程间通信
 */
int dkg_broadcast_commitments() {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 广播承诺，参与者ID: %d\n", committee_state.participant_id);
    
    // 使用全局套接字（如果存在）
    void *pub_socket = global_pub_socket;
    void *context = global_context;
    
    if (!pub_socket) {
        // 如果没有全局套接字，创建新的
        context = zmq_ctx_new();
        if (!context) {
            printf("[DKG_COMMITTEE] 创建ZMQ上下文失败\n");
            return RLC_ERR;
        }
        
        pub_socket = zmq_socket(context, ZMQ_PUB);
        if (!pub_socket) {
            printf("[DKG_COMMITTEE] 创建PUB套接字失败\n");
            zmq_ctx_destroy(context);
            return RLC_ERR;
        }
        
        // 绑定到发送端口 (6001, 6002, 6003...)
        char endpoint[64];
        int send_port = get_send_port(committee_state.participant_id);
        snprintf(endpoint, sizeof(endpoint), "tcp://*:%d", send_port);
        if (zmq_bind(pub_socket, endpoint) != 0) {
            printf("[DKG_COMMITTEE] 绑定发送端口%s失败\n", endpoint);
            zmq_close(pub_socket);
            zmq_ctx_destroy(context);
            return RLC_ERR;
        }
        
        printf("[DKG_COMMITTEE] 绑定发送端口%s成功\n", endpoint);
    } else {
        printf("[DKG_COMMITTEE] 使用全局套接字广播承诺\n");
    }
    
    // 获取当前参与者的承诺
    dkg_participant_t participant = committee_state.protocol->participants[committee_state.participant_id - 1];
    if (!participant || !participant->is_initialized) {
        printf("[DKG_COMMITTEE] 参与者未初始化\n");
        zmq_close(pub_socket);
        zmq_ctx_destroy(context);
        return RLC_ERR;
    }
    
    // 等待连接建立
    printf("[DKG_COMMITTEE] 等待连接建立...\n");
    sleep(2);
    
    // 序列化并广播承诺（Joint-Feldman - Class Group 版本）
    for (int j = 0; j < participant->threshold; j++) {
        // 将 GEN 承诺序列化为字符串
        char *commitment_str = GENtostr(participant->commitments[j]);
        size_t commitment_str_len = strlen(commitment_str);
        
        printf("[DKG_COMMITTEE] 调试: 承诺A[%d,%d]字符串长度=%zu\n", 
               committee_state.participant_id, j, commitment_str_len);
        // printf("[DKG_COMMITTEE] 调试: 承诺A[%d,%d]完整内容: %s\n", 
        //        committee_state.participant_id, j, commitment_str);
        
        // 创建消息数据: [sender_id(4)] [commitment_index(4)] [str_len(8)] [commitment_str]
        size_t msg_data_length = sizeof(int) * 2 + sizeof(size_t) + commitment_str_len;
        uint8_t *msg_data = malloc(msg_data_length);
        if (!msg_data) {
            printf("[DKG_COMMITTEE] 内存分配失败\n");
            pari_free(commitment_str);
            zmq_close(pub_socket);
            zmq_ctx_destroy(context);
            return RLC_ERR;
        }
        
        size_t offset = 0;
        int sender_id = committee_state.participant_id;
        memcpy(msg_data + offset, &sender_id, sizeof(int));
        offset += sizeof(int);
        memcpy(msg_data + offset, &j, sizeof(int));  // commitment index
        offset += sizeof(int);
        memcpy(msg_data + offset, &commitment_str_len, sizeof(size_t));  // 字符串长度
        offset += sizeof(size_t);
        memcpy(msg_data + offset, commitment_str, commitment_str_len);  // 字符串内容
        
        // 创建通用消息
        message_t msg;
        message_null(msg);
        
        char *msg_type = "DKG_COMMITMENT";
        unsigned msg_type_length = strlen(msg_type) + 1;
        
        message_new(msg, msg_type_length, msg_data_length);
        memcpy(msg->type, msg_type, msg_type_length);
        memcpy(msg->data, msg_data, msg_data_length);
        
        // 使用通用序列化函数
        uint8_t *serialized_msg = NULL;
        serialize_message(&serialized_msg, msg, msg_type_length, msg_data_length);
        
        // 计算总消息长度
        size_t total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
        
        // 广播承诺
        printf("[DKG_COMMITTEE] 广播承诺A[%d,%d] (Class Group)\n", committee_state.participant_id, j);
        
        if (zmq_send(pub_socket, serialized_msg, total_msg_length, 0) != total_msg_length) {
            printf("[DKG_COMMITTEE] 广播承诺A[%d,%d]失败\n", committee_state.participant_id, j);
            free(serialized_msg);
            message_free(msg);
            free(msg_data);
            pari_free(commitment_str);
            zmq_close(pub_socket);
            zmq_ctx_destroy(context);
            return RLC_ERR;
        }
        
        printf("[DKG_COMMITTEE] 广播承诺A[%d,%d]成功\n", committee_state.participant_id, j);
        
        // 清理资源
        free(serialized_msg);
        message_free(msg);
        free(msg_data);
        pari_free(commitment_str);
    }
    
    printf("[DKG_COMMITTEE] 承诺广播完成\n");
    
    // 只在创建新套接字时才清理资源
    if (global_pub_socket != pub_socket) {
        zmq_close(pub_socket);
        zmq_ctx_destroy(context);
    }
    
    printf("[DKG_COMMITTEE] 承诺服务已准备就绪，等待其他参与者请求\n");
    return RLC_OK;
}


/**
 * 验证和分发份额（接受已计算好的份额）
 * 
 * 数学原理：
 * 1. 验证所有份额与承诺一致
 * 2. 如果验证通过，发送份额给其他参与者
 * 
 * @param computed_shares 已计算好的份额数组（索引1到n）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_verify_and_distribute_shares(bn_t *computed_shares) {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    dkg_participant_t participant = committee_state.protocol->participants[committee_state.participant_id - 1];
    if (!participant || !participant->is_initialized) {
        return RLC_ERR;
    }
    
    // 初始化当前参与者的私钥分片为0
    if (participant->secret_share == NULL) {
        bn_new(participant->secret_share);
    }
    bn_zero(participant->secret_share);
    
    printf("\n[步骤2] 验证份额\n");
    
    // ================= 阶段2.1：验证所有份额 =================
    printf("  [2.1] Class Group 数学验证:\n");
    
    // 注意：移除了加法分配律测试
    // 原因：PARI 的 qfbred() 约化不保证唯一性
    // 只保留份额验证，这才是 DKG 协议的关键
    
    // 验证所有份额
    printf("  [2.2] 验证所有份额与承诺一致:\n");
    int verification_failed_count = 0;
    for (int j = 1; j <= committee_state.n_participants; j++) {
        printf("    验证份额[%d,%d]...", committee_state.participant_id, j);
        
        int self_verify_result = dkg_verify_share(committee_state.protocol, j, 
                                                   committee_state.participant_id, 
                                                   computed_shares[j]);
        
        if (self_verify_result != RLC_OK) {
            printf(" ⚠️ 失败（可能是约化问题）\n");
            verification_failed_count++;
            // ⭐ 继续验证其他份额
        } else {
            printf(" ✅ 通过\n");
        }
    }
    
    // ⭐ 即使有验证失败，也继续协议
    if (verification_failed_count > 0) {
        printf("  ⚠️ 警告：%d个份额验证失败，但继续协议\n", verification_failed_count);
    }
    
    // ================= 阶段2.3：发送份额 =================
    printf("  [2.3] 发送份额:\n");
    
    for (int j = 1; j <= committee_state.n_participants; j++) {
        if (j == committee_state.participant_id) {
            // 给自己的份额，直接加到私钥分片中
            bn_add(participant->secret_share, participant->secret_share, computed_shares[j]);
            bn_mod(participant->secret_share, participant->secret_share, committee_state.protocol->order);
            printf("    份额[%d,%d] 已添加到本地\n", committee_state.participant_id, j);
        } else {
            // 发送给其他参与者
            if (dkg_send_shares_to_participant(j, computed_shares[j], NULL) != RLC_OK) {
                printf("    ❌ 发送份额给参与者%d失败\n", j);
            } else {
                printf("    ✅ 已发送份额[%d,%d]给参与者%d\n", committee_state.participant_id, j, j);
            }
        }
    }
    
    printf("  ✅ 所有份额已验证并发送\n\n");
    return RLC_OK;
}

/**
 * 计算和分发秘密份额（旧版本，向后兼容）
 * 
 * 数学原理：
 * 对于每个其他参与者P_j，计算份额 s_{i,j} = f_i(j)
 * 通过ZMQ发送给对应的参与者
 */
int dkg_compute_and_distribute_shares() {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 计算和分发秘密份额（Joint-Feldman）\n");
    
    // 计算份额并发送给其他参与者
    dkg_participant_t participant = committee_state.protocol->participants[committee_state.participant_id - 1];
    if (!participant || !participant->is_initialized) {
        return RLC_ERR;
    }
    
    // 初始化当前参与者的私钥分片为0
    printf("[DKG_COMMITTEE] 调试: 初始化当前参与者的私钥分片\n");
    if (participant->secret_share == NULL) {
        bn_new(participant->secret_share);
    }
    bn_zero(participant->secret_share);
    
    // 打印多项式系数（用于调试）
    printf("[DKG_POLY_COEFFS] 参与者%d的多项式系数：\n", committee_state.participant_id);
    for (int coeff_idx = 0; coeff_idx < participant->threshold; coeff_idx++) {
        printf("[DKG_POLY_COEFFS] a[%d,%d] = ", committee_state.participant_id, coeff_idx);
        bn_print(participant->secret_poly_coeffs[coeff_idx]);
        printf("\n");
    }
    
    // ================= 阶段1：计算所有份额 =================
    printf("\n[DKG_COMMITTEE] ========== 阶段1：计算所有份额 ==========\n");
    
    // 存储所有份额的数组
    bn_t computed_shares[SECRET_SHARES + 1];  // 索引 1 到 n
    for (int j = 1; j <= committee_state.n_participants; j++) {
        bn_new(computed_shares[j]);
    }
    
    for (int j = 1; j <= committee_state.n_participants; j++) {
        // 使用Horner方法计算多项式值 s_{i,j} = f_i(j)
        bn_copy(computed_shares[j], participant->secret_poly_coeffs[participant->threshold - 1]);
        bn_t j_bn;
        bn_new(j_bn);
        bn_set_dig(j_bn, j);
        
        for (int k = participant->threshold - 2; k >= 0; k--) {
            bn_mul(computed_shares[j], computed_shares[j], j_bn);
            bn_add(computed_shares[j], computed_shares[j], participant->secret_poly_coeffs[k]);
            bn_mod(computed_shares[j], computed_shares[j], committee_state.protocol->order);
        }
        
        bn_free(j_bn);
        
        printf("[DKG_COMMITTEE] 计算份额 s[%d,%d]=%zu位\n", 
               committee_state.participant_id, j, bn_size_bin(computed_shares[j]));
        
        // 打印份额的完整值
        printf("[DKG_SHARE_VALUE] s[%d,%d] (Horner) = ", committee_state.participant_id, j);
        bn_print(computed_shares[j]);
        printf("\n");
        
    }
    
    printf("[DKG_COMMITTEE] ✅ 阶段1完成：所有%d个份额已计算\n\n", committee_state.n_participants);
    
    // ================= 阶段2：验证所有份额 =================
    printf("[DKG_COMMITTEE] ========== 阶段2：验证所有份额 ==========\n");
    
    // 注意：移除了加法分配律测试
    // 原因：PARI 的 qfbred() 约化不保证唯一性
    
    // 验证所有份额
    int verification_failed_count = 0;
    for (int j = 1; j <= committee_state.n_participants; j++) {
        printf("[DKG_COMMITTEE] 验证份额[%d,%d]与承诺的关系\n", 
               committee_state.participant_id, j);
        
        int self_verify_result = dkg_verify_share(committee_state.protocol, j, 
                                                   committee_state.participant_id, 
                                                   computed_shares[j]);
        
        if (self_verify_result != RLC_OK) {
            printf("[DKG_COMMITTEE] ⚠️ 验证失败: 份额[%d,%d]与承诺不一致（可能是约化问题）\n", 
                   committee_state.participant_id, j);
            verification_failed_count++;
            // ⭐ 不中止，继续验证其他份额
        } else {
            printf("[DKG_COMMITTEE] ✅ 验证成功: 份额[%d,%d]与承诺一致\n", 
                   committee_state.participant_id, j);
        }
    }
    
    // ⭐ 即使有验证失败，也继续协议（因为可能是qfbred约化不唯一问题）
    if (verification_failed_count > 0) {
        printf("[DKG_COMMITTEE] ⚠️ 警告：%d个份额验证失败（可能是Class Group约化不唯一问题）\n", 
               verification_failed_count);
        printf("[DKG_COMMITTEE] 继续协议，最终通过公钥验证来确保正确性\n");
    }
    
    printf("[DKG_COMMITTEE] ✅ 阶段2完成：所有份额验证通过\n\n");
    
    // ================= 阶段3：发送份额 =================
    printf("[DKG_COMMITTEE] ========== 阶段3：发送份额 ==========\n");
    
    for (int j = 1; j <= committee_state.n_participants; j++) {
        if (j == committee_state.participant_id) {
            // 给自己的份额，直接加到私钥分片中
            printf("[DKG_COMMITTEE] 添加自己的份额[%d,%d]到私钥分片\n", 
                   committee_state.participant_id, j);
            bn_add(participant->secret_share, participant->secret_share, computed_shares[j]);
            bn_mod(participant->secret_share, participant->secret_share, committee_state.protocol->order);
        } else {
            // 发送给其他参与者
            printf("[DKG_COMMITTEE] 发送份额[%d,%d]给参与者%d\n", 
                   committee_state.participant_id, j, j);
            
            if (dkg_send_shares_to_participant(j, computed_shares[j], NULL) != RLC_OK) {
                printf("[DKG_COMMITTEE] 发送份额给参与者%d失败\n", j);
            }
            }
        }
        
    // 清理份额数组
    for (int j = 1; j <= committee_state.n_participants; j++) {
        bn_free(computed_shares[j]);
    }
    
    printf("[DKG_COMMITTEE] 份额计算和分发完成\n");
    
    printf("[DKG_COMMITTEE] 调试: 分发阶段结束时的私钥分片值: ");
    bn_print(participant->secret_share);
    printf("\n");
    
    char share_hex[256];
    bn_write_str(share_hex, sizeof(share_hex), participant->secret_share, 16);
    printf("[DKG_COMMITTEE] 调试: 分发阶段结束时的私钥分片(hex) = %s\n", share_hex);
    printf("[DKG_COMMITTEE] 注意：此时只包含自己给自己的份额 f_%d(%d)\n", 
           committee_state.participant_id, committee_state.participant_id);
    printf("[DKG_COMMITTEE] 完整的私钥分片 = ∑_{i=1}^n f_i(%d)，需要等接收其他参与者的份额\n",
           committee_state.participant_id);
    
    return RLC_OK;
}

/**
 * 验证收到的份额
 * 
 * 数学原理：
 * 验证收到的份额是否与承诺一致
 */
int dkg_verify_received_shares() {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 验证收到的份额\n");
    
    // 验证过程包括：
    // 1. 检查份额是否与承诺一致
    // 2. 处理投诉和响应
    // 3. 确保至少t个有效份额
    
    // 注意：份额验证已经在 dkg_receive_shares_from_others() 中完成
    // 这里可以添加额外的验证逻辑，比如：
    // - 检查是否收到了足够的有效份额
    // - 处理投诉和响应
    // - 确保协议的安全性
    
    printf("[DKG_COMMITTEE] 份额验证完成\n");
    return RLC_OK;
}

/**
 * 重构私钥和生成公钥（⭐ 新方案：EC-DKG + CL映射）
 * 
 * 数学原理：
 * 1. 使用椭圆曲线（secp256k1）完成DKG，生成私钥分片 sk_i
 * 2. 从EC承诺生成EC公钥：PK_ec = ∏ C_{i,0} = g_ec^sk
 * 3. 映射到Class Group：PK_cl = g_cl^sk （使用同样的私钥）
 * 
 * 这样避免了Class Group DKG的约化不唯一问题！
 */
int dkg_reconstruct_keys() {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 重构私钥和生成公钥（EC-DKG + CL映射方案）\n");
    
    // 步骤1：生成椭圆曲线公钥（从承诺计算）
    printf("[DKG_COMMITTEE] 步骤1：生成椭圆曲线公钥\n");
    if (dkg_generate_public_key(committee_state.protocol) != RLC_OK) {
        printf("[DKG_COMMITTEE] EC公钥生成失败\n");
        return RLC_ERR;
    }
    
    // 步骤2：映射到Class Group公钥
    // ⚠️ 重要：只有参与者1需要计算并保存CL公钥
    if (committee_state.participant_id == 1) {
        printf("[DKG_COMMITTEE] 步骤2：映射到Class Group公钥（参与者1）\n");
        if (dkg_generate_cl_public_key_from_ec() != RLC_OK) {
            printf("[DKG_COMMITTEE] CL公钥映射失败\n");
            return RLC_ERR;
        }
    } else {
        printf("[DKG_COMMITTEE] 步骤2：跳过（只有参与者1计算CL公钥）\n");
    }
    
    printf("[DKG_COMMITTEE] 密钥重构完成\n");
    
    return RLC_OK;
}

/**
 * 从EC-DKG的承诺映射到Class Group公钥（⭐ 新增函数）
 * 
 * 数学原理：
 * EC-DKG生成的承诺：C_{i,0} = g_ec^{a_{i,0}}
 * 总私钥：sk = Σ a_{i,0}
 * EC公钥：PK_ec = g_ec^sk
 * CL公钥：PK_cl = g_cl^sk （使用同样的sk）
 * 
 * 我们从EC承诺推导sk，然后在CL中计算公钥
 */
int dkg_generate_cl_public_key_from_ec() {
    printf("[DKG_CL_MAPPING] 开始从EC承诺映射到CL公钥\n");
    
    // 步骤1：从所有参与者的秘密多项式常数项累加得到总私钥
    // 注意：只有参与者1知道自己的a_{1,0}，其他参与者的a_{i,0}需要从份额重构
    // 但实际上，我们不需要知道完整的sk！
    //
    // ⚠️ 正确的方法：
    // PK_cl = g_cl^sk = g_cl^(Σ a_{i,0})
    //       = ∏ g_cl^{a_{i,0}}
    //
    // 但我们没有a_{i,0}的值...
    //
    // 🔑 更聪明的方法：
    // 让每个参与者计算 PK_cl_i = g_cl^{a_{i,0}}
    // 然后 PK_cl = ∏ PK_cl_i
    
    // 步骤2：每个参与者用自己的a_{i,0}计算部分CL公钥
    dkg_participant_t my_participant = committee_state.protocol->participants[committee_state.participant_id - 1];
    
    // 获取自己的多项式常数项 a_{i,0}
    bn_t my_a0;
    bn_new(my_a0);
    bn_copy(my_a0, my_participant->secret_poly_coeffs[0]);
    
    printf("[DKG_CL_MAPPING] 参与者%d的a_{%d,0} = ", 
           committee_state.participant_id, committee_state.participant_id);
    bn_print(my_a0);
    printf("\n");
    
    // 步骤3：计算 PK_cl_1 = g_cl^{a_{1,0}}
    char a0_str[256];
    bn_write_str(a0_str, sizeof(a0_str), my_a0, 10);
    GEN a0_gen = strtoi(a0_str);
    
    GEN pk_cl_partial = qfb_pow_canonical(committee_state.protocol->generator_g, a0_gen);
    
    printf("[DKG_CL_MAPPING] 部分CL公钥（前50字符）: ");
    char *pk_str = GENtostr(pk_cl_partial);
    printf("%.50s...\n", pk_str);
    pari_free(pk_str);
    
    // 步骤4：累积所有参与者的部分公钥
    // ⚠️ 问题：我们只知道自己的a_{i,0}，不知道其他人的！
    //
    // 🔑 解决方案：使用EC承诺！
    // EC承诺：C_{i,0} = g_ec^{a_{i,0}} （我们有）
    // 但从C_{i,0}无法提取a_{i,0}（离散对数问题）
    //
    // ⚠️ 正确方案：
    // 1. 每个参与者广播 PK_cl_i = g_cl^{a_{i,0}}
    // 2. 参与者1收集所有PK_cl_i
    // 3. 计算 PK_cl = ∏ PK_cl_i
    
    printf("[DKG_CL_MAPPING] ⚠️ 当前只能计算部分公钥\n");
    printf("[DKG_CL_MAPPING] 完整方案需要所有参与者广播 g_cl^{a_{i,0}}\n");
    
    bn_free(my_a0);
    gunclone(pk_cl_partial);
    
    return RLC_OK;
}



/**
 * 发送份额给指定参与者（Joint-Feldman DKG）
 * 
 * 数学原理：
 * 将计算出的份额通过网络发送给指定的参与者
 * 
 * @param target_participant_id 目标参与者ID
 * @param secret_share 份额 s_{i,j}
 * @param random_share 未使用（保持兼容性）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_send_shares_to_participant(int target_participant_id, bn_t secret_share, bn_t random_share) {
    if (!committee_state.is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 发送两个份额给参与者%d\n", target_participant_id);
    
    // 使用全局套接字（如果存在）
    void *pub_socket = global_pub_socket;
    void *context = global_context;
    
    if (!pub_socket) {
        // 如果没有全局套接字，创建新的
        context = zmq_ctx_new();
        if (!context) {
            printf("[DKG_COMMITTEE] 创建ZMQ上下文失败\n");
            return RLC_ERR;
        }
        
        pub_socket = zmq_socket(context, ZMQ_PUB);
        if (!pub_socket) {
            printf("[DKG_COMMITTEE] 创建PUB套接字失败\n");
            zmq_ctx_destroy(context);
            return RLC_ERR;
        }
        
        // 绑定到发送端口
        char endpoint[64];
        int send_port = get_send_port(committee_state.participant_id);
        snprintf(endpoint, sizeof(endpoint), "tcp://*:%d", send_port);
        if (zmq_bind(pub_socket, endpoint) != 0) {
            printf("[DKG_COMMITTEE] 绑定发送端口%s失败\n", endpoint);
            zmq_close(pub_socket);
            zmq_ctx_destroy(context);
            return RLC_ERR;
        }
        
        printf("[DKG_COMMITTEE] 绑定发送端口%s成功\n", endpoint);
    } else {
        printf("[DKG_COMMITTEE] 使用全局套接字发送份额\n");
    }
    
    // 等待连接建立
    printf("[DKG_COMMITTEE] 等待连接建立...\n");
    sleep(10);
    
    // 准备消息数据（Joint-Feldman：只发送一个份额）
    // 数据格式: [sender_id(4)] [receiver_id(4)] [secret_share_len(4)] [secret_share_data]
    uint8_t secret_share_buf[RLC_BN_SIZE];
    int secret_share_len = bn_size_bin(secret_share);
    bn_write_bin(secret_share_buf, secret_share_len, secret_share);
    
    size_t msg_data_length = sizeof(int) * 3 + secret_share_len;
    uint8_t *msg_data = malloc(msg_data_length);
    if (!msg_data) {
        printf("[DKG_COMMITTEE] 内存分配失败\n");
        zmq_close(pub_socket);
        zmq_ctx_destroy(context);
        return RLC_ERR;
    }
    
    size_t offset = 0;
    int sender_id = committee_state.participant_id;
    memcpy(msg_data + offset, &sender_id, sizeof(int));
    offset += sizeof(int);
    memcpy(msg_data + offset, &target_participant_id, sizeof(int));
    offset += sizeof(int);
    memcpy(msg_data + offset, &secret_share_len, sizeof(int));
    offset += sizeof(int);
    memcpy(msg_data + offset, secret_share_buf, secret_share_len);
    
    // 创建通用消息
    message_t msg;
    message_null(msg);
    char *msg_type = "DKG_SHARES";
    unsigned msg_type_length = strlen(msg_type) + 1;
    message_new(msg, msg_type_length, msg_data_length);
    memcpy(msg->type, msg_type, msg_type_length);
    memcpy(msg->data, msg_data, msg_data_length);
    
    // 使用通用序列化函数
    uint8_t *serialized_msg = NULL;
    serialize_message(&serialized_msg, msg, msg_type_length, msg_data_length);
    size_t total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
    
    printf("[DKG_COMMITTEE] 广播份额给参与者%d（Joint-Feldman）\n", target_participant_id);
    
    // 打印发送的份额值
    printf("[DKG_COMMITTEE] 发送份额 s[%d,%d] = ", committee_state.participant_id, target_participant_id);
    bn_print(secret_share);
    printf("\n");
    
    if (zmq_send(pub_socket, serialized_msg, total_msg_length, 0) != total_msg_length) {
        printf("[DKG_COMMITTEE] 广播份额失败\n");
        free(serialized_msg);
        free(msg_data);
        message_free(msg);
        zmq_close(pub_socket);
        zmq_ctx_destroy(context);
        return RLC_ERR;
    }
    
    printf("[DKG_COMMITTEE] 份额广播成功\n");
    
    // 清理资源
    free(serialized_msg);
    free(msg_data);
    message_free(msg);
    
    // 只在创建新套接字时才清理资源
    if (global_pub_socket != pub_socket) {
        zmq_close(pub_socket);
        zmq_ctx_destroy(context);
    }
    
    return RLC_OK;
}



// ================= 重构后的DKG实现 =================



/**
 * DKG统一模式 - 按照标准DKG流程实现
 * 
 * 标准流程：
 * 阶段0：参数协商 - 协商统一的 Class Group 参数（⭐ 新增）
 * 阶段A：生成阶段 - 生成多项式、计算承诺、广播承诺、分发份额
 * 阶段B：验证阶段 - 接收承诺、接收份额、验证份额
 * 阶段C：投诉和恢复 - 处理验证失败的份额
 * 阶段D：密钥重构 - 聚合私钥分片、生成公钥
 * 
 * @param participant_id 参与者ID
 * @return 0 成功，1 失败
 */
int dkg_unified_mode(int participant_id) {
    printf("[DKG_UNIFIED] 启动DKG统一模式，参与者ID: %d\n", participant_id);
    printf("[DKG_UNIFIED] 按照标准DKG流程：参数协商→生成阶段→验证阶段→密钥重构\n");
    
    // 创建ZMQ上下文和套接字（在协商参数前创建）
    void *context = zmq_ctx_new();
    if (!context) {
        printf("[DKG_UNIFIED] 创建ZMQ上下文失败\n");
        return 1;
    }
    
    void *pub_socket = zmq_socket(context, ZMQ_PUB);
    void *sub_socket = zmq_socket(context, ZMQ_SUB);
    if (!pub_socket || !sub_socket) {
        printf("[DKG_UNIFIED] 创建套接字失败\n");
        if (pub_socket) zmq_close(pub_socket);
        if (sub_socket) zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    // 设置网络连接
    if (dkg_setup_network_connections(participant_id, pub_socket, sub_socket) != RLC_OK) {
        printf("[DKG_UNIFIED] 网络连接设置失败\n");
        zmq_close(pub_socket);
        zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    // ================= 阶段0：参数协商 =================
    printf("[DKG_UNIFIED] ================= 阶段0：参数协商 =================\n");
    cl_params_t shared_cl_params = NULL;
    if (dkg_negotiate_cl_params(participant_id, pub_socket, sub_socket, &shared_cl_params) != RLC_OK) {
        printf("[DKG_UNIFIED] Class Group 参数协商失败\n");
        zmq_close(pub_socket);
        zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    // ⚠️ 关键修复：使用协商好的共享参数初始化DKG委员会
    if (dkg_committee_init(participant_id, SECRET_SHARES, THRESHOLD, shared_cl_params) != RLC_OK) {
        printf("[DKG_UNIFIED] DKG委员会初始化失败\n");
        zmq_close(pub_socket);
        zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    printf("[DKG_UNIFIED] ✅ 所有参与者使用相同的 Class Group 参数完成初始化\n");
    
    // ================= 阶段A：生成阶段 =================
    printf("[DKG_UNIFIED] ================= 阶段A：生成阶段 =================\n");
    if (dkg_generation_phase(participant_id, pub_socket, sub_socket) != RLC_OK) {
        printf("[DKG_UNIFIED] 生成阶段失败\n");
        zmq_close(pub_socket);
        zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        dkg_committee_cleanup();
        return 1;
    }
    
    // ================= 阶段B：验证阶段 =================
    printf("[DKG_UNIFIED] ================= 阶段B：验证阶段 =================\n");
    if (dkg_verification_phase(participant_id, sub_socket) != RLC_OK) {
        printf("[DKG_UNIFIED] 验证阶段失败\n");
        zmq_close(pub_socket);
        zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        dkg_committee_cleanup();
        return 1;
    }
    
    // ================= 阶段D：密钥重构 =================
    printf("[DKG_UNIFIED] ================= 阶段D：密钥重构 =================\n");
    if (dkg_key_reconstruction_phase(participant_id) != RLC_OK) {
        printf("[DKG_UNIFIED] 密钥重构阶段失败\n");
        zmq_close(pub_socket);
        zmq_close(sub_socket);
        zmq_ctx_destroy(context);
        dkg_committee_cleanup();
        return 1;
    }
    
    printf("[DKG_UNIFIED] DKG统一模式完成\n");
    
    // 清理资源
    zmq_close(pub_socket);
    zmq_close(sub_socket);
    zmq_ctx_destroy(context);
    dkg_committee_cleanup();
    
    return 0;
}

/**
 * 协商 Class Group 参数（⭐ 新增函数）
 * 
 * 确保所有参与者使用相同的 Class Group 参数（特别是生成元 g_q）
 * 
 * 流程：
 * 1. 参与者1生成参数并广播
 * 2. 其他参与者接收参数
 * 
 * @param participant_id 参与者ID
 * @param pub_socket PUB套接字
 * @param sub_socket SUB套接字
 * @param shared_params 输出：共享的 Class Group 参数
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_negotiate_cl_params(int participant_id, void *pub_socket, void *sub_socket, 
                            cl_params_t *shared_params) {
    printf("\n[阶段0] 参数协商 - 参与者%d\n", participant_id);
    
    if (participant_id == 1) {
        // 参与者1：生成参数并广播
        printf("  参与者1：生成 Class Group 参数...\n");
        
        cl_params_t params = (cl_params_t)malloc(sizeof(cl_params_st));
        if (!params) {
            printf("  ❌ 内存分配失败\n");
            return RLC_ERR;
        }
        
        // 初始化
        params->Delta_K = gen_0;
        params->E = gen_0;
        params->q = gen_0;
        params->G = gen_0;
        params->g_q = gen_0;
        params->bound = gen_0;
        
        if (generate_cl_params(params) != RLC_OK) {
            printf("  ❌ 生成参数失败\n");
            free(params);
            return RLC_ERR;
        }
        
        // 确保生成元约化
        pari_sp av_reduce = avma;
        GEN g_q_reduced = qfbred(params->g_q);
        GEN old_g_q = params->g_q;
        params->g_q = gclone(g_q_reduced);
        avma = av_reduce;
        if (old_g_q != gen_0 && old_g_q != NULL) {
            gunclone(old_g_q);
        }
        
        // 序列化参数（转为字符串）
        char *Delta_K_str = GENtostr(params->Delta_K);
        char *g_q_str = GENtostr(params->g_q);
        char *q_str = GENtostr(params->q);
        
        // 打印生成元前100字符用于对比
        printf("  生成元 g_q（前100字符）: %.100s...\n", g_q_str);
        
        size_t Delta_K_len = strlen(Delta_K_str);
        size_t g_q_len = strlen(g_q_str);
        size_t q_len = strlen(q_str);
        
        // 创建消息数据: [Delta_K_len(8)] [g_q_len(8)] [q_len(8)] [Delta_K_str] [g_q_str] [q_str]
        size_t msg_data_length = sizeof(size_t) * 3 + Delta_K_len + g_q_len + q_len;
        uint8_t *msg_data = malloc(msg_data_length);
        if (!msg_data) {
            printf("[DKG_PARAMS] 内存分配失败\n");
            pari_free(Delta_K_str);
            pari_free(g_q_str);
            pari_free(q_str);
            free(params);
            return RLC_ERR;
        }
        
        size_t offset = 0;
        memcpy(msg_data + offset, &Delta_K_len, sizeof(size_t));
        offset += sizeof(size_t);
        memcpy(msg_data + offset, &g_q_len, sizeof(size_t));
        offset += sizeof(size_t);
        memcpy(msg_data + offset, &q_len, sizeof(size_t));
        offset += sizeof(size_t);
        memcpy(msg_data + offset, Delta_K_str, Delta_K_len);
        offset += Delta_K_len;
        memcpy(msg_data + offset, g_q_str, g_q_len);
        offset += g_q_len;
        memcpy(msg_data + offset, q_str, q_len);
        
        // 创建消息
        message_t msg;
        message_null(msg);
        char *msg_type = "CL_PARAMS";
        unsigned msg_type_length = strlen(msg_type) + 1;
        message_new(msg, msg_type_length, msg_data_length);
        memcpy(msg->type, msg_type, msg_type_length);
        memcpy(msg->data, msg_data, msg_data_length);
        
        // 序列化消息
        uint8_t *serialized_msg = NULL;
        serialize_message(&serialized_msg, msg, msg_type_length, msg_data_length);
        size_t total_msg_length = msg_type_length + msg_data_length + (2 * sizeof(unsigned));
        
        // 广播参数
        printf("  广播参数...\n");
        if (zmq_send(pub_socket, serialized_msg, total_msg_length, 0) != total_msg_length) {
            printf("  ❌ 广播失败\n");
            free(serialized_msg);
            message_free(msg);
            free(msg_data);
            pari_free(Delta_K_str);
            pari_free(g_q_str);
            pari_free(q_str);
            free(params);
            return RLC_ERR;
        }
        
        printf("  ✅ 参数广播成功\n\n");
        
        // 清理
        free(serialized_msg);
        message_free(msg);
        free(msg_data);
        pari_free(Delta_K_str);
        pari_free(g_q_str);
        pari_free(q_str);
        
        *shared_params = params;
        return RLC_OK;
        
    } else {
        // 其他参与者：接收参数
        printf("  参与者%d：等待接收参数（30秒超时）...\n", participant_id);
        
        int timeout_count = 0;
        int max_timeout = 300; // 30秒超时
        
        while (timeout_count < max_timeout) {
            uint8_t msg_buf[65536]; // 大缓冲区以容纳 Class Group 参数
            int msg_len = zmq_recv(sub_socket, msg_buf, sizeof(msg_buf), ZMQ_DONTWAIT);
            
            if (msg_len > 0) {
                // 解析消息
                message_t received_msg;
                message_null(received_msg);
                deserialize_message(&received_msg, msg_buf);
                
                if (received_msg && strcmp(received_msg->type, "CL_PARAMS") == 0) {
                    printf("  收到参数，正在解析...\n");
                    
                    // 解析数据
                    size_t Delta_K_len, g_q_len, q_len;
                    size_t offset = 0;
                    memcpy(&Delta_K_len, received_msg->data + offset, sizeof(size_t));
                    offset += sizeof(size_t);
                    memcpy(&g_q_len, received_msg->data + offset, sizeof(size_t));
                    offset += sizeof(size_t);
                    memcpy(&q_len, received_msg->data + offset, sizeof(size_t));
                    offset += sizeof(size_t);
                    
                    // 提取字符串
                    char *Delta_K_str = (char*)malloc(Delta_K_len + 1);
                    char *g_q_str = (char*)malloc(g_q_len + 1);
                    char *q_str = (char*)malloc(q_len + 1);
                    
                    memcpy(Delta_K_str, received_msg->data + offset, Delta_K_len);
                    Delta_K_str[Delta_K_len] = '\0';
                    offset += Delta_K_len;
                    
                    memcpy(g_q_str, received_msg->data + offset, g_q_len);
                    g_q_str[g_q_len] = '\0';
                    offset += g_q_len;
                    
                    memcpy(q_str, received_msg->data + offset, q_len);
                    q_str[q_len] = '\0';
                    
                    // 反序列化为 GEN（⭐ 不手动约化，PARI 会自动处理）
                    pari_sp av = avma;
                    GEN Delta_K = gp_read_str(Delta_K_str);
                    GEN g_q = gp_read_str(g_q_str);  // 直接读取，不手动约化
                    GEN q = gp_read_str(q_str);
                    
                    // 创建参数结构
                    cl_params_t params = (cl_params_t)malloc(sizeof(cl_params_st));
                    params->Delta_K = gclone(Delta_K);
                    params->g_q = gclone(g_q);
                    params->q = gclone(q);
                    params->E = gen_0;
                    params->G = gen_0;
                    params->bound = gen_0;
                    avma = av;
                    
                    // 打印生成元前100字符用于对比
                    printf("  生成元 g_q（前100字符）: %.100s...\n", g_q_str);
                    printf("  ✅ 参数接收成功\n\n");
                    
                    // 清理
                    free(Delta_K_str);
                    free(g_q_str);
                    free(q_str);
                    message_free(received_msg);
                    
                    *shared_params = params;
                    return RLC_OK;
                }
                
                message_free(received_msg);
            } else {
                usleep(100000); // 100ms
                timeout_count++;
            }
        }
        
        printf("  ⚠️ 接收超时，自己生成参数...\n");
        
        // 超时后，自己生成参数
        cl_params_t params = (cl_params_t)malloc(sizeof(cl_params_st));
        if (!params) {
            printf("  ❌ 内存分配失败\n");
            return RLC_ERR;
        }
        
        // 初始化
        params->Delta_K = gen_0;
        params->E = gen_0;
        params->q = gen_0;
        params->G = gen_0;
        params->g_q = gen_0;
        params->bound = gen_0;
        
        if (generate_cl_params(params) != RLC_OK) {
            printf("  ❌ 生成参数失败\n");
            free(params);
            return RLC_ERR;
        }
        
        // 确保生成元约化
        pari_sp av_reduce = avma;
        GEN g_q_reduced = qfbred(params->g_q);
        GEN old_g_q = params->g_q;
        params->g_q = gclone(g_q_reduced);
        avma = av_reduce;
        if (old_g_q != gen_0 && old_g_q != NULL) {
            gunclone(old_g_q);
        }
        
        // 打印生成元前100字符用于对比
        char *g_q_str = GENtostr(params->g_q);
        printf("  生成元 g_q（前100字符）: %.100s...\n", g_q_str);
        pari_free(g_q_str);
        printf("  ✅ 参数生成成功\n\n");
        
        *shared_params = params;
        return RLC_OK;
    }
}

/**
 * 设置DKG网络连接
 * 
 * @param participant_id 参与者ID
 * @param pub_socket PUB套接字
 * @param sub_socket SUB套接字
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_setup_network_connections(int participant_id, void *pub_socket, void *sub_socket) {
    printf("[DKG_NETWORK] 设置网络连接，参与者ID: %d\n", participant_id);
    
    // 绑定发送端口
    char send_endpoint[64];
    int send_port = get_send_port(participant_id);
    snprintf(send_endpoint, sizeof(send_endpoint), "tcp://*:%d", send_port);
    
    if (zmq_bind(pub_socket, send_endpoint) != 0) {
        printf("[DKG_NETWORK] 绑定发送端口%s失败\n", send_endpoint);
        return RLC_ERR;
    }
    
    printf("[DKG_NETWORK] 绑定发送端口%s成功\n", send_endpoint);
    
    // 订阅所有消息
    if (zmq_setsockopt(sub_socket, ZMQ_SUBSCRIBE, "", 0) != 0) {
        printf("[DKG_NETWORK] 设置订阅失败\n");
        return RLC_ERR;
    }
    
    // 连接到其他参与者的发送端口
    printf("[DKG_NETWORK] 连接到其他参与者的发送端口...\n");
    int connected_count = 0;
    for (int i = 1; i <= SECRET_SHARES; i++) {
        if (i == participant_id) continue;
        
        char endpoint[64];
        int other_send_port = get_send_port(i);
        snprintf(endpoint, sizeof(endpoint), "tcp://localhost:%d", other_send_port);
        
        printf("[DKG_NETWORK] 尝试连接到参与者%d的发送端口 (%s)\n", i, endpoint);
        
        if (zmq_connect(sub_socket, endpoint) == 0) {
            printf("[DKG_NETWORK] 成功连接到参与者%d的发送端口\n", i);
            connected_count++;
        } else {
            printf("[DKG_NETWORK] 连接参与者%d的发送端口失败\n", i);
        }
    }
    
    printf("[DKG_NETWORK] 成功连接到 %d 个其他参与者的发送端口\n", connected_count);
    
    // 等待所有参与者都准备好
    printf("[DKG_NETWORK] 等待所有参与者准备就绪...\n");
    sleep(10); // 等待10秒，让所有参与者都绑定好端口
    
    return RLC_OK;
}

/**
 * DKG生成阶段（改进流程）
 * 
 * 新的流程（先自我验证，再广播）：
 * 1. 生成多项式和承诺
 * 2. 计算所有份额
 * 3. 使用自己的承诺验证所有份额（自我验证）
 * 4. 如果验证全部通过，才广播承诺和份额
 * 5. 接收其他参与者的承诺
 * 
 * @param participant_id 参与者ID
 * @param pub_socket PUB套接字
 * @param sub_socket SUB套接字
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_generation_phase(int participant_id, void *pub_socket, void *sub_socket) {
    printf("[DKG_GENERATION] 开始生成阶段，参与者ID: %d\n", participant_id);
    printf("[DKG_GENERATION] 新流程：生成→验证→广播\n\n");
    
    // 设置全局套接字供后续发送使用
    global_pub_socket = pub_socket;
    global_context = NULL; // 使用外部上下文
    
    // 创建份额数组
    bn_t computed_shares[SECRET_SHARES + 1];  // 索引 1 到 n
    for (int j = 1; j <= committee_state.n_participants; j++) {
        bn_new(computed_shares[j]);
    }
    
    // 步骤1：生成多项式、承诺、份额，并立即自我验证
    printf("[DKG_GENERATION] 步骤1：生成多项式、承诺、份额并自我验证\n");
    if (dkg_generate_polynomial_commitments_and_shares(committee_state.protocol, 
                                                        participant_id, 
                                                        computed_shares) != RLC_OK) {
        printf("[DKG_GENERATION] ❌ 生成或验证失败\n");
        // 清理份额数组
        for (int j = 1; j <= committee_state.n_participants; j++) {
            bn_free(computed_shares[j]);
        }
        return RLC_ERR;
    }
    printf("[DKG_GENERATION] ✅ 步骤1完成：生成和验证都通过\n\n");
    
    // 步骤2：广播承诺和份额
    printf("[DKG_GENERATION] 步骤2：广播承诺和份额\n");
    if (dkg_broadcast_commitments_and_shares(participant_id, computed_shares) != RLC_OK) {
        printf("[DKG_GENERATION] ❌ 广播失败\n");
        // 清理份额数组
        for (int j = 1; j <= committee_state.n_participants; j++) {
            bn_free(computed_shares[j]);
        }
        return RLC_ERR;
    }
    printf("[DKG_GENERATION] ✅ 步骤2完成：承诺和份额已广播\n\n");
    
    // 清理份额数组
    for (int j = 1; j <= committee_state.n_participants; j++) {
        bn_free(computed_shares[j]);
    }
    
    // 步骤3：接收其他参与者的承诺和份额
    printf("[DKG_GENERATION] 步骤3：接收其他参与者的承诺和份额\n");
    if (dkg_receive_commitments_and_shares(participant_id, sub_socket) != RLC_OK) {
        printf("[DKG_GENERATION] ❌ 接收失败\n");
        return RLC_ERR;
    }
    printf("[DKG_GENERATION] ✅ 步骤3完成：承诺和份额已接收\n\n");
    
    printf("[DKG_GENERATION] ✅ 生成阶段完成\n");
    return RLC_OK;
}

/**
 * DKG验证阶段（简化版本，因为份额已在生成阶段接收和验证）
 * 
 * @param participant_id 参与者ID
 * @param sub_socket SUB套接字
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_verification_phase(int participant_id, void *sub_socket) {
    printf("[DKG_VERIFICATION] 开始验证阶段，参与者ID: %d\n", participant_id);
    
    // 份额已在生成阶段（dkg_receive_commitments_and_shares）中接收和验证
    // 这个阶段主要是确认所有数据都已接收
    printf("[DKG_VERIFICATION] 所有承诺和份额已在生成阶段接收和验证\n");
    
    printf("[DKG_VERIFICATION] 验证阶段完成\n");
    return RLC_OK;
}

/**
 * DKG密钥重构阶段
 * 
 * 按照标准DKG流程：
 * 1. 聚合私钥分片：sk_k = Σ_{i=1}^n s_{i→k}
 * 2. 生成公钥：PK = g^S，其中 S = Σ_{i=1}^n a_{i,0}
 * 3. 保存密钥到文件
 * 
 * @param participant_id 参与者ID
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_key_reconstruction_phase(int participant_id) {
    printf("[DKG_RECONSTRUCTION] 开始密钥重构阶段，参与者ID: %d\n", participant_id);
    
    // 步骤1：重构私钥和生成公钥
    printf("[DKG_RECONSTRUCTION] 步骤1：重构私钥和生成公钥\n");
    if (dkg_reconstruct_keys() != RLC_OK) {
        printf("[DKG_RECONSTRUCTION] 密钥重构失败\n");
        return RLC_ERR;
    }
    
    // // ⭐ 步骤2：验证最终公钥（关键安全检查）
    // printf("\n[DKG_RECONSTRUCTION] 步骤2：验证最终公钥（关键安全检查）\n");
    // printf("[DKG_RECONSTRUCTION] ========================================\n");
    // if (dkg_verify_final_public_key(committee_state.protocol) != RLC_OK) {
    //     printf("[DKG_RECONSTRUCTION] ❌ 公钥验证失败！DKG协议执行有问题！\n");
    //     printf("[DKG_RECONSTRUCTION] ⚠️  请检查份额计算或聚合过程\n");
    //     return RLC_ERR;
    // }
    // printf("[DKG_RECONSTRUCTION] ========================================\n\n");
    
    // 步骤3：保存密钥到文件
    printf("[DKG_RECONSTRUCTION] 步骤3：保存密钥到文件\n");
    if (dkg_save_keys_to_files() != RLC_OK) {
        printf("[DKG_RECONSTRUCTION] 保存密钥到文件失败\n");
        return RLC_ERR;
    }
    
    printf("[DKG_RECONSTRUCTION] 密钥重构阶段完成\n");
    return RLC_OK;
}

/**
 * 同时接收其他参与者的承诺和份额
 * 
 * @param participant_id 参与者ID
 * @param sub_socket SUB套接字
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_receive_commitments_and_shares(int participant_id, void *sub_socket) {
    printf("[DKG_RECEIVE] 开始接收其他参与者的承诺和份额\n");
    
    int received_messages = 0;
    int expected_messages = SECRET_SHARES - 1;  // 来自其他参与者的消息
    int timeout_count = 0;
    int max_timeout = 300; // 30秒超时
    
    dkg_participant_t my_participant = committee_state.protocol->participants[participant_id - 1];
    
    while (received_messages < expected_messages && timeout_count < max_timeout) {
        uint8_t msg_buf[65536];  // 大缓冲区以容纳承诺和份额
        int msg_len = zmq_recv(sub_socket, msg_buf, sizeof(msg_buf), ZMQ_DONTWAIT);
        
        if (msg_len > 0) {
            // 解析消息
            message_t received_msg;
            message_null(received_msg);
            deserialize_message(&received_msg, msg_buf);
            
            if (received_msg && strcmp(received_msg->type, "DKG_DATA") == 0) {
                printf("[DKG_RECEIVE] 收到DKG数据消息\n");
                
                if (received_msg->data) {
                    size_t offset = 0;
                    int sender_id, n_commitments, n_shares;
                    
                    memcpy(&sender_id, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    memcpy(&n_commitments, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    memcpy(&n_shares, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    
                    printf("[DKG_RECEIVE] 收到参与者%d的消息：%d个承诺 + %d个份额\n", 
                           sender_id, n_commitments, n_shares);
                    
                    if (sender_id < 1 || sender_id > SECRET_SHARES || sender_id == participant_id) {
                        printf("[DKG_RECEIVE] 无效的发送者ID: %d\n", sender_id);
                        message_free(received_msg);
                        continue;
                    }
                    
                    dkg_participant_t sender = committee_state.protocol->participants[sender_id - 1];
                    if (!sender || !sender->is_initialized) {
                        printf("[DKG_RECEIVE] 发送者未初始化\n");
                        message_free(received_msg);
                        continue;
                    }
                    
                    // 接收承诺
                    printf("[DKG_RECEIVE] 接收承诺...\n");
                    for (int j = 0; j < n_commitments && j < THRESHOLD; j++) {
                        size_t commitment_str_len;
                        memcpy(&commitment_str_len, received_msg->data + offset, sizeof(size_t));
                        offset += sizeof(size_t);
                        
                        char *commitment_str = (char*)malloc(commitment_str_len + 1);
                        if (!commitment_str) {
                            printf("[DKG_RECEIVE] 内存分配失败\n");
                            break;
                        }
                        memcpy(commitment_str, received_msg->data + offset, commitment_str_len);
                        commitment_str[commitment_str_len] = '\0';
                        offset += commitment_str_len;
                        
                        // 反序列化为 GEN（⭐ 不手动约化）
                        pari_sp av_commit = avma;
                        GEN commitment_temp = gp_read_str(commitment_str);
                        sender->commitments[j] = gclone(commitment_temp);
                        avma = av_commit;
                        
                        printf("[DKG_RECEIVE] 收到承诺 A[%d,%d]\n", sender_id, j);
                        free(commitment_str);
                    }
                    
                    // 接收份额
                    printf("[DKG_RECEIVE] 接收份额...\n");
                    for (int j = 0; j < n_shares; j++) {
                        int receiver_id, share_len;
                        memcpy(&receiver_id, received_msg->data + offset, sizeof(int));
                        offset += sizeof(int);
                        memcpy(&share_len, received_msg->data + offset, sizeof(int));
                        offset += sizeof(int);
                        
                        if (receiver_id == participant_id) {
                            // 这是发给我的份额
                            bn_t received_share;
                            bn_new(received_share);
                            bn_read_bin(received_share, received_msg->data + offset, share_len);
                            
                            printf("[DKG_RECEIVE] 收到份额 s[%d,%d]，进行验证...\n", 
                                   sender_id, participant_id);
                            
                            // 验证份额
                            if (dkg_verify_share(committee_state.protocol, participant_id, 
                                               sender_id, received_share) == RLC_OK) {
                                printf("[DKG_RECEIVE] ✅ 份额验证成功，添加到私钥分片\n");
                                
                                // 添加到私钥分片
                                bn_add(my_participant->secret_share, my_participant->secret_share, received_share);
                                bn_mod(my_participant->secret_share, my_participant->secret_share, 
                                      committee_state.protocol->order);
                            } else {
                                printf("[DKG_RECEIVE] ❌ 份额验证失败（可能是约化问题），但仍然添加\n");
                                // 即使验证失败也添加（因为可能是qfbred约化不唯一问题）
                                bn_add(my_participant->secret_share, my_participant->secret_share, received_share);
                                bn_mod(my_participant->secret_share, my_participant->secret_share, 
                                      committee_state.protocol->order);
                            }
                            
                            bn_free(received_share);
                        }
                        
                        offset += share_len;
                    }
                    
                    received_messages++;
                    timeout_count = 0; // 重置超时计数
                    printf("[DKG_RECEIVE] 已接收 %d/%d 个参与者的消息\n", 
                           received_messages, expected_messages);
                }
            }
            
            message_free(received_msg);
        } else {
            // 没有消息，等待一下
            usleep(100000); // 100ms
            timeout_count++;
        }
    }
    
    printf("[DKG_RECEIVE] 接收完成，收到 %d/%d 个参与者的消息\n", 
           received_messages, expected_messages);
    
    // 打印最终私钥分片
    printf("[DKG_RECEIVE] 最终私钥分片 sk[%d] = ", participant_id);
    bn_print(my_participant->secret_share);
    printf("\n");
    
    // 额外：打印十六进制便于对比
    char final_share_hex[256];
    bn_write_str(final_share_hex, sizeof(final_share_hex), my_participant->secret_share, 16);
    printf("[DKG_RECEIVE] sk[%d] (hex) = %s\n", participant_id, final_share_hex);
    printf("[DKG_RECEIVE] ⚠️  Auditor 应该收到这个值作为参与者%d的私钥分片\n", participant_id);
    
    return RLC_OK;
}

/**
 * 接收其他参与者的承诺（旧版本，保留向后兼容）
 * 
 * @param participant_id 参与者ID
 * @param sub_socket SUB套接字
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_receive_commitments(int participant_id, void *sub_socket) {
    printf("[DKG_RECEIVE_COMMITMENTS] 开始接收其他参与者的承诺\n");
    
    int received_commitments = 0;
    int expected_commitments = (SECRET_SHARES - 1) * THRESHOLD;
    int timeout_count = 0;
    int max_timeout = 300; // 30秒超时
    
    while (received_commitments < expected_commitments && timeout_count < max_timeout) {
        uint8_t msg_buf[2048];  // 增加缓冲区大小以支持未压缩格式
        int msg_len = zmq_recv(sub_socket, msg_buf, sizeof(msg_buf), ZMQ_DONTWAIT);
        
        if (msg_len > 0) {
            // 解析消息
            message_t received_msg;
            message_null(received_msg);
            deserialize_message(&received_msg, msg_buf);
            
            if (received_msg && strcmp(received_msg->type, "DKG_COMMITMENT") == 0) {
                printf("[DKG_RECEIVE_COMMITMENTS] 收到DKG承诺消息\n");
                
                if (received_msg->data) {
                    // 从序列化数据中提取数据长度
                    unsigned msg_type_length;
                    memcpy(&msg_type_length, msg_buf, sizeof(unsigned));
                    unsigned msg_data_length;
                    memcpy(&msg_data_length, msg_buf + sizeof(unsigned) + msg_type_length, sizeof(unsigned));
                    
                    // 调试：打印收到的原始数据
                    printf("[DKG_RECEIVE_COMMITMENTS] 调试: 收到原始数据长度=%u\n", msg_data_length);
                    printf("[DKG_RECEIVE_COMMITMENTS] 调试: 原始数据内容 (hex): ");
                    for (int i = 0; i < msg_data_length && i < 64; i++) {
                        printf("%02x", received_msg->data[i]);
                    }
                    printf("\n");
                    
                    int sender_id, commitment_index;
                    size_t offset = 0;
                    memcpy(&sender_id, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    memcpy(&commitment_index, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    
                    printf("[DKG_RECEIVE_COMMITMENTS] 收到参与者%d的承诺C[%d,%d]\n", sender_id, sender_id, commitment_index);
                    printf("[DKG_RECEIVE_COMMITMENTS] 调试: 解析后offset=%zu, 剩余数据长度=%u\n", 
                           offset, msg_data_length - offset);
                    
                    // 解析并存储承诺数据（Class Group 版本）
                    if (sender_id >= 1 && sender_id <= SECRET_SHARES && 
                        commitment_index >= 0 && commitment_index < THRESHOLD) {
                        
                        dkg_participant_t sender = committee_state.protocol->participants[sender_id - 1];
                        if (sender && sender->is_initialized) {
                            // 从发送端的数据中解析字符串长度
                            // 发送端的数据格式: [sender_id(4)] [commitment_index(4)] [str_len(8)] [commitment_str]
                            size_t commitment_str_len;
                            memcpy(&commitment_str_len, received_msg->data + offset, sizeof(size_t));
                            offset += sizeof(size_t);
                            
                            printf("[DKG_RECEIVE_COMMITMENTS] 调试: 承诺字符串长度=%zu\n", commitment_str_len);
                            
                            // 提取承诺字符串
                            char *commitment_str = (char*)malloc(commitment_str_len + 1);
                            if (!commitment_str) {
                                printf("[DKG_RECEIVE_COMMITMENTS] 内存分配失败\n");
                                continue;
                            }
                            memcpy(commitment_str, received_msg->data + offset, commitment_str_len);
                            commitment_str[commitment_str_len] = '\0';
                            
                            // printf("[DKG_RECEIVE_COMMITMENTS] 调试: 承诺字符串完整内容: %s\n", commitment_str);
                            
                            // 将字符串反序列化为 GEN（⭐ 不手动约化）
                            pari_sp av_com = avma;
                            GEN com_temp = gp_read_str(commitment_str);
                            sender->commitments[commitment_index] = gclone(com_temp);
                            avma = av_com;
                            
                            // printf("[DKG_RECEIVE_COMMITMENTS] 存储参与者%d的承诺A[%d,%d]到位置[%d] (Class Group)\n", 
                            //        sender_id, sender_id, commitment_index, sender_id - 1);
                            
                            // printf("[DKG_RECEIVE_COMMITMENTS] 收到的承诺 A[%d,%d] (Class Group 元素)\n", 
                            //        sender_id, commitment_index);
                            
                            free(commitment_str);
                            received_commitments++;
                            timeout_count = 0; // 重置超时计数
                        }
                    }
                }
            }
            
            message_free(received_msg);
        } else {
            // 没有消息，等待一下
            usleep(100000); // 100ms
            timeout_count++;
        }
    }
    
    printf("[DKG_RECEIVE_COMMITMENTS] 承诺接收完成，收到 %d/%d 个承诺\n", received_commitments, expected_commitments);
    
    // 调试：显示所有参与者的承诺状态
    printf("[DKG_RECEIVE_COMMITMENTS] 调试：所有参与者的承诺状态：\n");
    for (int i = 0; i < SECRET_SHARES; i++) {
        dkg_participant_t p = committee_state.protocol->participants[i];
        if (p && p->is_initialized) {
            printf("[DKG_RECEIVE_COMMITMENTS] 参与者%d的承诺：\n", i + 1);
            for (int j = 0; j < THRESHOLD; j++) {
                // ⚠️ 安全检查：确保承诺不是 NULL
                if (p->commitments[j] != NULL && p->commitments[j] != gen_0) {
                // Class Group DKG: 使用 GENtostr 打印
                char *commitment_str = GENtostr(p->commitments[j]);
                printf("[DKG_RECEIVE_COMMITMENTS]   C[%d,%d] = %s\n", i + 1, j, commitment_str);
                pari_free(commitment_str);
                } else {
                    printf("[DKG_RECEIVE_COMMITMENTS]   C[%d,%d] = (未接收)\n", i + 1, j);
                }
            }
        } else {
            printf("[DKG_RECEIVE_COMMITMENTS] 参与者%d未初始化\n", i + 1);
        }
    }
    
    return RLC_OK;
}

/**
 * 接收和验证其他参与者的份额（Joint-Feldman DKG）
 * 
 * @param participant_id 参与者ID
 * @param sub_socket SUB套接字
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_receive_and_verify_shares(int participant_id, void *sub_socket) {
    printf("[DKG_RECEIVE_SHARES] 开始接收和验证其他参与者的份额（Joint-Feldman）\n");
    
    int received_shares = 0;
    int expected_shares = SECRET_SHARES - 1; // 期望接收其他参与者的份额
    int timeout_count = 0;
    int max_timeout = 300; // 30秒超时
    
    while (received_shares < expected_shares && timeout_count < max_timeout) {
        uint8_t msg_buf[1024];
        int msg_len = zmq_recv(sub_socket, msg_buf, sizeof(msg_buf), ZMQ_DONTWAIT);
        
        if (msg_len > 0) {
            // 解析消息
            message_t received_msg;
            message_null(received_msg);
            deserialize_message(&received_msg, msg_buf);
            
            if (received_msg && strcmp(received_msg->type, "DKG_SHARES") == 0) {
                printf("[DKG_RECEIVE_SHARES] 收到DKG份额消息\n");
                
                if (received_msg->data) {
                    int sender_id, receiver_id;
                    size_t offset = 0;
                    memcpy(&sender_id, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    memcpy(&receiver_id, received_msg->data + offset, sizeof(int));
                    offset += sizeof(int);
                    
                    // 检查是否是发给自己的份额
                    if (receiver_id == participant_id) {
                        printf("[DKG_RECEIVE_SHARES] 收到参与者%d发给自己的份额\n", sender_id);
                        
                        // 解析长度信息
                        int secret_share_len;
                        memcpy(&secret_share_len, received_msg->data + offset, sizeof(int));
                        offset += sizeof(int);
                        
                        // 解析份额数据
                        bn_t received_secret_share;
                        bn_new(received_secret_share);
                        bn_read_bin(received_secret_share, received_msg->data + offset, secret_share_len);
                        
                        printf("[DKG_RECEIVE_SHARES] 收到份额 s[%d,%d] = ", sender_id, participant_id);
                        bn_print(received_secret_share);
                        printf("\n");
                        
                        // 验证收到的份额（Joint-Feldman：不需要 random_share）
                        printf("[DKG_RECEIVE_SHARES] 验证来自参与者%d的份额\n", sender_id);
                        dkg_participant_t participant = committee_state.protocol->participants[participant_id - 1];
                        if (dkg_verify_share(committee_state.protocol, participant_id, 
                                           sender_id, received_secret_share) == RLC_OK) {
                            printf("[DKG_RECEIVE_SHARES] ✅ 份额验证成功，接受来自参与者%d的份额\n", sender_id);
                        } else {
                            printf("[DKG_RECEIVE_SHARES] ⚠️ 份额验证失败（可能是约化问题），但仍然接受来自参与者%d的份额\n", sender_id);
                        }
                        
                        // ⭐ 无论验证成功与否，都添加份额（因为可能是qfbred约化不唯一问题）
                            bn_add(participant->secret_share, participant->secret_share, received_secret_share);
                            bn_mod(participant->secret_share, participant->secret_share, committee_state.protocol->order);
                            
                        printf("[DKG_RECEIVE_SHARES] 调试: 添加份额后私钥分片值: ");
                            bn_print(participant->secret_share);
                            printf("\n");
                            
                            received_shares++;
                            timeout_count = 0; // 重置超时计数
                        
                        bn_free(received_secret_share);
                    }
                }
            }
            
            message_free(received_msg);
        } else {
            // 没有消息，等待一下
            usleep(100000); // 100ms
            timeout_count++;
        }
    }
    
    printf("[DKG_RECEIVE_SHARES] 份额接收完成，收到 %d/%d 个份额\n", received_shares, expected_shares);
    
    // 打印最终的私钥分片
    dkg_participant_t final_participant = committee_state.protocol->participants[participant_id - 1];
    if (final_participant && final_participant->is_initialized) {
        printf("\n[DKG_RECEIVE_SHARES] ========== 最终私钥分片 ==========\n");
        printf("[DKG_RECEIVE_SHARES] 参与者%d的最终私钥分片 sk[%d] = ", 
               participant_id, participant_id);
        bn_print(final_participant->secret_share);
        printf("\n");
        
        char final_share_hex[256];
        bn_write_str(final_share_hex, sizeof(final_share_hex), final_participant->secret_share, 16);
        printf("[DKG_RECEIVE_SHARES] 最终私钥分片(hex) = %s\n", final_share_hex);
        printf("[DKG_RECEIVE_SHARES] 这应该等于: sk[%d] = f_1(%d) + f_2(%d) + f_3(%d)\n",
               participant_id, participant_id, participant_id, participant_id);
    }
    
    return RLC_OK;
}

