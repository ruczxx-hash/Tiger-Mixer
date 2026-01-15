"""实验配置模块"""
import os
from datetime import datetime

# ===================== Experiment Config =====================
# 攻击模式："rule" 或 "llm"
ATTACK_MODE = os.getenv("ATTACK_MODE", "llm")  # 支持通过环境变量设置
# =============================================================

# ===================== Run Identification =====================
# 运行标识符（基于时间戳）
RUN_ID = datetime.now().strftime("%Y%m%d_%H%M%S")

# 结果目录结构
RESULT_DIR = f"Result/{RUN_ID}"
TX_DIR = f"{RESULT_DIR}/transactions"
TX_DIR_FOR_ATTACK = f"{RESULT_DIR}/transactions/attack"
TX_DIR_FOR_NORMAL = f"{RESULT_DIR}/transactions/normal"
STATIC_DATA_DIR = f"{RESULT_DIR}/StaticData"
STATIC_DATA_FILE = f"{STATIC_DATA_DIR}/{RUN_ID}.txt"

# 统计文件路径
AUDITOR_STATS_FILE = f"{RESULT_DIR}/auditor_stats.txt"  # auditor统计文件
COMMITTEE_VOTE_STATS_FILE = f"{RESULT_DIR}/committee_vote_stats.txt"  # 委员会投票统计文件
PRECISION_FILE = f"{STATIC_DATA_DIR}/precision.txt"  # 三层拦截机制precision文件

# 参考样本目录和文件（位于DataSet文件夹中）
DATA_SET_DIR = os.getenv("DATA_SET_DIR", "DataSet")  # 数据集根目录
REFERENCE_SAMPLES_DIR = os.path.join(DATA_SET_DIR, "ReferenceSamples")  # 参考用户交易记录保存目录
REFERENCE_ILLEGAL_FILE = os.path.join(REFERENCE_SAMPLES_DIR, f"reference_illegal_{ATTACK_MODE}.json")  # 非法用户参考样本文件
REFERENCE_LEGITIMATE_FILE = os.path.join(REFERENCE_SAMPLES_DIR, "reference_legitimate.json")  # 合法用户参考样本文件

# 创建必要的目录结构
def _create_directories():
    """创建实验所需的目录结构"""
    os.makedirs(RESULT_DIR, exist_ok=True)
    os.makedirs(TX_DIR, exist_ok=True)
    os.makedirs(TX_DIR_FOR_ATTACK, exist_ok=True)
    os.makedirs(TX_DIR_FOR_NORMAL, exist_ok=True)
    os.makedirs(STATIC_DATA_DIR, exist_ok=True)
    os.makedirs(REFERENCE_SAMPLES_DIR, exist_ok=True)

# 初始化时创建目录
_create_directories()
# =============================================================
