#ifndef SECRET_SHARE_H
#define SECRET_SHARE_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include "relic.h"
#include "util.h"  // 包含 RLC_EC_SIZE_COMPRESSED 定义
#include "committee_integration.h"  // 添加委员会集成

// 宏定义
#define SECRET_SHARES 3
#define THRESHOLD 2
#define SHARE_SIZE 100000
#define MAX_MESSAGE_SIZE 100000
#define MSG_ID_MAXLEN 128
#define BLOCK_SIZE 30  // 每个块30字节（留2字节余量，确保不超过椭圆曲线阶）

// 固定端口配置 - 只改变地址
static const int RECEIVER_PORTS[SECRET_SHARES] = {
    5555,  // 接收者1
    5556,  // 接收者2
    5557   // 接收者3
};

// 动态端点配置 - 通过委员会集成获取
extern char RECEIVER_ENDPOINTS[SECRET_SHARES][64];

// 分享结构体
// 注意：已改为椭圆曲线阶上的分享（分块处理）
typedef struct {
    int x;  // 分享点的x坐标
    size_t block_index;  // 块索引（从0开始）
    // y值：椭圆曲线阶上的分享值（序列化为字节数组）
    // 格式：[长度(4字节)][大整数字节]
    uint8_t y[SHARE_SIZE];  // 分享点的y坐标（bn_t的序列化形式）
    size_t data_length;  // 原始秘密总长度（字节数）
    size_t block_size;  // 当前块的大小（字节数）
    // receiver专用
    int is_valid;
    char message_type[64];
    time_t received_time;
} secret_share_t;

// receiver专用：分享收集器
typedef struct {
    secret_share_t shares[SECRET_SHARES];
    int share_count;
    int threshold;
    pthread_mutex_t mutex;
    char expected_msg_type[64];
    int is_complete;
} share_collector_t;

// GF(256)运算
uint8_t gf256_add(uint8_t a, uint8_t b);
uint8_t gf256_mul(uint8_t a, uint8_t b);

// 拉格朗日插值（GF(256)版本 - 保留用于兼容性）
uint8_t lagrange_coefficient(int xi, int* x_coords, int k);

// 拉格朗日插值（椭圆曲线阶版本）
int lagrange_coefficient_ec(bn_t result, int xi, int* x_coords, int k, bn_t order);

// 创建秘密分享（分块版本）
// 返回：实际生成的分享数量（num_blocks * SECRET_SHARES）
// 注意：shares 数组必须足够大，至少为 (secret_len / BLOCK_SIZE + 1) * SECRET_SHARES
int create_secret_shares(const uint8_t* secret, size_t secret_len, secret_share_t* shares, size_t* num_shares_out);

// 发送分享（需要外部定义RECEIVER_ENDPOINTS）
// num_shares: 分享总数（num_blocks * SECRET_SHARES）
int send_shares_to_receivers(secret_share_t* shares, size_t num_shares, const char* msg_type, const char** receiver_endpoints);

// 解析分享消息
// int parse_share_message(uint8_t* data, size_t data_size, secret_share_t* share, char* msg_type);

// receiver专用：初始化收集器
void init_share_collector(share_collector_t* collector);
// receiver专用：添加分享
int add_share_to_collector(share_collector_t* collector, secret_share_t* share);
// receiver专用：重构秘密
int reconstruct_secret(share_collector_t* collector, uint8_t* reconstructed_data, size_t* data_length);

// auditor专用：重构秘密（直接用分片数组）
int reconstruct_secret_from_shares(secret_share_t* shares, int share_count, uint8_t* reconstructed_data, size_t* data_length);

// ================= VSS (Verifiable Secret Sharing) 相关 =================

// VSS 承诺结构体 (Feldman VSS)
// 注意：支持分块分享，每个块有独立的承诺
typedef struct {
    // 承诺数组：[块索引][系数索引] = 压缩椭圆曲线点
    // 支持最多 MAX_BLOCKS 个块，每个块有 THRESHOLD 个系数承诺
    #define MAX_BLOCKS 1000  // 最大块数（支持约30KB的秘密）
    uint8_t commitments[MAX_BLOCKS][THRESHOLD][RLC_EC_SIZE_COMPRESSED];  // 每个块的Feldman承诺
    size_t num_blocks;                           // 块数量
    char msgid[MSG_ID_MAXLEN];                   // 消息ID
    size_t secret_len;                           // 原始秘密总长度（字节数）
    time_t timestamp;                            // 时间戳
} vss_commitment_t;

// VSS 相关函数
int create_vss_commitments(const uint8_t* secret, size_t secret_len, 
                          secret_share_t* shares, vss_commitment_t* commitment, const char* msgid);
int verify_share_with_commitment(const secret_share_t* share, 
                                const vss_commitment_t* commitment);
int send_vss_commitment_to_auditor(const vss_commitment_t* commitment);

// 基于文件的 VSS 承诺存储
int save_vss_commitment_to_file(const vss_commitment_t* commitment);
int load_vss_commitment_from_file(const char* msgid, vss_commitment_t* commitment);
int list_vss_commitment_files(void);

// 委员会相关函数
int get_dynamic_endpoints(char endpoints[SECRET_SHARES][64]);
int get_my_committee_position(const char* my_address);

#endif // SECRET_SHARE_H 