import pandas as pd
import matplotlib.pyplot as plt

# 读取 CSV 文件
df = pd.read_csv("transaction_list.csv")

# 第一列为 user_id，后面的列为时间序列
user_ids = df.iloc[:, 0]
time_columns = df.columns[1:]   # 所有时间点列

plt.figure(figsize=(10, 6))

for idx, user in enumerate(user_ids):
    # 取该用户的所有时间点数值
    values = df.iloc[idx, 1:].tolist()
    
    # 绘制折线
    plt.plot(range(len(values)), values, marker='o', linewidth=2, label=user)

# 图表信息
plt.title("Transaction Time Series for Each User", fontsize=16)
plt.xlabel("Time Index", fontsize=14)
plt.ylabel("Value", fontsize=14)
plt.grid(True)
plt.legend(title="User ID")

plt.tight_layout()
plt.show()
