"""参考样本处理工具"""
import os
import json
import pandas as pd
from config.experiment_config import ATTACK_MODE, REFERENCE_ILLEGAL_FILE, REFERENCE_LEGITIMATE_FILE
from models.user import RetailUser, ExchangeUser, Rule_Based_User, LLM_Based_User


def load_reference_samples():
    """从本地文件加载参考用户交易记录"""
    reference_illegal_samples = []
    reference_legitimate_samples = []
    
    # 尝试加载非法用户参考样本
    if os.path.exists(REFERENCE_ILLEGAL_FILE):
        try:
            with open(REFERENCE_ILLEGAL_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
                saved_attack_mode = data.get('attack_mode', '')
                # 检查保存的攻击模式是否与当前模式匹配
                if saved_attack_mode == ATTACK_MODE:
                    reference_illegal_samples = data.get('samples', [])
                    print(f"✓ 成功加载非法用户参考样本 ({ATTACK_MODE}模式): {len(reference_illegal_samples)} 个样本")
                    if reference_illegal_samples:
                        print(f"  第一个样本交易记录数: {len(reference_illegal_samples[0])}")
                else:
                    print(f"⚠ 保存的非法用户参考样本模式 ({saved_attack_mode}) 与当前模式 ({ATTACK_MODE}) 不匹配，需要重新生成")
        except Exception as e:
            print(f"✗ 加载非法用户参考样本失败: {e}")
    
    # 尝试加载合法用户参考样本
    if os.path.exists(REFERENCE_LEGITIMATE_FILE):
        try:
            with open(REFERENCE_LEGITIMATE_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
                reference_legitimate_samples = data.get('samples', [])
            print(f"✓ 成功加载合法用户参考样本: {len(reference_legitimate_samples)} 个样本")
            for i, sample in enumerate(reference_legitimate_samples):
                print(f"  合法用户{i+1}交易记录数: {len(sample)}")
        except Exception as e:
            print(f"✗ 加载合法用户参考样本失败: {e}")
    
    return reference_illegal_samples, reference_legitimate_samples


def save_reference_samples(reference_illegal_samples, reference_legitimate_samples):
    """保存参考用户交易记录到本地文件"""
    # 保存非法用户参考样本
    try:
        illegal_data = {
            'attack_mode': ATTACK_MODE,
            'samples': reference_illegal_samples
        }
        with open(REFERENCE_ILLEGAL_FILE, 'w', encoding='utf-8') as f:
            json.dump(illegal_data, f, indent=2, ensure_ascii=False)
        print(f"✓ 成功保存非法用户参考样本到: {REFERENCE_ILLEGAL_FILE}")
    except Exception as e:
        print(f"✗ 保存非法用户参考样本失败: {e}")
    
    # 保存合法用户参考样本
    try:
        legitimate_data = {
            'samples': reference_legitimate_samples
        }
        with open(REFERENCE_LEGITIMATE_FILE, 'w', encoding='utf-8') as f:
            json.dump(legitimate_data, f, indent=2, ensure_ascii=False)
        print(f"✓ 成功保存合法用户参考样本到: {REFERENCE_LEGITIMATE_FILE}")
    except Exception as e:
        print(f"✗ 保存合法用户参考样本失败: {e}")


def generate_reference_samples(addr_df, addr_df_train):
    """
    生成参考用户交易记录
    
    参数:
        addr_df: 地址数据DataFrame（用于ExchangeUser）
        addr_df_train: 地址数据DataFrame（用于RetailUser）
    
    返回:
        tuple: (reference_illegal_samples, reference_legitimate_samples)
    """
    print("=" * 60)
    print("生成参考用户交易记录...")
    print("=" * 60)
    
    # 1个非法用户参考样本
    if ATTACK_MODE == "rule":
        reference_illegal_user = Rule_Based_User(10000, 4320, 0, -1)  # task, time_left, current_time, index=-1表示参考用户
    else:
        reference_illegal_user = LLM_Based_User(10000, 4320, 0, -1)

    # 3个detailed类型合法用户（RetailUser）和1个exchanged类型合法用户（ExchangeUser）
    reference_legitimate_users = []
    # 3个RetailUser（使用不同的索引避免冲突，使用addr_df_train的末尾索引）
    for i in range(3):
        # 使用addr_df_train的索引，从末尾开始选择，确保不越界
        addr_idx = len(addr_df_train) - 1 - i  # 从末尾倒序选择
        ref_user = RetailUser(index=30000 + i, address=addr_df_train.iloc[addr_idx, 0])
        reference_legitimate_users.append(ref_user)
    # 1个ExchangeUser（使用addr_df的末尾索引）
    addr_idx = len(addr_df) - 1
    ref_exchange_user = ExchangeUser(index=30003, address=addr_df.iloc[addr_idx, 0])
    reference_legitimate_users.append(ref_exchange_user)

    # 运行参考用户120分钟收集交易记录
    print("运行参考用户120分钟收集交易记录...")
    for ref_time in range(600):
        # 运行非法参考用户
        if reference_illegal_user.is_alive:
            reference_illegal_user.Decide()
            reference_illegal_user.tick()
        
        # 运行合法参考用户
        for ref_user in reference_legitimate_users:
            if ref_user.is_alive:
                ref_user.Decide()
                ref_user.tick()
        
        if (ref_time + 1) % 30 == 0:
            print(f"参考用户运行进度: {ref_time + 1}/600")

    # 收集参考用户的交易记录
    reference_illegal_samples = [reference_illegal_user.transaction_simple.copy()] if reference_illegal_user.transaction_simple else []
    reference_legitimate_samples = [user.transaction_simple.copy() for user in reference_legitimate_users if user.transaction_simple]

    print(f"参考非法用户交易记录数: {len(reference_illegal_user.transaction_simple)}")
    print(f"参考合法用户数量: {len(reference_legitimate_samples)}")
    for i, user in enumerate(reference_legitimate_users):
        print(f"  参考合法用户{i+1} ({user.__class__.__name__}) 交易记录数: {len(user.transaction_simple)}")
    
    # 保存参考样本
    save_reference_samples(reference_illegal_samples, reference_legitimate_samples)
    print("=" * 60)
    
    return reference_illegal_samples, reference_legitimate_samples
