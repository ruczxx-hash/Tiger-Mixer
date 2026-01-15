/**
 * 委员会轮换脚本（使用 truffle exec 运行）
 * 功能：
 * 1. 更新候选者声誉
 * 2. 使用 C 程序生成 VRF 随机数和证明
 * 3. 使用合约的 verify() 函数进行验证
 * 4. 执行委员会轮换
 * 
 * 前置条件：
 * 1. C 程序路径：/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test
 * 2. 已部署 VRFTestHelper、CommitteeRotation、ReputationManager 合约
 * 
 * 运行方式：
 * truffle exec scripts/update_and_rotate_new.js --network development
 */

const { execSync } = require('child_process');
const fs = require('fs');

// 配置
const VRF_TEST_PATH = '/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test';
const VRF_KEY_FILE = '/home/zxx/A2L/A2L-master/ecdsa/tumbler_vrf_key.bin';
const COMMITTEE_MEMBERS_FILE = '/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt';

// 合约地址（固定值，来自已部署的合约）
// 注意：这些地址来自 private 网络的部署
// 如果使用 development 网络，需要先部署合约并更新这些地址
const VRF_TEST_HELPER_ADDRESS = '0xCD4FB6a48bEB62dd3caD96e3b257bb1e4E3D4703';  // private网络地址
const COMMITTEE_ROTATION_ADDRESS = '0x7422B6F8bd5b4d72e3071e6E3166a223a0000f02';  // private网络地址
const REPUTATION_MANAGER_ADDRESS = '0xEb892af82bE8F7a1434da362c9434129Fa80FA9B';  // 两个网络相同

module.exports = async function(callback) {
    try {
        console.log("\n========================================");
        console.log("   委员会轮换 - 声誉更新 + VRF 验证 + 轮换");
        console.log("========================================\n");
        
        // 获取账户
        const accounts = await web3.eth.getAccounts();
        console.log("可用账户数量:", accounts.length);
        
        // 获取合约实例
        const VRFTestHelper = artifacts.require("VRFTestHelper");
        const CommitteeRotation = artifacts.require("CommitteeRotation");
        const ReputationManager = artifacts.require("ReputationManager");
        
        // 使用固定地址获取合约实例
        let helper, committeeRotation, reputationManager;
        
        try {
            helper = await VRFTestHelper.at(VRF_TEST_HELPER_ADDRESS);
        console.log("✅ VRFTestHelper 合约:", helper.address);
        } catch (error) {
            console.log("❌ 错误: 无法连接到 VRFTestHelper 合约");
            console.log("   地址:", VRF_TEST_HELPER_ADDRESS);
            console.log("   错误:", error.message);
            console.log("   提示: 请确保合约已部署到 development 网络");
            throw error;
        }
        
        try {
            committeeRotation = await CommitteeRotation.at(COMMITTEE_ROTATION_ADDRESS);
        console.log("✅ CommitteeRotation 合约:", committeeRotation.address);
        } catch (error) {
            console.log("❌ 错误: 无法连接到 CommitteeRotation 合约");
            console.log("   地址:", COMMITTEE_ROTATION_ADDRESS);
            console.log("   错误:", error.message);
            throw error;
        }
        
        try {
            reputationManager = await ReputationManager.at(REPUTATION_MANAGER_ADDRESS);
        console.log("✅ ReputationManager 合约:", reputationManager.address);
        } catch (error) {
            console.log("❌ 错误: 无法连接到 ReputationManager 合约");
            console.log("   地址:", REPUTATION_MANAGER_ADDRESS);
            console.log("   错误:", error.message);
            throw error;
        }
        
        // 辅助函数：将 hex 字符串转换为字节数组
        const hexToBytes = (hex) => {
            if (hex == null) {
                throw new Error('hexToBytes: 输入不能为 null 或 undefined');
            }
            
            let hexStr;
            if (typeof hex === 'string') {
                hexStr = hex;
            } else {
                if (hex && typeof hex.toString === 'function') {
                    hexStr = hex.toString();
                } else {
                    hexStr = String(hex);
                }
            }
            
            if (typeof hexStr !== 'string') {
                throw new Error(`hexToBytes: 无法将输入转换为字符串。输入类型: ${typeof hex}`);
            }
            
            // 移除 0x 前缀
            if (hexStr.indexOf('0x') === 0 || hexStr.indexOf('0X') === 0) {
                hexStr = hexStr.substring(2);
            }
            
            // 转换为字节数组
            const bytes = [];
            for (let i = 0; i < hexStr.length; i += 2) {
                if (i + 1 < hexStr.length) {
                    const byteStr = hexStr.substring(i, i + 2);
                    bytes.push(parseInt(byteStr, 16));
                }
            }
            return bytes;
        };
        
        // 检查 C 程序是否存在
        const checkCProgram = () => {
            if (!fs.existsSync(VRF_TEST_PATH)) {
                console.log(`\n⚠️  警告: C 程序不存在: ${VRF_TEST_PATH}`);
                return false;
            }
            return true;
        };
        
        // 从 C 程序生成 VRF 证明
        const generateVRFFromC = (message, keyFile = null) => {
            console.log(`\n--- 从 C 程序生成 VRF ---`);
            console.log(`消息: "${message}"`);
            
            const usedKeyFile = keyFile || (fs.existsSync(VRF_KEY_FILE) ? VRF_KEY_FILE : '');
            if (usedKeyFile) {
                console.log(`使用密钥文件: ${usedKeyFile}`);
            }
            
            try {
                const command = `"${VRF_TEST_PATH}" "${usedKeyFile}" "${message}"`;
                const output = execSync(command, { encoding: 'utf-8', timeout: 10000 });
                
                const lines = output.split('\n');
                let publicKey = '';
                let proof = '';
                let random = '';
                
                for (const line of lines) {
                    if (line.includes('序列化公钥:')) {
                        const parts = line.split(':');
                        if (parts.length > 1) {
                            publicKey = parts[1].trim();
                        }
                    } else if (line.trim().startsWith('证明:')) {
                        const parts = line.split(':');
                        if (parts.length > 1) {
                            proof = parts[1].trim();
                        }
                    } else if (line.includes('随机数输出:')) {
                        const parts = line.split(':');
                        if (parts.length > 1) {
                            random = parts[1].trim();
                        }
                    }
                }
                
                if (!publicKey || !proof || !random) {
                    console.error('C 程序原始输出:');
                    console.error(output);
                    throw new Error('无法从 C 程序输出中提取完整的 VRF 数据');
                }
                
                console.log(`✅ C 端生成成功`);
                console.log(`  公钥: ${publicKey}`);
                console.log(`  证明长度: ${proof.length / 2} 字节`);
                console.log(`  随机数: ${random}`);
                
                return { publicKey, proof, random, message };
            } catch (error) {
                console.error(`❌ C 程序执行失败: ${error.message}`);
                throw error;
            }
        };
        
        // 检查 C 程序
        if (!checkCProgram()) {
            throw new Error('C 程序不存在，无法运行测试');
        }
        
        // 检查密钥文件
        if (VRF_KEY_FILE && fs.existsSync(VRF_KEY_FILE)) {
            console.log("✅ 找到 Tumbler 密钥文件:", VRF_KEY_FILE);
        }
        
        // ============================================
        // 第一部分：基于真实决策数据更新声誉
        // ============================================
        console.log("\n========================================");
        console.log("   第一部分：基于真实决策数据更新声誉");
        console.log("========================================\n");
        
        // 获取当前委员会和候选池
        const currentCommittee = await committeeRotation.getCurrentCommittee();
        console.log("当前委员会成员:");
        for (let i = 0; i < currentCommittee.length; i++) {
            if (currentCommittee[i] !== "0x0000000000000000000000000000000000000000") {
                const reputation = await reputationManager.calculateReputation(currentCommittee[i]);
                console.log(`  ${i + 1}. ${currentCommittee[i]} - 声誉: ${reputation}`);
            }
        }
        
        const candidatePool = await committeeRotation.getCandidatePool();
        console.log(`\n候选池大小: ${candidatePool.length}`);
        
        // 步骤1: 计算统计数据（从决策记录中计算准确率和一致性）
        console.log("\n--- 步骤1: 计算统计数据 ---");
        const CALC_STATS_SCRIPT = '/home/zxx/A2L/A2L-master/ecdsa/bin/calculate_reputation_stats.sh';
        
        try {
            const { execSync } = require('child_process');
            const calcOutput = execSync(`bash ${CALC_STATS_SCRIPT}`, { 
                encoding: 'utf8',
                timeout: 30000,  // 30秒超时
                cwd: '/home/zxx/A2L/A2L-master/ecdsa'
            });
            console.log(calcOutput);
            console.log("✅ 统计数据计算完成");
        } catch (error) {
            console.log(`⚠️  计算统计数据失败: ${error.message}`);
            console.log("   继续执行，使用已有的统计数据（如果存在）");
        }
        
        // 步骤2: 更新声誉到区块链
        console.log("\n--- 步骤2: 更新声誉到区块链 ---");
        try {
            // 调用更新脚本
            const path = require('path');
            const updateScript = path.join(__dirname, 'update_reputation_from_decisions.js');
            
            // 使用child_process执行另一个truffle脚本
            const { execSync } = require('child_process');
            const updateOutput = execSync(`truffle exec ${updateScript} --network development`, {
                cwd: path.join(__dirname, '..'),
                encoding: 'utf8',
                timeout: 60000  // 60秒超时
            });
            console.log(updateOutput);
            console.log("✅ 声誉更新完成");
        } catch (error) {
            console.log(`⚠️  声誉更新失败: ${error.message}`);
            console.log("   继续执行后续步骤");
            // 不抛出错误，继续执行VRF和轮换步骤
        }
        
        // 显示更新后的声誉
        if (candidatePool.length > 0) {
            console.log("\n候选池成员（更新后）:");
            for (let i = 0; i < candidatePool.length; i++) {
                const reputation = await reputationManager.calculateReputation(candidatePool[i]);
                console.log(`  ${i + 1}. ${candidatePool[i]} - 声誉: ${reputation}`);
            }
        }
        
        // ============================================
        // 第二部分：VRF 生成和验证（保留原有逻辑）
        // ============================================
        console.log("\n========================================");
        console.log("   第二部分：VRF 生成和验证");
        console.log("========================================\n");
        
        // 步骤 1: 使用 C 程序生成 VRF
        const testMessage = 'test_vrf_' + Date.now();
        const vrfData = generateVRFFromC(testMessage);
        
        // 步骤 2: 转换数据格式
        console.log("\n--- 步骤 2: 转换数据格式 ---");
        const publicKeyBytes = '0x' + vrfData.publicKey;
        const proofBytes = '0x' + vrfData.proof;
        let messageBytes = web3.utils.utf8ToHex(testMessage);
        messageBytes = String(messageBytes);
        
        console.log(`  公钥长度: ${(publicKeyBytes.length - 2) / 2} 字节`);
        console.log(`  证明长度: ${(proofBytes.length - 2) / 2} 字节`);
        console.log(`  消息长度: ${(messageBytes.length - 2) / 2} 字节`);
        
        // 步骤 3: 解码公钥和证明
        console.log("\n--- 步骤 3: 解码数据 ---");
        const publicKey = await helper.decodePoint.call(publicKeyBytes);
        const proof = await helper.decodeProof.call(proofBytes);
        const message = hexToBytes(messageBytes);
        
        console.log("  公钥 X:", publicKey[0].toString(16));
        console.log("  公钥 Y:", publicKey[1].toString(16));
        console.log("  Gamma X:", proof[0].toString(16));
        console.log("  Gamma Y:", proof[1].toString(16));
        console.log("  c:", proof[2].toString(16));
        console.log("  s:", proof[3].toString(16));
        
        // 步骤 4: 在合约中验证
        console.log("\n--- 步骤 4: 合约验证 ---");
        const startTime = Date.now();
        const verifyResult = await helper.verify.call(publicKey, proof, message);
        const endTime = Date.now();
        
        console.log(`  验证结果: ${verifyResult ? '✅ 成功' : '❌ 失败'}`);
        console.log(`  耗时: ${endTime - startTime} ms`);
        
        if (!verifyResult) {
            throw new Error('合约验证失败');
        }
        
        // 步骤 5: 提取随机数并对比
        console.log("\n--- 步骤 5: 提取随机数并对比 ---");
        const contractRandom = await helper.gammaToHash.call(proof[0], proof[1]);
        const expectedRandom = '0x' + vrfData.random;
        
        console.log("  合约生成的随机数:", contractRandom);
        console.log("  C 程序生成的随机数:", expectedRandom);
        console.log("  随机数是否一致:", contractRandom.toLowerCase() === expectedRandom.toLowerCase() ? '✅' : '❌');
        
        if (contractRandom.toLowerCase() !== expectedRandom.toLowerCase()) {
            throw new Error('随机数不一致');
        }
        
        console.log("\n✅ VRF 验证完成：C 生成的证明在合约中验证成功！");
        console.log("========================================\n");
        
        // ============================================
        // 第三部分：委员会轮换
        // ============================================
        console.log("\n========================================");
        console.log("   第三部分：委员会轮换");
        console.log("========================================\n");
        
        // 检查是否可以轮换
        const canRotate = await committeeRotation.canRotate();
        console.log("是否可以进行轮换:", canRotate ? "✅ 是" : "❌ 否");
        
        if (!canRotate) {
            const rotationInfo = await committeeRotation.getRotationInfo();
            const nextRotationTime = rotationInfo.nextTime;
            const currentTime = Math.floor(Date.now() / 1000);
            const waitTime = nextRotationTime.toNumber() - currentTime;
            
            console.log(`⏰ 轮换时间未到，还需等待 ${waitTime} 秒`);
            console.log(`   下次轮换时间: ${new Date(nextRotationTime.toNumber() * 1000).toLocaleString()}`);
        } else {
            // 直接提交第二部分生成的新 VRF，替换合约中的旧 VRF
            console.log("\n📤 提交第二部分生成的新 VRF 到 CommitteeRotation 合约...");
            console.log(`   新 VRF: ${vrfData.random.substring(0, 16)}...（每次运行都不同）`);
            
            const randomBytes32 = '0x' + vrfData.random;
            const proofBytes = '0x' + vrfData.proof;
            const publicKeyBytes = '0x' + vrfData.publicKey;
            const messageBytes = web3.utils.utf8ToHex(testMessage);
            
            try {
                // 提交新 VRF（会自动替换旧的 VRF）
                const submitTx = await committeeRotation.submitVRFRandomWithProof(
                    randomBytes32,
                    proofBytes,
                    publicKeyBytes,
                    messageBytes,
                    { from: accounts[0] }
                );
                console.log("  ✅ VRF 提交成功，交易哈希:", submitTx.tx);
                
                // 验证 VRF 证明
                console.log("\n🔍 验证 VRF 证明...");
                const verifyTx = await committeeRotation.verifyVRFProof({ from: accounts[0] });
                
                // 检查验证结果
                const verifyResult = await committeeRotation.currentVRFVerified();
                console.log(`  ${verifyResult ? "✅" : "❌"} VRF 验证${verifyResult ? "成功" : "失败"}`);
                
            } catch (error) {
                console.log("  ❌ 提交或验证 VRF 失败:", error.message);
                throw error;
            }
            
            // 执行轮换
            // 检查候选池是否足够
            const poolSize = candidatePool.length;
            const maxCommitteeSize = await committeeRotation.MAX_COMMITTEE_SIZE();
            
            console.log(`\n候选池检查:`);
            console.log(`  候选池大小: ${poolSize}`);
            console.log(`  需要成员数: ${maxCommitteeSize.toNumber()}`);
            
            if (poolSize < maxCommitteeSize.toNumber()) {
                console.log(`  ⚠️  候选者不足，无法执行轮换（需要至少 ${maxCommitteeSize.toNumber()} 个候选者）`);
            } else {
                console.log("\n🔄 执行委员会轮换...");
                try {
                    const rotateTx = await committeeRotation.rotateCommittee({ from: accounts[0] });
                    console.log("  ✅ 轮换成功，交易哈希:", rotateTx.tx);
                    
                    // 获取新委员会
                    const newCommittee = await committeeRotation.getCurrentCommittee();
                    console.log("\n新委员会成员:");
                    const committeeAddresses = [];
                    for (let i = 0; i < newCommittee.length; i++) {
                        if (newCommittee[i] !== "0x0000000000000000000000000000000000000000") {
                            const reputation = await reputationManager.calculateReputation(newCommittee[i]);
                            console.log(`  ${i + 1}. ${newCommittee[i]} - 声誉: ${reputation}`);
                            committeeAddresses.push(newCommittee[i]);
                        }
                    }
                    
                    // 获取轮换信息
                    const newRotationInfo = await committeeRotation.getRotationInfo();
                    console.log("\n轮换信息:");
                    console.log(`  轮换次数: ${newRotationInfo.count.toString()}`);
                    console.log(`  上次轮换时间: ${new Date(newRotationInfo.lastTime.toNumber() * 1000).toLocaleString()}`);
                    console.log(`  下次轮换时间: ${new Date(newRotationInfo.nextTime.toNumber() * 1000).toLocaleString()}`);
                    
                    // 写入新委员会到文件（使用原子写入避免读取冲突）
                    try {
                        const out = committeeAddresses.map(a => a.toString()).join('\n');
                        const tmpPath = COMMITTEE_MEMBERS_FILE + '.tmp';
                        
                        // 先写入临时文件
                        fs.writeFileSync(tmpPath, out);
                        // 原子性地重命名（避免读取时文件不完整）
                        fs.renameSync(tmpPath, COMMITTEE_MEMBERS_FILE);
                        
                        console.log('\n✅ 已写入新委员会成员到文件');
                        console.log(`文件路径: ${COMMITTEE_MEMBERS_FILE}`);
                        console.log('新委员会成员:');
                        committeeAddresses.forEach((addr, idx) => {
                            console.log(`  成员 ${idx + 1}: ${addr}`);
                        });
                    } catch (fileError) {
                        console.log(`\n⚠️  保存文件失败: ${fileError.message}`);
                    }
                    
                } catch (error) {
                    console.log("  ❌ 轮换失败:", error.message);
                    console.log("  错误详情:", error);
                }
            }
        }
        
        console.log("\n========================================");
        console.log("   所有操作完成！");
        console.log("========================================\n");
        
        callback();
    } catch (error) {
        console.error('\n❌ 错误:', error.message);
        console.error(error.stack);
        callback(error);
    }
};
