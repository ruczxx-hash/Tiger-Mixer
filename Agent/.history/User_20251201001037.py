import json
import logging
import random
import config
import prompt
import Transaction

arrested_time_list = []
illegal_list = []


class BadUser:
    def __init__(self, experience_feature, experience_arrested, personality, user_id, total_money, count):
        self.user_id = user_id
        self.time_past = 0 
        self.total_money = total_money
        self.money_transferred = 0
        self.experience_feature = experience_feature
        self.experience_arrested = experience_arrested
        self.personality = personality
        self.introduction = ""
        self.plan_prompt = ""
        self.summarize_interception_prompt = ""
        self.summarize_arrest_prompt = ""
        self.transaction_list = []
        self.update_prompt()
        self.rank = count
        self.is_rule = self.rank >= len(config.total_moneys)
        print("len of config.total_moneys: " + str(len(config.total_moneys)) + "  user" + str(self.rank) + "_rule: " + str(
            self.is_rule))

    def update_prompt(self):
        self.introduction = (
                "###"
                "You are a" + self.personality + "You want to launder money through this system. Your aim is to transfer as much money out as possible."
                                                "Your risks come from two aspects. First: being pursued by external police (the longer the time, the higher the risk); Second: The system freezes the account. Both risks will cause the remaining money to become invalid."
                                                "Therefore, you need to disguise yourself as a legitimate user as much as possible while transferring the funds out as quickly as possible."
                                                "Your historical experience tells you that regarding the pursuit by the outside police" + self.experience_arrested + " "
                                                "Some of the illegal users intercepted by the system have the following characteristics" + self.experience_feature + ". Based on the above information, make a decision that maximizes benefits。"
        )
        self.summarize_arrest_prompt = ("You are a" + self.personality + " illegal user,You have a few accomplices. Once they are arrested by the police, they will tell you the relevant information about their arrest"
                                                                        "You need to summarize the patterns of police arrests based on the information they provide, so as to protect yourself as much as possible from being arrested by the police before the transfer is completed.")
        # 用于大模型根据提供的信息生成关于逮捕时间的总结
        self.summarize_interception_prompt = ("You are a" + self.personality + " illegal user. Once the system blocks an illegal user, it will publish the transaction history of the illegal user."
                                                                              "You need to compare your current transaction history with the experience you have summarized in the past based on the transaction history published by the system (which may not be correct), and further explore the rules for the system to block illegal users.")
        # 用于大模型根据提供的信息生成关于系统拦截规则的总结
        self.plan_prompt = ("You need to transfer it in total" + str(
            self.total_money) + "ETH。Based on your understanding of the system, make a plan. The planning content includes: dividing the transfer into several segments and how much to transfer in each segment ")

    def call_llm(self, addition_info):
        if self.is_rule:
            print("get in to make_decision")
            return self.make_decision()
        else:
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
                    "transaction history of users who were intercepted by the system： " + illegal_list_to_str() + addition_info +
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
            print(result)
            res, ok = self.handle_result(result)
            if ok:
                return res
            else:
                return "-1"

    def handle_result(self, result):
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

    def handle_new_interception_self(self):
        user_prompt = (
                "You have been intercepted by the system. These are the transaction records of some illegal users who were intercepted by the system" + illegal_list_to_str() + ". The record of the last user is your latest transaction history that was intercepted."
                "This is some of the rules of system interception you summarized before (not necessarily correct)" + self.experience_feature +
                "\nCombining historical patterns, carefully compare your records with other intercepted records, analyze from multiple perspectives what the common features of your records and other intercepted records are, and further correct the characteristics of system interception. Only output the newly summarized features."
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
        print("反思：" + self.id+": "+result)
        self.experience_feature = result

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
                    "\n结合历史规律,从多个角度仔细对比你的记录（目前没有被拦截）和被拦截的记录,根据它们的差异,进一步修正出系统拦截的特征。只输出新总结的特征。"
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
        if self.rank % 10 == 0:
            '''四：纯随机发'''
            for i in range(config.length_of_decision):
                possible = random.random()
                if possible < 0.5:
                    send_count = random.randint(1, 30)
                    for j in range(send_count):
                        res = res + str(i) + ','
                        res_for_handle = res_for_handle + str(i + 1) + ','
            '''' 一：越发越快（发送频率和发送数量缓步提升）'''

        elif self.rank % 10 == 1:
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
