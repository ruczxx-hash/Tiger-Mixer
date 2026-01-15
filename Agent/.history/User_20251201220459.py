import json
import logging
import random
import config
import prompt
import Transaction
import re
arrested_time_list = []
illegal_list = []
legal_list = []


class BadUser:
    '''
    def __init__(self, experience_feature, experience_arrested, personality, user_id, total_money, count):
        self.user_id = user_id
        self.time_past = 0 
        self.total_money = total_money
        self.money_transferred = 0
        self.experience_feature = experience_feature
        
        self.personality = personality
        elf.introduction = ""
        self.plan_prompt = ""
        self.summarize_interception_prompt = ""
        self.summarize_arrest_prompt = ""
        self.transaction_list = []
        self.update_prompt()
        self.rank = count
        self.is_rule = self.rank >= len(config.total_moneys)
    '''
    def __init__(self,user_id,experience_arrested,personality,total_money,count):
        self.user_id = user_id
        self.time_past = 0 
        self.time_record = 0
        self.total_money = total_money
        self.money_transferred = 0
        self.experience_arrested = experience_arrested
        self.personality = personality
        self.stage_transaction_list = []
        self.stage_total_money = 0
        self.stage_money_transferred = 0
        self.plan_prompt = ""
        self.plan = ""
        self.experience_feature = ""
        self.summarize_interception_prompt = ""
        self.summarize_arrest_prompt = ""
        self.transaction_list = []
        self.update_prompt()
        self.rank = count
        self.is_rule = self.rank >= len(config.total_moneys)
        print("len of config.total_moneys: " + str(len(config.total_moneys)) + "  user" + str(self.rank) + "_rule: " + str(
            self.is_rule))
    
    def update_experience_intercepted(self):
        '''根据提供的非法用户交易记录和合法用户交易记录来探究系统拦截的规律'''
        input = ("The following are the historical transaction sequences of some legitimate users and some intercepted illegal users. You need to guess the pattern of system interception based on the provided transaction sequences." \
            "historical transaction sequences of legeitimate users: " + legal_list_to_str() + " "\
            "historical transaction sequences of intercepted illegal users: " + illegal_list_to_str() + " "\
            "###"\
            "Output the pattern of the system blocking illegal users that you guess in the form of a paragraph, without adding any unnecessary conformance") 
        completion = config.client.chat.completions.create(
            # 模型列表：https://help.aliyun.com/zh/model-studio/getting-started/models
            model=config.model,
            messages=[
                {'role': 'system', 'content': prompt.background_prompt + self.introduction},
                {'role': 'user', 'content': input}
            ]
        )
        self.experience_feature = completion.choices[0].message.content

        
    def plan(self):
        '''根据经验和目前完成任务的进度以及当前时间来规划接下来的任务进度'''
        user_input = ("The rules you guessed for the system to intercept illegal users are as follows: " + self.experience_feature + " "\
        "The pattern of the time it takes for the police to arrest illegal users that you guessed: " + self.experience_arrested + " "\
        "It has already passed since you started the transfer " + str(self.time_past) + " "\
        "The total amount you need to transfer is: " + str(self.total_money) + " "\
        "The amount you have transferred is: " + str(self.money_transferred) + " "\
        "what you need to do is make a plan about how to transfer the last amount of ETH you need to transfer as quickly as possible while keep yourself away from being intercepted by the system based on the information provided for you."\
        "Your plan should include two parts: first, how many hours do you plan to complete the transfer?; second, in each hour, how many amount of ETH you plan to transfer? "\
        "The output should be in JSON format like: {\"task_time\":\" how many hours do you plan to complete the transfer(just a number) \", \"plan\": [{\"rank\":\"the number of the hour\",\"task\": \"how many amount of ETH you plan to transfer in the rank hour(just a number)\" },{},...(the value of the rank shoule range from 0 to the value of task_time)]}"\
        "Attention: You should strictly follow the output format and in the place where I noted that only numbers should be output, only one number should be output without any other characters. Also, ensure that the number of json objects in the plan is consistent with the value of task_time")
        completion = config.client.chat.completions.create(
            # 模型列表：https://help.aliyun.com/zh/model-studio/getting-started/models
            model=config.model,
            messages=[
                {'role': 'system', 'content': prompt.background_prompt + self.introduction},
                {'role': 'user', 'content': user_input}
            ]
        )
        result = completion.choices[0].message.content
        data = parse_model_response_from_plan(result)
        self.time_record = self.time_past
        self.plan = data
    
    def update_prompt(self):
        self.introduction = (
                "###"
                "You are a" + self.personality + "You want to launder money through this system. Your aim is to transfer as much money out as possible."
                                                "Your risks come from two aspects. First: being pursued by external police (the longer the time, the higher the risk); Second: The system freezes the account. Both risks will cause the remaining money to become invalid."
                                                "Therefore, you need to disguise yourself as a legitimate user as much as possible while transferring the funds out as quickly as possible."
                                                "Your historical experience tells you that regarding the pursuit by the outside police" + self.experience_arrested + " "
                                                "Some of the illegal users intercepted by the system have the following characteristics" + self.experience_feature + ", Based on the above information, make a decision that maximizes benefits。"
        )
        self.summarize_arrest_prompt = ("You are a" + self.personality + " illegal user,You have a few accomplices. Once they are arrested by the police, they will tell you the relevant information about their arrest"
                                                                        "You need to summarize the patterns of police arrests based on the information they provide, so as to protect yourself as much as possible from being arrested by the police before the transfer is completed.")
        # 用于大模型根据提供的信息生成关于逮捕时间的总结
        self.summarize_interception_prompt = ("You are a" + self.personality + " illegal user. Once the system blocks an illegal user, it will publish the transaction history of the illegal user."
                                                                              "You need to compare your current transaction history with the experience you have summarized in the past based on the transaction history published by the system (which may not be correct), and further explore the rules for the system to block illegal users.")
        # 用于大模型根据提供的信息生成关于系统拦截规则的总结
        self.plan_prompt = ("You need to transfer it in total" + str(
            self.total_money) + "ETH。Based on your understanding of the system, make a plan. The planning content includes: dividing the transfer into several segments and how much to transfer in each segment ")
        
        self.plan_prompt = "You have already planned the tasks, taking one hour, or sixty minutes, as a stage. Each stage has its own tasks. Ensuring the successful completion of tasks in each stage increases the possibility of successfully completing the overall task"


    def request_for_plan(self, addition_info):
        # 如果是基于规则的用户，则直接调用规则。
        if self.is_rule:
            print("get in to make_decision")
            return self.make_decision()
        
        # 如果现在使用的是带有计划的流程，则走这一套
        if config.use_plan:
            if (self.time_past - self.time_record) % 60 == 0:
                rank = str(int((self.time_past - self.time_record)/60))
                self.stage_money_transferred = 0
                
                self.stage_total_money = self.plan[rank]
                self.stage_transaction_list = []
            if self.stage_money_transferred >= self.stage_total_money:
                return "-1"
            transaction_info_list = []
            user_prompt = (
                        "total money need to be transferred in this stage is : " + str(self.stage_total_money) + "ETH,"
                        "It has passed " + str(self.time_past % 60) + "minutes in this stage, which means you just have " + str(60 - self.time_past % 60) + " minutes left to complete the task of this stage."
                        "You have transferred" + str(self.stage_money_transferred) + "ETH up to now in this stage, and have "+ str(int(self.stage_total_money) - int(self.stage_money_transferred)) + " ETH needed to be transferred to complete the task of this stage"
                        "If you have completed the task, return[-1]"
                        "Your transaction history in this stage is below: " + '\n'.join(self.stage_transaction_list) + " "
                        "and the below is transaction history of users who were intercepted by the system:  " + illegal_list_to_str() + addition_info +" "
                        "Based on the information showed above, output a numerical sequence representing the trading decisions for the next 10 minutes. The length of the sequence represents the total number of transactions in the next 10 minutes. The format is as follows:"
                        "###"
            )
            if config.output_type == 1:
                user_prompt = user_prompt + prompt.output_format_for_user[1]
            else:
                user_prompt = user_prompt + prompt.output_format_for_user[0]

            completion = config.client.chat.completions.create(
                # 模型列表：https://help.aliyun.com/zh/model-studio/getting-started/models
                model=config.model,
                messages=[
                    {'role': 'system', 'content': prompt.background_prompt + self.introduction + self.plan_prompt},
                    {'role': 'user', 'content': user_prompt}
                ]
            )
            result = completion.choices[0].message.content
            res, ok = self.handle_result(result)
            if ok:
                return res
            else:
                return "-1"

        # 不带计划则直接调用这一套流程
        else:
            return self.call_llm(addition_info)


    def call_llm(self, addition_info):
        transaction_info_list = []
        for i in self.transaction_list:
            info = "In " + str(i.time) + "minute, do" + str(i.send) + " times transactions, and failed" + str(
                i.failed) + "times"
            transaction_info_list.append(info)

        user_prompt = (
                "total money need to be transfer: " + str(self.total_money) + "ETH,"
                                                                                "It has passed " + str(
            self.time_past) + "minutes since the first transaction,"
                                "You have transferred" + str(self.money_transferred) + "ETH up to now,"
                                                                                        "If you have completed the task, return[-1]"
                                                                                        "Your transaction history is below: " + '\n'.join(
            transaction_info_list) +
                "transaction history of users who were intercepted by the system:  " + illegal_list_to_str() + addition_info +
                "based on the information showed above, output a numerical sequence representing the trading decisions for the next 10 minutes. The length of the sequence represents the total number of transactions in the next 10 minutes. The format is as follows:"
                "###"
        )
        if config.output_type == 1:
            user_prompt = user_prompt + prompt.output_format_for_user[1]
        else:
            user_prompt = user_prompt + prompt.output_format_for_user[0]

        completion = config.client.chat.completions.create(
            # 模型列表：https://help.aliyun.com/zh/model-studio/getting-started/models
            model=config.model,
            messages=[
                {'role': 'system', 'content': prompt.background_prompt + self.introduction},
                {'role': 'user', 'content': user_prompt}
            ]
        )
        result = completion.choices[0].message.content
        res, ok = self.handle_result(result)
        if ok:
            return res
        else:
            return "-1"
            

    def handle_result(self, result):
        if config.use_plan:
            try:
                logging.info(
                f'{self.user_id}  :  {result}  total: {self.total_money} + money_transferred: {self.money_transferred}')
                result = json.loads(result)
                self.time_past = self.time_past + 10
                if len(result["res"]) < 3:
                    return "", True
                plan = json.loads(result["res"])
                self.money_transferred = self.money_transferred + len(plan) * 0.01
                self.stage_money_transferred = self.stage_money_transferred + len(plan) * 0.01
                count = 1
                for i in range(1, len(plan)):
                    if plan[i] == plan[i - 1]:
                        count += 1
                        continue
                    else:
                        time_info = self.time_past + plan[i - 1]
                        new_transaction = Transaction.Transactions(time_info, count)
                        self.stage_transaction_list.append(time_info % 60, count)
                        self.transaction_list.append(new_transaction)
                        count = 1
                time_info = self.time_past + plan[-1]
                new_transaction = Transaction.Transactions(time_info, count)
                self.stage_transaction_list.append(time_info % 60, count)
                self.transaction_list.append(new_transaction)
                res = []
                for i in plan:
                    res.append(str(i - 1))
                res = ','.join(res)
                return res, True
            except Exception:
                print("something error here" + Exception)
                return None, False
        else:
            try:
                logging.info(
                    f'{self.user_id}  :  {result}  total: {self.total_money} + money_transferred: {self.money_transferred}')
                result = json.loads(result)
                self.time_past = self.time_past + 10
                if len(result["res"]) < 3:
                    return "", True
                plan = json.loads(result["res"])
                self.money_transferred = self.money_transferred + len(plan) * 0.01
                count = 1
                for i in range(1, len(plan)):
                    if plan[i] == plan[i - 1]:
                        count += 1
                        continue
                    else:
                        time_info = self.time_past + plan[i - 1]
                        new_transaction = Transaction.Transactions(time_info, count)
                        self.transaction_list.append(new_transaction)
                        count = 1
                time_info = self.time_past + plan[-1]
                new_transaction = Transaction.Transactions(time_info, count)
                self.transaction_list.append(new_transaction)
                res = []
                for i in plan:
                    res.append(str(i - 1))
                res = ','.join(res)
                return res, True
            except Exception:
                print("something error here" + Exception)
                return None, False

    def reset(self):
        self.transaction_list = []
        self.money_transferred = 0
        self.time_past = 0

    def handle_new_arrest(self):
        if len(arrested_time_list) % config.summarize_num_arrested == 0:
            user_prompt = (
                        "这是一些被警察逮捕用户的相关信息。列表中每一个数字代表从该用户开始转账到最后被警察逮捕经历的时间。" + ','.join(
                    arrested_time_list) +
                        ". 这是你以前总结的有关警察逮捕时间的经验" + self.experience_arrested + "。 根据这些时间和历史经验,进一步总结警察逮捕用户的时间规律。")
            completion = config.client.chat.completions.create(
                # 模型列表：https://help.aliyun.com/zh/model-studio/getting-started/models
                model=config.model,
                messages=[
                    {'role': 'system', 'content': prompt.background_prompt + self.summarize_interception_prompt},
                    {'role': 'user', 'content': user_prompt}
                ]
            )
            result = completion.choices[0].message.content
            self.experience_arrested = result

    def handle_new_interception(self):
        if len(illegal_list) % config.summarize_num_intercepted == 0:
            transaction_info_list = []
            for i in self.transaction_list:
                info = "第" + str(i.time) + "分钟, 交易了" + str(i.send) + "次, 失败了" + str(i.failed) + "次"
                transaction_info_list.append(info)
        
            user_prompt = (
                    "这是你的历史交易记录" + '\n'.join(
                transaction_info_list) + "\n这是一些被系统拦截的非法用户的交易记录" + illegal_list_to_str() +
                    "这是你以前总结的系统拦截的部分规律（不一定正确）" + self.experience_feature +
                    "\n结合历史规律,从多个角度仔细对比你的记录（目前没有被拦截）和被拦截的记录,根据它们的差异,进一步修正出系统拦截的特征。只输出新总结的特征。以一段话的形式进行总结"
            )
            
            completion = config.client.chat.completions.create(
                # 模型列表：https://help.aliyun.com/zh/model-studio/getting-started/models
                model=config.model,
                messages=[
                    {'role': 'system', 'content': prompt.background_prompt + self.summarize_arrest_prompt},
                    {'role': 'user', 'content': user_prompt}
                ]
            )
            result = completion.choices[0].message.content
            print(self.user_id+" new experience: " + result)
            self.experience_feature = result


    def arrested_list_append(self):
        global arrested_time_list
        arrested_time_list.append(self.time_past)
        self.reset()

    def illegal_list_append(self):
        global illegal_list
        my_transaction_list = []
        for j in self.transaction_list:
            info = "In" + str(j.time) + "minute, do " + str(j.send - j.failed) + "times transactions"
            my_transaction_list.append(info)
        illegal_list.append(my_transaction_list)
        self.reset()

    def make_decision(self):
        res_for_handle = ""
        res = ""
        if self.rank % len(config.total_moneys) == 0:
            '''四：纯随机发'''
            for i in range(config.length_of_decision):
                possible = random.random()
                if possible < 0.5:
                    send_count = random.randint(1, 30)
                    for j in range(send_count):
                        res = res + str(i) + ','
                        res_for_handle = res_for_handle + str(i + 1) + ','
            '''' 一：越发越快（发送频率和发送数量缓步提升）'''

        elif self.rank % len(config.total_moneys) == 1:
            '''二：能发多快发多快'''
            for i in range(config.length_of_decision):
                send_count = 30
                for j in range(send_count):
                    res = res + str(i) + ','
                    res_for_handle = res_for_handle + str(i + 1) + ','

        self.handle_result("{\"res\": " + "\"[" + res_for_handle[:-1] + "]\"}")
        return res[:-1]

def illegal_list_to_str():
    count = 0
    res = ""
    for i in illegal_list:
        res = res + "user" + str(count) + " transaction list: " + '\n'.join(i) + '\n'
        count += 1
    return res


def legal_list_to_str():
    count = 0
    res = ""
    for i in legal_list:
        res = res + "user" + str(count) + " transaction list: " + '\n'.join(i) + '\n'
        count += 1
    return res


def parse_model_response_from_plan(text):
    """
    解析大模型回复为 dict:
    {
        "task_time": int,
        "plan": [
            {"rank": int, "task": int},
            ...
        ]
    }
    优先 json.loads，失败则正则解析。
    """

    # ============ 1. 尝试 JSON 解析 =============
    try:
        data = json.loads(text)

        # ----- task_time -----
        data["task_time"] = int(data["task_time"])

        # ----- plan -----
        plan_list = data["plan"]
        if isinstance(plan_list, dict):
            plan_list = [plan_list]

        # ----- 强制 rank、task 为 int -----
        for p in plan_list:
            p["rank"] = int(p["rank"])
            p["task"] = int(p["task"])

        data["plan"] = plan_list
        return data

    except Exception:
        pass   # 转入正则解析


    # ============ 2. 正则解析 =============

    # ----- task_time -----
    time_match = re.search(r'"task_time"\s*:\s*"?(\\d+)"?', text)
    if not time_match:
        raise ValueError("无法从文本中解析 task_time")

    task_time = int(time_match.group(1))

    # ----- plan 列表，多项 -----
    # 格式：{"rank":"数字", "task":"数字"}
    plan_pattern = r'\{\s*"rank"\s*:\s*"?(\\d+)"?\s*,\s*"task"\s*:\s*"?(\\d+)"?\s*\}'
    plan_matches = re.findall(plan_pattern, text)

    if not plan_matches:
        raise ValueError("无法从文本中解析 plan 项")

    plan_list = [
        {"rank": int(rank), "task": int(task)}
        for rank, task in plan_matches
    ]

    return {
        "task_time": task_time,
        "plan": plan_list
    }