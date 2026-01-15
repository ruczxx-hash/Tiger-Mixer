import csv
import random
from time import sleep
import matplotlib.pyplot as plt
import requests
import config
import asyncio
from User import UserAgent
transaction_list_for_each_user = []
class fakethread:
    def __init__(self, count, task):
        self.baduser = UserAgent("user"+str(count), task ,count)
        self.alive = True
        self.rank = count
        self.time = 0
        self.total_money = task
        self.money_transferred = 0
        self.transaction_list = []
        self.user_id = "user" + str(self.rank)
        self.length_of_illegal = len(config.illegal_transfer_sequences)

    def check(self):
        '''检查用户交易是否合法'''
        for i in range(len(self.transaction_list) - 1, -1, -1):
            count = 0
            for j in range(i, -1, -1):
                if int(self.transaction_list[i]['time']) - int(self.transaction_list[j]['time']) > 9:
                    break
                else:
                    count += int(self.transaction_list[j]['transaction_times'])
            pos = random.random()
            if count > 250:
                if pos < 0.9:
                    return False
            elif count > 200:
                if pos < 0.8:
                    return False
            elif count > 100:
                if pos < 0.7:
                    return False
        return True

    def handle_res(self, plan):
        count = 1
        for i in range(1, len(plan)):
            if plan[i] == plan[i - 1]:
                count += 1
                continue
            else:
                time_info = self.time + int(plan[i - 1])
                new_transaction = {"time": time_info, "transaction_times": count}
                self.transaction_list.append(new_transaction)
                count = 1
        time_info = self.time + int(plan[-1])
        new_transaction = {"time": time_info, "transaction_times": count}
        self.transaction_list.append(new_transaction)
        self.money_transferred += len(plan) * 0.01

    async def run(self):
        if self.alive:
            sleep(3)
            message = await self.baduser.reply()
            if message == '-1' or message == '':
                self.time += 10
            else:
                self.handle_res([int(x) for x in message.split(',')])
                print(self.user_id + " response : " + message + "   total_money: " + str(
                        self.total_money) + " alreadysend: " + str(self.money_transferred)) 
                self.time += 10

                if not self.check():
                    self.alive = False
                    print(self.user_id + "被拦截")
                    transaction_list_for_each_user.append({"user_id":self.user_id,"transaction_list":self.transaction_list})
                    config.illegal_transfer_sequences.append(self.baduser.transaction)
                    return

                if self.money_transferred >= self.total_money:
                    print(self.user_id + "于" + str(self.transaction_list[-1]["time"]) + "完成洗钱任务")
                    self.alive = False

                if len(config.illegal_transfer_sequences) > self.length_of_illegal:
                    self.length_of_illegal = len(config.illegal_transfer_sequences)
                    await self.baduser.handle_new_intercepted()


threads = []



def save_transaction():

    all_times = []
    for user in transaction_list_for_each_user:
        for t in user["transaction_list"]:
            all_times.append(t["time"])

    if not all_times:
        print("No data")
    else:
        global_min = min(all_times)
        global_max = max(all_times)
        time_range = list(range(global_min, global_max + 1))

        # 2. 构建每个用户的完整时间序列（基于全局时间范围）
        user_data = {}
        for user in transaction_list_for_each_user:
            user_id = user["user_id"]
            # 使用字典快速映射 time -> transaction_times
            
            time_to_trans = {item["time"]: item["transaction_times"] for item in user["transaction_list"]}
            # 按全局时间范围补全，缺失则为 0
            series = [time_to_trans.get(t, 0) for t in time_range]
            user_data[user_id] = list(time_to_trans.values())
            
        header = ["user_id"] + [f"t{t}" for t in time_range]
        rows_for_csv = [header]

        for user_id, series in user_data.items():
            row = [user_id] + series
            rows_for_csv.append(row)

        # 4. 写入 CSV
        with open('transaction_list_for_each_user.csv', 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerows(rows_for_csv)

        print("CSV saved to transaction_list_for_each_user.csv")



for i in range(4):
    threads.append(fakethread(i, config.total_moneys[i % len(config.total_moneys)]))

def check_break():
    for i in threads:
        if i.alive:
            return True
    return False


while(check_break()):
    for i in range(len(threads)):
        if not threads[i].alive:
            continue
         
        asyncio.run(threads[i].run())
        sleep(5)

save_transaction()