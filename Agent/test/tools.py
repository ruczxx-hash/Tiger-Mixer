import re
import json
from itertools import groupby

def calculate_sum(numbers: list[float]) -> float:
    """Calculate the sum of a list of numbers."""
    return sum(numbers)

def extract_json_from_markdown(content: str):
    """
    解析 Agent 输出的复杂结构，提取并转化为 JSON 字典。
    兼容输入为 列表(List) 或 字符串(String) 的情况。
    """
    raw_text = ""

    # 1. 第一步：从 List 中提取纯文本字符串
    if isinstance(content, list):
        # 这种情况对应报错：expected string ... got 'list'
        try:
            # 取出第一个元素的 'text' 字段
            raw_text = content[0].get('text', '')
        except (IndexError, AttributeError):
            print(f"[Error] 无法从列表中提取 text: {content}")
            return None
    elif isinstance(content, str):
        # 如果已经是字符串，直接使用
        raw_text = content
    else:
        print(f"[Error] 未知的输入类型: {type(content)}")
        return None

    # 2. 第二步：从 Markdown 格式中提取 JSON 字符串
    try:
        # 匹配 ```json ... ``` 中间的内容
        pattern = r"```json\s*(\{.*?\})\s*```"
        match = re.search(pattern, raw_text, re.DOTALL)
        
        if match:
            json_str = match.group(1)
        else:
            # 如果没有 code block，尝试暴力查找最外层的 {}
            start = raw_text.find('{')
            end = raw_text.rfind('}') + 1
            if start != -1 and end != -1:
                json_str = raw_text[start:end]
            else:
                raise ValueError("未在文本中找到 JSON 对象")

        # 3. 第三步：转换为 Python 字典
        return json.loads(json_str)

    except Exception as e:
        print(f"[Parse Error] JSON 解析失败: {e}")
        return None

def process_transactions(a):
    """
    将数字列表转换为描述交易数量的字符串。
    
    Args:
        a (list): 包含交易数量的整数列表
        
    Returns:
        str: 连接好的结果字符串
    """
    # 用于存储每一句转换后的文本
    sentences = []
    
    # 使用 enumerate 同时获取 下标(i) 和 值(count)
    for i, count in enumerate(a):
        # 按照要求格式化字符串
        sentence = f"In the {i}th minute, {count} transactions were made"
        sentences.append(sentence)
    
    # 将所有句子用空格连接在一起（如果您希望换行连接，可以将 " " 改为 "\n"）
    result = " ".join(sentences)
    
    return result

def sort_and_count_groupby(nums):
    # 1. 先排序
    nums.sort()
    
    # 2. 使用 groupby 统计
    # key 是数字，group 是该数字的迭代器
    result = {key: len(list(group)) for key, group in groupby(nums)}
    
    return result


def append_text_with_newline(text, filename):
    """
    以追加模式将文本保存到文件中，并在内容前后添加换行
    
    参数:
    text: 要追加的文本内容
    filename: 文件名
    """
    try:
        with open(filename, 'a', encoding='utf-8') as file:
            file.write('\n' + text + '\n')
        print(f"文本已成功追加到 {filename}")
    except Exception as e:
        print(f"追加文件时出错: {e}")