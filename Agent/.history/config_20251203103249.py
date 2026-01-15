from openai import OpenAI
#total_moneys = [10, 13, 21, 22, 25, 41, 39, 31, 29, 27]
port = 8001
count_num = 15
#total_moneys = [10, 10, 10, 10, 10, 10, 10, 10, 10, 10]
total_moneys = [500,500]
#total_moneys = [30, 30, 30, 30, 30, 30, 30, 30, 30, 30]

use_plan =True

api_key_tongyi="sk-7394f6c37d9b400db23ce8bb7e97e336"
output_type = 2 # 1是无分析,其它随便哪个数都是有分析
length_of_decision = 10
model = "qwen3-max" # 最强版
#model = "qwen-plus" # 稍微差一点
summarize_num_intercepted = 1 # 触发内部拦截规律总结的数量要求
summarize_num_arrested = 1 # 触发外部逮捕时间规律总结的数量要求

judger_output_with_explain = True # 陪审团是否输出原因

client = OpenAI(
    # 新加坡和北京地域的API Key不同。获取API Key：https://help.aliyun.com/zh/model-studio/get-api-key
    api_key=api_key_tongyi,
    # 以下是北京地域base_url,如果使用新加坡地域的模型,需要将base_url替换为：https://dashscope-intl.aliyuncs.com/compatible-mode/v1
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
)


# 待办：实验框架撰写，改为英文。