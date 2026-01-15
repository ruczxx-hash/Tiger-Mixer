"""模拟系统初始化模块"""
import pandas as pd
from models.user import RetailUser, ExchangeUser, Rule_Based_User, LLM_Based_User
from models.committee import Committee
from utils.committee_utils import select_committee
from utils.reference_samples import load_reference_samples, generate_reference_samples
from config.system_config import (
    NUM_LEGIT_COMMITTEES, NUM_ILLEGAL_COMMITTEES, COMMITTEE_SELECT_COUNT,
    COMMITTEE_INITIAL_WEIGHT, NUM_RETAIL_USERS, NUM_EXCHANGE_USERS,
    NUM_SMALL_TASK_ATTACKERS, NUM_LARGE_TASK_ATTACKERS, SMALL_TASK_AMOUNT,
    LARGE_TASK_AMOUNT, TASK_TIME_LEFT, ETH_ADDRESS_FILE, ETH_ADDRESS_TRAIN_FILE
)
from config.experiment_config import ATTACK_MODE


class SimulationInitializer:
    """负责初始化模拟系统的所有组件"""
    
    def __init__(self):
        self.candidates = []
        self.committees = []
        self.normal_users = []
        self.attack_users = []
        self.reference_illegal_samples = []
        self.reference_legitimate_samples = []
    
    def initialize_committees(self):
        """初始化委员会候选人"""
        self.candidates = []
        
        for _ in range(NUM_LEGIT_COMMITTEES):
            self.candidates.append(Committee(COMMITTEE_INITIAL_WEIGHT, "Legit", "LLM"))
        
        for _ in range(NUM_ILLEGAL_COMMITTEES):
            self.candidates.append(Committee(COMMITTEE_INITIAL_WEIGHT, "Illegal", "LLM"))
        
        self.committees = select_committee(self.candidates, COMMITTEE_SELECT_COUNT)
        print(f"✓ 初始化了 {len(self.candidates)} 个委员会候选人，选中 {len(self.committees)} 个")
    
    def initialize_users(self):
        """初始化用户（正常用户和攻击用户）"""
        # 加载地址数据
        addr_df = pd.read_csv(ETH_ADDRESS_FILE, header=None)
        addr_df_train = pd.read_csv(ETH_ADDRESS_TRAIN_FILE, header=None)
        
        # 初始化正常用户
        self.normal_users = []
        for i in range(NUM_RETAIL_USERS):
            self.normal_users.append(
                RetailUser(index=20000 - i, address=addr_df_train.iloc[20000 - i, 0])
            )
            if i % 100 == 0:
                print(f'=======    normal user loading {i}    =======')
        
        # 添加交易所用户
        self.normal_users.extend([
            ExchangeUser(index=i * 199, address=addr_df.iloc[i * 199, 0]) 
            for i in range(NUM_EXCHANGE_USERS)
        ])
        
        # 标记部分用户为白名单
        for i in range(19985, 19999):
            if i < len(self.normal_users):
                self.normal_users[i].is_marked = True
        
        # 初始化攻击用户
        self.attack_users = []
        if ATTACK_MODE == "rule":
            for i in range(NUM_SMALL_TASK_ATTACKERS):
                self.attack_users.append(
                    Rule_Based_User(task=SMALL_TASK_AMOUNT, time_left=TASK_TIME_LEFT, index=i)
                )
            for i in range(NUM_LARGE_TASK_ATTACKERS):
                self.attack_users.append(
                    Rule_Based_User(task=LARGE_TASK_AMOUNT, time_left=TASK_TIME_LEFT, index=i + 8)
                )
        elif ATTACK_MODE == "llm":
            for i in range(NUM_SMALL_TASK_ATTACKERS):
                self.attack_users.append(
                    LLM_Based_User(task=SMALL_TASK_AMOUNT, time_left=TASK_TIME_LEFT, index=i)
                )
            for i in range(NUM_LARGE_TASK_ATTACKERS):
                self.attack_users.append(
                    LLM_Based_User(task=LARGE_TASK_AMOUNT, time_left=TASK_TIME_LEFT, index=i + 8)
                )
        
        print(f"✓ 初始化了 {len(self.normal_users)} 个正常用户，{len(self.attack_users)} 个攻击用户")
    
    def initialize_reference_samples(self):
        """初始化参考样本"""
        print("=" * 60)
        print("加载参考用户交易记录...")
        print("=" * 60)
        
        self.reference_illegal_samples, self.reference_legitimate_samples = load_reference_samples()
        
        # 如果加载失败或文件不存在，则生成新的参考样本
        if not self.reference_illegal_samples or not self.reference_legitimate_samples:
            print("参考样本文件不存在或加载失败，开始生成新的参考样本...")
            addr_df = pd.read_csv(ETH_ADDRESS_FILE, header=None)
            addr_df_train = pd.read_csv(ETH_ADDRESS_TRAIN_FILE, header=None)
            self.reference_illegal_samples, self.reference_legitimate_samples = generate_reference_samples(
                addr_df, addr_df_train
            )
        else:
            print("✓ 参考样本加载完成，跳过生成步骤")
            print("=" * 60)
    
    def initialize_all(self):
        """初始化所有组件"""
        self.initialize_committees()
        self.initialize_users()
        self.initialize_reference_samples()
        return {
            'candidates': self.candidates,
            'committees': self.committees,
            'normal_users': self.normal_users,
            'attack_users': self.attack_users,
            'reference_illegal_samples': self.reference_illegal_samples,
            'reference_legitimate_samples': self.reference_legitimate_samples
        }
