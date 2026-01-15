"""系统配置模块"""
import os

# ===================== 业务参数配置 =====================
# 允许的交易金额（ETH）
ALLOWED_AMOUNTS = {0.1, 1, 10, 100}

# ===================== 委员会配置 =====================
# 委员会成员数量
NUM_LEGIT_COMMITTEES = int(os.getenv("NUM_LEGIT_COMMITTEES", "27"))  # 合法委员会成员数量
NUM_ILLEGAL_COMMITTEES = int(os.getenv("NUM_ILLEGAL_COMMITTEES", "3"))  # 非法委员会成员数量
COMMITTEE_SELECT_COUNT = int(os.getenv("COMMITTEE_SELECT_COUNT", "10"))  # 每次选择的委员会成员数量

# 委员会初始权重
COMMITTEE_INITIAL_WEIGHT = int(os.getenv("COMMITTEE_INITIAL_WEIGHT", "1000"))

# ===================== 用户配置 =====================
# 正常用户数量
NUM_RETAIL_USERS = int(os.getenv("NUM_RETAIL_USERS", "19980"))  # 零售用户数量
NUM_EXCHANGE_USERS = int(os.getenv("NUM_EXCHANGE_USERS", "20"))  # 交易所用户数量

# 攻击用户数量
NUM_ATTACK_USERS = int(os.getenv("NUM_ATTACK_USERS", "10"))  # 攻击用户总数
NUM_SMALL_TASK_ATTACKERS = int(os.getenv("NUM_SMALL_TASK_ATTACKERS", "16"))  # 小任务攻击用户（10000 ETH）
NUM_LARGE_TASK_ATTACKERS = int(os.getenv("NUM_LARGE_TASK_ATTACKERS", "4"))  # 大任务攻击用户（60000 ETH）

# 攻击用户任务配置
SMALL_TASK_AMOUNT = int(os.getenv("SMALL_TASK_AMOUNT", "10000"))  # 小任务金额（ETH）
LARGE_TASK_AMOUNT = int(os.getenv("LARGE_TASK_AMOUNT", "60000"))  # 大任务金额（ETH）
TASK_TIME_LEFT = int(os.getenv("TASK_TIME_LEFT", "4320"))  # 任务剩余时间（分钟）

# ===================== 数据文件配置 =====================
# 数据集根目录
DATA_SET_DIR = os.getenv("DATA_SET_DIR", "DataSet")  # 数据集根目录

# 地址数据文件路径（相对于项目根目录）
ETH_ADDRESS_FILE = os.getenv("ETH_ADDRESS_FILE", os.path.join(DATA_SET_DIR, "eth_10000.csv"))  # 地址文件1
ETH_ADDRESS_TRAIN_FILE = os.getenv("ETH_ADDRESS_TRAIN_FILE", os.path.join(DATA_SET_DIR, "eth_20000.csv"))  # 地址文件2（训练集）

# 交易数据目录（相对于项目根目录）
TX_DATA_DIR = os.getenv("TX_DATA_DIR", os.path.join(DATA_SET_DIR, "transaction"))  # 交易数据目录

# ===================== 监控配置 =====================
# Isolation Forest配置
IF_CONTAMINATION = float(os.getenv("IF_CONTAMINATION", "0.1"))  # 污染率
IF_RANDOM_STATE = int(os.getenv("IF_RANDOM_STATE", "42"))  # 随机种子

# 审计间隔（分钟）
AUDIT_INTERVAL = int(os.getenv("AUDIT_INTERVAL", "10"))  # 每10分钟进行一次审计

# 参考样本收集时间（分钟）
REFERENCE_SAMPLE_COLLECTION_TIME = int(os.getenv("REFERENCE_SAMPLE_COLLECTION_TIME", "120"))

# ===================== 特征提取配置 =====================
# 交易历史记录数量（用于LLM分析）
RECENT_TRANSACTION_COUNT = int(os.getenv("RECENT_TRANSACTION_COUNT", "20"))  # 最近N笔交易

# 失败分析回看时间（分钟）
FAILURE_ANALYSIS_LOOKBACK = int(os.getenv("FAILURE_ANALYSIS_LOOKBACK", "120"))  # 回看120分钟

# ===================== 委员会投票配置 =====================
# 非法委员投票阈值
CORRUPT_VOTE_THRESHOLD = float(os.getenv("CORRUPT_VOTE_THRESHOLD", "8000"))  # 剩余任务超过此值则投不赞同

# =============================================================
