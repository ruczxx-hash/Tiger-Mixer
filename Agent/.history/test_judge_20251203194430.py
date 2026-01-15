import random

def generate_good_sequence(n, probability_of_zero=0.7):
    """
    生成一个包含 n 个元组的序列，每个元组的第一个数递增，第二个数为随机数
    第二个数有一定概率为0，返回一个没有逗号和空格的紧密相连的字符串。

    参数:
    - n: 序列中元组的数量
    - probability_of_zero: 第二个数为0的概率（0到1之间）
    
    返回:
    - 字符串：格式为 "(i, random_number)"，各个元组之间没有逗号和空格，紧密相连
    """
    sequence = []
    
    for i in range(n):
        # 生成第二个数，按照给定的概率可能为0
        second_number = 0 if random.random() < probability_of_zero else random.randint(1, 10)
        sequence.append(f"({i},{second_number})")
    
    # 将所有元组连接成一个紧密相连的字符串
    return "".join(sequence)

def generate_bad_sequence(n, probability_of_zero=0.4):
    """
    生成一个包含 n 个元组的序列，每个元组的第一个数递增，第二个数为随机数
    第二个数有一定概率为0，返回一个没有逗号和空格的紧密相连的字符串。

    参数:
    - n: 序列中元组的数量
    - probability_of_zero: 第二个数为0的概率（0到1之间）
    
    返回:
    - 字符串：格式为 "(i, random_number)"，各个元组之间没有逗号和空格，紧密相连
    """
    sequence = []
    
    for i in range(n):
        # 生成第二个数，按照给定的概率可能为0
        second_number = 0 if random.random() < probability_of_zero else random.randint(1, 10)
        sequence.append(f"({i},{second_number})")
    
    # 将所有元组连接成一个紧密相连的字符串

