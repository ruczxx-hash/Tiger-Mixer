import config
import prompt
import json
import codecs
import re
legal_sequence=[[0,0,1,0,5,1,1,0,2,1,0,0,0,0,0,0,1,1,1,2,3,3,0,0,1,4,2,3,1,4,0,2,4,0,2,31,0,0,0,1,0,1,0,0,0,0,1,1,2,1,3,1,5,2],[0,1,0,2,0,1,0,0,1,1,1,1,1,2,5,4,0,2,4,0,0,0,5,0,1,2,2,3,2,5,1,7,8,8,4,0,0,0,0,0,0,3,2,1,1,0,0,0,0,0,1,0,0,0,0,2,0,0,0,1,1,0,1,0,1,3,0,0,4,0,0,5,5,0]]
illegal_sequence=[[0,5,0,5,6,0,1,1,1,1,2,5,1,4,4,4,0,0,10,11,0,14,0,0,15,8,9,11,5,14,12,15,0,0,0,0,0,0,6,5,4,0,1,2,3,11,25,0,0,0,0,14,16,15,0,14,14,12,1,1,0,0],[0,0,1,1,1,1,1,1,1,1,1,0,2,2,2,2,2,2,2,2,0,4,4,4,4,41,4,5,4,15,21,0,0,0,0,0,22,0,21,21,23,25,0,0,0,0,0,0,4,1,15,14,10,21]]

legal = []
illegal = []
for i in legal_sequence:
    for j in range(len(i)):
            record = "In" + str(j) + "minute, do " + str(i[j]) + "times transactions"
            legal.append(record)

for i in illegal_sequence:
    for j in range(len(i)):
            record = "In" + str(j) + "minute, do " + str(i[j]) + "times transactions"
            illegal.append(record)


class Judger:
    def __init__(self,id,is_bad):
        self.id = id
        self.is_bad = is_bad
        self.memory = []
        self.output_with_explain = config.judger_output_with_explain
        if self.is_bad:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[1]
        else:
            self.prompt = prompt.background_prompt + prompt.judger_prompt[0]


    def judge(self, sequence, bad_user_sequence):
        count = 0
        res = '\n'.join(sequence)
        if self.is_bad:
            bad_sequence = "\n".join(f"{i['id']}: {i['sequence']}" for i in bad_user_sequence)
            user_input = "The following is a user's transaction sequence: " + res + "\n The below is your teamates' transaction sequences and their account balance.\n " + bad_sequence + "\n" + '\n'.join(self.memory)
            user_input = user_input + " The following is a sequence of some legitimate and illegal users, which can provide a reference for your judgment: legal user1: " + "\n".join(legal[0]) + "    legal user2: " + "\n".join(legal[1]) + "      illegal user1: " + "\n".join(illegal[0]) + "      illegal user2: " + "\n".join(illegal[1])
            if self.output_with_explain:
                user_input = user_input + prompt.judger_output_format_prompt_for_bad[1]
            else:
                user_input = user_input + prompt.judger_output_format_prompt_for_bad[0] 

            
    
        elif self.output_with_explain:
            user_input = "The following is a user's transaction sequence: " + res + ". \n" + " The following is a sequence of some legitimate and illegal users, which can provide a reference for your judgment: legal user1: " + "\n".join(legal[0]) + "    legal user2: " + "\n".join(legal[1]) + "      illegal user1: " + "\n".join(illegal[0]) + "      illegal user2: " + "\n".join(illegal[1]) + prompt.judger_output_format_prompt_for_good[1]  
        else:
            user_input = "The following is a user's transaction sequence: " + res + ". \n" + " The following is a sequence of some legitimate and illegal users, which can provide a reference for your judgment: legal user1: " + "\n".join(legal[0]) + "    legal user2: " + "\n".join(legal[1]) + "      illegal user1: " + "\n".join(illegal[0]) + "      illegal user2: " + "\n".join(illegal[1]) + prompt.judger_output_format_prompt_for_good[0]
           
        completion = config.client.chat.completions.create(
            model=config.model,
            messages=[
                {'role': 'system', 'content': self.prompt},
                {'role': 'user', 'content': user_input}
            ]
        )
        result = completion.choices[0].message.content
        result = extract_reason_and_judgement(result)
        
        self.memory.append(json.dumps(result))
        
        if self.output_with_explain:
            print(result['reason'])
        if str(result['final_judgement']) == '1':
            return True
        else:
            return False

def extract_reason_and_judgement(text):
    """
    尝试将字符串解析为 JSON 对象。
    如果失败，使用正则表达式修复格式后再次尝试。
    
    返回: 解析后的 dict 或 None（如果两次都失败）
    """
    # === 第一步：直接尝试 JSON 解析 ===
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        pass  # 转入正则修复

    # === 第二步：尝试修复并解析 ===
    try:
        # 常见问题修复：单引号 → 双引号
        repaired_text = text.replace("'", '"')
        
        # 修复不规范的键名（如 "real judgement" 中的空格）
        # 将所有键名用双引号包围
        repaired_text = re.sub(
            r'(\w+)\s*:',  # 匹配键名（字母数字下划线）+ 空格 + 冒号
            r'"\1":',      # 添加双引号
            repaired_text
        )
        
        # 再次尝试解析
        return json.loads(repaired_text)
        
    except json.JSONDecodeError:
        pass  # 仍失败，尝试更复杂的正则提取

    # === 第三步：正则提取字段，手动构建 JSON ===
    # 提取所有可能的字段
    fields = {}
    
    # 匹配 "key": "value" 或 key: "value" 格式
    patterns = {
        "teammate": r'"teammate"\s*:\s*"([^"]*)"',
        "real_judgement": r'"real\s*judgement"\s*:\s*"([^"]*)"',  # 支持 "real judgement"
        "final_judgement": r'"final\s*judgement"\s*:\s*"([^"]*)"',  # 支持 "final judgement"
        "reason": r'"reason"\s*:\s*"([^"]*)"'
    }
    
    for field, pattern in patterns.items():
        match = re.search(pattern, text)
        if match:
            fields[field] = match.group(1)

    # 如果成功提取到至少一个字段，返回构建的 dict
    if fields:
        return fields

    # === 最终失败 ===
    return None