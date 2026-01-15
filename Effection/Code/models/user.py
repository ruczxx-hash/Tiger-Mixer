import math
import os
import numpy as np
import json
import pandas as pd
import random
from datetime import datetime
from datetime import timedelta
import secrets

from dateutil.relativedelta import relativedelta
from agents.llm_strategy_agent import LLM_Strategy_Agent

BASE_DATETIME = datetime(2025, 4, 1, 0, 0, 0)
DATETIME_FMT = "%Y/%m/%d %H:%M"

# 使用容差进行浮点数比较，防止精度问题
ALLOWED_AMOUNTS = {0.1, 1, 10, 100}

used_addresses: set[str] = set()


def generate_txhash():
    return '0x' + secrets.token_hex(32)


# ==========================================
# 用户定义
# ==========================================
class User:
    def __init__(self, user_type, task, time_left, current_time=0, index=0, address=""):
        self.address = address if address else f"0x{secrets.token_hex(20)}"
        self.index = index
        self.type = user_type
        self.task = task
        self.task_initial = task
        self.time_left = time_left
        self.time = current_time
        self.base_datetime = BASE_DATETIME
        self.transaction = []
        self.transaction_simple = []
        self.is_alive = True
        self.features = None
        self.is_marked = False
        self._stat = None

    def tick(self):
        self.time += 1
        self.time_left -= 1

    def _log_transaction(self, amount):
        if not self.is_alive:
            return

        self.transaction_simple.append({"time": self.time, "amount": amount})
        new_tx = self.convert_parsed_tx_to_generated_format({"time": self.time, "amount": amount})

        # 逻辑合并优化：无论首次还是后续，统一入口
        if self._stat is None:
            self.update_features([new_tx])
        else:
            self.update_features([new_tx])
            self.transaction.append(new_tx)

    def Decide(self):
        raise NotImplementedError

    def init_user_from_eth_csv(self):
        from config.system_config import TX_DATA_DIR
        fixed_to_address: str = "0x910cbd523d972eb0a6f4cae4618ad62622b39dbf"
        TX_DIR = TX_DATA_DIR

        if not os.path.exists(TX_DIR):
            self.transaction = []
            return self

        tx_file = os.path.join(TX_DIR, f"{self.address}.csv")

        if not os.path.exists(tx_file):
            self.transaction = []
            return self

        try:
            user_eth = pd.read_csv(tx_file)
            transactions = []

            for idx, row in user_eth.iterrows():
                dt = datetime.strptime(row["DateTime"], "%Y/%m/%d %H:%M") + relativedelta(years=3)
                transactions.append({
                    "user_type": self.type,
                    "Index": len(transactions),
                    "Txhash": generate_txhash(),
                    "UnixTimestamp": int(dt.timestamp()),
                    "DateTime": dt.strftime("%Y/%m/%d %H:%M"),
                    "From": self.address,
                    "To": fixed_to_address,
                    "Value_IN": float(row["Value_IN"]),
                    "Value_OUT": 0,
                    "Method": "Deposit",
                    "GasPrice": 0
                })
            self.transaction = transactions
        except Exception as e:
            print(f"Error loading CSV for {self.address}: {e}")
            self.transaction = []

        return self

    def convert_parsed_tx_to_generated_format(self, parsed_tx):
        total_seconds = parsed_tx["time"] * 60
        tx_datetime = self.base_datetime + timedelta(seconds=total_seconds)
        to_address = "0x910cbd523d972eb0a6f4cae4618ad62622b39dbf"
        return {
            "user_type": self.type,
            "Index": len(self.transaction),
            "Txhash": generate_txhash(),
            "UnixTimestamp": int(tx_datetime.timestamp()),
            "DateTime": tx_datetime.strftime('%Y/%m/%d %H:%M'),
            "From": self.address,
            "To": to_address,
            "Value_IN": parsed_tx["amount"],
            "Value_OUT": 0,
            "Method": "Deposit",
            "GasPrice": 0
        }

    def init_features(self):
        txs = self.transaction
        if not txs:
            self.features = None
            return

        self._stat = {
            "count": 0,
            "sum": 0.0,
            "sum_sq": 0.0,
            "values": [],
            "first_ts": None,
            "last_ts": None,
            "intervals": [],
            "amount_counts": {0.1: 0, 1.0: 0, 10.0: 0, 100.0: 0, "other": 0}
        }

        for tx in sorted(txs, key=lambda x: x["UnixTimestamp"]):
            self._update_stat_with_tx(tx)

        self._rebuild_features()

    def _update_stat_with_tx(self, tx):
        s = self._stat
        ts = tx["UnixTimestamp"]
        val = tx["Value_IN"]

        s["count"] += 1
        s["sum"] += val
        s["sum_sq"] += val * val
        s["values"].append(val)

        if s["first_ts"] is None:
            s["first_ts"] = ts
        if s["last_ts"] is not None:
            s["intervals"].append(ts - s["last_ts"])
        s["last_ts"] = ts

        found = False
        for k in s["amount_counts"]:
            if k == "other": continue
            if math.isclose(val, k, rel_tol=1e-5):
                s["amount_counts"][k] += 1
                found = True
                break
        if not found:
            s["amount_counts"]["other"] += 1

    def update_features(self, new_txs):
        if not new_txs:
            return

        if self._stat is None:
            self.transaction.extend(new_txs)
            self.init_features()
            return

        for tx in sorted(new_txs, key=lambda x: x["UnixTimestamp"]):
            self._update_stat_with_tx(tx)

        self._rebuild_features()

    def _rebuild_features(self):
        # 【核心修复】：使用 NumPy 计算统计量，避免复杂数错误和逻辑冗余
        s = self._stat
        n = s["count"]
        values = np.array(s["values"], dtype=float)

        mean_val = np.mean(values) if n > 0 else 0.0
        std_val = np.std(values) if n > 1 else 0.0

        intervals = np.array(s["intervals"], dtype=float)
        avg_interval = np.mean(intervals) if len(intervals) > 0 else 0.0
        min_interval = np.min(intervals) if len(intervals) > 0 else 0.0
        max_interval = np.max(intervals) if len(intervals) > 0 else 0.0
        std_interval = np.std(intervals) if len(intervals) > 1 else 0.0

        time_span = s["last_ts"] - s["first_ts"] if n > 1 else 0
        hours = time_span / 3600 if time_span > 0 else 1

        self.features = {
            "total_tx": float(n),
            "value_mean": float(mean_val),
            "value_median": float(np.median(values)) if n > 0 else 0.0,
            "value_std": float(std_val),
            "value_min": float(np.min(values)) if n > 0 else 0.0,
            "value_max": float(np.max(values)) if n > 0 else 0.0,
            "value_sum": float(s["sum"]),

            "time_span_hours": float(time_span / 3600) if time_span > 0 else 0.0,
            "tx_per_hour": float(n / hours),

            "avg_interval": float(avg_interval),
            "min_interval": float(min_interval),
            "max_interval": float(max_interval),
            "std_interval": float(std_interval),

            "amount_0_1": s["amount_counts"][0.1],
            "amount_1": s["amount_counts"][1.0],
            "amount_10": s["amount_counts"][10.0],
            "amount_100": s["amount_counts"][100.0],
            "amount_other": s["amount_counts"]["other"],

            "pct_0_1": s["amount_counts"][0.1] / n if n > 0 else 0,
            "pct_1": s["amount_counts"][1.0] / n if n > 0 else 0,
            "pct_10": s["amount_counts"][10.0] / n if n > 0 else 0,
            "pct_100": s["amount_counts"][100.0] / n if n > 0 else 0,
            "pct_other": s["amount_counts"]["other"] / n if n > 0 else 0,

            "value_cv": float(std_val / mean_val) if mean_val > 0 else 0.0,
        }


# ==========================================
# Normal User（合法）
# ==========================================
class RetailUser(User):
    def __init__(self, rate=0.003, index=0, address=""):
        super().__init__(user_type="NormalUser", task=1000, time_left=1000, current_time=0, index=index,
                         address=address)
        self.rate = rate
        self.init_user_from_eth_csv()
        self.init_features()

    def Decide(self):
        if random.random() >= self.rate:
            return
        tx_count = random.choices(
            population=[1, 2],
            weights=[0.85, 0.15]
        )[0]

        amounts = [0.1, 1, 10]
        weights = [0.6, 0.3, 0.1]

        for _ in range(tx_count):
            amt = random.choices(amounts, weights)[0]
            self._log_transaction(amt)


class ExchangeUser(User):
    def __init__(self, rate=0.1, index=0, address=""):
        super().__init__(user_type="NormalUser", task=1000, time_left=1000, current_time=0, index=index,
                         address=address)
        self.rate = rate
        self.init_user_from_eth_csv()
        self.init_features()

    def Decide(self):
        if random.random() > self.rate:
            return

        tx_count = random.choices(
            population=[1, 2, 4, 6],
            weights=[0.3, 0.3, 0.25, 0.15]
        )[0]

        amounts = [0.1, 1, 10, 100]
        weights = [0.1, 0.2, 0.5, 0.2]

        for i in range(tx_count):
            if i > 0:
                weights = [0.3, 0.35, 0.25, 0.1]

            amt = random.choices(amounts, weights)[0]
            self._log_transaction(amt)


# ==========================================
# Rule-Based 洗钱用户
# ==========================================
class Rule_Based_User(User):
    def __init__(self, task, time_left, current_time=0, index=0):
        super().__init__("Rule_Based_User", task, time_left, current_time, index = index)
        self.base_prob = 0.45
        self.address = f"0x{index}0011110c0bc14115f9f67fde839f285547423"
        self.amount_pool = [0.1, 1, 10, 100]

    def Decide(self):
        if not self.is_alive or self.task <= 0 or self.time_left <= 0:
            return
        
        # 决定是否发送
        send = random.random()
        if send > 0.35:
            return
        
        # 选择交易数量
        tx_count = random.choices(
            population=[2, 5, 6, 9],
            weights=[0.3,0.4,0.2,0.1]
        )[0]
        
        amounts = [0.1, 1, 10, 100]
        for i in range(tx_count):
            if self.task <= 0:
                break
            amt = random.choices(amounts, [0.1,0.2,0.4,0.3])[0]
            # 确保不超过剩余任务
            if amt > self.task:
                possible = [x for x in amounts if x <= self.task]
                if possible:
                    amt = max(possible)
                else:
                    amt = self.task
            
            self._log_transaction(amt)
            self.task -= amt


# ==========================================
# LLM-Based User
# ==========================================
class LLM_Based_User(User):
    def __init__(self, task, time_left, current_time=0, index=0):
        super().__init__("LLM_Based_User", task, time_left, current_time, index=index)
        self.agent = LLM_Strategy_Agent(
            api_key="sk-7394f6c37d9b400db23ce8bb7e97e336",
            base_url="https://dashscope.aliyuncs.com/compatible-mode/v1"
        )
        self.address = f"0x{index}0011110ac3c14115f9f67fde839f285547423"
        self.action_plan = {}
        self.learned_rules = []

    def _generate_memory_context(self):
        lookback = 30
        recent = [tx for tx in self.transaction_simple if tx['time'] >= self.time - lookback]
        total = sum(tx['amount'] for tx in recent)
        active = len(set(tx['time'] for tx in recent))
        window = min(self.time + 1, lookback)

        return (
            f"Last {window} min | Total ETH: {total:.2f} | "
            f"Active mins: {active}"
        )

    def _calculate_transaction_frequency(self, lookback=30):
        """
        计算最近交易的频率：最近交易的总次数除以窗口值
        """
        recent = [tx for tx in self.transaction_simple if tx["time"] >= self.time - lookback]
        window = min(self.time + 1, lookback)
        transaction_count = len(recent)
        frequency = transaction_count / window if window > 0 else 0.0
        return frequency, transaction_count, window

    def _generate_speed_assessment(self):
        lookback = 30
        recent = [tx for tx in self.transaction_simple if tx["time"] >= self.time - lookback]

        total_eth = sum(tx["amount"] for tx in recent)
        window = min(self.time + 1, lookback)

        recent_rate = total_eth / window if window > 0 else 0
        remaining_task = self.task
        remaining_time = max(self.time_left, 1)

        required_rate = remaining_task / remaining_time
        speed_ratio = recent_rate / required_rate if required_rate > 0 else 1.0

        assessment = (
            f"=== TRANSFER SPEED ASSESSMENT ===\n"
            f"- Recent Average Speed: {recent_rate:.2f} ETH/min\n"
            f"- Required Average Speed to Finish: {required_rate:.2f} ETH/min\n"
            f"- Speed Adequacy Ratio (recent / required): {speed_ratio:.2f}\n\n"
        )

        if speed_ratio < 0.7:
            assessment += "STATUS: CRITICALLY TOO SLOW. Increase risk to finish.\n"
        elif speed_ratio < 1.0:
            assessment += "STATUS: SLIGHTLY TOO SLOW. Gradually increase speed.\n"
        else:
            assessment += "STATUS: SPEED IS SUFFICIENT. Prioritize stealth.\n"

        return assessment

    def observe_ban(self, banned_user):
        #print(f"\n[LLM AGENT] Observed {banned_user.type} being banned.")
        try:
            # 格式转换：简化数据，减少 Token，避免复杂对象传递错误
            hist = []
            for tx in banned_user.transaction:
                hist.append({"time": (tx["UnixTimestamp"] - banned_user.base_datetime.timestamp()) / 60,
                             "amount": tx["Value_IN"]})

            rule = self.agent.analyze_failure(
                banned_user.type,
                hist,
                self.time
            )
            #print(f"[LLM AGENT] Learned rule: {rule}")
            self.learned_rules.append(rule)
            self.action_plan.clear()
        except Exception as e:
            print(f"Error observing ban: {e}")

    def Decide(self):
        if not self.is_alive or self.task <= 0:
            return

        if self.time not in self.action_plan:
            speed_assessment = self._generate_speed_assessment()
            memory = self._generate_memory_context()
            transaction_frequency, tx_count, window = self._calculate_transaction_frequency()

            combined_context = speed_assessment + "\n\n" + memory

            print(f"\n--- [LLM AGENT] Planning at {self.time} min ---", flush=True)
            print(f"Transaction Frequency: {transaction_frequency:.4f} (Recent {tx_count} transactions in {window} min window)", flush=True)
            print(f'Task Completed: {self.task:.2f} ETH', flush=True)

            plan = self.agent.get_decision(
                self.task,
                self.time_left,
                combined_context,
                self.learned_rules,
                transaction_frequency
            )
            # print(f"Decision: {json.dumps(plan)}")
            for i in range(10):
                self.action_plan[self.time + i] = plan.get(f"{i}min", [])

        txs = self.action_plan.get(self.time, [])
        for amt in txs:
            if self.task >= amt:
                self._log_transaction(amt)
                self.task -= amt
