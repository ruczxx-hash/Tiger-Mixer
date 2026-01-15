from openai import OpenAI
#total_moneys = [10, 13, 21, 22, 25, 41, 39, 31, 29, 27]
port = 8001
count_num = 15
total_moneys = [10, 10, 10, 10, 10, 10, 10, 10, 10, 10]
#total_moneys = [30, 30, 30, 30, 30, 30, 30, 30, 30, 30]
api_key_tongyi="sk-7394f6c37d9b400db23ce8bb7e97e336"
output_type = 2 # 1是无分析,其它随便哪个数都是有分析
length_of_decision = 10
model = "qwen3-max" # 最强版
#model = "qwen-plus" # 稍微差一点
summarize_num_intercepted = 3 # 触发内部拦截规律总结的数量要求
summarize_num_arrested = 5 # 触发外部逮捕时间规律总结的数量要求

judger_output_with_explain = 0 # 

client = OpenAI(
    # 新加坡和北京地域的API Key不同。获取API Key：https://help.aliyun.com/zh/model-studio/get-api-key
    api_key=api_key_tongyi,
    # 以下是北京地域base_url,如果使用新加坡地域的模型,需要将base_url替换为：https://dashscope-intl.aliyuncs.com/compatible-mode/v1
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
)

# 特征还有向外转的时候,转出地址及其分散
# UTC时间特征（某个时间点狠狠发）,节假日,错峰
# 转出行为呈“月末集中”模式
#
# 80%以上的转出交易集中在每月最后5天内爆发,其余时间保持极低活跃度,可能与资金结算周期或账务掩盖节奏相关。

#转出行为具有“节假日跳变”特征
#在公共假期前1天突然静默,假期首日即触发异常高强度爆发（如单日500+笔）,节后迅速回归低频,形成节假日驱动的跳变模式。
#转出行为呈现“工作日抑制”模式

#高频爆发几乎全部集中在周五晚间至周一凌晨,工作日（周二至周四）转出频率极低或为零,疑似规避监管工作周期。
#转出交易Gas费异常一致
#所有 0.01 ETH 转出交易均使用几乎完全相同的Gas价格（如精确到25.3 Gwei）,远超普通用户波动范围,暗示使用统一脚本或钱包模板批量操作


# 待办：实验框架撰写，改为英文。