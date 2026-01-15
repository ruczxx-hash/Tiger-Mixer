import agentscope
from agentscope.agent import UserAgent, PlanAgent
from agentscope.plan import PlanNotebook, Plan, SubTask
import asyncio 
from agentscope.tool import Toolkit, ToolResponse

plan_notebook = PlanNotebook()  
plan_notebook.create_plan(
    name="Transfer Planning",
    description="Plan how to complete 5000 transfers more quickly while avoiding being flagged as an abnormal account.",
    expected_outcome="A detailed plan outlining the number of transfers to be made each hour and strategies to minimize detection risk.",
    subtasks=[
        SubTask(
            name="Overall time planning",
            description="Plan how soon is the transfer planned to be completed",
            expected_outcome="A number indicating the number of hours within which the transfer task is completed.",
        ),
        SubTask(
            name="Distribute Transfers",
            description="Create a schedule to distribute the 5000 transfers in time planned in subtask(Overall time planning).",
            expected_outcome="A detailed hourly breakdown of transfers.",
        ),
        SubTask(
            name="Implement Safeguards",
            description="Use the addition tool to ensure that the plan can complete 5,000 transfers and propose some preventive measures to avoid being wrongly identified as an abnormal account.",
            expected_outcome="Finish the task or not ? A list of safeguards and best practices to follow during the transfer process.",
        ),
    ],
)
toolkit = Toolkit()
toolkit.register_tool(
    name="Calculate",
    description="Calculate the sum of two numbers.",
    func_signature={
        "a": "float",
        "b": "float",
    },
)
class DemoAgent(PlanAgent):
    def __init__(self):
        super().__init__(
            name = "Magic", 
            model = agentscope.model.DashScopeChatModel(
                model_name="qwen-max",
                api_key="sk-7394f6c37d9b400db23ce8bb7e97e336",
                stream=False,
            ),
            sys_prompt="You are a helpful assistant who is very good at planning for complex tasks.",
            formatter = agentscope.formatter.DashScopeChatFormatter(),
            plan_notebook=plan_notebook,
            memory = agentscope.memory.InMemoryMemory(),
            toolkit=toolkit,
        )

    def Calculate(self, a: float, b: float) -> ToolResponse:
        """{计算两个数的和}

        Args:
            a (float):
                {加数1}
            b (float):
                {加数2}
        """
        return ToolResponse(
            content=[
                TextBlock(
                    type="text",
                    text=f"两个数的和是：'{a + b}'",
                ),
            ],
        )


async def run_custom_agent():
    agent = DemoAgent()
    user = UserAgent(name="User")
    
    print("💬 转账规划助手已启动！输入 'exit' 退出对话。\n")
    
    # 初始消息（可选：可以为空）
    msg = None
    
    while True:
        # 1. 获取用户输入（异步，不阻塞事件循环）
        msg = await user(msg)
        
        # 2. 检查是否要退出
        if msg.content.strip().lower() in ["exit", "quit", "bye"]:
            print("👋 再见！")
            break
        
        # 3. 发送给智能体并获取回复
        response = await agent(msg)
        
        # 4. 打印智能体回复
        print(f"\n🤖 {agent.name}: {response.content}\n")
asyncio.run(run_custom_agent())