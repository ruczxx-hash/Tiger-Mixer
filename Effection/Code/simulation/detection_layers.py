"""三层拦截机制处理模块"""
from concurrent.futures import ThreadPoolExecutor, as_completed
from monitors.crypto_monitor import CryptoMonitor
from config.experiment_config import ATTACK_MODE, RESULT_DIR


class DetectionLayers:
    """处理三层拦截机制：IF层、Auditor层、Committee层"""
    
    def __init__(self, committees, reference_illegal_samples, reference_legitimate_samples):
        self.committees = committees
        self.reference_illegal_samples = reference_illegal_samples
        self.reference_legitimate_samples = reference_legitimate_samples
        self.auditor = None
    
    def _process_user(self, u):
        """并行处理单个用户的决策和tick"""
        try:
            if u.is_alive:
                u.Decide()
                u.tick()
        except Exception as e:
            print(f'[{ATTACK_MODE}] Error processing user {u.index} ({u.type}): {e}')
    
    def process_all_users(self, normal_users, attack_users):
        """处理所有用户的决策和tick"""
        # 处理正常用户
        for u in normal_users:
            if u.is_alive:
                u.Decide()
                u.tick()
        
        # 处理攻击用户
        if attack_users and attack_users[0].type == "Rule_Based_User":
            for u in attack_users:
                if u.is_alive:
                    u.Decide()
                    u.tick()
        else:
            with ThreadPoolExecutor(max_workers=min(5, len(attack_users))) as executor:
                executor.map(self._process_user, attack_users)
    
    def first_layer_if_audit(self, all_suspension, current_time):
        """第一道关卡：孤独森林拦截 (batch_audit)"""
        if self.auditor is None:
            self.auditor = CryptoMonitor(all_suspension, f"{RESULT_DIR}/auditor.txt")
        
        first_banned_users = self.auditor.batch_audit(all_suspension)
        print(f"[{ATTACK_MODE}] time = {current_time}, first_banned_users: {len(first_banned_users)}")
        
        # 统计第一道关卡的拦截数据
        first_normal = [u for u in first_banned_users if u.type == "NormalUser"]
        first_attackers = [u for u in first_banned_users if u.type != "NormalUser"]
        first_total = len(first_banned_users)
        first_attacker_count = len(first_attackers)
        first_normal_count = len(first_normal)
        
        print(f"[{ATTACK_MODE}] time = {current_time}, first_normal_count: {first_normal_count}, "
              f"first_attacker_count: {first_attacker_count}, first_total: {first_total}")
        
        return {
            'first_banned_users': first_banned_users,
            'first_normal_count': first_normal_count,
            'first_attacker_count': first_attacker_count,
            'first_total': first_total
        }
    
    def _detect_user(self, u, current_time):
        """并行检测单个用户"""
        try:
            print(f'[{ATTACK_MODE}] time = {current_time}, 开始检测用户 {u.index} ({u.type})...', flush=True)
            result = self.auditor.detect(u)
            print(f'[{ATTACK_MODE}] time = {current_time}, 用户 {u.index} ({u.type}) 检测结果: {result}', flush=True)
            return (u, result)
        except Exception as e:
            print(f'[{ATTACK_MODE}] Error detecting user {u.index}: {e}', flush=True)
            return (u, False)
    
    def second_layer_auditor_detect(self, first_banned_users, current_time):
        """第二道关卡：auditor 大模型判断 (detect)"""
        banned_users = []
        auditor_decisions = {}  # {user: auditor_decision(bool)}
        
        # 在第二层开始处过滤白名单用户（is_marked检查）
        users_to_detect = [u for u in first_banned_users if u.is_alive and not u.is_marked]
        print(f"[{ATTACK_MODE}] time = {current_time}, 过滤白名单后剩余用户: {len(users_to_detect)} (原始: {len(first_banned_users)})")
        
        if users_to_detect:
            print(f'[{ATTACK_MODE}] time = {current_time}, 开始并行检测 {len(users_to_detect)} 个用户...', flush=True)
            with ThreadPoolExecutor(max_workers=min(3, len(users_to_detect))) as executor:
                future_to_user = {executor.submit(self._detect_user, u, current_time): u 
                                 for u in users_to_detect}
                for future in as_completed(future_to_user):
                    u, is_banned = future.result()
                    # 检测完成后再次检查is_marked（防止检测过程中被标记）
                    if u.is_marked:
                        continue
                    auditor_decisions[u] = is_banned
                    if is_banned:
                        banned_users.append(u)
                        print(f'[{ATTACK_MODE}] time = {current_time}, {u.type} user {u.index} 被禁止', flush=True)
        
        print(f"[{ATTACK_MODE}] time = {current_time}, banned_users: {len(banned_users)}")
        
        # 统计第二道关卡的拦截数据
        second_normal = [u for u in banned_users if u.type == "NormalUser"]
        second_attackers = [u for u in banned_users if u.type != "NormalUser"]
        second_total = len(banned_users)
        second_attacker_count = len(second_attackers)
        second_normal_count = len(second_normal)
        
        return {
            'banned_users': banned_users,
            'normal_banned': second_normal,
            'banned_attackers': second_attackers,
            'auditor_decisions': auditor_decisions,
            'second_normal_count': second_normal_count,
            'second_attacker_count': second_attacker_count,
            'second_total': second_total
        }
    
    def _vote_committee(self, c, u, current_time):
        """单个委员会成员的投票"""
        try:
            print(f'[{ATTACK_MODE}] time = {current_time}, 委员会成员 {c.identity_type} ({c.judge_type}) '
                  f'开始投票用户 {u.index}...', flush=True)
            result = c.Vote(u, current_time, self.reference_legitimate_samples, self.reference_illegal_samples)
            print(f'[{ATTACK_MODE}] time = {current_time}, 委员会成员 {c.identity_type} ({c.judge_type}) '
                  f'投票结果: {result} (用户 {u.index})', flush=True)
            return (c, result)
        except Exception as e:
            print(f'[{ATTACK_MODE}] Error voting for user {u.index} by committee: {e}', flush=True)
            return (c, False)
    
    def _vote_for_user(self, u, current_time):
        """并行投票处理单个用户"""
        votes_t = []
        count = 0
        
        if self.committees:
            with ThreadPoolExecutor(max_workers=min(3, len(self.committees))) as executor:
                future_to_committee = {executor.submit(self._vote_committee, c, u, current_time): c 
                                      for c in self.committees}
                for future in as_completed(future_to_committee):
                    c = future_to_committee[future]
                    try:
                        c_obj, v = future.result()
                        if v:
                            count += 1
                        c.vote.append(v)
                        votes_t.append(v)
                    except Exception as e:
                        print(f'[{ATTACK_MODE}] Error getting vote result: {e}')
                        c.vote.append(False)
                        votes_t.append(False)
        
        return (u, votes_t, count)
    
    def _vote_for_attacker(self, u, current_time):
        """并行投票处理单个攻击者用户"""
        votes_t = []
        count = 0
        committee_votes = []  # [(committee, vote_result, identity_type)]
        
        if self.committees:
            with ThreadPoolExecutor(max_workers=min(10, len(self.committees))) as executor:
                future_to_committee = {executor.submit(self._vote_committee, c, u, current_time): c 
                                      for c in self.committees}
                for future in as_completed(future_to_committee):
                    c = future_to_committee[future]
                    try:
                        c_obj, v = future.result()
                        if v:
                            count += 1
                        c.vote.append(v)
                        votes_t.append(v)
                        committee_votes.append((c_obj, v, c_obj.identity_type))
                    except Exception as e:
                        print(f'[{ATTACK_MODE}] Error getting vote result: {e}')
                        c.vote.append(False)
                        votes_t.append(False)
                        committee_votes.append((c, False, c.identity_type))
        
        return (u, votes_t, count, committee_votes)
    
    def third_layer_committee_vote(self, normal_banned, banned_attackers, current_time):
        """第三道关卡：委员会判断"""
        votes = []
        decision = []
        answer = []
        committee_decisions = {}  # {user: committee_decision(bool)}
        third_normal_banned = 0
        third_attackers_banned = 0
        attacker_vote_stats = []
        
        # 处理正常用户
        if normal_banned:
            with ThreadPoolExecutor(max_workers=min(3, len(normal_banned))) as executor:
                future_to_user = {executor.submit(self._vote_for_user, u, current_time): u 
                                 for u in normal_banned}
                for future in as_completed(future_to_user):
                    u, votes_t, count = future.result()
                    answer.append(False)
                    
                    committee_decision = count > 5
                    committee_decisions[u] = committee_decision
                    
                    if count > 5:
                        decision.append(True)
                        third_normal_banned += 1
                        u.is_marked = True
                    else:
                        decision.append(False)
                    
                    votes.append(votes_t)
        
        # 处理攻击用户
        if banned_attackers:
            with ThreadPoolExecutor(max_workers=min(5, len(banned_attackers))) as executor:
                future_to_user = {executor.submit(self._vote_for_attacker, u, current_time): u 
                                 for u in banned_attackers}
                for future in as_completed(future_to_user):
                    u, votes_t, count, committee_votes = future.result()
                    answer.append(True)
                    
                    # 统计合法委员和非法委员的投票情况
                    legit_votes = [v for _, v, identity in committee_votes if identity == "Legit"]
                    illegal_votes = [v for _, v, identity in committee_votes if identity == "Illegal"]
                    legit_approve_count = sum(legit_votes)
                    illegal_approve_count = sum(illegal_votes)
                    legit_approve_ratio = legit_approve_count / len(legit_votes) if len(legit_votes) > 0 else 0.0
                    illegal_approve_ratio = illegal_approve_count / len(illegal_votes) if len(illegal_votes) > 0 else 0.0
                    
                    committee_decision = count > 5
                    committee_decisions[u] = committee_decision
                    
                    attacker_vote_stats.append({
                        'user_index': u.index,
                        'legit_approve_count': legit_approve_count,
                        'legit_total': len(legit_votes),
                        'legit_approve_ratio': legit_approve_ratio,
                        'illegal_approve_count': illegal_approve_count,
                        'illegal_total': len(illegal_votes),
                        'illegal_approve_ratio': illegal_approve_ratio,
                        'final_result': committee_decision
                    })
                    
                    if count > 5:
                        decision.append(True)
                        u.is_alive = False
                        third_attackers_banned += 1
                    else:
                        decision.append(False)
                    
                    votes.append(votes_t)
        
        return {
            'votes': votes,
            'decision': decision,
            'answer': answer,
            'committee_decisions': committee_decisions,
            'third_normal_banned': third_normal_banned,
            'third_attackers_banned': third_attackers_banned,
            'attacker_vote_stats': attacker_vote_stats
        }
