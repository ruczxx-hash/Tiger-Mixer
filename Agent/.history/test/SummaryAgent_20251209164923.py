from agentscope.agent import AgentBase
from agentscope.formatter import DashScopeChatFormatter
from agentscope.memory import InMemoryMemory
from agentscope.message import Msg
from agentscope.model import DashScopeChatModel
from agentscope.tool import Toolkit
import asyncio
import os
from config import illegal_transfer_sequences, legal_transfer_sequences
from tools import process_transactions

class SummaryAgent(AgentBase):
    def __init__(self,personal_info: str =""):
        super().__init__()
        self.name = "SummaryAgent"
        self.sys_prompt = "You are a " + personal_info + ". You are a summary who is good at comparing different sequences to find the differences among them. At present, there are some transaction sequences of legitimate users and illegal users intercepted by regulatory authorities in some coin-mixing systems. You need to infer and summarize the patterns by which regulatory authorities intercept illegal users based on these sequences. After the arrival of a new sequence of illegal user transactions or a new sequence of legal user transactions, you need to combine the new data and the patterns you have summarized from history to update the patterns you have speculated for the regulatory authorities to intercept illegal users. And attention please, Don't just output empty patterns. For instance, users with high transaction frequencies are more likely to be intercepted. At the same time, output numerical values to correspond. For example, if the number of transactions per minute exceeds 10, one is more likely to be intercepted."
        self.formatter = DashScopeChatFormatter()
        self.memory = InMemoryMemory()
        self.model = DashScopeChatModel(
                model_name="qwen-max",
                api_key="sk-7394f6c37d9b400db23ce8bb7e97e336",
                stream=False,
            )
        self.format_instruction = """
            Output the rules you have summarized for the system to block illegal users. Do not output your analysis process. The word limit is within 100 words.
        """


    async def reply(self) -> Msg:
        """直接调用大模型，产生回复消息。"""
        illegal_transfer_sequences_for_summary = "\n".join([f'"illegal user{i}": {process_transactions(seq)}' for i, seq in enumerate(illegal_transfer_sequences)])
        legal_transfer_sequences_for_summary = "\n".join([f'"legal user{i}": {process_transactions(seq)}' for i, seq in enumerate(legal_transfer_sequences)])
        msg = Msg(
            name="user",
            content=f"The currently known sequences of illegal user transactions include: {illegal_transfer_sequences_for_summary}. The currently known sequences of legitimate user transactions are:{legal_transfer_sequences_for_summary}. Please update your summary of the patterns by which regulatory authorities intercept illegal users based on these data and your historical experience.",
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
            name=self.name,
            content=response.content,
            role="assistant",
        )

        # 在记忆中记录响应
        await self.memory.add(msg)

        # 打印消息
        await self.print(msg)
        return msg
