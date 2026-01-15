# 代码重构指南

由于原始文件较大，本指南说明如何完成代码拆分。

## 步骤说明

### 1. 创建 monitors/crypto_monitor.py

从 `user_4amount.py` 中提取 `CryptoMonitor` 类（第33-405行），添加以下导入：

```python
"""加密监控器模块"""
import math
import os
import numpy as np
import json
import pandas as pd
import traceback
import time
from openai import OpenAI
from openai import APITimeoutError, APIError, RateLimitError
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler

# 然后复制 CryptoMonitor 类的完整代码
```

### 2. 创建 agents/llm_strategy_agent.py

从 `user_4amount.py` 中提取 `LLM_Strategy_Agent` 类（第409-855行），添加以下导入和常量：

```python
"""LLM策略代理模块（用于攻击用户）"""
import math
import json
import re
import time
from openai import OpenAI
from openai import APITimeoutError, APIError, RateLimitError

# 使用容差进行浮点数比较，防止精度问题
ALLOWED_AMOUNTS = {0.1, 1, 10, 100}

# 然后复制 LLM_Strategy_Agent 类的完整代码
```

### 3. 创建 agents/llm_agent.py

从 `candidate.py` 中提取 `LLM_Agent` 类（第8-298行），添加以下导入：

```python
"""LLM代理模块（用于委员会投票）"""
import json
import re
import time
from openai import OpenAI
from openai import APITimeoutError, APIError

# 然后复制 LLM_Agent 类的完整代码
```

### 4. 更新 models/user.py

从 `user_4amount.py` 中提取用户相关类（第861-1340行），并：
- 移除 `CryptoMonitor` 类
- 移除 `LLM_Strategy_Agent` 类
- 添加导入：`from agents.llm_strategy_agent import LLM_Strategy_Agent`
- 保留所有用户类：`User`, `RetailUser`, `ExchangeUser`, `Rule_Based_User`, `LLM_Based_User`

### 5. 更新 models/committee.py

从 `candidate.py` 中提取委员会相关类（第301-443行），并：
- 移除 `LLM_Agent` 类（已移到agents模块）
- 添加导入：`from agents.llm_agent import LLM_Agent`
- 保留 `Candidate` 和 `Committee` 类

### 6. 创建 main.py

从原始 `main.py` 重构主程序：
- 添加导入：`from config.experiment_config import *`
- 添加导入：`from models.user import *`
- 添加导入：`from models.committee import *`
- 添加导入：`from monitors.crypto_monitor import CryptoMonitor`
- 添加导入：`from utils.file_utils import *`
- 添加导入：`from utils.reference_samples import *`
- 移除配置相关代码（已移到config模块）
- 移除工具函数（已移到utils模块）

## 快速完成方法

可以使用以下Python脚本快速完成拆分：

```python
# split_code.py
import re

# 读取原始文件
with open('../user_4amount.py', 'r', encoding='utf-8') as f:
    content = f.read()

# 提取各个类（使用正则表达式）
# ... 参考之前创建的split_code.py脚本
```

## 验证

完成拆分后，确保：
1. 所有导入路径正确
2. 所有类和方法完整
3. 代码逻辑未改变
4. 可以正常运行
