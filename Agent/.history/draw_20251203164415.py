import matplotlib.pyplot as plt
import seaborn as sns
import matplotlib
import pandas as pd

# ===================== Style Configuration =====================
matplotlib.rcParams['text.usetex'] = True
matplotlib.rcParams['text.latex.preamble'] = r'''
\usepackage{times}
\usepackage{bm}
\usepackage{amsmath}
\renewcommand{\rmdefault}{ptm}  % Times Roman
\renewcommand{\sfdefault}{phv} % Helvetica
\renewcommand{\ttdefault}{pcr} % Courier
'''

sns.set_theme(
    context="paper",
    font_scale=1.6,
    rc={
        'axes.edgecolor': 'black',
        'axes.linewidth': 3,
        'axes.labelweight': 'bold',
        'axes.titlesize': 28,
        'axes.titleweight': 'bold',
        'figure.titlesize': 28,
        'font.family': 'serif',
        'font.serif': ['Times New Roman'],
        'font.weight': 'bold',
        'figure.dpi': 300,
        'savefig.dpi': 600,
        'savefig.format': 'pdf',
        'savefig.bbox': 'tight',
        'savefig.pad_inches': 0.1,
        'figure.autolayout': True,
        'figure.facecolor': 'white',
        'axes.facecolor': 'white',
    }
)

# ===================== Draw Plot =====================
# 读取 CSV（自动处理 header）
df = pd.read_csv("transaction_list_for_each_user.csv")

# 第一列为 user_id
user_ids = df.iloc[:, 0]
# 后面所有列是时间序列
values = df.iloc[:, 1:]

plt.figure(figsize=(10, 6))

for idx, user in enumerate(user_ids):
    y = values.iloc[idx].tolist()
    x = list(range(len(y)))

    plt.plot(x, y, marker='o', linewidth=3, label=str(user))

plt.title(r"\textbf{Transaction Time Series}", fontsize=28)
plt.xlabel(r"\textbf{Time Index}", fontsize=24)
plt.ylabel(r"\textbf{Value}", fontsize=24)

plt.grid(True, linewidth=1.2)
plt.legend(title=r"\textbf{User ID}", fontsize=18)

# 保存 PDF
save_path = "transaction_plot.pdf"
plt.savefig(save_path)

print(f"图像已保存为：{save_path}")

plt.show()
