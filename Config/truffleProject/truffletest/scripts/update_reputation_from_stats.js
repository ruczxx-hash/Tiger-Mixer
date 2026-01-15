/**
 * 基于真实统计数据更新委员会成员声誉
 * 从 reputation_stats_*.json 文件读取统计数据，计算声誉值，并更新到区块链
 */

const fs = require('fs');
const path = require('path');

// 配置
const STATS_DIR = '/home/zxx/A2L/A2L-master/ecdsa/log_game';
const REPUTATION_MANAGER_ADDRESS = '0xEb892af82bE8F7a1434da362c9434129Fa80FA9B';

module.exports = async function(callback) {
    try {
        console.log("\n========================================");
        console.log("   基于真实统计数据更新声誉");
        console.log("========================================\n");
        
        const ReputationManager = artifacts.require("ReputationManager");
        const reputationManager = await ReputationManager.at(REPUTATION_MANAGER_ADDRESS);
        
        const accounts = await web3.eth.getAccounts();
        console.log("可用账户数量:", accounts.length);
        
        // 读取3个委员会成员的统计数据
        const committeeMembers = await reputationManager.committeeMembers();
        console.log("当前委员会成员:");
        for (let i = 0; i < committeeMembers.length; i++) {
            console.log(`  ${i + 1}. ${committeeMembers[i]}`);
        }
        
        let updatedCount = 0;
        let skippedCount = 0;
        
        // 处理每个委员会成员
        for (let participantId = 1; participantId <= 3; participantId++) {
            const statsFile = path.join(STATS_DIR, `reputation_stats_${participantId}.json`);
            
            console.log(`\n--- 处理成员 ${participantId} ---`);
            
            if (!fs.existsSync(statsFile)) {
                console.log(`  ⚠️  统计数据文件不存在: ${statsFile}`);
                console.log(`  ℹ️  跳过该成员，使用默认值或保持当前值`);
                skippedCount++;
                continue;
            }
            
            // 读取统计数据
            let stats;
            try {
                const statsContent = fs.readFileSync(statsFile, 'utf8');
                stats = JSON.parse(statsContent);
            } catch (error) {
                console.log(`  ❌ 读取统计数据失败: ${error.message}`);
                skippedCount++;
                continue;
            }
            
            console.log(`  ✅ 读取统计数据成功`);
            console.log(`     地址: ${stats.address}`);
            console.log(`     总请求数: ${stats.total_requests}`);
            console.log(`     成功响应: ${stats.successful_responses}`);
            console.log(`     失败响应: ${stats.failed_responses}`);
            console.log(`     不需要审计: ${stats.no_audit_needed}`);
            console.log(`     Shares验证成功: ${stats.shares_verified}`);
            console.log(`     Shares验证失败: ${stats.shares_failed_verify}`);
            
            // 检查是否有足够的统计数据
            if (stats.total_requests === 0) {
                console.log(`  ⚠️  没有统计数据（总请求数为0），跳过更新`);
                skippedCount++;
                continue;
            }
            
            // 获取计算出的声誉值
            const participationRate = stats.participation_rate || 50;
            const accuracy = stats.accuracy || 50;
            const consistency = stats.consistency || 50;
            const totalReputation = stats.total_reputation || (participationRate + accuracy + consistency);
            
            console.log(`\n  计算出的声誉值:`);
            console.log(`     参与率: ${participationRate}%`);
            console.log(`     准确率: ${accuracy}%`);
            console.log(`     一致性: ${consistency}%`);
            console.log(`     综合声誉: ${totalReputation}`);
            
            // 验证声誉值范围
            if (participationRate > 100 || accuracy > 100 || consistency > 100) {
                console.log(`  ⚠️  声誉值超出范围，跳过更新`);
                skippedCount++;
                continue;
            }
            
            // 查找对应的账户地址
            const memberAddress = stats.address.toLowerCase();
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
                    // 解锁账户（需要密码，这里假设为空密码或已解锁）
                    await web3.eth.personal.unlockAccount(memberAddress, '', 3600);
                } catch (unlockError) {
                    console.log(`  ⚠️  无法解锁账户: ${unlockError.message}`);
                    console.log(`  ℹ️  尝试使用 accounts[0] 作为发送者（需要合约支持）`);
                    accountIndex = 0;  // 使用第一个账户作为fallback
                }
            }
            
            // 更新声誉
            try {
                const senderAccount = accountIndex >= 0 ? accounts[accountIndex] : memberAddress;
                
                console.log(`\n  📤 更新声誉到区块链...`);
                console.log(`     发送账户: ${senderAccount}`);
                
                const tx = await reputationManager.updateReputation(
                    Math.round(accuracy),
                    Math.round(participationRate),
                    Math.round(consistency),
                    { from: senderAccount }
                );
                
                console.log(`  ✅ 更新成功，交易哈希: ${tx.tx}`);
                
                // 验证更新结果
                const newReputation = await reputationManager.calculateReputation(memberAddress);
                console.log(`  ✅ 验证: 新的综合声誉 = ${newReputation}`);
                
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


