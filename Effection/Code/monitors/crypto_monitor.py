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


class CryptoMonitor:
    def __init__(self, users, name):
        self.name = name
        self.feature_weights = None
        self.client = OpenAI(
            api_key="sk-7394f6c37d9b400db23ce8bb7e97e336",
            base_url="https://dashscope.aliyuncs.com/compatible-mode/v1"
        )
        self.IF, self.scaler, self.features_col = self.init_IF(users)

    def _serialize_user_data(self, user) -> str:
        """
        将User对象转化为大模型可读的JSON字符串。
        关键点：必须移除 user.type (Ground Truth)，否则模型会直接读取答案。
        """

        # 提取统计特征
        stats_data = user._stat.copy()

        # 提取最近的N笔交易行为（避免Token溢出，通常洗钱看模式不需要全部历史）
        recent_txs = []
        for tx in user.transaction[-20:]:  # 取最近20笔
            recent_txs.append({
                "DateTime": tx["DateTime"],
                "Value_IN": tx["Value_IN"],
                "Method": tx["Method"]
            })

        # 构造发给LLM的数据包
        context_data = {
            "address_anonymized": user.address[:6] + "...",  # 仅作为标识
            "statistics": {
                "total_transactions": stats_data["count"],
                "total_volume": stats_data["sum"],
                "specific_denomination_counts": stats_data["amount_counts"],  # 混币器常见特征
                "transaction_intervals": stats_data["intervals"]  # 交易时间间隔
            },
            "recent_transaction_history": recent_txs
        }

        return json.dumps(context_data, indent=2)

    def _construct_prompt(self, user_data_json: str) -> str:
        """
        构建提示词 (Prompt Engineering)
        只能依靠待判断用户的交易记录进行分析
        """
        prompt = f"""
            ### Role Definition
            You are a **Regulator** (监管者) specializing in cryptocurrency mixing services. Your task is to act as a supervisor and identify suspicious users who may be engaged in money laundering activities.
            
            ### Your Mission
            As a regulator, your goal is to:
            1. **Minimize false negatives**: Try not to miss any illegal users (尽量不放过任何非法用户)
            2. **Avoid false positives**: Do not flag everyone as suspicious (不能通杀)
            3. **Focus on clear anomalies**: Only mark users as suspicious when their behavior shows **obvious and significant anomalies** (对于行为有明显异常的才判断为嫌疑用户)
            
            ### Task
            Analyze the provided user's transaction data and determine if the user exhibits **obviously suspicious behavior** that warrants being flagged as a potential money launderer.
            
            **IMPORTANT**: You can ONLY rely on the target user's transaction records provided below. You do NOT have access to:
            - Examples of legitimate users
            - Examples of illegal users
            - Survival time information
            - Any external reference data
            
            You must make your judgment **solely based on the patterns and anomalies** visible in the target user's own transaction history.
            
            ### Indicators of Suspicious Behavior (Money Laundering):
            Look for **obvious and significant anomalies** such as:
            1. **Highly Standardized Amounts**: Excessive repetition of exact round numbers (0.1, 1.0, 10.0, 100.0) in a pattern that suggests automation
            2. **Extremely High Frequency**: Unnaturally rapid succession of deposits with very short, consistent intervals
            3. **Programmatic Patterns**: Very consistent time intervals or transaction values that deviate significantly from human-like behavior
            4. **Unusual Volume Patterns**: Extremely high total volume split into many small, standardized chunks in a short time period
            
            ### Judgment Criteria
            - **Only flag as suspicious** if the user's behavior shows **clear, obvious, and significant anomalies** that strongly suggest automated or programmatic money laundering activity
            - **Do NOT flag** users with normal variations in transaction patterns, even if they use standard amounts occasionally
            - **Be strict but fair**: Your goal is to catch illegal users while avoiding false positives on legitimate privacy-seeking users
            - **When in doubt, err on the side of caution**: If the behavior is ambiguous or could be normal, do NOT flag as suspicious
            
            ### Target User Data (JSON)
            {user_data_json}
            
            ### Output Requirement
            You must output a raw JSON object only. Do not output markdown code blocks. The JSON must contain:
            1. "reasoning": A brief explanation of your analysis focusing on the specific anomalies observed (max 50 words).
            2. "is_money_launderer": Boolean (true or false). Only set to true if you observe **obvious and significant anomalies**.
            
            Example Output:
            {{
                "reasoning": "User shows highly automated pattern: 50+ transactions of exactly 10.0 ETH with perfectly consistent 1-minute intervals, indicating programmatic smurfing.",
                "is_money_launderer": true
            }}
            """
        return prompt

    def detect(self, user) -> bool:
        """
        主入口函数
        只能依靠待判断用户的交易记录进行分析
        """
        # 1. 准备数据
        user_json = self._serialize_user_data(user)

        # 2. 构建提示词
        prompt = self._construct_prompt(user_json)

        # 3. 调用大模型，带重试机制：最大尝试3次
        for attempt in range(3):
            try:
                response = self.client.chat.completions.create(
                    model="qwen3-max",
                    messages=[{"role": "user", "content": prompt}],
                    temperature=0.1 # 低温度以获得确定性结果
                )
                response_text = response.choices[0].message.content
                break  # 成功则跳出循环
            except RateLimitError as e:
                print(f"Monitor Rate Limit Error (attempt {attempt + 1}/3): {e}")
                if attempt < 2:
                    print("Rate limit exceeded, sleeping 60 seconds before retry...")
                    time.sleep(60)  # 睡眠1分钟
                    continue
                else:
                    print("Max retries exceeded for rate limit. Raising error.")
                    raise
            except (APITimeoutError, TimeoutError) as e:
                print(f"Monitor Timeout Error (attempt {attempt + 1}/3): {e}")
                if attempt < 2:
                    time.sleep(60)  # 睡眠1分钟
                    continue
                else:
                    print("Max retries exceeded for timeout. Raising error.")
                    raise
            except (APIError, ConnectionError) as e:
                error_str = str(e).lower()
                # 检查是否是速率限制相关的错误
                if "rate" in error_str or "limit" in error_str or "quota" in error_str:
                    print(f"Monitor Rate Limit Error (attempt {attempt + 1}/3): {e}")
                    if attempt < 2:
                        print("Rate limit exceeded, sleeping 60 seconds before retry...")
                        time.sleep(60)  # 睡眠1分钟
                        continue
                    else:
                        print("Max retries exceeded for rate limit. Raising error.")
                        raise
                else:
                    print(f"Monitor API Error (attempt {attempt + 1}/3): {e}")
                    if attempt < 2:
                        time.sleep(60)  # 睡眠1分钟
                        continue
                    else:
                        print("Max retries exceeded. Raising error.")
                        raise
            except KeyboardInterrupt:
                # 允许用户中断程序
                raise
            except Exception as e:
                print(f"Monitor Unexpected Error (attempt {attempt + 1}/3): {e}")
                if attempt < 2:
                    time.sleep(60)  # 睡眠1分钟
                    continue
                else:
                    print("Max retries exceeded. Raising error.")
                    raise
        else:
            # 如果所有重试都失败，抛出异常
            raise Exception("All retries failed for monitor detect method")

        # 4. 解析结果
        try:
            # 清理可能的 Markdown 标记
            clean_text = response_text.replace("```json", "").replace("```", "").strip()
            result = json.loads(clean_text)

            #print(f"Agent Reasoning: {result.get('reasoning')}")
            return result.get("is_money_launderer", False)

        except json.JSONDecodeError:
            print("Error: LLM output format invalid.")
            return False


    def batch_audit(self, users, debug=True):
        if not hasattr(self, 'IF') or self.IF is None:
            return [False] * len(users)

        valid_users = [u for u in users if u.features is not None]
        if not valid_users:
            return []

        # --- 1. 构建矩阵 ---
        X_data = []
        for u in valid_users:
            row = []
            for col in self.features_col:
                val = u.features.get(col, 0.0)
                if isinstance(val, complex): val = val.real
                if pd.isna(val) or np.isinf(val): val = 0.0
                row.append(val)
            X_data.append(row)
        X = np.array(X_data, dtype=float)

        try:
            # --- 2. 变换数据 ---
            X_scaled = self.scaler.transform(X)

            # 检查：如果有权重，应用权重
            if self.feature_weights is not None and len(self.feature_weights) == X_scaled.shape[1]:
                X_scaled *= self.feature_weights

            # --- 3. 预测 ---
            scores = self.IF.decision_function(X_scaled)

            # --- 4. 自动诊断逻辑 (关键) ---
            # 如果异常比例超过 50%，或者显式开启 debug，则触发诊断
            anomalies_count = np.sum(scores < 0)
            if debug:
                self._print_diagnosis(valid_users, X, X_scaled, scores)

        except Exception as e:
            print(f"Batch audit failed: {e}")
            traceback.print_exc()
            return []

        # --- 5. 选出异常的用户（score < 0）---
        # score < 0 表示异常，选择所有 score < 0 的用户
        banned_users = []
        if len(valid_users) > 0:
            # 创建(score, user)的元组列表，筛选出 score < 0 的用户
            banned_users = [user for score, user in zip(scores, valid_users) if score < 0]
            
            if debug:
                print(f"[IF Audit] 总用户数: {len(valid_users)}, 选出异常用户（score < 0）: {len(banned_users)} 个")
                if len(banned_users) > 0:
                    min_score = min(scores)
                    max_score = max(scores)
                    banned_scores = [score for score, user in zip(scores, valid_users) if score < 0]
                    banned_min_score = min(banned_scores) if banned_scores else 0
                    banned_max_score = max(banned_scores) if banned_scores else 0
                    print(f"[IF Audit] Score范围: [{min_score:.4f}, {max_score:.4f}], "
                          f"被选中用户Score范围: [{banned_min_score:.4f}, {banned_max_score:.4f}]")

        return banned_users

    def _print_diagnosis(self, users, X_raw, X_scaled, scores, top_n=10):
        """
        打印并追加保存"谁导致了异常"的详细报告
        文件名: self.name
        """
        n_users = len(users)
        n_anomalies = np.sum(scores < 0)

        def log(msg=""):
            with open(self.name, "a", encoding="utf-8") as f:
                f.write(msg + "\n")

        log("\n" + "!" * 80)
        log(f"!!! 严重警告: 异常率过高 ({n_anomalies}/{n_users}) - 启动诊断模式 !!!")
        log("!" * 80)

        # 1. 找出得分最低的 top_n 个用户（最异常）
        worst_indices = np.argsort(scores)[:top_n]

        for rank, idx in enumerate(worst_indices, 1):
            user = users[idx]
            score = scores[idx]

            log(f"\n>> 第 {rank} 异常用户: {user.address} ({user.type})")
            log(f">> 异常得分: {score:.4f} (小于0即为异常)")

            # 2. 找出该用户哪个特征"爆表"
            scaled_row = np.abs(X_scaled[idx])
            top_feat_indices = np.argsort(scaled_row)[::-1][:3]

            log("\n>> 【罪魁祸首】导致该用户异常的核心特征 (Scaled Value > 3 为巨大偏差):")
            log(f"{'Feature Name':<20} | {'Raw Value':<15} | {'Scaled (Z-Score)':<15} | {'Weight'}")
            log("-" * 70)

            for feat_idx in top_feat_indices:
                feat_name = self.features_col[feat_idx]
                raw_val = X_raw[idx][feat_idx]
                scaled_val = X_scaled[idx][feat_idx]
                weight = self.feature_weights[feat_idx] if self.feature_weights is not None else 1.0

                log(f"{feat_name:<20} | {raw_val:<15.4f} | {scaled_val:<15.4f} | {weight:.2f}")

            log("-" * 70)

        log(f"\n>> 总结: 输出了前 {len(worst_indices)} 个最异常用户")
        log(">> 分析建议:")
        log("   1. 如果 Scaled 值超过 10 或 100，说明该特征方差极小(StandardScaler分母接近0)。")
        log("   2. 虽然该用户 Raw Value 看起来正常，但在低方差背景下会被判定为巨型离群值。")
        log("!" * 80 + "\n")

    def init_IF(self, users):
        # ... (保持之前的 init_IF 逻辑不变) ...
        # 确保引用 train_isolation_forest 的新逻辑

        user_features_list = []
        total_tx_count = 0
        for user in users:
            if hasattr(user, "features") and user.features is not None:
                f = user.features.copy()
                f['From'] = user.address
                f['user_type'] = user.type
                user_features_list.append(f)
                total_tx_count += f.get('total_tx', 0)

        if len(user_features_list) < 5:  # 稍微降低门槛方便调试
            return None, None, []

        user_df = pd.DataFrame(user_features_list)
        return self.train_isolation_forest(user_df)

    def train_isolation_forest(self, user_df, amount_weight=0.8, frequency_weight=0.9, time_weight=1.3):
        # ... (保持之前提供的 train_isolation_forest 逻辑不变) ...
        # ... 确保包含 col_stds < 1e-6 的判断 ...
        if user_df.empty:
            return None, None, []

        feature_cols = [c for c in user_df.columns if c not in ("From", "user_type")]

        # 特征分组
        amount_features = {
            'value_mean', 'value_median', 'value_std', 'value_min', 'value_max',
            'value_sum', 'amount_0_1', 'amount_1', 'amount_10', 'amount_100',
            'amount_other', 'pct_0_1', 'pct_1', 'pct_10', 'pct_100', 'pct_other',
            'value_cv'
        }
        frequency_features = {'total_tx', 'tx_per_hour'}
        time_features = {
            'time_span_hours', 'avg_interval',
            'min_interval', 'max_interval', 'std_interval'
        }

        # 数据准备
        X = user_df[feature_cols].values
        X = np.nan_to_num(X, nan=0.0, posinf=1e10, neginf=-1e10)

        # 动态方差检测
        col_stds = np.std(X, axis=0)

        feature_weights = np.ones(len(feature_cols))
        for i, col in enumerate(feature_cols):
            # 放宽一点限制，如果是完全常量才剔除，或者保留极小权重
            if col_stds[i] < 1e-9:
                feature_weights[i] = 0.0
                continue

            if col in amount_features:
                feature_weights[i] = amount_weight
            elif col in frequency_features:
                feature_weights[i] = frequency_weight
            elif col in time_features:
                feature_weights[i] = time_weight

        self.feature_weights = feature_weights

        scaler = StandardScaler()
        X_scaled = scaler.fit_transform(X)
        X_scaled *= self.feature_weights

        iso_forest = IsolationForest(
            contamination=0.002,
            n_estimators=100,
            random_state=42,
            n_jobs=-1
        )

        iso_forest.fit(X_scaled)
        return iso_forest, scaler, feature_cols
