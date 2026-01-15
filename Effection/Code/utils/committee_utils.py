"""委员会相关工具函数"""
import random


def select_committee(candidates, count_=10):
    """
    从候选人中根据权重选择委员会成员
    
    参数:
        candidates: 候选人列表
        count_: 需要选择的委员会成员数量（默认10）
    
    返回:
        选中的委员会成员列表
    
    异常:
        ValueError: 当候选人数量不足时抛出
    """
    if count_ > len(candidates):
        raise ValueError("候选人数量不足")

    pool = list(candidates)
    selected = []

    for _ in range(count_):
        weights = [cs.weight for cs in pool]
        chosen = random.choices(pool, weights=weights, k=1)[0]
        selected.append(chosen)
        pool.remove(chosen)

    return selected
