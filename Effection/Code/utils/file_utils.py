"""文件操作工具函数"""
from collections import defaultdict


def save_committee_info(committee, filename):
    """保存委员会信息到文件"""
    with open(filename, "w", encoding="utf-8") as f:
        for i, cs in enumerate(committee):
            f.write(
                f"#{i} | type={cs.identity_type}, "
                f"weight={cs.weight}, rep={cs.rep}\n"
            )


def save_user_info(users, filename):
    """保存用户信息到文件"""
    with open(filename, "w", encoding="utf-8") as f:
        for i, u in enumerate(users):
            f.write(
                f"#{i} | alive={u.is_alive}, task={u.task}\n"
            )


def save_transactions_to_file(user_obj, index, folder):
    """保存用户交易记录到文件"""
    filename = f"{folder}/{user_obj.type}_{index}.txt"

    # 按 time 分组，保留每次交易的 amount
    time_to_amounts = defaultdict(list)

    for r in user_obj.transaction_simple:
        t = r["time"]
        a = r["amount"]
        time_to_amounts[t].append(a)

    # 按时间排序写入
    with open(filename, "w", encoding="utf-8") as f:
        for t in sorted(time_to_amounts.keys()):
            amounts_str = ",".join(str(a) for a in time_to_amounts[t])
            f.write(f"time={t}, amount={amounts_str}\n")
