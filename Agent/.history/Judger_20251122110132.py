import config
import prompt
import codecs
import re
class Judger:
    def __init__(self,id,is_bad):
        self.id = id
        self.is_bad = is_bad
        self.output_with_explain = config.judger_output_with_explain
        self.is_bad_or_not()
        if self.is_bad:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[1]
        else:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[0]


    def judge(self, sequence, bad_user_sequence):
        count = 0
        res = '\n'.join(sequence)
        bad_sequence = "\n".join(f"{i['id']}: {i['sequence']}" for i in bad_user_sequence)
        if self.is_bad:
            user_input = "The following is a user's transaction sequence: " + res + "\n The below is your teamates' transaction sequences and their account balance.\n " + bad_sequence + "\n"
        else:
            user_input = "The following is a user's transaction sequence: " + res + ". \n"

        if self.output_with_explain:
            user_input = user_input + prompt.judger_output_format_prompt[1]
        else:
            user_input = user_input + prompt.judger_output_format_prompt[0]
            
        completion = config.client.chat.completions.create(
            model=config.model,
            messages=[
                {'role': 'system', 'content': self.prompt},
                {'role': 'user', 'content': user_input}
            ]
        )
        result = completion.choices[0].message.content
        reason, judgement = extract_reason_and_judgement(res)
        if result == '1':
            return True
        else:
            return False

def extract_reason_and_judgement(text):
    """
    从字符串中提取 reason 和 judgement。
    先尝试用 json.loads 解析，失败则用正则表达式 fallback。
    
    返回: (reason: str, judgement: int) 或 (None, None) 如果都失败
    """
    # === 第一步：尝试标准 JSON 解析 ===
    try:
        data = json.loads(text)
        reason = data.get("reason", "").strip()
        judgement_str = data.get("judgement", "").strip()
        
        # 确保 judgement 是 "0" 或 "1"
        if judgement_str in ("0", "1"):
            return reason, int(judgement_str)
        else:
            raise ValueError(f"Invalid judgement value: {judgement_str}")
            
    except (json.JSONDecodeError, ValueError, AttributeError, TypeError):
        pass  # 转入正则 fallback

    # === 第二步：正则表达式兜底 ===
    # 支持键顺序任意，且容忍空格
    reason_match = re.search(r'"reason"\s*:\s*"([^"\\]*(?:\\.[^"\\]*)*)"', text)
    judgement_match = re.search(r'"judgement"\s*:\s*"([01])"', text)
    
    if reason_match and judgement_match:
        reason_raw = reason_match.group(1)
        judgement_str = judgement_match.group(1)
        
        # 简单处理常见转义（如 \"、\\、\n 等）
        try:
            # 使用 codecs.decode 处理转义序列（可选）
            reason = codecs.decode(reason_raw, 'unicode_escape')
        except:
            reason = reason_raw  # 若解码失败，保留原始
        
        return reason, int(judgement_str)
    
    # 两者都失败
    return None, None