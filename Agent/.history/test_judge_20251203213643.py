import random
import requests
import csv
def generate_user_type_sequence(n, probability_of_zero=0.7,probability_of_zero_1 = 0.4):
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
            second_number = 0 if random.random() < probability_of_zero_1 else random.randint(1, 10)
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
        if i % group_size == 0 and random.random() < probability_of_zero_group:
            # 如果是该组的第二个数都为0
            for j in range(group_size):
                sequence.append(f"({i + j},{0})")
        else:
            # 否则，根据给定的概率生成第二个数
            second_number = 0 if random.random() < probability_of_zero_value else random.randint(1, 30)
        
        sequence.append(f"({i},{second_number})")
    
    # 将所有元组连接成一个紧密相连的字符串
    return "".join(sequence)


count = 0
data=[]
while count < 10:
    if random.random() < 0.7:
        sequence_to_judge = generate_bad_sequence(random.randint(30, 100))
        bad = True
    else:
        sequence_to_judge = generate_user_type_sequence(random.randint(30, 100))
        bad = False
        
    url = "http://127.0.0.1:8001/judge"
    payload1 = {"id": "1", "record": sequence_to_judge}
    payload2 = {"id": "2", "record": sequence_to_judge}
    payload3 = {"id": "3", "record": sequence_to_judge}
    teammate = False
    if random.random() < 0.2:
        teammate = True
        payload4 = {"id": str(random.randint(4, 10)), "record": sequence_to_judge}
        response = requests.post("http://127.0.0.1:8001/add_baduser_sequence", json=payload4, timeout=50).json() 

    try:
        response_1 = requests.post(url, json=payload1, timeout=50).json()  # 10秒超时
        response_2 = requests.post(url, json=payload2, timeout=50).json()  # 10秒超时
        response_3 = requests.post(url, json=payload3, timeout=50).json()  # 10秒超时
        data.append({'a': response_1['message'], 'b': response_2['message'], 'c': response_3['message'], 'user_type': bad, 'teammate': teammate})
    except requests.exceptions.Timeout:
        print("请求超时")
        continue
    except requests.exceptions.RequestException as e:
        print(f"请求出错: {e}")
        continue
    count += 1


def write_judge_csv():
    # 模拟的数据，a, b, c, user_type, teammate
    
    # 打开 CSV 文件进行写入
    with open('judge.csv', 'w', newline='') as file:
        writer = csv.writer(file)
        
        # 写入表头
        writer.writerow(['user_type', 'judger1', 'judger2', 'judger3', 'teammate'])
        
        # 写入每个用户的判断信息
        for entry in data:
            # 根据 user_type 和 teammate 来决定写入内容
            user_type_status = '合法用户' if entry['user_type'] else '非法用户'
            a_status = '同意' if entry['a'] == "1" else '不同意'
            b_status = '同意' if entry['b'] == "1" else '不同意'
            c_status = '同意' if entry['c'] == "1" else '不同意'
            teammate_status = '队友' if entry['teammate'] else '陌生人'
            
            # 写入一行数据
            writer.writerow([user_type_status, a_status, b_status, c_status, teammate_status])

write_judge_csv()
print("Over ")