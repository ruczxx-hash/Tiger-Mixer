#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// HTTP 响应结构体
struct ResponseData {
    char *data;
    size_t size;
};

// 回调函数，用于接收 HTTP 响应数据
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, struct ResponseData *response) {
    size_t total_size = size * nmemb;
    
    // 重新分配内存
    char *new_data = realloc(response->data, response->size + total_size + 1);
    if (new_data == NULL) {
        printf("[ERROR] 内存分配失败\n");
        return 0;
    }
    
    response->data = new_data;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = 0; // 添加字符串结束符
    
    return total_size;
}

// 调用零知识证明 HTTP 服务
int call_zk_proof_service(const char *alpha_str, const char *g_alpha_x_str, const char *g_alpha_y_str) {
    CURL *curl;
    CURLcode res;
    struct ResponseData response = {0};
    
    printf("[HTTP] 开始调用零知识证明服务...\n");
    printf("[HTTP] alpha = %s\n", alpha_str);
    printf("[HTTP] g_alpha_x = %s\n", g_alpha_x_str);
    printf("[HTTP] g_alpha_y = %s\n", g_alpha_y_str);
    
    // 初始化 curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (!curl) {
        printf("[ERROR] curl 初始化失败\n");
        return -1;
    }
    
    // 构建 JSON 请求体
    char json_request[2048];
    snprintf(json_request, sizeof(json_request),
        "{"
        "\"alpha\":\"%s\","
        "\"g_to_the_alpha_x\":\"%s\","
        "\"g_to_the_alpha_y\":\"%s\","
        "\"proof_type\":\"alpha_knowledge\""
        "}",
        alpha_str, g_alpha_x_str, g_alpha_y_str);
    
    printf("[HTTP] JSON 请求: %s\n", json_request);
    
    // 设置 curl 选项
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8080/generate-proof");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(json_request));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, 
        curl_slist_append(NULL, "Content-Type: application/json"));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30秒超时
    
    // 执行 HTTP 请求
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        printf("[ERROR] HTTP 请求失败: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        if (response.data) free(response.data);
        return -1;
    }
    
    // 获取 HTTP 状态码
    long http_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    printf("[HTTP] 响应状态码: %ld\n", http_code);
    printf("[HTTP] 响应内容: %s\n", response.data);
    
    // 清理资源
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    
    if (response.data) {
        free(response.data);
    }
    
    if (http_code == 200) {
        printf("[HTTP] 零知识证明生成成功\n");
        return 0;
    } else {
        printf("[ERROR] 零知识证明生成失败，HTTP 状态码: %ld\n", http_code);
        return -1;
    }
}
