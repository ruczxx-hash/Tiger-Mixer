"""模拟器主逻辑模块"""
import sys
import os
from utils.committee_utils import select_committee
from utils.file_utils import save_committee_info, save_user_info, save_transactions_to_file
from config.experiment_config import (
    ATTACK_MODE, RESULT_DIR, TX_DIR_FOR_ATTACK, TX_DIR_FOR_NORMAL,
    STATIC_DATA_FILE, STATIC_DATA_DIR
)

# 添加父目录到路径，以便导入 draw 模块
# Code/simulation/simulator.py -> Code/ -> version_latest/
parent_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)
import draw


class Simulator:
    """模拟器主类，负责运行模拟循环"""
    
    def __init__(self, initializer, detection_layers, statistics_collector):
        self.initializer = initializer
        self.detection_layers = detection_layers
        self.statistics = statistics_collector
        
        self.normal_users = initializer.normal_users
        self.attack_users = initializer.attack_users
        self.candidates = initializer.candidates
        self.committees = initializer.committees
        
        self.current_time = 0
        self.decision = []
        self.answer = []
        self.transferred_total_records = []
        self.marked_normal = 0
    
    def update_committees(self):
        """更新委员会（每30分钟）"""
        if self.decision:
            for c in self.committees:
                c.update_rep(self.decision, self.answer)
            
            rep_min = min(c.rep for c in self.committees)
            rep_max = max(c.rep for c in self.committees)
            
            for c in self.committees:
                c.update_weight(rep_min, rep_max)
            
            self.decision.clear()
            self.answer.clear()
        
        self.committees = select_committee(self.candidates)
        self.detection_layers.committees = self.committees
    
    def record_transferred_total(self):
        """记录所有攻击用户的总转账金额"""
        transferred_total = 0
        for u in self.attack_users:
            for transaction in u.transaction_simple:
                transferred_total += transaction['amount']
        self.transferred_total_records.append((self.current_time, transferred_total))
        print(f"[{ATTACK_MODE}] time = {self.current_time}, transferred_total = {transferred_total}")
    
    def handle_llm_observations(self, banned_users):
        """处理LLM攻击用户的观察学习"""
        if ATTACK_MODE == "llm":
            for banned_user in banned_users:
                for other in self.attack_users:
                    other.observe_ban(banned_user)
    
    def run_simulation_step(self):
        """运行一个模拟步骤"""
        # 更新所有用户
        all_suspension = []
        all_suspension.extend([u for u in self.normal_users if u.is_alive])
        all_suspension.extend([u for u in self.attack_users if u.is_alive])
        
        # 处理所有用户的决策和tick
        self.detection_layers.process_all_users(self.normal_users, self.attack_users)
        
        # 每10分钟进行一次检测
        if self.current_time % 10 == 0 and self.current_time != 0:
            print(f"[{ATTACK_MODE}] time = {self.current_time}, all_suspension: {len(all_suspension)}")
            
            # 第一道关卡：IF层
            first_result = self.detection_layers.first_layer_if_audit(all_suspension, self.current_time)
            
            # 第二道关卡：Auditor层（在第一层结果基础上进行is_marked检查）
            second_result = self.detection_layers.second_layer_auditor_detect(
                first_result['first_banned_users'], self.current_time
            )
            
            # 第三道关卡：Committee层
            third_result = self.detection_layers.third_layer_committee_vote(
                second_result['normal_banned'], 
                second_result['banned_attackers'], 
                self.current_time
            )
            
            # 更新统计
            self.statistics.update_cumulative_stats(first_result, second_result, third_result)
            
            # 处理被拦截的用户
            banned_users_for_observation = []
            for u in third_result['committee_decisions']:
                if third_result['committee_decisions'][u]:
                    if u.type == "NormalUser":
                        u.is_marked = True
                        self.marked_normal += 1
                        self.statistics.total_marked_normal += 1
                        # 正常用户被标记时，LLM攻击用户也需要观察学习
                        banned_users_for_observation.append(u)
                    else:
                        u.is_alive = False
                        self.statistics.total_banned_attackers += 1
                        banned_users_for_observation.append(u)
            
            # 统一处理LLM观察学习
            if banned_users_for_observation:
                self.handle_llm_observations(banned_users_for_observation)
            
            # 更新decision和answer列表
            self.decision.extend(third_result['decision'])
            self.answer.extend(third_result['answer'])
            
            # 计算统计指标
            precision_recall = self.statistics.calculate_precision_recall(
                first_result, second_result, third_result, 
                self.attack_users, first_result['first_banned_users']
            )
            
            auditor_stats = self.statistics.calculate_auditor_statistics(
                second_result['auditor_decisions'],
                third_result['committee_decisions'],
                second_result['normal_banned'],
                second_result['banned_attackers']
            )
            
            # 保存和打印统计
            self.statistics.save_statistics(
                self.current_time, precision_recall, auditor_stats,
                third_result['attacker_vote_stats'], second_result, third_result
            )
            self.statistics.print_statistics(self.current_time, precision_recall, auditor_stats)
        
        # 每10分钟记录转账总额
        if self.current_time % 10 == 0:
            self.record_transferred_total()
        
        # 每30分钟更新委员会
        if self.current_time % 30 == 0:
            self.update_committees()
        
        self.current_time += 1
    
    def should_continue(self):
        """判断是否应该继续模拟"""
        # 检查是否还有存活的攻击用户
        for u in self.attack_users:
            if u.is_alive:
                return True
        return False
    
    def run(self, max_time=4320):
        """运行完整模拟"""
        print(f"\n{'='*60}")
        print(f"开始模拟 (最大时间: {max_time} 分钟)")
        print(f"{'='*60}\n")
        
        while self.current_time < max_time and self.should_continue():
            self.run_simulation_step()
            print(f"[{ATTACK_MODE}] time = {self.current_time}, "
                  f"marked_normal = {self.marked_normal}")
        
        print(f"\n{'='*60}")
        print("模拟完成")
        print(f"{'='*60}\n")
    
    def save_results(self):
        """保存结果"""
        # 保存委员会信息
        save_committee_info(self.candidates, f"{RESULT_DIR}/committee_info.txt")
        
        # 保存用户信息
        save_user_info(self.attack_users, f"{RESULT_DIR}/{ATTACK_MODE}_users_info.txt")
        
        # 保存交易记录
        for idx, u in enumerate(self.attack_users):
            save_transactions_to_file(u, idx, TX_DIR_FOR_ATTACK)
        
        for idx, u in enumerate(self.normal_users):
            save_transactions_to_file(u, idx, TX_DIR_FOR_NORMAL)
        
        # 绘制交易图表
        draw.plot_user_transactions(
            self.attack_users,
            f"{RESULT_DIR}/{ATTACK_MODE}_based_plot.pdf"
        )
        
        # 计算最终统计
        money_in_normal = sum(
            j['amount'] for i in self.normal_users for j in i.transaction_simple
        )
        money_in_attacker = sum(
            j['amount'] for i in self.attack_users for j in i.transaction_simple
        )
        ratio = money_in_normal / (money_in_attacker + 1)
        
        banned_in_normal = sum(1 for i in self.normal_users if not i.is_alive)
        marked_in_normal = sum(1 for i in self.normal_users if i.is_marked)
        banned_in_attacker = sum(1 for i in self.attack_users if not i.is_alive)
        marked_in_attacker = sum(1 for i in self.attack_users if i.is_marked)
        
        # 保存最终统计结果
        with open(f"{RESULT_DIR}/Result Statistic.txt", "a", encoding="utf-8") as f:
            f.write(f'Money Transferred Ration between Normal and Attacker: {ratio}\n'
                   f'Banned Normal User Num: {banned_in_normal}\n'
                   f'Banned Attacker User Num: {banned_in_attacker}\n'
                   f'Marked Normal User Num: {marked_in_normal}\n'
                   f'Marked Attacker User Num: {marked_in_attacker}')
        
        # 保存攻击用户总转账金额记录
        TRANSFERRED_TOTAL_FILE = f"{RESULT_DIR}/transferred_total_records.txt"
        with open(TRANSFERRED_TOTAL_FILE, "w", encoding="utf-8") as f:
            f.write("time,transferred_total\n")
            for time, total in self.transferred_total_records:
                f.write(f"{time},{total}\n")
        print(f"攻击用户总转账金额记录已保存到: {TRANSFERRED_TOTAL_FILE}")
        
        print(f"Money transfered by normal user is {money_in_normal}; "
              f"Money transferred by attackers is {money_in_attacker}\n. The ratio is {ratio}")
        print(f"\n✅ Experiment finished. Results saved to:\n{RESULT_DIR}")
        
        # 绘制 Precision 和 Recall 图表
        print("\n" + "=" * 60)
        print("绘制 Precision 和 Recall 图表...")
        print("=" * 60)
        draw.plot_precision_recall(STATIC_DATA_FILE, STATIC_DATA_DIR)
        print("=" * 60)
