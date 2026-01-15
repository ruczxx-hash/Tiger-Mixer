#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Escrow交易特征提取脚本
从escrow_transaction_hashes.txt中读取交易哈希，调用geth获取交易详情，提取特征
"""

import json
import subprocess
import pandas as pd
import numpy as np
from datetime import datetime
import time
import re  # Move this import to the top

class EscrowFeatureExtractor:
    def __init__(self, geth_ipc_path="geth.ipc"):
        self.geth_ipc_path = geth_ipc_path
        self.features = []
        
    def call_geth_method(self, method, params=None):
        """调用geth方法"""
        try:
            if params:
                # 修正：使用正确的JavaScript语法，确保参数格式正确
                cmd = f'geth attach {self.geth_ipc_path} --exec "{method}({params})"'
            else:
                # 修正：eth.blockNumber等是属性，不需要括号
                cmd = f'geth attach {self.geth_ipc_path} --exec "{method}"'
            
            print(f"执行命令: {cmd}")
            
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            
            if result.returncode != 0:
                print(f"Geth调用失败: {result.stderr}")
                return None
            
            output = result.stdout.strip()
            print(f"Geth返回: {output[:100]}...")
            
            # 检查是否有错误信息
            if "Error:" in output or "error:" in output.lower() or "TypeError:" in output:
                print(f"Geth返回错误: {output}")
                return None
                
            return output
            
        except Exception as e:
            print(f"调用geth失败: {e}")
            return None
    
    def convert_js_to_json(self, js_string):
        """将JavaScript对象格式转换为标准JSON格式"""
        try:
            # 移除可能的换行符和多余空格
            js_string = js_string.strip()
            
            print(f"原始JavaScript字符串: {js_string[:100]}...")
            
            # 检查是否已经是标准JSON格式
            # 更严格的检查：确保属性名都有引号
            if js_string.startswith('{') and '"' in js_string[:20]:
                # 检查第一个属性名是否有引号
                first_prop_match = re.search(r'^\s*\{?\s*"(\w+)"\s*:', js_string)
                if first_prop_match:
                    print("已经是标准JSON格式，无需转换")
                    return js_string
                else:
                    print("检测到引号但属性名格式不正确，需要转换")
            
            print("开始转换JavaScript到JSON格式...")
            
            # 步骤1: 将属性名用双引号包围
            # 匹配模式：word: 后面跟着空格和值
            pattern = r'(\w+):\s+'
            js_string = re.sub(pattern, r'"\1": ', js_string)
            print(f"步骤1 - 属性名转换后: {js_string[:100]}...")
            
            # 步骤2: 处理十六进制值，确保它们是字符串
            # 匹配: : 0x... 模式（注意：现在属性名已经有引号了）
            hex_pattern = r':\s*(0x[a-fA-F0-9]+)'
            js_string = re.sub(hex_pattern, r': "\1"', js_string)
            print(f"步骤2 - 十六进制值转换后: {js_string[:100]}...")
            
            # 步骤3: 处理数字值，确保它们不被引号包围
            # 匹配: "数字" 模式
            number_pattern = r':\s*"(\d+)"'
            js_string = re.sub(number_pattern, r': \1', js_string)
            print(f"步骤3 - 数字值转换后: {js_string[:100]}...")
            
            # 步骤4: 处理布尔值
            js_string = js_string.replace('"true"', 'true')
            js_string = js_string.replace('"false"', 'false')
            js_string = js_string.replace('"null"', 'null')
            print(f"步骤4 - 布尔值转换后: {js_string[:100]}...")
            
            # 步骤5: 最终清理 - 确保没有多余的引号
            # 处理可能出现的 : "数字" 情况
            final_cleanup = r':\s*"(\d+)"'
            js_string = re.sub(final_cleanup, r': \1', js_string)
            print(f"步骤5 - 最终清理后: {js_string[:100]}...")
            
            print(f"转换完成，最终JSON: {js_string[:100]}...")
            
            return js_string
            
        except Exception as e:
            print(f"JavaScript到JSON转换失败: {e}")
            return js_string
    
    def test_geth_connection(self):
        """测试geth连接"""
        try:
            print("测试geth连接...")
            
            # 测试基本连接 - eth.blockNumber是属性，不是函数
            result = self.call_geth_method("eth.blockNumber")
            if result:
                print(f"Geth连接成功，当前区块: {result}")
                return True
            else:
                print("Geth连接失败")
                return False
                
        except Exception as e:
            print(f"测试geth连接失败: {e}")
            return False
    
    def get_transaction(self, tx_hash):
        """获取交易详情"""
        try:
            # 修正：使用正确的JavaScript语法，确保tx_hash是字符串
            # 注意：在JavaScript中，字符串需要用单引号或双引号包围
            result = self.call_geth_method("eth.getTransaction", f"'{tx_hash}'")
            
            if not result or result == "null":
                print(f"交易 {tx_hash} 未找到")
                return None
            
            # 将JavaScript对象格式转换为标准JSON格式
            json_string = self.convert_js_to_json(result)
            
            # 解析JSON结果
            tx_data = json.loads(json_string)
            return tx_data
            
        except json.JSONDecodeError as e:
            print(f"JSON解析失败: {e}")
            print(f"原始结果: {result}")
            print(f"转换后的JSON: {json_string}")
            return None
        except Exception as e:
            print(f"获取交易失败: {e}")
            return None
    
    def get_transaction_receipt(self, tx_hash):
        """获取交易收据"""
        try:
            result = self.call_geth_method("eth.getTransactionReceipt", f"'{tx_hash}'")
            
            if not result or result == "null":
                return None
            
            # 将JavaScript对象格式转换为标准JSON格式
            json_string = self.convert_js_to_json(result)
            
            receipt = json.loads(json_string)
            return receipt
            
        except:
            return None
    
    def get_block_info(self, block_number):
        """获取区块信息"""
        try:
            result = self.call_geth_method("eth.getBlock", f"'{block_number}'")
            
            if not result or result == "null":
                return None
            
            # 将JavaScript对象格式转换为标准JSON格式
            json_string = self.convert_js_to_json(result)
            
            block = json.loads(json_string)
            return block
            
        except:
            return None
    
    def extract_basic_features(self, tx_data, receipt_data, block_data):
        """提取基本交易特征"""
        features = {}
        
        # 基本信息
        features['address'] = tx_data.get('from', '')
        features['label'] = 'escrow'  # 标记为escrow交易
        
        # 交易金额相关特征
        value_wei = int(tx_data.get('value', '0'), 16) if isinstance(tx_data.get('value'), str) and tx_data.get('value', '0').startswith('0x') else int(tx_data.get('value', '0'))
        value_eth = value_wei / 1e18
        
        # 标准化金额特征（与comprehensive_transaction_statistics.csv保持一致）
        features['value_0'] = value_eth
        features['value_1'] = value_eth
        features['value_2'] = value_eth
        features['value_3'] = value_eth
        features['value_4'] = value_eth
        features['value_5'] = value_eth
        features['value_6'] = value_eth
        features['value_7'] = value_eth
        features['value_8'] = value_eth
        features['value_9'] = value_eth
        
        # 差值特征
        features['diff_0'] = 0.001197  # 使用默认值
        features['diff_1'] = 0.001197
        features['diff_2'] = 0.002394
        features['diff_3'] = 0.001197
        features['diff_4'] = 0.001197
        
        # 累积特征
        features['cumulative_0'] = value_eth
        features['cumulative_1'] = value_eth
        features['cumulative_2'] = value_eth * 2
        features['cumulative_3'] = value_eth * 3
        features['cumulative_4'] = value_eth * 4
        
        # 统计特征
        features['mean'] = value_eth
        features['median'] = value_eth
        features['std'] = 0.0
        features['min'] = value_eth
        features['max'] = value_eth
        
        # 交易计数特征
        features['tx_count_0'] = 1
        features['tx_count_1'] = 1
        features['tx_count_2'] = 2
        features['tx_count_3'] = 1
        features['tx_count_4'] = 1
        features['tx_count_5'] = 1
        features['tx_count_6'] = 2
        features['tx_count_7'] = 1
        features['tx_count_8'] = 1.0
        features['tx_count_9'] = 1.0
        
        # 总交易数
        features['total_tx_count'] = 6
        
        # 时间相关特征
        if block_data:
            timestamp = int(block_data.get('timestamp', '0'), 16) if isinstance(block_data.get('timestamp'), str) and block_data.get('timestamp', '0').startswith('0x') else int(block_data.get('timestamp', '0'))
            features['block_timestamp'] = timestamp
        else:
            features['block_timestamp'] = int(time.time())
        
        # 区块高度
        block_number = int(tx_data.get('blockNumber', '0'), 16) if isinstance(tx_data.get('blockNumber'), str) and tx_data.get('blockNumber', '0').startswith('0x') else int(tx_data.get('blockNumber', '0'))
        features['block_number'] = block_number
        
        # Gas相关特征
        gas_used = int(receipt_data.get('gasUsed', '0'), 16) if receipt_data and isinstance(receipt_data.get('gasUsed'), str) and receipt_data.get('gasUsed', '0').startswith('0x') else 0
        gas_price = int(tx_data.get('gasPrice', '0'), 16) if isinstance(tx_data.get('gasPrice'), str) and tx_data.get('gasPrice', '0').startswith('0x') else int(tx_data.get('gasPrice', '0'))
        
        features['gas_used'] = gas_used
        features['gas_price'] = gas_price
        features['gas_cost'] = gas_used * gas_price
        
        # 交易状态
        features['status'] = 1 if receipt_data and receipt_data.get('status') == '0x1' else 0
        
        # 输入数据长度
        input_data = tx_data.get('input', '')
        features['input_length'] = len(input_data) // 2 - 1 if input_data.startswith('0x') else len(input_data)
        
        # 交互地址数量（简化处理）
        features['unique_addresses'] = 2  # from + to
        features['interaction_count'] = 1
        
        # 交易类型特征
        features['is_contract_interaction'] = 1 if input_data and input_data != '0x' else 0
        features['is_value_transfer'] = 1 if value_wei > 0 else 0
        
        # 时间间隔特征（使用默认值）
        features['time_interval_0'] = 0.04008542
        features['time_interval_1'] = 0.03888842
        features['time_interval_2'] = 0.07897384
        features['time_interval_3'] = 0.03948692
        features['time_interval_4'] = 0.03948692
        
        # 其他统计特征
        features['avg_time_interval'] = 0.03948692
        features['time_std'] = 0.0
        
        return features
    
    def extract_advanced_features(self, tx_data, receipt_data):
        """提取高级特征"""
        features = {}
        
        # 交易复杂度
        input_data = tx_data.get('input', '')
        features['complexity_score'] = len(input_data) // 100  # 简化计算
        
        # 合约调用深度
        features['call_depth'] = 1
        
        # 交易大小
        features['tx_size'] = len(json.dumps(tx_data))
        
        # 签名特征
        v = int(tx_data.get('v', '0'), 16) if isinstance(tx_data.get('v'), str) and tx_data.get('v', '0').startswith('0x') else int(tx_data.get('v', '0'))
        features['signature_v'] = v
        features['is_eip155'] = 1 if v > 27 else 0
        
        # 交易类型
        tx_type = int(tx_data.get('type', '0x0'), 16) if isinstance(tx_data.get('type'), str) and tx_data.get('type', '0x0').startswith('0x') else int(tx_data.get('type', '0x0'))
        features['transaction_type'] = tx_type
        
        return features
    
    def process_escrow_file(self, escrow_file_path):
        """处理escrow交易哈希文件"""
        try:
            with open(escrow_file_path, 'r') as f:
                lines = f.readlines()
            
            print(f"读取到 {len(lines)} 条escrow交易记录")
            
            # 按地址分组收集交易数据
            address_transactions = {}
            
            for i, line in enumerate(lines, 1):
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                # 解析格式：时间戳 | 地址 | 交易哈希
                parts = line.split(' | ')
                if len(parts) != 3:
                    print(f"跳过格式错误的行 {i}: {line}")
                    continue
                
                timestamp, address, tx_hash = parts
                print(f"\n[{i}/{len(lines)}] 处理交易: {tx_hash[:20]}...")
                
                # 获取交易详情
                tx_data = self.get_transaction(tx_hash)
                if not tx_data:
                    continue
                
                # 获取交易收据
                receipt_data = self.get_transaction_receipt(tx_hash)
                
                # 获取区块信息
                block_data = None
                if tx_data.get('blockNumber'):
                    block_data = self.get_block_info(tx_data['blockNumber'])
                
                # 将交易数据按地址分组
                if address not in address_transactions:
                    address_transactions[address] = []
                
                address_transactions[address].append({
                    'timestamp': timestamp,
                    'tx_data': tx_data,
                    'receipt_data': receipt_data,
                    'block_data': block_data
                })
                
                # 添加延迟避免过快调用
                time.sleep(0.1)
            
            print(f"\n成功处理 {len(address_transactions)} 个地址的交易")
            
            # 为每个地址生成聚合特征
            for address, transactions in address_transactions.items():
                print(f"\n为地址 {address} 生成聚合特征 (共 {len(transactions)} 笔交易)")
                aggregated_features = self.aggregate_address_features(address, transactions)
                if aggregated_features:
                    self.features.append(aggregated_features)
            
            print(f"\n成功生成 {len(self.features)} 个地址的特征")
            
        except FileNotFoundError:
            print(f"找不到文件: {escrow_file_path}")
        except Exception as e:
            print(f"处理文件失败: {e}")
    
    def aggregate_address_features(self, address, transactions):
        """为单个地址聚合所有交易特征"""
        try:
            if not transactions:
                return None
            
            print(f"聚合 {len(transactions)} 笔交易的特征...")
            
            # 收集所有交易的特征
            all_values = []
            all_timestamps = []
            all_gas_used = []
            all_gas_prices = []
            all_input_lengths = []
            
            for tx_info in transactions:
                tx_data = tx_info['tx_data']
                receipt_data = tx_info['receipt_data']
                block_data = tx_info['block_data']
                
                # 收集数值用于聚合
                value_wei = int(tx_data.get('value', '0'), 16) if isinstance(tx_data.get('value'), str) and tx_data.get('value', '0').startswith('0x') else int(tx_data.get('value', '0'))
                value_eth = value_wei / 1e18
                all_values.append(value_eth)
                
                if block_data:
                    timestamp = int(block_data.get('timestamp', '0'), 16) if isinstance(block_data.get('timestamp'), str) and block_data.get('timestamp', '0').startswith('0x') else int(block_data.get('timestamp', '0'))
                    all_timestamps.append(timestamp)
                
                gas_used = int(receipt_data.get('gasUsed', '0'), 16) if receipt_data and isinstance(receipt_data.get('gasUsed'), str) and receipt_data.get('gasUsed', '0').startswith('0x') else 0
                all_gas_used.append(gas_used)
                
                gas_price = int(tx_data.get('gasPrice', '0'), 16) if isinstance(tx_data.get('gasPrice'), str) and tx_data.get('gasPrice', '0').startswith('0x') else int(tx_data.get('gasPrice', '0'))
                all_gas_prices.append(gas_price)
                
                input_data = tx_data.get('input', '')
                input_length = len(input_data) // 2 - 1 if input_data.startswith('0x') else len(input_data)
                all_input_lengths.append(input_length)
            
            # 生成聚合特征 - 使用与训练模型一致的特征列名
            aggregated = {}
            aggregated['address'] = address
            aggregated['label'] = 'escrow'
            
            # 发送ETH特征 (sent_eth_*)
            if all_values:
                aggregated['sent_eth_max'] = max(all_values)
                aggregated['sent_eth_min'] = min(all_values)
                aggregated['sent_eth_total'] = sum(all_values)
                aggregated['sent_eth_average'] = sum(all_values) / len(all_values)
                aggregated['sent_eth_median'] = sorted(all_values)[len(all_values)//2] if len(all_values) % 2 == 1 else (sorted(all_values)[len(all_values)//2-1] + sorted(all_values)[len(all_values)//2]) / 2
            else:
                aggregated['sent_eth_max'] = aggregated['sent_eth_min'] = aggregated['sent_eth_total'] = aggregated['sent_eth_average'] = aggregated['sent_eth_median'] = 0
            
            # 接收ETH特征 (received_eth_*) - 对于escrow交易，接收为0
            aggregated['received_eth_max'] = 0
            aggregated['received_eth_min'] = 0
            aggregated['received_eth_total'] = 0
            aggregated['received_eth_average'] = 0
            aggregated['received_eth_median'] = 0
            
            # Gas费用特征 (gas_fee_*)
            if all_gas_used and all_gas_prices:
                gas_costs = [used * price for used, price in zip(all_gas_used, all_gas_prices)]
                aggregated['gas_fee_max'] = max(gas_costs)
                aggregated['gas_fee_min'] = min(gas_costs)
                aggregated['gas_fee_total'] = sum(gas_costs)
                aggregated['gas_fee_average'] = sum(gas_costs) / len(gas_costs)
                aggregated['gas_fee_median'] = sorted(gas_costs)[len(gas_costs)//2] if len(gas_costs) % 2 == 1 else (sorted(gas_costs)[len(gas_costs)//2-1] + sorted(gas_costs)[len(gas_costs)//2]) / 2
            else:
                aggregated['gas_fee_max'] = aggregated['gas_fee_min'] = aggregated['gas_fee_total'] = aggregated['gas_fee_average'] = aggregated['gas_fee_median'] = 0
            
            # 总价值特征 (total_value_*)
            if all_values:
                total_values = [v + (all_gas_prices[i] * all_gas_used[i] / 1e18) if i < len(all_gas_prices) and i < len(all_gas_used) else v for i, v in enumerate(all_values)]
                aggregated['total_value_max'] = max(total_values)
                aggregated['total_value_min'] = min(total_values)
                aggregated['total_value_total'] = sum(total_values)
                aggregated['total_value_average'] = sum(total_values) / len(total_values)
                aggregated['total_value_median'] = sorted(total_values)[len(total_values)//2] if len(total_values) % 2 == 1 else (sorted(total_values)[len(total_values)//2-1] + sorted(total_values)[len(total_values)//2]) / 2
            else:
                aggregated['total_value_max'] = aggregated['total_value_min'] = aggregated['total_value_total'] = aggregated['total_value_average'] = aggregated['total_value_median'] = 0
            
            # Gas比例百分比
            if all_values and all_gas_used and all_gas_prices:
                total_gas_cost = sum([used * price for used, price in zip(all_gas_used, all_gas_prices)])
                total_eth_value = sum(all_values)
                if total_eth_value > 0:
                    aggregated['gas_ratio_percentage'] = (total_gas_cost / 1e18) / total_eth_value * 100
                else:
                    aggregated['gas_ratio_percentage'] = 0
            else:
                aggregated['gas_ratio_percentage'] = 0
            
            # 交易计数特征
            tx_count = len(transactions)
            aggregated['total_transactions'] = tx_count
            
            # 输入/输出交易计数
            aggregated['input_transactions_count'] = 0  # escrow交易都是输出
            aggregated['output_transactions_count'] = tx_count
            
            # 唯一合作伙伴数量
            unique_addresses = set()
            for tx in transactions:
                tx_data = tx['tx_data']
                unique_addresses.add(tx_data.get('from', ''))
                unique_addresses.add(tx_data.get('to', ''))
            aggregated['unique_partners'] = len(unique_addresses)
            
            # 交互特征 (interaction_*)
            if all_values:
                aggregated['interaction_max'] = max(all_values)
                aggregated['interaction_min'] = min(all_values)
                aggregated['interaction_total'] = sum(all_values)
                aggregated['interaction_average'] = sum(all_values) / len(all_values)
                aggregated['interaction_median'] = sorted(all_values)[len(all_values)//2] if len(all_values) % 2 == 1 else (sorted(all_values)[len(all_values)//2-1] + sorted(all_values)[len(all_values)//2]) / 2
            else:
                aggregated['interaction_max'] = aggregated['interaction_min'] = aggregated['interaction_total'] = aggregated['interaction_average'] = aggregated['interaction_median'] = 0
            
            # 区块间隔特征 (block_interval_*)
            if all_timestamps and len(all_timestamps) > 1:
                sorted_timestamps = sorted(all_timestamps)
                time_intervals = [sorted_timestamps[i] - sorted_timestamps[i-1] for i in range(1, len(sorted_timestamps))]
                if time_intervals:
                    aggregated['all_block_interval_max'] = max(time_intervals)
                    aggregated['all_block_interval_min'] = min(time_intervals)
                    aggregated['input_block_interval_max'] = 0  # 无输入交易
                    aggregated['input_block_interval_min'] = 0
                    aggregated['output_block_interval_max'] = max(time_intervals)
                    aggregated['output_block_interval_min'] = min(time_intervals)
                else:
                    for key in ['all_block_interval_max', 'all_block_interval_min', 'input_block_interval_max', 'input_block_interval_min', 'output_block_interval_max', 'output_block_interval_min']:
                        aggregated[key] = 0
            else:
                for key in ['all_block_interval_max', 'all_block_interval_min', 'input_block_interval_max', 'input_block_interval_min', 'output_block_interval_max', 'output_block_interval_min']:
                    aggregated[key] = 0
            
            # 区块信息
            block_numbers = [int(tx['tx_data'].get('blockNumber', '0'), 16) if isinstance(tx['tx_data'].get('blockNumber'), str) and tx['tx_data']['blockNumber'].startswith('0x') else int(tx['tx_data'].get('blockNumber', '0')) for tx in transactions]
            if block_numbers:
                aggregated['first_transaction_block'] = min(block_numbers)
                aggregated['last_transaction_block'] = max(block_numbers)
                aggregated['first_output_block'] = min(block_numbers)
                aggregated['last_output_block'] = max(block_numbers)
                aggregated['first_input_block'] = 0  # 无输入交易
                aggregated['last_input_block'] = 0
            else:
                for key in ['first_transaction_block', 'last_transaction_block', 'first_output_block', 'last_output_block', 'first_input_block', 'last_input_block']:
                    aggregated[key] = 0
            
            # 周转率特征 (turnover_ratio_*)
            if all_values and len(all_values) > 1:
                # 计算相邻交易的周转率
                turnover_ratios = []
                for i in range(1, len(all_values)):
                    if all_values[i-1] > 0:
                        ratio = abs(all_values[i] - all_values[i-1]) / all_values[i-1]
                        turnover_ratios.append(ratio)
                
                if turnover_ratios:
                    aggregated['turnover_ratio_max'] = max(turnover_ratios)
                    aggregated['turnover_ratio_min'] = min(turnover_ratios)
                    aggregated['turnover_ratio_average'] = sum(turnover_ratios) / len(turnover_ratios)
                    aggregated['turnover_ratio_median'] = sorted(turnover_ratios)[len(turnover_ratios)//2] if len(turnover_ratios) % 2 == 1 else (sorted(turnover_ratios)[len(turnover_ratios)//2-1] + sorted(turnover_ratios)[len(turnover_ratios)//2]) / 2
                else:
                    for key in ['turnover_ratio_max', 'turnover_ratio_min', 'turnover_ratio_average', 'turnover_ratio_median']:
                        aggregated[key] = 0
            else:
                for key in ['turnover_ratio_max', 'turnover_ratio_min', 'turnover_ratio_average', 'turnover_ratio_median']:
                    aggregated[key] = 0
            
            # 24小时高频期间
            if all_timestamps and len(all_timestamps) > 1:
                # 简化为基于区块间隔的高频检测
                sorted_timestamps = sorted(all_timestamps)
                short_intervals = sum(1 for i in range(1, len(sorted_timestamps)) if sorted_timestamps[i] - sorted_timestamps[i-1] < 100)  # 假设100个区块为高频
                aggregated['high_frequency_24h_periods'] = short_intervals
            else:
                aggregated['high_frequency_24h_periods'] = 0
            
            # 累积特征
            if all_values:
                aggregated['cumulative_received_eth'] = 0  # escrow交易无接收
                aggregated['cumulative_sent_eth'] = sum(all_values)
            else:
                aggregated['cumulative_received_eth'] = aggregated['cumulative_sent_eth'] = 0
            
            print(f"地址 {address} 的特征聚合完成")
            return aggregated
            
        except Exception as e:
            print(f"聚合地址 {address} 特征失败: {e}")
            return None
    
    def save_features(self, output_file):
        """保存特征到CSV文件"""
        if not self.features:
            print("没有特征数据可保存")
            return
        
        try:
            # 转换为DataFrame
            df = pd.DataFrame(self.features)
            
            # 确保列的顺序与训练模型时使用的特征列名保持一致
            expected_columns = [
                'address', 'label', 'sent_eth_max', 'sent_eth_min', 'sent_eth_total', 'sent_eth_average', 'sent_eth_median',
                'received_eth_max', 'received_eth_min', 'received_eth_total', 'received_eth_average', 'received_eth_median',
                'gas_fee_max', 'gas_fee_min', 'gas_fee_total', 'gas_fee_average', 'gas_fee_median',
                'total_value_max', 'total_value_min', 'total_value_total', 'total_value_average', 'total_value_median',
                'gas_ratio_percentage', 'total_transactions', 'input_transactions_count', 'output_transactions_count',
                'unique_partners', 'interaction_max', 'interaction_min', 'interaction_total', 'interaction_average', 'interaction_median',
                'all_block_interval_max', 'all_block_interval_min', 'input_block_interval_max', 'input_block_interval_min',
                'output_block_interval_max', 'output_block_interval_min', 'first_transaction_block', 'last_transaction_block',
                'first_output_block', 'last_output_block', 'first_input_block', 'last_input_block',
                'turnover_ratio_max', 'turnover_ratio_min', 'turnover_ratio_average', 'turnover_ratio_median',
                'high_frequency_24h_periods', 'cumulative_received_eth', 'cumulative_sent_eth'
            ]
            
            # 重新排序列
            df = df.reindex(columns=expected_columns, fill_value=0)
            
            # 保存到CSV
            df.to_csv(output_file, index=False)
            print(f"特征已保存到: {output_file}")
            print(f"特征数据形状: {df.shape}")
            
            # 显示前几行
            print("\n特征数据预览:")
            print(df.head())
            
        except Exception as e:
            print(f"保存特征失败: {e}")

def main():
    """主函数"""
    print("=== Escrow交易特征提取器 ===")
    
    # 配置参数
    escrow_file = "/home/zxx/A2L/A2L-master/ecdsa/bin/escrow_transaction_hashes.txt"
    output_file = "/home/zxx/A2L/A2L-master/ecdsa/bin/escrow_features.csv"
    geth_ipc = "/home/zxx/Config/blockchain/consortium_blockchain/myblockchain/geth.ipc"
    
    # 创建特征提取器
    extractor = EscrowFeatureExtractor(geth_ipc)
    
    # 测试geth连接
    if not extractor.test_geth_connection():
        print("Geth连接失败，请检查geth是否运行")
        return
    
    # 处理escrow文件
    print(f"开始处理文件: {escrow_file}")
    extractor.process_escrow_file(escrow_file)
    
    # 保存特征
    if extractor.features:
        print(f"\n保存特征到: {output_file}")
        extractor.save_features(output_file)
    else:
        print("没有提取到任何特征")

if __name__ == "__main__":
    main()