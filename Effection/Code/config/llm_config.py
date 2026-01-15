"""LLM配置模块"""
import os

# ===================== LLM API Configuration =====================
# API密钥配置（建议通过环境变量设置，避免硬编码）
LLM_API_KEY = os.getenv("LLM_API_KEY", "sk-7394f6c37d9b400db23ce8bb7e97e336")
LLM_BASE_URL = os.getenv("LLM_BASE_URL", "https://dashscope.aliyuncs.com/compatible-mode/v1")
LLM_MODEL = os.getenv("LLM_MODEL", "qwen3-max")

# LLM请求配置
LLM_TIMEOUT = float(os.getenv("LLM_TIMEOUT", "30.0"))  # 请求超时时间（秒）
LLM_MAX_RETRIES = int(os.getenv("LLM_MAX_RETRIES", "2"))  # 最大重试次数

# LLM温度参数配置（不同场景使用不同温度）
LLM_TEMPERATURE_DEFAULT = float(os.getenv("LLM_TEMPERATURE_DEFAULT", "0.6"))  # 默认温度
LLM_TEMPERATURE_STRATEGY = float(os.getenv("LLM_TEMPERATURE_STRATEGY", "0.3"))  # 策略代理温度（更确定性）
LLM_TEMPERATURE_MONITOR = float(os.getenv("LLM_TEMPERATURE_MONITOR", "0.1"))  # 监控器温度（高确定性）

# LLM重试配置
LLM_MAX_ATTEMPTS = int(os.getenv("LLM_MAX_ATTEMPTS", "3"))  # 最大尝试次数
LLM_SLEEP_TIME = int(os.getenv("LLM_SLEEP_TIME", "60"))  # 重试间隔（秒）
# =============================================================
