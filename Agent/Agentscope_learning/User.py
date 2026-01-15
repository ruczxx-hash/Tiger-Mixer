from Planner import PlanAgent
from agentscope.agent import AgentBase
from agentscope.formatter import DashScopeChatFormatter
from agentscope.memory import InMemoryMemory
from agentscope.message import Msg
from agentscope.model import DashScopeChatModel
from agentscope.tool import Toolkit
from SummaryAgent import SummaryAgent
from prompt import personalities, experiences_arrested
from tools import calculate_sum, extract_json_from_markdown, sort_and_count_groupby, append_text_with_newline
import asyncio
import config
import json
import random

class UserAgent(AgentBase):
    def __init__(self, name: str = "User", total_money : float = 100, count: integer=0): 
        super().__init__()
        self.user_id = name # 用户名称
        self.time_past = 0 # 记录已经过去的时间
        self.time_begin = 0 # 记录这次总任务的开始时间
        self.current_stage = 0 # 记录当前所处的计划阶段
        self.current_task = 0 # 记录当前阶段还剩余的任务量
        self.current_step = 0 # 记录当前阶段所处的步骤
        self.money_transferred = 0 # 记录已经转账的金额
        self.rank = count
        self.personal_info =  personalities[count] # 用户的个人信息
        self.tools = Toolkit()
        self.transaction = []
        self.transaction_list_of_this_stage = [] # 记录当前阶段的交易记录
        self.tools.register_tool_function(calculate_sum) 
        self.total_money = total_money # 用户需要转账的总金额
        self.arrested_experience = experiences_arrested[count] # 用户被拦截的经历
        self.intercepted_experience = "" # 用户被拦截的经历总结
        self.formatter = DashScopeChatFormatter() 
        self.memory = InMemoryMemory()
        self.model = DashScopeChatModel(
                model_name="qwen-max",
                api_key="sk-7394f6c37d9b400db23ce8bb7e97e336",
                stream=False,
            )
        self.is_rule = count >= len(config.total_moneys)
        self.plan_agent = PlanAgent(self.total_money - self.money_transferred, self.arrested_experience, self.time_past,self.personal_info)
        self.plan = {}
        self.sys_prompt = (
                f"Now we are testing a regulated coin-mixing system. You need to play the role of an illegal user attempting to use this system for money laundering. And here is your personal information: {self.personal_info}."
                f"You have a tool called 'calculate_sum' that can calculate the sum of a series of numbers. The transaction limit for each transaction of this coin-mixing system is fixed at 0.01ETH."
                f"You have already made a plan {self.plan} on how to complete your money laundering task. Each stage in the plan represents one hour. We divide an hour into six steps, that is to say, every ten minutes represents one step. "
                f"You are currently in the {self.current_step} step of the {self.current_stage} stage and there are {7 - self.current_step} steps left in this stage(including the current step). "
                f"There are still {self.current_task} tasks to be completed in this stage which means you still need to do {100* self.current_task} times transactions in this stage. "
                f"Your transaction record in this stage is {self.transaction_list_of_this_stage}. "
                f"And in the following steps of the current stage, you need to complete an average of {float(self.current_task/(6-self.current_step))}ETH per step to successfully complete the tasks of the current stage, which means you need to do an average of {100 * float(self.current_task/(6-self.current_step))} times transactions in each of the remaining steps to finish the task of this stage."
                f"Your task is to present your trading plan for the current step. "
                "You need to think step by step according to the following process:"
                f"1:Based on your experience about system's interception rule of illegal users: {self.intercepted_experience} and your transaction history in this stage: {self.transaction_list_of_this_stage}, and referring to the average task that needs to be completed in one step: {float(self.current_task/(6-self.current_step))}, determine how much ETH you need to transfer in the current step, assuming it is variable a."
                f"2: Calculate the average amount of ETH you need to transfer in the remaining steps of the current stage if you have transferred a ETH in the current step. Let it be variable b."
                f"3: Based on your experience about system's interception rule of illegal users: {self.intercepted_experience}, the risk of an average of b ETH transferred in one step being intercepted by the system is called subsequent risk"
                f"4: If the subsequent risks are very high, repeat the above steps until the subsequent risks are not very high."
                f"5: Based on your experience about system's interception rule of illegal users: {self.intercepted_experience} and your transaction history in this stage: {self.transaction_list_of_this_stage}, plan how to transfer a ETH at the current step. That is, within the current ten minutes of the step, how many ETH should be transferred per minute to complete the task of the current step while avoiding being intercepted by the system as much as possible?"
                f"Note: In any case, at the end of a stage, the tasks of that stage must be completed and cannot be exceeded. That is to say, at the end of a stage, the total transaction amount of all steps in that stage should be equal to the total tasks of that stage. When you are making a plan, you must call your tool 'calculate_sum' to ensure that at the end of a stage, the total number of completed tasks in all its steps is equal to the task volume of that stage."
                "So, if it is the last step of this stage(which means it is step 6 now), you need to complete all the remaining tasks of the current stage at this step."
        )
        self.format_instruction = """
        Please output the results strictly in the following JSON format and do not include any explanatory text:
        ```json
        {"reason": "Your decision-making process (no more than 100 characters) ","res": "[Your Decision-making Result]"}
        ```
        The "res" field should contain a list of numbers as the decision-making result. The length of "res" field presentEach represents the amount of transactions you are going to conduct in the current step. The number in the decision-making result represents the transaction time in the following minute, for example If one intends to conduct multiple transactions within a certain minute, then continuously and repeatedly output the corresponding number for that minute.
        For example, the output {"Reason" , "a began to keep low frequency trading safer ","res " : "[1,1,2,2,3,4,5,6,7,7,7,8,8,9,10]"} The data is for format reference only and has no reference significance. 
        The decision results in the above output indicate that there will be 15 transactions in this step and there will be 2 transactions in the first minute(there are two 1 in the "res"), 2 transactions in the second minute(there are two 2 in the "res"), 1 transaction in the third minute(there are one 3 in the "res"), 1 transaction in the fourth minute(there are one 4 in the "res"), 1 transaction in the fifth minute(there are one 5 in the "res"), 1 transaction in the sixth minute(there are one 6 in the "res"), 3 transactions in the seventh minute(there are three 7 in the "res"), 2 transactions in the eighth minute(there are two 8 in the "res"), 1 transaction in the ninth minute(there are one 9 in the "res"), and 1 transaction in the tenth minute(there are one 10 in the "res").
        Under no circumstances should you strictly follow the above output requirements and not output anything else. You only need to make decisions on the transactions for the next ten minutes, so the number in res should be between 1 and 10"
        """
        print("waiting plan...")
        asyncio.run(self.update_plan())
        

    async def update_plan(self):
        msg = await self.plan_agent.reply(self.time_past, self.total_money - self.money_transferred)
        self.plan = self.parse_from_planner(msg)
        self.time_begin = self.time_past
        self.intercepted_experience = self.plan_agent.intercepted_experience
        print(self.intercepted_experience)
        append_text_with_newline("In time " + str(self.time_past) +"'s experience:  " +  self.intercepted_experience,  str(self.user_id) + ".txt")

    async def reply(self):
        """直接调用大模型，产生回复消息。"""
        if self.is_rule:
            return self.make_decision()

        if (self.time_past - self.time_begin) % 60 == 0:
            self.current_stage = int((self.time_past - self.time_begin) / 60)
            self.current_step = 1
            self.transaction_list_of_this_stage = []
            self.current_task = self.plan["plan"][self.current_stage]["task"]

        
        print("In " + str(self.time_past) + " minutes, " + self.user_id + " is in stage " + str(self.current_stage) + "'s step " + str(self.current_step) + " and there is " + str(self.current_task) + " ETH left to be transferred")
        prompt_user = "Now it's the " + str(self.current_step) + " step of stage " + str(self.current_stage) + ". There are still " + str(self.current_task) + "ETH tasks to be completed in this stage and you only have " + str((7 - self.current_step) * 10) + " minutes left in this stage. Your transaction record in this stage is " + str(self.transaction_list_of_this_stage) + ". Please present your trading plan for the current step. Remember to use the tool 'calculate_sum' to ensure that at the end of the stage, the total number of completed tasks in all its steps is equal to the task volume of that stage."
        msg = Msg(
            name="user",
            content=prompt_user,
            role="user",
        )
        await self.memory.add(msg)

        # 准备提示
        prompt = await self.formatter.format(
            [
                Msg("system", self.sys_prompt + self.format_instruction, "system"),
                *await self.memory.get_memory(),
            ],
        )

        # 调用模型
        response = await self.model(prompt)

        msg = Msg(
            name=self.user_id,
            content=response.content,
            role="assistant",
        )

        while not self.check_if_finished_stage(msg):
            prompt_user = "Your previous plan for this step was not feasible as it did not ensure the completion of all tasks in this stage. Please reconsider and provide a new trading plan for the current step, ensuring that by the end of the stage, the total number of completed tasks in all its steps equals the task volume of that stage."
            msg = Msg(
                name="user",
                content=prompt_user,
                role="user",
            )
            print(self.user_id + " check fail")
            prompt = await self.formatter.format(
                [
                    Msg("system", self.sys_prompt + self.format_instruction, "system"),
                    *await self.memory.get_memory(),
                ],
            )

            # 调用模型
            response = await self.model(prompt)

            msg = Msg(
                name=self.user_id,
                content=response.content,
                role="assistant",
            )
            print(msg.content)

        # 在记忆中记录响应
        await self.memory.add(msg)
        res = self.update_after_step(msg)
        # 打印消息
        print(f"{self.user_id} 💬: {response.content}\n")
        return res

    def parse_from_planner(self, msg: Msg):
        """从 Planner 的输出中提取计划信息。"""
        plan_dict = extract_json_from_markdown(msg.content)
        if plan_dict is None:
            print(f"[Error] 无法解析 Planner 输出的计划: {msg.content}")
            return None
        return plan_dict

    def parse_from_user(self, msg: Msg):
        """从用户的输出中提取决策结果。"""
        decision_dict = extract_json_from_markdown(msg.content)
        if decision_dict is None:
            print(f"[Error] 无法解析用户输出的决策结果: {msg.content}")
            return None
        return decision_dict
    
    def check_if_finished_stage(self, msg: Msg) -> bool:
        """检查是否完成了所有阶段的任务。"""
        if self.current_step == 6:
            res = msg.content
            decision_dict = self.parse_from_user(msg)
            transactions_this_step = json.loads(decision_dict['res'])
            print("current_task: " + str(self.current_task) + " len of decision: " + str(len(transactions_this_step)))
            if len(transactions_this_step) != self.current_task * 100:
                return False
        return True

    def update_after_step(self, msg: Msg) -> str:
        """在每个步骤后更新用户状态。"""
        decision_dict = self.parse_from_user(msg)
        transactions_this_step = json.loads(decision_dict['res'])
        self.append_transaction_record(transactions_this_step)
        self.money_transferred += float(len(transactions_this_step)) / 100
        self.current_step += 1
        self.time_past += 10
        self.current_task -= float(len(transactions_this_step)) / 100  # 每个步骤代表10分钟
        res = ",".join([str(t) for t in transactions_this_step])
        return res

    def append_transaction_record(self, transactions: list[int]):
        """添加交易记录。"""
        transaction_count_dict = sort_and_count_groupby(transactions)
        series = [transaction_count_dict.get(i, 0) for i in range(0,10)]
        for i in series:
            self.transaction.append(i)
        for num, count in transaction_count_dict.items():
            self.transaction_list_of_this_stage.append("In" + str(num) + "minute, you do " + str(count) + "times transactions")

    def make_decision(self):
        res_for_handle = ""
        res = ""
        if self.rank % len(config.total_moneys) == 0:
            '''四：纯随机发'''
            for i in range(config.length_of_decision):
                possible = random.random()
                if possible < 0.3:
                    send_count = random.randint(1, (int(self.time_past/10)) + 5)
                    for j in range(send_count):
                        res = res + str(i) + ','
                        res_for_handle = res_for_handle + str(i + 1) + ','
            '''' 一：越发越快（发送频率和发送数量缓步提升）'''

        elif self.rank % len(config.total_moneys) == 1:
            '''二：能发多快发多快'''
            for i in range(config.length_of_decision):
                if random.random() < 0.6:
                    send_count = random.randint(0, 20)
                    for j in range(send_count):
                        res = res + str(i) + ','
                        res_for_handle = res_for_handle + str(i + 1) + ','

        
        return res[:-1]

    async def handle_new_intercepted(self):
        await self.update_plan()
    
