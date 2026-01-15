#ifndef HTTP_ZK_CLIENT_H
#define HTTP_ZK_CLIENT_H

// 调用零知识证明 HTTP 服务
// 参数：
//   alpha_str: alpha 的字符串表示
//   g_alpha_x_str: g_to_the_alpha 的 x 坐标字符串
//   g_alpha_y_str: g_to_the_alpha 的 y 坐标字符串
// 返回：
//   0: 成功
//   -1: 失败
int call_zk_proof_service(const char *alpha_str, const char *g_alpha_x_str, const char *g_alpha_y_str);

#endif // HTTP_ZK_CLIENT_H
