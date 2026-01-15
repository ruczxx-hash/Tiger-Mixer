import logging
logging.basicConfig(
    level=logging.INFO,  # 设置日志级别（DEBUG, INFO, WARNING, ERROR, CRITICAL）
    format='%(asctime)s - %(levelname)s - %(message)s',  # 日志格式
    handlers=[
        logging.FileHandler("app.log", encoding='utf-8'),  # 保存到本地文件
        logging.StreamHandler()  # 同时输出到控制台（可选）
    ]
)