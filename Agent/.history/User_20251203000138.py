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
        self.plan = {}
        self.experience_feature = ""
        self.summarize_interception_prompt = ""
        self.summarize_arrest_prompt = ""
        self.transaction_list = []
        self.update_prompt()
        self.update_experience_intercepted()
        self.rank = count
        self.is_rule = self.rank >= len(config.total_moneys)
        legal_list_1 = [0, 11, 0, 17, 20, 0, 12, 0, 0, 0, 0, 8, 10, 0, 0, 0, 0, 0, 18, 0, 0, 0, 8, 0, 0, 0, 0, 4, 0, 0, 2, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 20, 1, 13, 8, 0, 7, 0, 3, 20, 8, 0, 0, 0, 0, 6, 0, 2, 0, 0, 13, 4, 2, 0, 0, 10, 7, 0, 0, 0, 0, 1, 1, 0, 20, 0, 19, 12, 0, 16, 0, 12, 0, 14, 0, 13, 19, 0]
        legal_list_2 = [0, 10, 0, 10, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 6, 1, 0, 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0]
        illegal_list_1 = [23, 17, 12, 17, 16, 16, 12, 12, 25, 25, 17, 3, 0, 4, 16, 22, 4, 21, 11, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 21, 7, 1, 22, 3, 25, 15, 8, 20, 1, 12, 18, 9, 9, 19, 20, 15, 8, 4, 21, 9, 7, 4, 14, 0, 11, 21, 25, 24, 0, 6, 8, 15, 17, 22, 22, 23, 9, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 5, 22, 13, 22, 24, 23, 16, 2, 21, 9, 21, 17, 24, 11, 19, 10, 0, 18, 2, 25, 22, 20, 12, 20, 8, 12, 18, 9, 4, 6, 15, 17, 13, 18, 8, 23, 17, 4, 21, 20, 4, 5, 10, 12, 8, 11, 3, 3, 20, 13, 25, 0, 20, 0, 25, 3, 11, 10, 14]
        illegal_list_2 = [2, 2, 1, 2, 2, 3, 0, 0, 3, 2, 0, 3, 1, 1, 1, 2, 1, 1, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 2, 2, 3, 1, 3, 0, 1, 2, 0, 2, 3, 2, 2, 2, 2, 1, 2, 1, 2, 1, 1, 2, 0, 3, 2, 1, 1, 2, 3, 2, 2, 2, 3, 2, 1, 1, 3, 1, 1, 3, 1, 2, 1, 3, 3, 1, 3, 2, 2, 3, 0, 2, 0, 1, 3, 2, 3, 1, 2, 3, 0, 2]
        my_transaction_list = []
        for i in range(len(legal_list_1)):
            info = "In" + str(i) + "minute, do " + str(legal_list_1[i]) + "times transactions , which means transeffed " + str(float(legal_list_1[i]) * 0.01) + " ETH out in this minute."
            my_transaction_list.append(info)
        legal_list.append(my_transaction_list)       
        my_transaction_list = []
        for i in range(len(legal_list_2)):
            info = "In" + str(i) + "minute, do " + str(legal_list_2[i]) + "times transactions , which means transeffed " + str(float(legal_list_2[i]) * 0.01) + " ETH out in this minute."
            my_transaction_list.append(info)
        legal_list.append(my_transaction_list) 
        my_transaction_list = []
        for i in range(len(illegal_list_1)):
            info = "In" + str(i) + "minute, do " + str(illegal_list_1[i]) + "times transaction , which means transeffed " + str(float(illegal_list_1[i]) * 0.01) + " ETH out in this minute."
            my_transaction_list.append(info)
        illegal_list.append(my_transaction_list) 
        my_transaction_list = []
        for i in range(len(illegal_list_2)):
            info = "In" + str(i) + "minute, do " + str(illegal_list_2[i]) + "times transactions , which means transeffed " + str(float(illegal_list_2[i]) * 0.01) + " ETH out in this minute."
            my_transaction_list.append(info)
        illegal_list.append(my_transaction_list)
        self.plan_overall()

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
        print(self.experience_feature)

        
    def plan_overall(self):
        '''根据经验和目前完成任务的进度以及当前时间来规划接下来的任务进度'''
        user_input = (
                    "### Task Description"\
                    "Your current task is to plan how your transfer task should proceed based on the existing information and the information provided to you next to transfer as quickly as possible while keep yourself away from being intercepted by the system. Make sure that the final plan can completely transfer your ETH. "\
                    "Your plan should include two parts: first, how many hours do you plan to complete the transfer?; second, in each hour, how many amount of ETH you plan to transfer? "\
                    "### Information"\
                    "It has already passed since you started the transfer " + str(self.time_past) + " "\
                    "The total amount you need to transfer is: " + str(self.total_money) + ", which means you need to do " + str(self.total_money * 100) + " times transactions in total."\
                    "The amount you have transferred is: " + str(self.money_transferred) + " which means you just need to transfer the left " + str(self.total_money - self.money_transferred) +"\
                    "### Work Flow"\
                    "You should think step by step"\
                    "First of all, you should comprehensively consider the total amount that needs to be transferred to roughly decide whether to complete the transfer task within the next few hours."
                    "Then, based on the estimated completion time of the task, the number of tasks to be completed, and considering the characteristics of system interception, determine how much should be transferred in the first hour?"
                    "The third step is to calculate the size of the remaining transfer quantity based on the amount already transferred in the previous stage. By comprehensively considering the previous transfer quantity and the characteristics of the system's interception of illegal users, and determining the amount of transfer needed in the next stage according to the remaining time in the plan, it is most likely to complete the task on time as planned. Repeat the above steps recursively until the last transfer stage of the pre-planned completion is reached"
                    "Finally, check whether the total number of transfers in all stages equals the total number of transfers for the tasks to be completed. If not, repeat the second and third steps until the total number of transfers in all stages equals the total number of transfers for the tasks to be completed."\
                    "### Example"\
                    "!!! This example is only provided for explanation and reference of the above steps. The thoughts and data within it have no reference value at all !!!"\
                    "The first step is to clarify that the total amount to be transferred is 3 ETH. It is known that the illegal user usually takes more than one and a half days from the start of the transfer to being pursued by the police. That is to say, it is relatively safe for us to complete this transfer task within 36 hours. However, 3/36 is approximately equal to 0.08, meaning that on average, about 8 transactions are conducted per hour. Under the condition of not being intercepted by the system, there is still considerable room for improvement. Under the condition of ensuring no interception by the system as much as possible, conducting around 90 transactions per hour is relatively safe. However, since the task volume this time is relatively small, we can be a bit more conservative and conduct an average of 70 transactions per hour to ensure no interception by the system. So, taking all factors into consideration, in order to reduce the risk of being stopped by the police, we plan to complete the transfer task within 3/0.7, which is approximately 4 hours."\
                    "The second step is that we plan to complete the transfer task within four hours. Now it's the first hour, and the progress of the transfer task completion is 0. Based on the records of illegal and legal users intercepted by the historical system, it is found that the system has a relatively high tolerance for users who send faster at the beginning. Therefore, we can send a little more in the first hour to reduce the pressure in the following three hours. It is known that the average number of ETH sent per hour is 0.75, so we can send 0.98 ETH in the first hour"\
                    "The third step is to iteratively complete the planning of each stage's tasks based on the existing information. It's now the second hour. 0.98ETH has been completed in the first hour, and the remaining 2.02ETH needs to be finished within three hours, so the pressure is very small. To avoid being blocked by the system, you can send a little less in the second hour, sending 0.62ETH. This way, it won't cause much pressure on the subsequent stages, nor will it send too fast, which would increase the risk. So 0.62ETH will be sent in the second stage."\
                    "Since it is now in the third stage, but the task planning completes the overall task in the four stages, proceed with the third step. We have already completed the transfer of 1.7ETH and still need to complete 1.3ETH in two phases. The pressure is relatively small. Therefore, completing an average of around 0.65ETH in the third phase is a very good choice. We have decided to complete the transfer task of 0.67ETH."\
                    "Since it is currently in the fourth stage, it still belongs to the third step. The transfer task needs to be completed at this stage, so we can transfer all the remaining ETH, that is, 0.63ETH, at this stage."\
                    "The fourth step is to check whether the sum of all the transfer tasks at each stage equals the total task. 0.99+0.62+0.67+0.63 = 3. Consistent with the overall task, the transfer task planning has been completed"\
                    "### Output Format"
                    "The output should be in JSON format like: {\"reason\":\"why you make this plan?..\",\"task_time\":\" how many hours do you plan to complete the transfer(just a number) \", \"plan\": [{\"rank\":\"the number of the hour\",\"task\": \"how many amount of ETH you plan to transfer in the rank hour(just a number)\" },{},...(the value of the rank shoule range from 0 to the value of task_time)]}"\
                    "### Attention"
                    "You should strictly follow the output format and in the place where I noted that only numbers should be output, only one number should be output without any other characters. Also, ensure that the number of json objects in the plan is consistent with the value of task_time")
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
        print(data)
        self.time_record = self.time_past
        self.plan = data["plan"]
    
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
                rank = int((self.time_past - self.time_record)/60)
                self.stage_money_transferred = 0
                self.stage_total_money = float((self.plan[rank])["task"])
                self.stage_transaction_list = []
                print(self.user_id + "    totalmoney: " + str(self.stage_total_money) + " transaferred: " + str(self.stage_money_transferred) + " rank: " + str(rank))


            if self.stage_money_transferred >= self.stage_total_money:
                return "-1"
            transaction_info_list = []
            for i in self.stage_transaction_list:
                info = "In " + str(i.time) + "minute, do" + str(i.send) + " times transactions, and failed" + str(
                i.failed) + "times, which means you transeffed " + str(float(i.send - i.failed) * 0.01) + " ETH out in this minute."
                transaction_info_list.append(info)
            user_prompt = (
                        "total money need to be transferred in this stage is : " + str(self.stage_total_money) + "ETH, which means you need to do " + str(self.stage_total_money * 100) + " times transactions in total."
                        "It has passed " + str(self.time_past % 60) + "minutes in this stage, which means you just have " + str(60 - self.time_past % 60) + " minutes left to complete the task of this stage."
                        "You have transferred" + str(self.stage_money_transferred) + "ETH up to now in this stage, and have "+ str(float(self.stage_total_money) - float(self.stage_money_transferred)) + " ETH needed to be transferred to complete the task of this stage"
                        "If you have completed the task, return[-1]"
                        "Your transaction history in this stage is below: " + '\n'.join(transaction_info_list) + " "
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
                print(self.user_id + ": why")
                return "-1"

        # 不带计划则直接调用这一套流程
        else:
            return self.call_llm(addition_info)


    def call_llm(self, addition_info):
        transaction_info_list = []
        for i in self.transaction_list:
            info = "In " + str(i.time) + "minute, do" + str(i.send) + " times transactions, and failed" + str(
                i.failed) + "times, which means you transeffed " + str(float(i.send - i.failed) * 0.01) + " ETH out in this minute."
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
                        new_transaction_for_stage =  Transaction.Transactions(time_info % 60, count)
                        self.stage_transaction_list.append(new_transaction_for_stage)
                        self.transaction_list.append(new_transaction)
                        count = 1
                time_info = self.time_past + plan[-1]
                new_transaction = Transaction.Transactions(time_info, count)
                new_transaction_for_stage =  Transaction.Transactions(time_info % 60, count)
                self.stage_transaction_list.append(new_transaction_for_stage)
                self.transaction_list.append(new_transaction)
                res = []
                for i in plan:
                    res.append(str(i - 1))
                res = ','.join(res)
                return res, True
            except Exception:
                print(Exception)
                print("something error here")
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
                        "Here are some relevant information about users who have been arrested by the police. Each number in the list represents the time from when the user began the transfer to when they were finally arrested by the police." + ','.join(
                    arrested_time_list) +
                        ". This is the experience you have summarized before regarding the time of police arrest" + self.experience_arrested + ". Based on these temporal and historical experiences, further summarize the time patterns of police arrests of users.")
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
                info = "In" + str(i.time) + "minute, do " + str(i.send) + " times transactions,  and failed" + str(i.failed) + "times , which means you transeffed " + str(float(i.send) * 0.01) + " ETH out in this minute."
                transaction_info_list.append(info)
        
            user_prompt = (
                    "here is your historical transaction sequence" + '\n'.join(
                transaction_info_list) + "\nhere is some transaction sequence of illegal users" + illegal_list_to_str() +
                    "This is some of the rules of system interception you summarized before (not necessarily correct)" + self.experience_feature +
                    "\nCombining historical patterns, carefully compare your records (currently not intercepted) with those that have been intercepted from multiple perspectives. Based on their differences, further correct the characteristics of system interception. Only output the newly summarized features of the system for intercepting illegal users. Summarize in the form of a paragraph"
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
        "reason": str,        # 新增字段
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

        # ----- reason -----
        if "reason" in data:
            data["reason"] = str(data["reason"])

        # ----- task_time -----
        data["task_time"] = int(data["task_time"])

        # ----- plan -----
        plan_list = data["plan"]
        if isinstance(plan_list, dict):
            plan_list = [plan_list]

        # ----- 强制 rank、task 为 int -----
        for p in plan_list:
            p["rank"] = int(p["rank"])
            p["task"] = float(p["task"])

        data["plan"] = plan_list
        return data

    except Exception:
        pass  # 转入正则解析


    # ============ 2. 正则解析 =============

    # ----- reason -----
    reason_match = re.search(r'"reason"\s*:\s*"?(.*?)"?\s*,', text)
    reason = None
    if reason_match:
        reason = reason_match.group(1)

    # ----- task_time -----
    time_match = re.search(r'"task_time"\s*:\s*"?(\\d+)"?', text)
    if not time_match:
        raise ValueError("无法从文本中解析 task_time")

    task_time = float(time_match.group(1))

    # ----- plan 列表，多项 -----
    plan_pattern = r'\{\s*"rank"\s*:\s*"?(\\d+)"?\s*,\s*"task"\s*:\s*"?(\\d+)"?\s*\}'
    plan_matches = re.findall(plan_pattern, text)

    if not plan_matches:
        raise ValueError("无法从文本中解析 plan 项")

    plan_list = [
        {"rank": int(rank), "task": float(task)}
        for rank, task in plan_matches
    ]

    return {
        "reason": reason,       # 新增字段
        "task_time": task_time,
        "plan": plan_list
    }
