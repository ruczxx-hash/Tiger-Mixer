import random

def generate_good_sequence(n, probability_of_zero=0.7,probability_of_zero_1 = 0.3):
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
    if random.random() < 0.5:
        for i in range(n):
        # 生成第二个数，按照给定的概率可能为0
            second_number = 0 if random.random() < probability_of_zero_1 else random.randint(1, 6)
            sequence.append(f"({i},{second_number})")
    else:
        for i in range(n):
                # 生成第二个数，按照给定的概率可能为0
                second_number = 0 if random.random() < probability_of_zero else random.randint(1, 20)
                sequence.append(f"({i},{second_number})")
        
    # 将所有元组连接成一个紧密相连的字符串
    return "".join(sequence)


def generate_bad_sequence(n, group_size=10, probability_of_zero_group=0.3, probability_of_zero_value=0.2):
    """
    生成一个包含 n 个元组的序列，每个元组的第一个数递增，第二个数为随机数
    每10个元组为一组，每组有一定概率所有第二个数为0。
    
    参数:
    - n: 总共生成的元组数量
    - group_size: 每组元组的数量，默认10
    - probability_of_zero_group: 每组元组第二个数都为0的概率（0到1之间）
    - probability_of_zero_value: 每个元组第二个数为0的概率（仅在没有整组为0时有效）
    
    返回:
    - 字符串：格式为 "(i, random_number)"，各个元组之间没有逗号和空格，紧密相连
    """
    sequence = []
    
    for i in range(n):
        # 每 group_size 个元组一组，判断是否该组的第二个数都为0
        if (i // group_size) > 0 and random.random() < probability_of_zero_group:
            # 如果是该组的第二个数都为0
            second_number = 0
        else:
            # 否则，根据给定的概率生成第二个数
            second_number = 0 if random.random() < probability_of_zero_value else random.randint(1, 30)
        
        sequence.append(f"({i},{second_number})")
    
    # 将所有元组连接成一个紧密相连的字符串
    return "".join(sequence)
