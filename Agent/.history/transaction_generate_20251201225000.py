import random

def generate_sequence(n):
    sequence = []
    for _ in range(n):
        if random.random() > 0.5  :  # 0.2 的概率为 0
            sequence.append(0)
        else:
            sequence.append(random.randint(1, 20))  # 否则为 1 到 100 的随机整数
    return sequence

# 示例：生成一个长度为 10 的序列
n = 90
sequence = generate_sequence(n)
print(sequence)
