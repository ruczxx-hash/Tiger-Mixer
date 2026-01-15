/**
 * 基于真实决策数据更新委员会成员声誉
 * 从 reputation_stats.csv 文件读取统计数据，计算声誉值，并更新到区块链
 */

const fs = require('fs');
const path = require('path');

// 配置
const STATS_FILE = '/home/zxx/A2L/A2L-master/ecdsa/log_game/reputation_stats.csv';
const REPUTATION_MANAGER_ADDRESS = '0xEb892af82bE8F7a1434da362c9434129Fa80FA9B';

/**
 * 解析CSV文件
 */
function parseCSV(filePath) {
    const content = fs.readFileSync(filePath, 'utf8');
    const lines = content.trim().split('\n');
    
    if (lines.length < 2) {
        return [];
    }
    
    // 解析表头
    const headers = lines[0].split(',');
    
    // 解析数据行
    const data = [];
    for (let i = 1; i < lines.length; i++) {
        const values = lines[i].split(',');
        const row = {};
        headers.forEach((header, index) => {
            row[header.trim()] = values[index] ? values[index].trim() : '';
        });
        data.push(row);
    }
    
    return data;
}

module.exports = async function(callback) {
    try {
        console.log("\n========================================");
        console.log("   基于真实决策数据更新声誉");
        console.log("========================================\n");
        
        // 检查统计文件是否存在
        if (!fs.existsSync(STATS_FILE)) {
            console.log(`⚠️  统计文件不存在: ${STATS_FILE}`);
            console.log("   请先运行 reputation_tracker_calculate_and_save_stats() 生成统计数据");
            callback();
            return;
        }
        
        const ReputationManager = artifacts.require("ReputationManager");
        const reputationManager = await ReputationManager.at(REPUTATION_MANAGER_ADDRESS);
        
        const accounts = await web3.eth.getAccounts();
        console.log("可用账户数量:", accounts.length);
        
        // 读取统计数据
        const stats = parseCSV(STATS_FILE);
        
        if (stats.length === 0) {
            console.log("⚠️  统计数据为空，跳过更新");
            callback();
            return;
        }
        
        console.log(`读取到 ${stats.length} 个成员的统计数据\n`);
        
        let updatedCount = 0;
        let skippedCount = 0;
        
        // 处理每个委员会成员（基于address）
        for (const stat of stats) {
            // 新格式：address,total_decisions,correct_decisions,accuracy,consistency,participation,total_reputation,last_update
            const address = stat.address;
            const accuracy = parseInt(stat.accuracy);
            const consistency = parseInt(stat.consistency);
            const participation = parseInt(stat.participation || stat.participationRate || accuracy); // 支持新格式和旧格式
            const totalDecisions = parseInt(stat.total_decisions);
            const correctDecisions = parseInt(stat.correct_decisions);
            
            console.log(`--- 处理成员 ---`);
            console.log(`  地址: ${address}`);
            console.log(`  总决策数: ${totalDecisions}`);
            console.log(`  正确决策数: ${correctDecisions}`);
            console.log(`  准确率: ${accuracy}% (仅用于参考，不用于声誉计算)`);
            console.log(`  一致性 (S_consistency): ${consistency}%`);
            console.log(`  参与度 (S_participation): ${participation}%`);
            
            // 计算使用权重系数后的增量声誉（符合论文公式）
            // 论文公式：ΔRep = ω_c * S_consistency + ω_p * S_participation
            // 其中 ω_c = 0.6, ω_p = 0.4
            // 注意：准确率(accuracy)不参与声誉计算，仅用于参考
            const omegaC = 0.6;
            const omegaP = 0.4;
            const deltaRep = (consistency * omegaC) + (participation * omegaP);
            console.log(`  增量声誉 (ΔRep): ${deltaRep.toFixed(2)}`);
            console.log(`     = ω_c * S_consistency + ω_p * S_participation`);
            console.log(`     = ${omegaC} * ${consistency} + ${omegaP} * ${participation}`);
            console.log(`  注意: 综合声誉 = λ * 历史声誉 + ΔRep (由合约计算)`);
            
            // 检查是否有足够的统计数据
            if (totalDecisions === 0) {
                console.log(`  ⚠️  没有统计数据（总决策数为0），跳过更新`);
                skippedCount++;
                continue;
            }
            
            // 验证声誉值范围（只验证consistency和participation）
            if (consistency > 100 || participation > 100 || 
                consistency < 0 || participation < 0) {
                console.log(`  ⚠️  声誉值超出范围，跳过更新`);
                skippedCount++;
                continue;
            }
            
            // 查找对应的账户地址
            const memberAddress = address.toLowerCase();
            let accountIndex = -1;
            
            for (let i = 0; i < accounts.length; i++) {
                if (accounts[i].toLowerCase() === memberAddress) {
                    accountIndex = i;
                    break;
                }
            }
            
            if (accountIndex === -1) {
                console.log(`  ⚠️  未找到对应的账户地址，尝试直接使用地址`);
                // 尝试使用地址本身（如果账户已解锁）
                try {
                    await web3.eth.personal.unlockAccount(memberAddress, '', 3600);
                } catch (unlockError) {
                    console.log(`  ⚠️  无法解锁账户: ${unlockError.message}`);
                    console.log(`  ℹ️  尝试使用 accounts[0] 作为发送者（需要合约支持）`);
                    accountIndex = 0;  // 使用第一个账户作为fallback
                }
            }
            
            // 更新声誉
            // 注意：ReputationManager.updateReputation 的参数顺序是 (participation, consistency)
            // 根据论文：只使用 consistency 和 participation，不使用 accuracy
            try {
                const senderAccount = accountIndex >= 0 ? accounts[accountIndex] : memberAddress;
                
                console.log(`\n  📤 更新声誉到区块链...`);
                console.log(`     发送账户: ${senderAccount}`);
                console.log(`     一致性 (S_consistency): ${consistency}%`);
                console.log(`     参与度 (S_participation): ${participation}%`);
                console.log(`     准确率: ${accuracy}% (仅用于参考，不参与声誉计算)`);
                
                const tx = await reputationManager.updateReputation(
                    participation,  // S_participation: 基于响应率和任务完成率
                    consistency,   // S_consistency: 成员投票与最终正确结果的对齐
                    { from: senderAccount }
                );
                
                console.log(`  ✅ 更新成功，交易哈希: ${tx.tx}`);
                
                // 验证更新结果
                const newReputation = await reputationManager.calculateReputation(memberAddress);
                const deltaRep = await reputationManager.calculateDeltaReputation(memberAddress);
                const previousRep = await reputationManager.getPreviousReputation(memberAddress);
                console.log(`  ✅ 验证:`);
                console.log(`     增量声誉 (ΔRep) = ${deltaRep} (ω_c * S_consistency + ω_p * S_participation)`);
                console.log(`     历史声誉 (Rep^(t)) = ${previousRep}`);
                console.log(`     综合声誉 (Rep^(t+1)) = ${newReputation} (λ * Rep^(t) + ΔRep)`);
                
                updatedCount++;
                
            } catch (error) {
                console.log(`  ❌ 更新失败: ${error.message}`);
                console.log(`     错误详情:`, error);
                skippedCount++;
            }
        }
        
        console.log("\n========================================");
        console.log("   更新完成");
        console.log("========================================");
        console.log(`  成功更新: ${updatedCount} 个成员`);
        console.log(`  跳过: ${skippedCount} 个成员`);
        console.log("========================================\n");
        
        callback();
    } catch (error) {
        console.error('\n❌ 错误:', error.message);
        console.error('错误详情:', error);
        callback(error);
    }
};

