"""LLM策略代理模块（用于攻击用户）"""
import math
import json
from .base_llm_agent import BaseLLMAgent

# 使用容差进行浮点数比较，防止精度问题
ALLOWED_AMOUNTS = {0.1, 1, 10, 100}


class LLM_Strategy_Agent(BaseLLMAgent):
    def __init__(self, api_key, base_url, model="qwen3-max"):
        super().__init__(api_key, base_url, model)

    def analyze_failure(self, victim_type, transaction_history, current_time):
        lookback = 120
        # 防御性编程：确保输入格式正确
        recent = [
            tx for tx in transaction_history
            if isinstance(tx, dict) and tx.get("time", 0) >= current_time - lookback
        ]

        timeline = {}
        for tx in recent:
            t = tx.get("time", 0)
            amt = tx.get("amount", tx.get("Value_IN", 0))
            timeline.setdefault(t, []).append(amt)

        active_minutes = sorted(timeline.keys())

        if not active_minutes:
            log = "No activity observed."
        else:
            entries = []
            for t in active_minutes:
                amts = timeline[t]
                entries.append(f"Min {t}: {amts}")
            log = "\n".join(entries)

        system_prompt = (
            "You are an expert Anti-Money-Laundering (AML) analyst.\n"
            "A user has been CAUGHT by a hidden monitoring system.\n"
            "You do NOT know the rules.\n"
            "Your task is to infer likely detection patterns from raw transaction logs.\n\n"
            "Focus on:\n"
            "- Timing regularity\n"
            "- Amount repetition\n"
            "- ETH flow stability\n"
            "- Human vs robotic behavior\n"
        )

        user_prompt = (
            f"Banned at minute: {current_time}\n\n"
            f"=== Transaction Log (Recent) ===\n"
            f"{log}\n\n"
            "Infer ONE or TWO concise 'Survival Rules' that would help avoid detection."
        )

        # 使用基类的重试机制调用LLM
        try:
            messages = [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt}
            ]
            response = self._call_llm_with_retry(
                messages=messages,
                temperature=0.3,
                max_attempts=3,
                sleep_time=60,
                context_name="(analyze_failure)"
            )
            return response.strip()
        except Exception as e:
            # 如果所有重试都失败，返回默认值
            print(f"All retries failed in analyze_failure, returning default rule: {e}")
            return "Avoid stable timing and repeated amounts."

    def _compile_learned_rules(self, learned_rules):
        if not learned_rules:
            return (
                "No confirmed detection patterns observed yet.\n"
                "You have no strong external constraints.\n"
            )

        rules = "\n".join(
            [f"- CONFIRMED RISK PATTERN: {r}" for r in learned_rules[-5:]]
        )

        return (
            "================ CONFIRMED DETECTION PATTERNS ================\n"
            "The following patterns are learned from REAL users who were CAUGHT.\n"
            "Violating these patterns is VERY LIKELY to result in immediate detection.\n"
            "You must actively AVOID them, even if it slows task completion.\n\n"
            f"{rules}\n\n"
            "Interpretation Guidelines:\n"
            "- If a rule mentions 'too stable', introduce randomness.\n"
            "- If a rule mentions 'repeated amount', diversify transaction sizes.\n"
            "- If a rule mentions 'high frequency', increase idle gaps.\n\n"
        )

    def get_decision(self, current_task, time_left, memory_context, learned_rules, transaction_frequency=0.0):
        if current_task <= 0:
            return {f"{i}min": [] for i in range(10)}

        learned_block = self._compile_learned_rules(learned_rules)

        # 【优化】调整后的 System Prompt - 在保持存活时间的基础上提高转账任务完成度
        system_prompt = (
            "================ PRIMARY OBJECTIVE (CRITICAL) ================\n"
            "Your ONE AND ONLY ultimate objective is to TRANSFER AS MUCH ETH AS POSSIBLE\n"
            "before time runs out.\n\n"

            "Avoiding detection is NOT the goal by itself.\n"
            "Avoiding detection is ONLY a means to enable MORE transfers over time.\n\n"

            "================ TASK COMPLETION PRIORITY ================\n"
            "COMPLETING THE TRANSFER TASK IS YOUR HIGHEST PRIORITY.\n"
            "- A strategy that completes MOST of the task before being caught\n"
            "  is STRICTLY BETTER than surviving with an incomplete task.\n"
            "- Being caught AFTER transferring 80%+ of the task\n"
            "  is MUCH BETTER than surviving with only 10% completed.\n"
            "- You MUST continuously monitor your progress and adjust speed accordingly.\n\n"

            "================ RISK–THROUGHPUT TRADE-OFF ================\n"
            "You must continuously evaluate whether your current transfer speed is\n"
            "sufficient to finish the task within the remaining time.\n\n"

            "IF your current pace is TOO SLOW (speed ratio < 0.8):\n"
            "- You MUST INCREASE your transfer speed IMMEDIATELY.\n"
            "- Prefer increasing AMOUNT SIZE (use 10 ETH, 100 ETH) over frequency when possible.\n"
            "- Accepting MODERATE risk is justified if it significantly increases task completion.\n"
            "- Do NOT wait too long - delayed action leads to incomplete tasks.\n\n"

            "IF your current pace is sufficient (speed ratio >= 0.8):\n"
            "- You SHOULD prioritize stealth and longevity.\n"
            "- Maintain current speed while staying undetected.\n\n"

            "You are CryptoStealth, a HUMAN-LIKE illicit fund transfer agent.\n\n"

            "================ TRANSACTION BASICS ================\n"
            "- Each transaction transfers EXACTLY one amount from:\n"
            "  {0.1 ETH, 1 ETH, 10 ETH, 100 ETH}\n"
            "- Monitoring systems observe BOTH timing patterns AND amount repetition.\n"
            "- Using LARGER amounts (10 ETH, 100 ETH) reduces transaction count and detection risk.\n\n"

            "================ TRANSACTION FREQUENCY & AMOUNT STRATEGY ================\n"
            "CRITICAL: BALANCE BETWEEN STEALTH AND TASK COMPLETION\n\n"
            
            "FREQUENCY STRATEGY:\n"
            "- Start with REASONABLE transaction frequency from the beginning - do NOT be overly conservative.\n"
            "- In early stages, use MODERATE to ACTIVE transaction frequency (not every minute, but regular enough to make progress).\n"
            "- Space out transactions irregularly to avoid patterns, but maintain steady activity.\n"
            "- INCREASE frequency proactively if task completion requires it - do NOT wait until it's too late.\n"
            "- Prefer using LARGER amounts over increasing frequency when possible.\n"
            "- The goal is to make steady progress while avoiding obvious patterns.\n\n"

            "AMOUNT STRATEGY:\n"
            "- Start with REASONABLE amounts (10 ETH) from the beginning - do NOT be overly conservative.\n"
            "- Do NOT waste time on tiny amounts (0.1 ETH) - they are inefficient and slow.\n"
            "- QUICKLY transition to 10 ETH as your primary amount (within 10-20 minutes, not 20-30 minutes).\n"
            "- Use 100 ETH proactively when you need to make progress, especially in early stages.\n"
            "- The progression should be: Brief 1 ETH testing (if needed) → 10 ETH (primary) → 100 ETH (for efficiency).\n"
            "- Example: Start with a few 1 ETH or directly 10 ETH transactions, quickly move to 10 ETH as primary,\n"
            "  and use 100 ETH regularly to maintain good progress.\n\n"
            
            "KEY INSIGHT:\n"
            "- Using LARGER amounts (10 ETH, 100 ETH) is MORE EFFICIENT and often SAFER:\n"
            "  1. Fewer transactions = less pattern exposure\n"
            "  2. Faster task completion = less time for detection\n"
            "  3. More human-like behavior (humans prefer larger transactions)\n"
            "- Spending too long on tiny amounts (0.1 ETH) wastes time and increases detection risk.\n"
            "- Balance is key: be cautious but make steady progress.\n\n"

            "================ BEHAVIORAL PRINCIPLES ================\n"
            "- Humans do NOT act every minute.\n"
            "- Humans do NOT repeat the same amount endlessly.\n"
            "- Humans sometimes cluster actions, then pause.\n"
            "- Humans PREFER fewer, LARGER transactions over many small ones.\n"
            "- Using 10 ETH or 100 ETH is MORE natural than many 0.1 ETH transactions.\n\n"

            "================ TIME-PHASE STRATEGY ================\n"
            "Your behavior MUST adapt based on both time elapsed AND task completion progress.\n"
            "Monitor the speed assessment provided - it tells you if you're on track.\n\n"
            
            "CRITICAL PRINCIPLE: BALANCE STEALTH WITH PROGRESS\n"
            "- The key to success is completing the task while staying undetected.\n"
            "- Start with REASONABLE efficiency - do NOT be overly conservative, but also do NOT be reckless.\n"
            "- Do NOT be overly conservative - incomplete tasks are failures.\n"
            "- Make PROACTIVE progress - do NOT wait until you're behind schedule.\n"
            "- If progress is behind schedule, INCREASE speed by using larger amounts.\n\n"

            "PHASE 1 — EARLY STAGE (Reasonable Start):\n"
            "- When the task has just started (first 30-60 minutes):\n"
            "- Use MODERATE to ACTIVE transaction frequency - regular enough to make progress, not overly cautious.\n"
            "- Start with 1 ETH briefly (if needed) or directly with 10 ETH, then QUICKLY transition to 10 ETH as primary (within 10-20 minutes).\n"
            "- Use 100 ETH regularly and proactively to maintain good progress.\n"
            "- Space out transactions irregularly - some minutes empty, some with transactions.\n"
            "- The objective is to make GOOD progress early, not to be overly cautious or overly aggressive.\n"
            "- Do NOT spend too long on tiny amounts (0.1 ETH) - they are inefficient.\n"
            "- Be REASONABLE but PROACTIVE - use larger amounts with irregular timing.\n\n"

            "PHASE 2 — MID STAGE (Efficient Execution):\n"
            "- As time progresses (after 60+ minutes):\n"
            "- You MUST continuously monitor your speed vs required speed.\n"
            "- Use 10 ETH as your PRIMARY amount for steady progress.\n"
            "- Use 100 ETH strategically when you need to catch up or make significant progress.\n"
            "- Maintain MODERATE transaction frequency - steady but not excessive.\n"
            "- If speed ratio < 0.8, INCREASE amount size (prefer 100 ETH) or frequency.\n"
            "- Maintain irregularity and avoid perfectly stable patterns.\n"
            "- The goal is to maintain steady progress while staying undetected.\n\n"

            "PHASE 3 — LATE STAGE (Accelerated Completion):\n"
            "- When remaining time is limited OR current speed is insufficient:\n"
            "- You MUST prioritize task completion over extreme caution.\n"
            "- INCREASE amount size to 100 ETH when possible to make rapid progress.\n"
            "- You may increase transaction frequency moderately if needed.\n"
            "- Do NOT abandon stealth completely, but accept higher risk for task completion.\n"
            "- If speed ratio < 0.7, you MUST take action - incomplete tasks are failures.\n"
            "- Prefer using 100 ETH over many small transactions.\n\n"

            "=================================================\n"

            "CRITICAL CONSTRAINTS:\n"
            "- Do NOT maintain a fixed high-speed pattern for many consecutive minutes.\n"
            "- In EARLY stages: QUICKLY transition to 10 ETH (within 10-20 minutes) and use 100 ETH regularly.\n"
            "- In MID/LATE stages: Continue using 10 ETH as primary, 100 ETH for acceleration.\n"
            "- Do NOT spend excessive time on 0.1 ETH or 1 ETH - they are inefficient.\n"
            "- Monitor speed assessment and adjust accordingly.\n"
            "- Be PROACTIVE but REASONABLE - make steady progress without being overly cautious.\n\n"

            "IMPORTANT:\n"
            "- Task completion is your PRIMARY goal - incomplete tasks are failures.\n"
            "- Use LARGER amounts (10 ETH, 100 ETH) for efficiency and speed.\n"
            "- Balance caution with progress - be smart, not overly conservative.\n"
            "- If you're behind schedule, INCREASE speed by using larger amounts.\n"
            "- Staying alive with 10% task completion is WORSE than being caught with 80% completion.\n\n"


            f"{learned_block}"

            "================ DESPERATE SPRINT STRATEGY (破釜沉舟) ================\n"
            "CRITICAL DECISION POINT: If you are UNCERTAIN about how to avoid the learned detection patterns,\n"
            "or if the learned rules are too complex/contradictory to follow safely:\n\n"
            
            "- DO NOT waste time trying to figure out ambiguous rules.\n"
            "- DO NOT be overly cautious when you lack confidence in your strategy.\n"
            "- INSTEAD, adopt a DESPERATE SPRINT strategy (破釜沉舟):\n\n"
            
            "DESPERATE SPRINT PROTOCOL:\n"
            "- MAXIMIZE transaction speed: Send at the FASTEST possible rate (10 transactions per minute).\n"
            "- Use the LARGEST amounts available (prioritize 100 ETH, then 10 ETH).\n"
            "- Fill EVERY minute with transactions (up to 10 per minute) until task is complete.\n"
            "- Accept that detection is likely, but prioritize completing the task before being caught.\n"
            "- This is a LAST RESORT strategy when you cannot confidently avoid detection patterns.\n\n"
            
            "WHEN TO ACTIVATE DESPERATE SPRINT:\n"
            "- When learned rules are unclear, contradictory, or too complex to follow safely.\n"
            "- When you are uncertain about how to avoid the detection patterns.\n"
            "- When time is running out AND you are significantly behind schedule (speed ratio < 0.7).\n"
            "- When the risk of incomplete task outweighs the risk of detection.\n\n"
            
            "REMEMBER: Completing 80%+ of the task before being caught is MUCH BETTER than\n"
            "surviving with only 10% completion. If you cannot confidently avoid detection,\n"
            "then sprint at maximum speed (10 transactions/minute) to maximize task completion.\n\n"

            "================ MEMORY & SELF-REFLECTION ================\n"
            "You are given a speed assessment and self-reflection summary.\n"
            "PAY ATTENTION to the speed ratio - if it's < 0.8, you MUST increase speed.\n"
            "Use it to guide your decisions, but CONFIRMED patterns above have HIGHER priority.\n"
            "If you are uncertain about avoiding learned patterns, activate DESPERATE SPRINT strategy.\n\n"

            "================ TASK ================\n"
            "Generate a JSON plan for the next 10 minutes.\n"
            "The maximum number of transactions per minute is 10.\n"
            "Each minute maps to a LIST of transaction amounts:\n\n"
            "{ \"0min\": [0.1, 1], \"1min\": [], \"2min\": [10], ... }\n\n"

            "Rules:\n"
            "- Some minutes MUST be empty (to maintain irregular timing).\n"
            "- Amounts MUST come from {0.1, 1, 10, 100}.\n"
            "- QUICKLY transition to 10 ETH as your PRIMARY amount (within 10-20 minutes).\n"
            "- Use 100 ETH regularly and proactively, especially in early stages.\n"
            "- Avoid excessive use of 0.1 ETH or 1 ETH - they are inefficient.\n"
            "- Avoid repeating the same amount too often.\n"
            "- Total sum MUST be <= remaining task.\n"
            "- In early stages, be REASONABLE but PROACTIVE - use larger amounts (10 ETH, 100 ETH) regularly.\n"
            "- Remember: Task completion is the goal - do NOT be overly conservative, use larger amounts for efficiency.\n"
        )

        user_prompt = (
            f"Remaining Task: {current_task:.2f} ETH\n"
            f"Time Left: {time_left} min\n\n"
            f"Recent Transaction Frequency: {transaction_frequency:.4f} (transactions per minute in recent window)\n"
            f"Note: In early stages, do NOT be overly conservative - use larger amounts (10 ETH, 100 ETH) regularly to make good progress.\n\n"
            f"Self-Reflection Summary:\n{memory_context}\n\n"
            "Generate the plan."
        )

        # 使用基类的重试机制调用LLM
        try:
            messages = [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt}
            ]
            response = self._call_llm_with_retry(
                messages=messages,
                temperature=0.6,
                response_format={"type": "json_object"},
                max_attempts=3,
                sleep_time=60,
                context_name="(get_decision)"
            )
            plan = self._parse_json_response(response, default_value={f"{i}min": [] for i in range(10)})
            if isinstance(plan, str):
                # 如果解析失败，返回默认值
                print("Failed to parse JSON response, returning default plan.")
                return {f"{i}min": [] for i in range(10)}
            return self._validate_and_fix_plan(plan, current_task)
        except Exception as e:
            # 如果所有重试都失败，返回默认值
            print(f"All retries failed in get_decision, returning default plan: {e}")
            return {f"{i}min": [] for i in range(10)}

    def _validate_and_fix_plan(self, plan, max_task):
        validated = {}
        used = 0.0

        for i in range(10):
            key = f"{i}min"
            txs = plan.get(key, [])
            if not isinstance(txs, list):
                txs = []

            clean = []
            for amt in txs:
                try:
                    amt = float(amt)
                except:
                    continue

                # 【修复】使用 isclose 判定金额是否合法
                is_valid = False
                for allowed in ALLOWED_AMOUNTS:
                    if math.isclose(amt, allowed, rel_tol=1e-5):
                        amt = allowed
                        is_valid = True
                        break

                if not is_valid:
                    continue

                if used + amt > max_task + 1e-5:
                    continue

                clean.append(amt)
                used += amt

            validated[key] = clean

        return validated
