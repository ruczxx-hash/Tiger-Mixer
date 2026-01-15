#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
地址洗钱风险检测系统
基于训练好的随机森林模型检测以太坊地址的洗钱嫌疑
支持单地址检测和批量检测
"""

import pandas as pd
import numpy as np
import json
import sys
import warnings
warnings.filterwarnings('ignore')

def load_model_and_scaler():
    """加载训练好的模型和标准化器"""
    try:
        import joblib
        
        # 加载模型
        model = joblib.load('rf_model.pkl')
        print("模型加载成功")
        
        # 加载标准化器
        scaler = joblib.load('scaler.pkl')
        print("标准化器加载成功")
        
        # 加载特征列名
        with open('feature_columns.json', 'r') as f:
            feature_columns = json.load(f)
        print("特征列名加载成功")
        
        return model, scaler, feature_columns
        
    except FileNotFoundError as e:
        print(f"错误：找不到模型文件 {e}")
        print("请先运行 RF.py 训练并保存模型")
        return None, None, None
    except ImportError:
        print("错误：未安装joblib库，请运行: pip install joblib")
        return None, None, None
    except Exception as e:
        print(f"模型加载失败: {e}")
        return None, None, None

def detect_address_risk(address, features_data, model, scaler, feature_columns):
    """检测指定地址的洗钱风险"""
    try:
        # 查找目标地址
        target_data = features_data[features_data['address'] == address]
        if len(target_data) == 0:
            return None
        
        # 准备特征
        features = target_data[feature_columns].values
        
        # 检查缺失值和无穷值
        if np.isnan(features).any():
            features = np.nan_to_num(features, nan=0.0)
        if np.isinf(features).any():
            features = np.where(np.isinf(features), 0.0, features)
        
        # 标准化特征
        features_scaled = scaler.transform(features)
        
        # 预测
        prediction = model.predict(features_scaled)[0]
        probabilities = model.predict_proba(features_scaled)[0]
        
        # 结果
        result = {
            'address': address,
            'prediction': '非法用户' if prediction == 1 else '合法用户',
            'illegal_probability': probabilities[1],
            'legal_probability': probabilities[0],
            'risk_assessment': get_risk_assessment(probabilities[1])
        }
        
        return result
        
    except Exception as e:
        print(f"地址 {address} 检测失败: {e}")
        return None

def get_risk_assessment(illegal_prob):
    """根据非法概率评估风险等级"""
    if illegal_prob >= 0.8:
        return "极高风险"
    elif illegal_prob >= 0.6:
        return "高风险"
    elif illegal_prob >= 0.4:
        return "中等风险"
    elif illegal_prob >= 0.2:
        return "低风险"
    else:
        return "极低风险"

def batch_detect_all_addresses(features_file, model, scaler, feature_columns, output_file=None):
    """批量检测特征文件中的所有地址"""
    try:
        print(f"正在加载特征文件: {features_file}")
        df_features = pd.read_csv(features_file)
        print(f"特征数据加载成功，共 {len(df_features)} 条记录")
        
        # 检测所有地址
        print(f"\n开始检测所有地址...")
        results = []
        
        for i, row in df_features.iterrows():
            address = row['address']
            print(f"[{i+1}/{len(df_features)}] 检测: {address}")
            
            result = detect_address_risk(address, df_features, model, scaler, feature_columns)
            if result:
                results.append(result)
                # 简化输出：只显示地址、预测结果和风险概率
                risk_level = "高风险" if result['illegal_probability'] >= 0.6 else "中等风险" if result['illegal_probability'] >= 0.4 else "低风险"
                print(f"   {result['prediction']} | 非法概率: {result['illegal_probability']:.4f} | 风险: {result['risk_assessment']}")
            else:
                print(f"   检测失败")
        
        # 汇总结果
        if results:
            print(f"\n检测完成，共检测 {len(results)} 个地址")
            
            # 分类统计
            illegal_users = [r for r in results if r['prediction'] == '非法用户']
            legal_users = [r for r in results if r['prediction'] == '合法用户']
            
            print(f"\n检测结果统计:")
            print(f"   非法用户: {len(illegal_users)} 个")
            print(f"   合法用户: {len(legal_users)} 个")
            
            # 按风险排序显示非法用户
            if illegal_users:
                print(f"\n非法用户列表 (按风险排序):")
                illegal_users.sort(key=lambda x: x['illegal_probability'], reverse=True)
                for i, user in enumerate(illegal_users, 1):
                    print(f"   {i:2d}. {user['address']} | 非法概率: {user['illegal_probability']:.4f} | 风险: {user['risk_assessment']}")
            
            # 按风险排序显示合法用户
            if legal_users:
                print(f"\n合法用户列表 (按风险排序):")
                legal_users.sort(key=lambda x: x['illegal_probability'], reverse=True)
                for i, user in enumerate(legal_users, 1):
                    print(f"   {i:2d}. {user['address']} | 非法概率: {user['illegal_probability']:.4f} | 风险: {user['risk_assessment']}")
            
            # 保存结果
            if output_file:
                save_results(results, output_file)
            else:
                save_results(results, 'all_addresses_detection_results.csv')
                print(f"\n检测结果已保存到: all_addresses_detection_results.csv")
            
            return results
        else:
            print("没有成功检测到任何地址")
            return []
            
    except FileNotFoundError:
        print(f"找不到特征文件: {features_file}")
        return []
    except Exception as e:
        print(f"批量检测失败: {e}")
        return []

def single_address_detect(address, features_file, model, scaler, feature_columns, output_file=None):
    """单地址检测"""
    try:
        print(f"正在加载特征文件: {features_file}")
        df_features = pd.read_csv(features_file)
        print(f"特征数据加载成功，共 {len(df_features)} 条记录")
        
        print(f"\n正在检测地址: {address}")
        result = detect_address_risk(address, df_features, model, scaler, feature_columns)
        
        if result:
            # 简化输出：只显示关键信息
            print(f"\n检测结果:")
            print(f"   地址: {result['address']}")
            print(f"   预测结果: {result['prediction']}")
            print(f"   非法概率: {result['illegal_probability']:.4f} ({result['illegal_probability']*100:.2f}%)")
            print(f"   合法概率: {result['legal_probability']:.4f} ({result['legal_probability']*100:.2f}%)")
            print(f"   风险评估: {result['risk_assessment']}")
            
            # 保存结果
            if output_file:
                save_results([result], output_file)
            else:
                save_results([result], f'detection_result_{address[:10]}.csv')
                print(f"\n检测结果已保存到: detection_result_{address[:10]}.csv")
            
            return result
        else:
            print(f"地址 {address} 检测失败")
            return None
            
    except FileNotFoundError:
        print(f"找不到特征文件: {features_file}")
        return None
    except Exception as e:
        print(f"单地址检测失败: {e}")
        return None

def save_results(results, filename):
    """保存检测结果"""
    try:
        results_df = pd.DataFrame(results)
        results_df.to_csv(filename, index=False)
        print(f"结果已保存到: {filename}")
    except Exception as e:
        print(f"保存结果失败: {e}")

def main():
    """主函数"""
    import argparse
    
    # 创建命令行参数解析器
    parser = argparse.ArgumentParser(
        description='地址洗钱风险检测系统 - 基于随机森林算法',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 检测特征文件中的所有地址
  python detect_address.py --all --features escrow_features.csv
  
  # 检测单个地址
  python detect_address.py 0x1234567890abcdef1234567890abcdef12345678 --features escrow_features.csv
  
  # 保存结果到指定文件
  python detect_address.py --all --features escrow_features.csv --output results.csv
        """
    )
    
    # 添加参数
    parser.add_argument('address', nargs='?', help='要检测的以太坊地址')
    parser.add_argument('--all', action='store_true', help='检测特征文件中的所有地址')
    parser.add_argument('--features', required=True, help='特征文件路径')
    parser.add_argument('--output', help='输出结果文件路径')
    
    # 解析参数
    args = parser.parse_args()
    
    # 检查参数
    if not args.address and not args.all:
        parser.error("必须提供地址参数或使用 --all 选项")
    
    # 加载模型
    print("正在加载模型...")
    model, scaler, feature_columns = load_model_and_scaler()
    if model is None:
        sys.exit(1)
    print("模型加载完成")
    
    # 执行检测
    if args.all:
        # 检测所有地址
        batch_detect_all_addresses(args.features, model, scaler, feature_columns, args.output)
    else:
        # 单地址检测
        single_address_detect(args.address, args.features, model, scaler, feature_columns, args.output)

if __name__ == "__main__":
    main()
