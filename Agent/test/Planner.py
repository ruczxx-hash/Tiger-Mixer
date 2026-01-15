from agentscope.agent import AgentBase
from agentscope.formatter import DashScopeChatFormatter
from agentscope.memory import InMemoryMemory
from agentscope.message import Msg
from agentscope.model import DashScopeChatModel
from agentscope.tool import Toolkit
from SummaryAgent import SummaryAgent
from tools import calculate_sum, extract_json_from_markdown
import asyncio
import json 
import os
class PlanAgent(AgentBase):
    def __init__(self,total_money: float = 100, arrested_experience: str = "",time_past: int =0, personal_info: str =""):
        super().__init__()
        self.name = "PlanAgent"
        self.total_money = total_money
        self.intercepted_experience = ""
        self.personal_info = personal_info
        self.time_past = time_past  # 已经过去的时间，单位：小时
        self.arrested_experience = arrested_experience
        self.sys_prompt = ""
        self.tools = Toolkit()
        self.formatter = DashScopeChatFormatter()
        self.memory = InMemoryMemory()
        self.summaryagent = SummaryAgent()
        self.model = DashScopeChatModel(
                model_name="qwen-max",
                api_key="sk-7394f6c37d9b400db23ce8bb7e97e336",
                stream=False,
        )
        self.tools.register_tool_function(calculate_sum)   
        self.format_instruction = """
        Please output the results strictly in the following JSON format and do not include any explanatory text:
        ```json
        {"reason": "why you made this plan","task_time": a number (total time in hours to complete the plan),"plan": [{"stage": an integer (the order of the task),"task": a float (how many amount of ETH you plan to transfer in the rank hour(just a number))}...(more tasks ,number of tasks should be equal to task_time)]}
        ```
        """
        self.update_sys_prompt()


    def update_sys_prompt(self): 
        return (
                    "### Role Defination"\
                    "You are a transfer planning assistant. Here is your personality: " + self.personal_info + ". Your goal is to help users plan their transfer tasks to complete the transfer of their ETH before being arrested by the police while minimizing the risk of being intercepted by the system. You have a tool called calculate_sum which can help you calculate the sum of a list of numbers."\
                    "### Task Description"\
                    "Your current task is to plan how your transfer task should proceed based on the existing information and the information provided to you next to transfer as quickly as possible while keeping yourself away from being intercepted by the system. Make sure that the final plan can completely transfer your ETH. "\
                    "Your plan should include two parts: first, how many hours do you plan to complete the transfer?; second, in each hour, how many amount of ETH you plan to transfer? "\
                    "### Information"\
                    "It has already passed since you started the transfer " + str(self.time_past) + " minutes "\
                    "The left total amount of ETH you need to transfer is: " + str(self.total_money) + ", which means you need to do " + str(self.total_money* 100) + " times transactions in total."\
                    "From your previous experience about being arrested by the police, " + self.arrested_experience + ""\
                    "From your previous experience, you have summarized some rules about how the system intercepts illegal users: " + self.intercepted_experience + ""\
                    "If it is too difficult to complete the task before the police arrest without being intercepted by the system, then transfer as much ETH as possible before being intercepted or arrested"\
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
                    "You must use the tool to assist in calculating the sum of numbers to make sure the plan you make can complete the whole task."\
                    )


    async def reply(self,time_past,total_money) -> Msg:
        """直接调用大模型，产生回复消息。"""
        self.time_past = time_past
        self.total_money = total_money
        self.update_sys_prompt()
        print("Generating summary...")
        self.intercepted_experience = (await self.summaryagent.reply()).content[0]["text"]
        msg = Msg(
            name="user",
            content=f"Based on your experience, give a transfer plan that enables you to complete the transfer task as quickly as possible while minimizing the possibility of being blocked by the system. Make sure that the total amount of your planned transactions is equal to {self.total_money} ETH.",
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

        print("Calling model to generate plan...")
        # 调用模型
        response = await self.model(prompt)

        msg = Msg(
            name=self.name,
            content=response.content,
            role="assistant",
        )

        print("Checking plan validity...")
        print("Plan content:", response.content)

        res = extract_json_from_markdown(response.content)
        while not self.check(res): # 检查计划是否符合要求
            print("Plan invalid, requesting revision...")
            correction_msg = Msg(
                name="user",
                content=f"The plan you provided does not meet the requirement that the total amount of money transferred must equal {self.total_money} ETH. Please revise your plan accordingly.",
                role="user",
            )
            await self.memory.add(correction_msg)

            # 准备提示
            prompt = await self.formatter.format(
                [
                    Msg("system", self.sys_prompt + self.format_instruction, "system"),
                    *await self.memory.get_memory(),
                ],
            )

            # 重新调用模型
            response = await self.model(prompt)

            msg = Msg(
                name=self.name,
                content=response.content,
                role="assistant",
            )

            res = extract_json_from_markdown(response.content)

        await self.memory.add(msg)

        # 打印消息
        await self.print(msg)
        return msg
    
    def check(self, plan_json: dict) -> bool:
        """检查生成的计划是否符合要求，即所有任务的金额之和是否等于 total_money。"""
        planned_sum = sum(task["task"] for task in plan_json.get("plan", []))
        print("plan sum: " + str(planned_sum) + " total_money: " + str(self.total_money) )
        return abs(planned_sum - self.total_money) < 1e-6  # 允许微小误差
