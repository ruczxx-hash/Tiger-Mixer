"""统计和记录模块"""
from config.experiment_config import (
    ATTACK_MODE, RESULT_DIR, AUDITOR_STATS_FILE, COMMITTEE_VOTE_STATS_FILE,
    STATIC_DATA_FILE, PRECISION_FILE
)


class StatisticsCollector:
    """负责收集和记录统计信息"""
    
    def __init__(self, total_attackers):
        self.total_attackers = total_attackers
        self.total_banned_attackers = 0
        self.total_marked_normal = 0
        
        # 三层拦截机制的累计统计变量
        self.cumulative_if_banned_attackers = 0
        self.cumulative_if_banned_total = 0
        self.cumulative_auditor_committee_attackers = 0
        self.cumulative_auditor_committee_total = 0
        self.cumulative_committee_true_attackers = 0
        self.cumulative_committee_true_total = 0
    
    def calculate_precision_recall(self, first_result, second_result, third_result, 
                                   attack_users, second_banned_users):
        """计算各层级的precision和recall"""
        # 第一道关卡
        first_precision = (first_result['first_attacker_count'] / first_result['first_total'] 
                          if first_result['first_total'] > 0 else 0.0)
        left_attackers = sum(1 for u in attack_users if u.is_alive)
        first_recall = (first_result['first_attacker_count'] / left_attackers 
                       if left_attackers > 0 else 0.0)
        
        # 第二道关卡
        second_precision = (second_result['second_attacker_count'] / second_result['second_total'] 
                           if second_result['second_total'] > 0 else 0.0)
        left_attackers_second = sum(1 for u in second_banned_users if u.type != "NormalUser")
        second_recall = (second_result['second_attacker_count'] / left_attackers_second 
                        if left_attackers_second > 0 else 0.0)
        
        # 第三道关卡
        third_total = third_result['third_normal_banned'] + third_result['third_attackers_banned']
        third_precision = (third_result['third_attackers_banned'] / third_total 
                          if third_total > 0 else 0.0)
        third_recall = (third_result['third_attackers_banned'] / len(second_result['banned_attackers']) 
                       if len(second_result['banned_attackers']) > 0 else 0.0)
        
        # 总的precision和recall
        total_precision = (self.total_banned_attackers / (self.total_banned_attackers + self.total_marked_normal) 
                          if (self.total_banned_attackers + self.total_marked_normal) > 0 else 0.0)
        total_recall = (self.total_banned_attackers / self.total_attackers 
                       if self.total_attackers > 0 else 0.0)
        
        # 三层拦截机制的累计precision
        if_precision = (self.cumulative_if_banned_attackers / self.cumulative_if_banned_total 
                       if self.cumulative_if_banned_total > 0 else 0.0)
        auditor_precision = (self.cumulative_auditor_committee_attackers / self.cumulative_auditor_committee_total 
                           if self.cumulative_auditor_committee_total > 0 else 0.0)
        committee_precision = (self.cumulative_committee_true_attackers / self.cumulative_committee_true_total 
                               if self.cumulative_committee_true_total > 0 else 0.0)
        
        return {
            'first_precision': first_precision,
            'first_recall': first_recall,
            'second_precision': second_precision,
            'second_recall': second_recall,
            'third_precision': third_precision,
            'third_recall': third_recall,
            'total_precision': total_precision,
            'total_recall': total_recall,
            'if_precision': if_precision,
            'auditor_precision': auditor_precision,
            'committee_precision': committee_precision
        }
    
    def update_cumulative_stats(self, first_result, second_result, third_result):
        """更新累计统计"""
        self.cumulative_if_banned_attackers += first_result['first_attacker_count']
        self.cumulative_if_banned_total += first_result['first_total']
        self.cumulative_auditor_committee_attackers += len(second_result['banned_attackers'])
        self.cumulative_auditor_committee_total += len(second_result['banned_users'])
        self.cumulative_committee_true_total += (third_result['third_normal_banned'] + 
                                                third_result['third_attackers_banned'])
        self.cumulative_committee_true_attackers += third_result['third_attackers_banned']
    
    def calculate_auditor_statistics(self, auditor_decisions, committee_decisions, 
                                     normal_banned, banned_attackers):
        """计算auditor和委员会决策不一致的统计"""
        auditor_judged_count = len(auditor_decisions)
        committee_judged_count = len(normal_banned) + len(banned_attackers)
        
        # 总体统计
        inconsistent_count = 0
        total_comparable = 0
        for u in normal_banned + banned_attackers:
            if u in auditor_decisions and u in committee_decisions:
                total_comparable += 1
                if auditor_decisions[u] != committee_decisions[u]:
                    inconsistent_count += 1
        inconsistent_ratio = inconsistent_count / total_comparable if total_comparable > 0 else 0.0
        
        # 合法用户统计
        normal_auditor_decisions = {u: d for u, d in auditor_decisions.items() 
                                   if u.type == "NormalUser"}
        normal_auditor_judged_count = len(normal_auditor_decisions)
        normal_committee_judged_count = len(normal_banned)
        normal_inconsistent_count = 0
        normal_total_comparable = 0
        for u in normal_banned:
            if u in normal_auditor_decisions and u in committee_decisions:
                normal_total_comparable += 1
                if normal_auditor_decisions[u] != committee_decisions[u]:
                    normal_inconsistent_count += 1
        normal_inconsistent_ratio = (normal_inconsistent_count / normal_total_comparable 
                                    if normal_total_comparable > 0 else 0.0)
        
        # 非法用户统计
        illegal_auditor_decisions = {u: d for u, d in auditor_decisions.items() 
                                    if u.type != "NormalUser"}
        illegal_auditor_judged_count = len(illegal_auditor_decisions)
        illegal_committee_judged_count = len(banned_attackers)
        illegal_inconsistent_count = 0
        illegal_total_comparable = 0
        for u in banned_attackers:
            if u in illegal_auditor_decisions and u in committee_decisions:
                illegal_total_comparable += 1
                if illegal_auditor_decisions[u] != committee_decisions[u]:
                    illegal_inconsistent_count += 1
        illegal_inconsistent_ratio = (illegal_inconsistent_count / illegal_total_comparable 
                                     if illegal_total_comparable > 0 else 0.0)
        
        return {
            'auditor_judged_count': auditor_judged_count,
            'committee_judged_count': committee_judged_count,
            'inconsistent_count': inconsistent_count,
            'total_comparable': total_comparable,
            'inconsistent_ratio': inconsistent_ratio,
            'normal_auditor_judged_count': normal_auditor_judged_count,
            'normal_committee_judged_count': normal_committee_judged_count,
            'normal_inconsistent_count': normal_inconsistent_count,
            'normal_total_comparable': normal_total_comparable,
            'normal_inconsistent_ratio': normal_inconsistent_ratio,
            'illegal_auditor_judged_count': illegal_auditor_judged_count,
            'illegal_committee_judged_count': illegal_committee_judged_count,
            'illegal_inconsistent_count': illegal_inconsistent_count,
            'illegal_total_comparable': illegal_total_comparable,
            'illegal_inconsistent_ratio': illegal_inconsistent_ratio
        }
    
    def save_statistics(self, current_time, precision_recall, auditor_stats, 
                       attacker_vote_stats, second_result, third_result):
        """保存所有统计数据到文件"""
        # 保存precision和recall
        with open(STATIC_DATA_FILE, "a", encoding="utf-8") as f:
            f.write(f"time={current_time}, "
                   f"first_precision={precision_recall['first_precision']:.6f}, "
                   f"first_recall={precision_recall['first_recall']:.6f}, "
                   f"second_precision={precision_recall['second_precision']:.6f}, "
                   f"second_recall={precision_recall['second_recall']:.6f}, "
                   f"third_precision={precision_recall['third_precision']:.6f}, "
                   f"third_recall={precision_recall['third_recall']:.6f}, "
                   f"total_precision={precision_recall['total_precision']:.6f}, "
                   f"total_recall={precision_recall['total_recall']:.6f}\n")
        
        # 保存三层拦截机制的累计precision
        with open(PRECISION_FILE, "a", encoding="utf-8") as f:
            f.write(f"time={current_time}, "
                   f"IF_precision={precision_recall['if_precision']:.6f}, "
                   f"Auditor_precision={precision_recall['auditor_precision']:.6f}, "
                   f"Committee_precision={precision_recall['committee_precision']:.6f}\n")
        
        # 保存auditor统计数据（总体）
        with open(AUDITOR_STATS_FILE, "a", encoding="utf-8") as f:
            f.write(f"time={current_time}, "
                   f"auditor_judged_count={auditor_stats['auditor_judged_count']}, "
                   f"committee_judged_count={auditor_stats['committee_judged_count']}, "
                   f"inconsistent_count={auditor_stats['inconsistent_count']}, "
                   f"total_comparable={auditor_stats['total_comparable']}, "
                   f"inconsistent_ratio={auditor_stats['inconsistent_ratio']:.6f}\n")
        
        # 保存合法用户的auditor统计数据
        AUDITOR_STATS_NORMAL_FILE = f"{RESULT_DIR}/auditor_stats_normal.txt"
        with open(AUDITOR_STATS_NORMAL_FILE, "a", encoding="utf-8") as f:
            f.write(f"time={current_time}, "
                   f"auditor_judged_count={auditor_stats['normal_auditor_judged_count']}, "
                   f"committee_judged_count={auditor_stats['normal_committee_judged_count']}, "
                   f"inconsistent_count={auditor_stats['normal_inconsistent_count']}, "
                   f"total_comparable={auditor_stats['normal_total_comparable']}, "
                   f"inconsistent_ratio={auditor_stats['normal_inconsistent_ratio']:.6f}\n")
        
        # 保存非法用户的auditor统计数据
        AUDITOR_STATS_ILLEGAL_FILE = f"{RESULT_DIR}/auditor_stats_illegal.txt"
        with open(AUDITOR_STATS_ILLEGAL_FILE, "a", encoding="utf-8") as f:
            f.write(f"time={current_time}, "
                   f"auditor_judged_count={auditor_stats['illegal_auditor_judged_count']}, "
                   f"committee_judged_count={auditor_stats['illegal_committee_judged_count']}, "
                   f"inconsistent_count={auditor_stats['illegal_inconsistent_count']}, "
                   f"total_comparable={auditor_stats['illegal_total_comparable']}, "
                   f"inconsistent_ratio={auditor_stats['illegal_inconsistent_ratio']:.6f}\n")
        
        # 保存委员会投票统计数据
        if attacker_vote_stats:
            with open(COMMITTEE_VOTE_STATS_FILE, "a", encoding="utf-8") as f:
                f.write(f"time={current_time}, "
                       f"illegal_user_count={len(attacker_vote_stats)}\n")
                for stat in attacker_vote_stats:
                    f.write(f"  user_index={stat['user_index']}, "
                           f"legit_approve={stat['legit_approve_count']}/{stat['legit_total']} "
                           f"(ratio={stat['legit_approve_ratio']:.6f}), "
                           f"illegal_approve={stat['illegal_approve_count']}/{stat['illegal_total']} "
                           f"(ratio={stat['illegal_approve_ratio']:.6f}), "
                           f"final_result={stat['final_result']}\n")
    
    def print_statistics(self, current_time, precision_recall, auditor_stats):
        """打印统计信息"""
        print(f"[{ATTACK_MODE}] time = {current_time}, Statistics saved:")
        print(f"  第一道关卡: precision={precision_recall['first_precision']:.6f}, "
              f"recall={precision_recall['first_recall']:.6f}")
        print(f"  第二道关卡: precision={precision_recall['second_precision']:.6f}, "
              f"recall={precision_recall['second_recall']:.6f}")
        print(f"  第三道关卡: precision={precision_recall['third_precision']:.6f}, "
              f"recall={precision_recall['third_recall']:.6f}")
        print(f"  总计: precision={precision_recall['total_precision']:.6f}, "
              f"recall={precision_recall['total_recall']:.6f}")
        print(f"  三层拦截机制累计precision: IF层={precision_recall['if_precision']:.6f}, "
              f"Auditor层={precision_recall['auditor_precision']:.6f}, "
              f"Committee层={precision_recall['committee_precision']:.6f}")
        print(f"  Auditor统计(总体): 评判用户数={auditor_stats['auditor_judged_count']}, "
              f"交给委员会={auditor_stats['committee_judged_count']}, "
              f"决策不一致比例={auditor_stats['inconsistent_ratio']:.6f}")
        print(f"  Auditor统计(合法用户): 评判用户数={auditor_stats['normal_auditor_judged_count']}, "
              f"交给委员会={auditor_stats['normal_committee_judged_count']}, "
              f"决策不一致比例={auditor_stats['normal_inconsistent_ratio']:.6f}")
        print(f"  Auditor统计(非法用户): 评判用户数={auditor_stats['illegal_auditor_judged_count']}, "
              f"交给委员会={auditor_stats['illegal_committee_judged_count']}, "
              f"决策不一致比例={auditor_stats['illegal_inconsistent_ratio']:.6f}")
