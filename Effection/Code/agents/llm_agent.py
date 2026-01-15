"""LLM代理模块（用于委员会投票）"""
import json
from .base_llm_agent import BaseLLMAgent


class LLM_Agent(BaseLLMAgent):
    def __init__(self, api_key, base_url, model="qwen3-max", timeout=30.0, max_retries=2):
        super().__init__(api_key, base_url, model, timeout, max_retries)

    def get_corrupt_vote(self, target_transaction_history, legitimate_samples, illegal_samples, task_ratio, remaining_task, is_teammate, survival_time=None):
        """
        非法委员的投票逻辑：
        - 如果不是队友，全部投不赞同（vote_ban = True）
        - 如果是队友：
          - 如果队友剩余金额超过8000，则投不赞同（vote_ban = True）
          - 否则看队友具体行为而定
        remaining_task: 队友剩余的任务量（ETH）
        is_teammate: 是否是队友（True表示是队友，False表示不是队友）
        survival_time: 用户的存活时间（从第一次交易到最后一次交易的时间间隔，单位：分钟）
        """
        if not is_teammate:
            # 不是队友，直接返回投不赞同
            return json.dumps({"vote_ban": True, "reason": "Not a teammate, vote to ban"})
        
        # 是队友的情况
        if remaining_task > 8000:
            # 队友剩余金额超过8000，投不赞同
            return json.dumps({"vote_ban": True, "reason": f"Teammate has {remaining_task:.2f} ETH remaining (over 8000), vote to ban"})
        
        # 队友剩余金额不超过8000，需要看具体行为
        system_prompt = (
            "You are a corrupt committee member. You are voting for a teammate (illegal user).\n"
            "**Important Context:**\n"
            "This user is your teammate (an illegal user like you). "
            f"Their remaining task amount is {remaining_task:.2f} ETH (not exceeding 8000 ETH).\n\n"
            "**Your Task:**\n"
            "Analyze your teammate's transaction history carefully. "
            "Legitimate and illegal user samples will be provided as reference materials only. "
            "You should make your own independent judgment based on your analysis of your teammate's behavior patterns.\n\n"
            "**Important:**\n"
            "The legitimate and illegal user samples are provided ONLY as reference materials to help you understand typical patterns. "
            "You should make your own independent judgment based on your analysis of your teammate's transaction history. "
            "Do not blindly follow the reference samples. Use your own reasoning to make the most accurate judgment possible.\n\n"
            "**Key Indicators for Suspicious Behavior:**\n"
            "- **Transaction Frequency**: Users with HIGH transaction frequency are MORE LIKELY to be illegal users compared to users with LOW transaction frequency\n"
            "- **Transaction Amount**: Users with LARGE transaction amounts are MORE LIKELY to be illegal users compared to users with SMALL transaction amounts\n"
            "- Users with both high frequency AND large amounts should be considered more suspicious\n"
            "- However, make your judgment based on comprehensive analysis, not just these two factors alone\n\n"
            "**Considerations:**\n"
            "- Focus on your teammate's transaction patterns and behavior\n"
            "- The reference samples are for reference only - use them to understand patterns, but make your own judgment\n"
            "- Pay special attention to transaction frequency and transaction amounts\n"
            "- Consider whether their behavior is too obvious and might get them caught\n"
            "- Make a judgment based on your own thorough analysis of their specific behavior patterns\n"
            "- Strive to make the most accurate judgment possible\n\n"
            "**Output JSON:** {\"vote_ban\": boolean, \"reason\": string}\n"
            "- vote_ban: True means vote to BAN (if you think the teammate is too suspicious), False means vote to PASS\n"
            "- reason: Brief explanation of your judgment"
        )

        # 格式化目标用户交易历史（使用完整历史记录）
        target_history_str = json.dumps(target_transaction_history, indent=2)
        
        # 格式化合法用户样本（使用完整历史记录，包含所有样本）
        legitimate_samples_str = "[\n"
        for i, sample in enumerate(legitimate_samples):  # 使用所有样本
            legitimate_samples_str += f"  Legitimate Sample {i+1}: {json.dumps(sample, indent=4)},\n"
        legitimate_samples_str += "]"
        
        # 格式化非法用户样本（使用完整历史记录，包含所有样本）
        illegal_samples_str = "[\n"
        for i, sample in enumerate(illegal_samples):  # 使用所有样本
            illegal_samples_str += f"  Illegal Sample {i+1}: {json.dumps(sample, indent=4)},\n"
        illegal_samples_str += "]"

        user_prompt = (
            f"**Teammate Transaction History:**\n"
            f"{target_history_str}\n\n"
            f"**Legitimate User Samples (for reference only):**\n"
            f"{legitimate_samples_str}\n\n"
            f"**Illegal User Samples (for reference only):**\n"
            f"{illegal_samples_str}\n\n"
            f"**Additional Context:**\n"
            f"- Remaining Task: {remaining_task:.2f} ETH\n"
            "**Your Analysis:**\n"
            "This is your teammate. Their remaining task amount is not exceeding 8000 ETH. "
            "Please carefully analyze their transaction history. "
            "The legitimate and illegal user samples above are provided ONLY as reference materials to help you understand typical patterns. "
            "You should make your own independent judgment based on your thorough analysis of your teammate's behavior, "
            "not simply based on which reference samples they seem more similar to.\n\n"
            "**Key Points to Consider:**\n"
            "- Pay special attention to transaction frequency: users with high frequency are more likely to be illegal\n"
            "- Pay special attention to transaction amounts: users with large amounts are more likely to be illegal\n"
            "- Users with both high frequency AND large amounts should be considered more suspicious\n"
            "- However, make a comprehensive judgment considering all factors, not just frequency and amount\n\n"
            "Focus on your teammate's actual transaction patterns, timing, amounts, and behavior characteristics. "
            "Use your own reasoning to determine whether their behavior is too obvious and might get them caught, "
            "or if their behavior is reasonable and can be justified. "
            "Strive to make the most accurate judgment possible.\n\n"
            "Vote True to BAN if your analysis indicates their behavior is too obvious and might get them caught, "
            "or False to PASS if your analysis indicates their behavior is reasonable and can be justified."
        )

        # 使用基类的重试机制调用LLM
        try:
            messages = [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt}
            ]
            response_str = self._call_llm_with_retry(
                messages=messages,
                response_format={"type": "json_object"},
                max_attempts=3,
                sleep_time=60,
                context_name="(get_corrupt_vote)"
            )
            
            # 解析初步判断结果
            result = self._parse_json_response(response_str, default_value={"vote_ban": True, "reason": "Parse error"})
            if isinstance(result, str):
                # 如果解析失败，返回默认值
                return json.dumps({"vote_ban": True, "reason": "Parse error"})
            
            initial_vote_ban = result.get("vote_ban", False)
            initial_reason = result.get("reason", "")
            
            # 如果初步判断为有嫌疑（vote_ban = True），且存活时间超过3年，则改为合法用户
            if initial_vote_ban and survival_time is not None and survival_time > 1576800:  # 3年 = 1576800分钟
                survival_time_days = survival_time / (24 * 60)
                survival_time_years = survival_time_days / 365
                return json.dumps({
                    "vote_ban": False,
                    "reason": f"Initially considered suspicious, but survival time ({survival_time_years:.2f} years) exceeds 3 years, indicating legitimate long-term user. Original reason: {initial_reason}"
                })
            else:
                return json.dumps(result)
                
        except Exception as e:
            # 如果所有重试都失败，返回默认值
            print(f"All retries failed in get_corrupt_vote: {e}")
            return json.dumps({"vote_ban": True, "reason": "Max retries exceeded"})

    def get_legit_vote(self, target_transaction_history, legitimate_samples, illegal_samples, survival_time=None):
        """
        合法委员：基于交易记录做出合理的判断
        提供目标用户的交易记录、合法用户参考样本和非法用户参考样本
        survival_time: 用户的存活时间（从第一次交易到最后一次交易的时间间隔，单位：分钟）
        """
        system_prompt = (
            "You are an honest committee member. Your task is to analyze transaction records and make reasonable judgments.\n\n"
            "**Your Task:**\n"
            "You will be provided with:\n"
            "1. The target user's transaction history\n"
            "2. Legitimate user reference samples (for reference only)\n"
            "3. Illegal user reference samples (for reference only)\n\n"
            "**Important:**\n"
            "The legitimate and illegal user samples are provided ONLY as reference materials to help you understand typical patterns. "
            "You should make your own independent judgment based on your analysis of the target user's transaction history. "
            "Do not blindly follow the reference samples. Use your own reasoning and analysis to make the most accurate judgment possible.\n\n"
            "**Your Goal:**\n"
            "Carefully analyze the target user's transaction history and make an accurate judgment on whether the user should be banned. "
            "The reference samples are just examples to help you understand typical patterns, but your final decision should be based on your own thorough analysis.\n\n"
            "**Key Indicators for Suspicious Behavior:**\n"
            "- **Transaction Frequency**: Users with HIGH transaction frequency are MORE LIKELY to be illegal users compared to users with LOW transaction frequency\n"
            "- **Transaction Amount**: Users with LARGE transaction amounts are MORE LIKELY to be illegal users compared to users with SMALL transaction amounts\n"
            "- Users with both high frequency AND large amounts should be considered more suspicious\n"
            "- However, make your judgment based on comprehensive analysis, not just these two factors alone\n\n"
            "**Analysis Guidelines:**\n"
            "- Focus primarily on the target user's transaction history and behavior patterns\n"
            "- Compare the target user's patterns with the reference samples to understand typical behaviors\n"
            "- Pay special attention to transaction frequency and transaction amounts\n"
            "- Consider timing patterns, amount variations, and other behavioral characteristics\n"
            "- Make your judgment based on your own thorough analysis, not just similarity to reference samples\n"
            "- Strive to make the most accurate judgment possible\n\n"
            "**Output JSON:** {\"vote_ban\": boolean, \"reason\": string}\n"
            "- vote_ban: True means vote to BAN the user (if you think the user is suspicious), False means vote to PASS the user\n"
            "- reason: Brief explanation of your judgment"
        )

        # 格式化目标用户交易历史（使用完整历史记录）
        target_history_str = json.dumps(target_transaction_history, indent=2)
        
        # 格式化合法用户样本（使用完整历史记录，包含所有样本）
        legitimate_samples_str = "[\n"
        for i, sample in enumerate(legitimate_samples):  # 使用所有样本
            legitimate_samples_str += f"  Legitimate Sample {i+1}: {json.dumps(sample, indent=4)},\n"
        legitimate_samples_str += "]"
        
        # 格式化非法用户样本（使用完整历史记录，包含所有样本）
        illegal_samples_str = "[\n"
        for i, sample in enumerate(illegal_samples):  # 使用所有样本
            illegal_samples_str += f"  Illegal Sample {i+1}: {json.dumps(sample, indent=4)},\n"
        illegal_samples_str += "]"

        user_prompt = (
            f"**Target User Transaction History:**\n"
            f"{target_history_str}\n\n"
            f"**Legitimate User Samples (for reference only):**\n"
            f"{legitimate_samples_str}\n\n"
            f"**Illegal User Samples (for reference only):**\n"
            f"{illegal_samples_str}\n\n"
            "**Your Analysis:**\n"
            "Please carefully analyze the target user's transaction history. "
            "The legitimate and illegal user samples above are provided ONLY as reference materials to help you understand typical patterns. "
            "You should make your own independent judgment based on your thorough analysis of the target user's behavior, "
            "not simply based on which reference samples the target user seems more similar to.\n\n"
            "**Key Points to Consider:**\n"
            "- Pay special attention to transaction frequency: users with high frequency are more likely to be illegal\n"
            "- Pay special attention to transaction amounts: users with large amounts are more likely to be illegal\n"
            "- Users with both high frequency AND large amounts should be considered more suspicious\n"
            "- However, make a comprehensive judgment considering all factors, not just frequency and amount\n\n"
            "Focus on the target user's actual transaction patterns, timing, amounts, and behavior characteristics. "
            "Use your own reasoning to determine whether this user should be banned or passed. "
            "Strive to make the most accurate judgment possible.\n\n"
            "Vote True to BAN the user if your analysis indicates they are suspicious and should be banned, "
            "or False to PASS the user if your analysis indicates they should be passed."
        )

        # 使用基类的重试机制调用LLM（合法委员使用较短的等待时间）
        try:
            messages = [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt}
            ]
            response_str = self._call_llm_with_retry(
                messages=messages,
                response_format={"type": "json_object"},
                max_attempts=3,
                sleep_time=1,  # 合法委员使用较短的等待时间
                context_name="(get_legit_vote)"
            )
            
            # 解析初步判断结果
            result = self._parse_json_response(response_str, default_value={"vote_ban": False, "reason": "Parse error - default pass"})
            if isinstance(result, str):
                # 如果解析失败，返回保守判定（默认通过）
                return json.dumps({"vote_ban": False, "reason": "Parse error - default pass"})
            
            initial_vote_ban = result.get("vote_ban", False)
            initial_reason = result.get("reason", "")
            
            # 如果初步判断为有嫌疑（vote_ban = True），且存活时间超过3年，则改为合法用户
            if initial_vote_ban and survival_time is not None and survival_time > 1576800:  # 3年 = 1576800分钟
                survival_time_days = survival_time / (24 * 60)
                survival_time_years = survival_time_days / 365
                return json.dumps({
                    "vote_ban": False,
                    "reason": f"Initially considered suspicious, but survival time ({survival_time_years:.2f} years) exceeds 3 years, indicating legitimate long-term user. Original reason: {initial_reason}"
                })
            else:
                return json.dumps(result)
                
        except Exception as e:
            # 如果所有重试都失败，使用保守判定（默认通过）
            print(f"All retries failed in get_legit_vote: {e}")
            return json.dumps({"vote_ban": False, "reason": "Max retries exceeded - default pass"})
