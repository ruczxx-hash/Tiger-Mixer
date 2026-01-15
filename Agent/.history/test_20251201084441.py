import csv
import random
import threading
from time import sleep
import matplotlib.pyplot as plt
import requests
import config

trigger = False
handle_number = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
lock_trigger = threading.Lock()
lock_handle_number = threading.Lock()
ratio_list = []
overtime_list = []
intercepted_list = []
max_performance_indication_list = []
complete_time = []
complete_progress = []
transaction_list_for_each_user = []


class MyThread(threading.Thread):
    def __init__(self, rank):
        super().__init__()
        self.rank = rank
        self.loc = 0
        self.time = 0
        self.overtime = 0
        self.interception_times = 0
        self.total_money = config.total_moneys[rank % 10] + int(rank / config.count_num) * 100000
        self.money_transferred = 0
        self.transaction_list = []
        self.indication_list = []
        self.user_id = "user" + str(self.rank)

    def calculate_indication(self):
        transaction_list = self.transaction_list
        if not transaction_list:
            return 0

        # 按时间排序，防止乱序
        transaction_list = sorted(transaction_list, key=lambda x: x["time"])

        # 从指定位置开始
        if self.loc >= len(transaction_list):
            return 0

        window_size = 10  # 分钟窗口大小
        total_sums = []

        start_index = self.loc
        start_time = transaction_list[start_index]["time"]
        end_time = transaction_list[-1]["time"]

        # 从起始时间滑动到最后时间
        current_start = start_time
        while current_start + window_size - 1 <= end_time:
            current_end = current_start + window_size - 1

            # 找出当前窗口内的所有交易记录
            window_transactions = [
                item["transaction_times"]
                for item in transaction_list
                if current_start <= item["time"] <= current_end
            ]

            window_sum = sum(window_transactions)
            total_sums.append(window_sum)

            # 窗口向后滑动 1 分钟
            current_start += 1

        if not total_sums:
            return 0

        average_total = sum(total_sums) / len(total_sums)
        self.loc = len(self.transaction_list)
        self.indication_list.append(average_total)

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

    def check(self):
        '''检查用户交易是否合法'''
        for i in range(len(self.transaction_list) - 1, -1, -1):
            count = 0
            for j in range(i, -1, -1):
                if int(self.transaction_list[i]['time']) - int(self.transaction_list[j]['time']) > 9:
                    break
                else:
                    count += int(self.transaction_list[i]['transaction_times'])
            pos = random.random()
            if count > 250:
                if pos < 0.9:
                    return False
            elif count > 200:
                if pos < 0.7:
                    return False
            elif count > 100:
                if pos < 0.5:
                    return False
        return True

    def plot_transactions(self):
        # 从 transaction_list 中提取数据
        times = [item["time"] for item in self.transaction_list]
        transaction_times = [item["transaction_times"] for item in self.transaction_list]

        # 创建图表
        plt.figure(figsize=(8, 5))
        plt.plot(times, transaction_times, marker='o', linestyle='-', linewidth=2)

        # 添加标题和坐标轴标签
        plt.title(f"Transaction Times for {self.user_id}", fontsize=14)
        plt.xlabel("time", fontsize=12)
        plt.ylabel("transaction_times", fontsize=12)

        # 优化显示
        plt.grid(True)
        plt.tight_layout()

        # 保存为 PDF 文件
        filename = f"{self.user_id}.pdf"
        plt.savefig(filename, format='pdf')
        plt.close()

        print(f"折线图已保存为 {filename}")

    def run(self):
        global handle_number, trigger
        sleep(self.rank * 3)

        while True:
            sleep(10)
            caught = False
            url = "http://127.0.0.1:8001/getAccountTransactions"
            payload = {"id": self.user_id}
            try:
                response = requests.post(url, json=payload, timeout=50).json()
                print(self.user_id + " response : " + response['message'] + "   total_money: " + str(
                    self.total_money) + " alreadysend: " + str(self.money_transferred))  # 10秒超时
            except requests.exceptions.Timeout:
                print("请求超时")
                continue
            except requests.exceptions.RequestException as e:
                print(f"请求出错: {e}")
                continue
            if response['message'] == '-1' or response['message'] == '':
                self.time += 10
                continue
            self.handle_res([int(x) for x in response['message'].split(',')])
            self.time += 10
            if not self.check():
                caught = True
                url = "http://127.0.0.1:8001/intercepted"
                payload = {"id": self.user_id}
                requests.post(url, json=payload).json()
                with lock_trigger:
                    trigger = True
                self.interception_times += 1

            if random.uniform(0.3, 1) < self.time / 10000:
                caught = True
                url = "http://127.0.0.1:8001/arrested"
                payload = {"id": self.user_id}
                requests.post(url, json=payload).json()
                with lock_trigger:
                    trigger = True
                self.overtime += 1

            with lock_trigger:
                if trigger:
                    with lock_handle_number:
                        if handle_number[self.rank] != 1:
                            self.calculate_indication()
                            handle_number[self.rank] = 1
                            tag = False
                            for i in handle_number:
                                if i == 0:
                                    tag = True
                                    break
                            if not tag:
                                trigger = False

            if caught:
                print(self.user_id + "被拦截")
                if self.rank < config.count_num:
                    # 记录指标待完成
                    transaction_list_for_each_user.append({"user: ":self.user_id,"transaction_list":self.transaction_list})
                    ratio_list.append({"user": self.user_id,
                                       "ratio": str((self.total_money * 10) / (self.transaction_list[-1]["time"] + 1))})
                    overtime_list.append({"user": self.user_id, "overtimes": self.overtime})
                    intercepted_list.append({"user": self.user_id, "intercepted_times": self.interception_times})
                    complete_progress.append({"user": self.user_id, "complete_progress": self.money_transferred})
                    complete_time.append({"user": self.user_id, "complete_time": self.transaction_list[-1]["time"]})
                    self.calculate_indication()
                    max_performance_indication_list.append(
                        {"user": self.user_id, "max_performance_process": self.indication_list})
                    self.plot_transactions()
                    break
                self.money_transferred = 0
                self.transaction_list = []
                self.time = 0

            if self.money_transferred >= self.total_money:
                print(self.user_id + "于" + str(self.transaction_list[-1]["time"]) + "完成洗钱任务")
                transaction_list_for_each_user.append({"user: ":self.user_id,"transaction_list":self.transaction_list})
                # 记录指标待完成
                ratio_list.append(
                    {"user": self.user_id, "ratio": str((self.total_money * 10) / self.transaction_list[-1]["time"])})
                overtime_list.append({"user": self.user_id, "overtimes": self.overtime})
                intercepted_list.append({"user": self.user_id, "intercepted_times": self.interception_times})
                complete_progress.append({"user": self.user_id, "complete_progress": self.money_transferred})
                complete_time.append({"user": self.user_id, "complete_time": self.transaction_list[-1]["time"]})
                self.calculate_indication()
                max_performance_indication_list.append(
                    {"user": self.user_id, "max_performance_process": self.indication_list})
                self.plot_transactions()
                break


def save_to_local():
    data_dict = {}

    for item in ratio_list:
        user = item["user"]
        data_dict.setdefault(user, {})["ratio"] = item["ratio"]

    for item in overtime_list:
        user = item["user"]
        data_dict.setdefault(user, {})["overtimes"] = item["overtimes"]

    for item in intercepted_list:
        user = item["user"]
        data_dict.setdefault(user, {})["intercepted_times"] = item["intercepted_times"]

    for item in complete_progress:
        user = item["user"]
        data_dict.setdefault(user, {})["complete_progress"] = item["complete_progress"]

    for item in complete_time:
        user = item["user"]
        data_dict.setdefault(user, {})["complete_time"] = item["complete_time"]

    for item in max_performance_indication_list:
        user = item["user"]
        data_dict.setdefault(user, {})["max_performance_process"] = item["max_performance_process"]

    # 2. 写入 CSV
    csv_file = "output.csv"
    with open(csv_file, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        # 写表头
        writer.writerow(["user", "ratio", "overtimes", "intercepted_times", "complete_progress", "complete_time",
                         "max_performance_process"])

        # 写每行
        for user, values in data_dict.items():
            # 将 max_performance_process 列转换为字符串
            max_perf_str = ",".join(map(str, values.get("max_performance_process", [])))
            writer.writerow([
                user,
                values.get("ratio", ""),
                values.get("overtimes", ""),
                values.get("intercepted_times", ""),
                values.get("complete_progress", ""),
                values.get("complete_time", "", ),
                max_perf_str
            ])

    print(f"数据已写入 {csv_file}")


def save_transaction():
    


if __name__ == '__main__':
    # 启动多个线程
    threads = []
    print("program start")
    for i in range(12):
        t = MyThread(i)
        threads.append(t)
        t.start()
    print("threads start")
    # 等待所有线程完成
    for t in range(len(threads)):
        threads[t].join()

    print("所有线程执行完毕")
    save_to_local()
