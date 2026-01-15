"""委员会模型模块"""
import json
import re
from agents.llm_agent import LLM_Agent


# ==========================================
# 1. 候选人基类
# ==========================================
class Candidate:
    def __init__(self, weight, identity_type, judge_type):
        self.weight = weight
        self.weight_base = 1000
        self.weight_upper_bound = 2000
        self.rep = 1
        self.rep_base = 1
        self.wc = 0.25
        self.wa = 0.5
        self.vote = []
        self.identity_type = identity_type
        self.judge_type = judge_type

    def update_rep(self, decision, answer):
        count_c = 0
        count_a = 0
        for i in range(len(decision)):
            if decision[i] == self.vote[i]:
                count_c = count_c + 1
            if answer[i] == self.vote[i]:
                count_a = count_a + 1
        consistency = count_c/len(decision)
        accuracy = count_a/len(decision)
        self.rep = 0.5 * self.rep + (self.wc * consistency + self.wa * accuracy) * self.rep_base

    def update_weight(self, rep_min, rep_max):
        self.weight = self.weight_base + (self.rep - self.rep_base) * (self.weight_upper_bound - self.weight_base)/(rep_max - rep_min + self.rep_base)

# ==========================================
# 2. 委员会类 (核心逻辑修改)
# ==========================================
class Committee(Candidate):
    def __init__(self, weight, identity_type, judge_type, api_key="sk-7394f6c37d9b400db23ce8bb7e97e336", base_url="https://dashscope.aliyuncs.com/compatible-mode/v1"):
        super().__init__(weight, identity_type, judge_type)
        if self.judge_type == "LLM":
            self.llm_agent = LLM_Agent(api_key, base_url)

    def _calculate_suspicion_score(self, transaction_history, current_time):
        """
        [修改点]：优化评分标准，让嚣张行为更容易得分
        """
        if not transaction_history: return 0.0

        recent_txs = [t for t in transaction_history if t['time'] >= current_time - 60]
        if not recent_txs: return 0.0

        active_mins = len(set(t['time'] for t in recent_txs))
        duration = min(current_time, 60)
        if duration < 1: duration = 1

        ratio = active_mins / duration
        total_tx = len(recent_txs)
        intensity = total_tx / active_mins if active_mins > 0 else 0

        score = 0.0
        # 1. 活跃度评分 (Ratio)
        if ratio >= 0.7:
            score += 6.0  # 几乎每分钟都在动 -> 极高分
        elif ratio >= 0.4:
            score += 4.0
        elif ratio >= 0.5:
            score += 2.0

        # 2. 强度评分 (Intensity)
        if intensity >= 3.0:
            score += 4.0
        elif intensity >= 2.0:
            score += 2.0

        return min(10.0, score)

    def Vote(self, target_user, current_time, legitimate_samples=None, illegal_samples=None):
        """
        [修改点]：移除评分计算，直接传入交易历史、合法用户样本和非法用户样本
        legitimate_samples: 合法用户的交易历史列表，每个元素是一个交易历史列表
        illegal_samples: 非法用户的交易历史列表，每个元素是一个交易历史列表
        """
        # 获取目标用户的交易历史（只包含time和amount）
        target_history = target_user.transaction_simple.copy()
        
        # 如果没有提供合法用户样本，使用空列表
        if legitimate_samples is None:
            legitimate_samples = []
        
        # 如果没有提供非法用户样本，使用空列表
        if illegal_samples is None:
            illegal_samples = []

        # 计算用户的存活时间（从第一次交易到最后一次交易的时间间隔，单位：分钟）
        survival_time = None
        if target_history and len(target_history) > 0:
            first_time = min(tx['time'] for tx in target_history)
            last_time = max(tx['time'] for tx in target_history)
            survival_time = last_time - first_time

        # --- 场景 A: 合法委员 ---
        if self.identity_type == "Legit":
            if self.judge_type == "LLM":
                # [修改点]：传入交易历史、合法用户样本和非法用户样本，不传评分
                response_str = self.llm_agent.get_legit_vote(target_history, legitimate_samples, illegal_samples, survival_time)
                return self._parse_llm_vote(response_str)
            else:
                # 传统的基于规则的判定 (保持兼容)
                suspicion_score = self._calculate_suspicion_score(target_user.transaction_simple, current_time)
                return True if suspicion_score >= 3 else False

        # --- 场景 B: 非法委员 ---
        elif self.identity_type == "Illegal":
            is_teammate = (target_user.type != "Legit")  # 判断是否是队友
            
            if not is_teammate:
                # 不是队友，全部投不赞同
                return True
            
            # 是队友的情况
            remaining_task = target_user.task  # 剩余任务量（ETH）
            
            if self.judge_type == "Rule":
                # 基于规则的判定：队友剩余金额超过8000则投不赞同，否则投赞同
                return remaining_task > 8000
            
            elif self.judge_type == "LLM":
                # LLM 判定：队友剩余金额超过8000则投不赞同，否则看具体行为
                progress_ratio = (target_user.task_initial - target_user.task) / target_user.task_initial
                response_str = self.llm_agent.get_corrupt_vote(target_history, legitimate_samples, illegal_samples, progress_ratio, remaining_task, is_teammate, survival_time)
                return self._parse_llm_vote(response_str)

        return False

    def _parse_llm_vote(self, response_str):
        try:
            if "```" in response_str:
                match = re.search(r"```(?:json)?(.*?)```", response_str, re.DOTALL)
                if match: response_str = match.group(1).strip()
            data = json.loads(response_str)
            return data.get("vote_ban", False)
        except:
            return True
