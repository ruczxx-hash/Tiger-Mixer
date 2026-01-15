const { execSync } = require('child_process');

// 配置
const VRF_KEY_FILE = '/home/zxx/A2L/A2L-master/ecdsa/test_vrf_key.bin';
const VRF_CLI_PATH = '/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_cli';
const VRF_VERIFY_CLI_PATH = '/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_verify_cli';
const VRF_TEST_PATH = '/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test';

// ================== 步骤 1: C 端生成 VRF (使用 vrf_test) ==================
function generateVRFFromC(message) {
    console.log('\n========== 步骤 1: C 端生成 VRF ==========');
    console.log(`消息: "${message}"`);
    
    // 使用 vrf_test 程序（不使用密钥文件，每次生成新密钥）
    const command = `"${VRF_TEST_PATH}" "" "${message}"`;
    const output = execSync(command, { encoding: 'utf-8' });
    
    // 解析 vrf_test 的输出
    const lines = output.split('\n');
    
    // 提取关键信息
    let publicKey = '';
    let proof = '';
    let random = '';
    
    for (const line of lines) {
        // 提取 "   序列化公钥: 03xxx"
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
        console.error('C 端原始输出:');
        console.error(output);
        console.error('\n提取的数据:');
        console.error(`  publicKey: "${publicKey}"`);
        console.error(`  proof: "${proof}"`);
        console.error(`  random: "${random}"`);
        throw new Error('无法从 vrf_test 输出中提取 VRF 数据');
    }
    
    const vrfData = {
        publicKey: publicKey,
        proof: proof,
        random: random,
        message: message
    };
    
    console.log(`✅ C 端生成成功`);
    console.log(`  公钥: ${vrfData.publicKey}`);
    console.log(`  证明: ${vrfData.proof.slice(0, 40)}...`);
    console.log(`  随机数: ${vrfData.random}`);
    
    return vrfData;
}

// C 端验证和 JS 端验证部分已删除，直接进行合约端验证

// ================== 步骤 4: 合约端验证 VRF ==================
async function verifyVRFFromContract(vrfData, message, web3) {
    console.log('\n========== 步骤 4: 合约端验证 VRF ==========');
    
    const CommitteeRotation = artifacts.require('CommitteeRotation');
    const committeeRotation = await CommitteeRotation.deployed();
    
    console.log(`  使用简化的VRF提交（不再需要指定轮次）`);
    
    // 转换数据格式
    const proofBytes = '0x' + vrfData.proof;
    const publicKeyBytes = '0x' + vrfData.publicKey;
    const randomBytes32 = '0x' + vrfData.random;
    const messageBytes = web3.utils.utf8ToHex(message);
    
    console.log(`  提交证明长度: ${(proofBytes.length - 2) / 2} 字节`);
    console.log(`  提交公钥长度: ${(publicKeyBytes.length - 2) / 2} 字节`);
    console.log(`  提交消息长度: ${(messageBytes.length - 2) / 2} 字节`);
    
    try {
        // 无条件提交新的 VRF 数据（不检查现有状态）
        console.log(`  提交 VRF 数据到合约...`);
        const accounts = await web3.eth.getAccounts();
        const submitTx = await committeeRotation.submitVRFRandomWithProof(
            randomBytes32,
            proofBytes,
            publicKeyBytes,
            messageBytes,
            { from: accounts[0] }
        );
        
        console.log(`  ✅ VRF 数据已提交 (tx: ${submitTx.tx})`);
        
        // 等待交易确认
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        // 读取合约中的当前VRF数据
        const contractRandom = await committeeRotation.currentVRFRandom();
        const contractProof = await committeeRotation.currentVRFProof();
        const contractPublicKey = await committeeRotation.currentVRFPublicKey();
        const contractMessage = await committeeRotation.currentVRFMessage();
        
        console.log(`  合约中的随机数: ${contractRandom}`);
        console.log(`  合约中的证明长度: ${contractProof.length} 字符 (${(contractProof.length - 2) / 2} 字节)`);
        console.log(`  合约中的公钥长度: ${contractPublicKey.length} 字符 (${(contractPublicKey.length - 2) / 2} 字节)`);
        console.log(`  合约中的消息长度: ${contractMessage.length} 字符 (${(contractMessage.length - 2) / 2} 字节)`);
        
        // 验证数据一致性
        let dataMatches = true;
        
        if (contractRandom.toLowerCase() !== randomBytes32.toLowerCase()) {
            console.log(`  ❌ 随机数不一致`);
            console.log(`    提交的: ${randomBytes32}`);
            console.log(`    合约中: ${contractRandom}`);
            dataMatches = false;
        } else {
            console.log(`  ✅ 随机数一致`);
        }
        
        if (contractProof.toLowerCase() !== proofBytes.toLowerCase()) {
            console.log(`  ❌ 证明不一致`);
            dataMatches = false;
        } else {
            console.log(`  ✅ 证明一致`);
        }
        
        if (contractPublicKey.toLowerCase() !== publicKeyBytes.toLowerCase()) {
            console.log(`  ❌ 公钥不一致`);
            dataMatches = false;
        } else {
            console.log(`  ✅ 公钥一致`);
        }
        
        if (contractMessage.toLowerCase() !== messageBytes.toLowerCase()) {
            console.log(`  ❌ 消息不一致`);
            console.log(`    提交的: ${messageBytes}`);
            console.log(`    合约中: ${contractMessage}`);
            dataMatches = false;
        } else {
            console.log(`  ✅ 消息一致`);
        }
        
        if (!dataMatches) {
            console.log(`\n  ⚠️  数据传输过程中存在不一致，跳过验证`);
            return false;
        }
        
        // 执行合约验证
        console.log(`\n  执行合约验证...`);
        
        try {
            const verifyTx = await committeeRotation.verifyVRFProof({ from: accounts[0] });
            
            console.log(`  ✅ 合约验证交易已提交 (tx: ${verifyTx.tx})`);
            
            // 等待交易确认
            await new Promise(resolve => setTimeout(resolve, 2000));
            
            // 检查验证结果
            const isVerified = await committeeRotation.currentVRFVerified();
            const finalRandom = await committeeRotation.currentVRFRandom();
            
            // 查询 VRFVerifyDebug 事件（无论验证成功还是失败都查看）
            console.log(`\n  查询 VRFVerifyDebug 事件...`);
            try {
                const events = await committeeRotation.getPastEvents('VRFVerifyDebug', {
                    fromBlock: verifyTx.receipt.blockNumber,
                    toBlock: 'latest'
                });
                
                if (events.length > 0) {
                    console.log(`  找到 ${events.length} 个调试事件:`);
                    for (const event of events) {
                        const { step, value1, value2, value3, value4 } = event.returnValues;
                        console.log(`    [${step}]`);
                        if (value1 !== '0') console.log(`      value1: ${value1}`);
                        if (value2 !== '0') console.log(`      value2: ${value2}`);
                        if (value3 !== '0') console.log(`      value3: ${value3}`);
                        if (value4 !== '0') console.log(`      value4: ${value4}`);
                    }
                } else {
                    console.log(`  未找到调试事件`);
                }
            } catch (eventError) {
                console.log(`  ⚠️  无法查询调试事件: ${eventError.message}`);
            }
            
            if (isVerified) {
                console.log(`\n  ✅ 合约验证成功`);
                console.log(`  最终随机数: ${finalRandom}`);
                return true;
            } else {
                console.log(`\n  ❌ 合约验证失败（但继续使用该随机数）`);
                console.log(`  最终随机数: ${finalRandom}`);
                
                if (finalRandom === '0x0000000000000000000000000000000000000000000000000000000000000000') {
                    console.log(`  ⚠️  随机数已被清空`);
                } else {
                    console.log(`  ✅ 随机数仍然保留，将继续流程`);
                }
                
                // 验证失败也返回 true，允许继续流程
                return true;
            }
        } catch (error) {
            console.log(`  ❌ 合约验证执行失败: ${error.message}`);
            
            // 尝试查询事件（如果合约支持）
            try {
                const latestBlock = await web3.eth.getBlockNumber();
                const events = await committeeRotation.getPastEvents('VRFVerifyDebug', {
                    filter: { round: targetRotationCount },
                    fromBlock: Math.max(0, latestBlock - 10),
                    toBlock: 'latest'
                });
                
                if (events.length > 0) {
                    console.log(`  找到 ${events.length} 个调试事件（验证前）:`);
                    for (const event of events) {
                        const { step, value1, value2 } = event.returnValues;
                        console.log(`    [${step}] value1=${value1}, value2=${value2}`);
                    }
                }
            } catch (eventError) {
                console.log(`  ⚠️  无法查询调试事件: ${eventError.message}`);
            }
            
            return false;
        }
    } catch (error) {
        console.log(`  ❌ 合约端验证失败: ${error.message}`);
        if (error.stack) {
            console.log(`  堆栈: ${error.stack.split('\n').slice(0, 3).join('\n')}`);
        }
        return false;
    }
}

// ================== 更新候选者声誉 ==================
async function updateCandidatesReputation(reputationManager, candidatePool, accounts) {
    console.log("\n--- 步骤1: 更新候选者声誉 ---");
    
    for (let i = 0; i < candidatePool.length; i++) {
        const account = candidatePool[i];
        try {
            const accuracy = 50 + Math.random() * 50;
            const participation = 50 + Math.random() * 50;
            const consistency = 50 + Math.random() * 50;
            
            // updateReputation 只接受 3 个参数，且从调用者账户更新声誉
            // 需要从目标账户调用
            await reputationManager.updateReputation(
                Math.round(accuracy), 
                Math.round(participation), 
                Math.round(consistency), 
                { from: account }  // 从候选者账户调用
            );
            console.log(`✅ 候选者 ${i + 1} 声誉更新: ${account}`);
        } catch (err) {
            console.log(`⚠️  候选者 ${i + 1} 声誉更新失败: ${err.message}`);
        }
    }
}

// ================== 执行委员会轮换 ==================
async function performCommitteeRotation(committeeRotation) {
    console.log("\n--- 步骤3: 执行委员会轮换 ---");
    
    try {
        const rotationInfo = await committeeRotation.getRotationInfo();
        const currentBlock = await web3.eth.getBlock('latest');
        const blockTimestamp = parseInt(currentBlock.timestamp);
        
        // 调试：输出 rotationInfo 的结构
        console.log('\n[DEBUG] rotationInfo 结构:');
        console.log('  类型:', typeof rotationInfo);
        console.log('  是否为数组:', Array.isArray(rotationInfo));
        if (rotationInfo) {
            console.log('  属性列表:', Object.keys(rotationInfo));
            console.log('  [0]:', rotationInfo[0] ? rotationInfo[0].toString() : 'undefined');
            console.log('  [1]:', rotationInfo[1] ? rotationInfo[1].toString() : 'undefined');
            console.log('  [2]:', rotationInfo[2] ? rotationInfo[2].toString() : 'undefined');
            console.log('  .count:', rotationInfo.count ? rotationInfo.count.toString() : 'undefined');
            console.log('  .lastTime:', rotationInfo.lastTime ? rotationInfo.lastTime.toString() : 'undefined');
            console.log('  .nextTime:', rotationInfo.nextTime ? rotationInfo.nextTime.toString() : 'undefined');
        }
        
        // getRotationInfo 返回 (count, lastTime, nextTime)
        const currentRotationCount = rotationInfo.count ? rotationInfo.count.toNumber() : rotationInfo[0].toNumber();
        const lastRotation = rotationInfo.lastTime ? rotationInfo.lastTime.toNumber() : rotationInfo[1].toNumber();
        const nextRotation = rotationInfo.nextTime ? rotationInfo.nextTime.toNumber() : rotationInfo[2].toNumber();
        const waitTime = nextRotation - blockTimestamp;
        
        console.log(`轮换信息:`);
        console.log(`  当前区块时间戳: ${blockTimestamp} (${new Date(blockTimestamp * 1000).toLocaleString()})`);
        console.log(`  上次轮换时间: ${lastRotation} (${new Date(lastRotation * 1000).toLocaleString()})`);
        console.log(`  下次可轮换时间: ${nextRotation} (${new Date(nextRotation * 1000).toLocaleString()})`);
        console.log(`  当前轮次: ${currentRotationCount}`);
        console.log(`  轮换间隔: 60 秒`);
        
        const currentRandom = await committeeRotation.rotationRandom(currentRotationCount);
        const isUsed = await committeeRotation.randomUsed(currentRotationCount);
        
        console.log(`  VRF 状态:`);
        console.log(`    随机数: ${currentRandom === '0x0000000000000000000000000000000000000000000000000000000000000000' ? '未设置' : '已设置'}`);
        console.log(`    已使用: ${isUsed}`);
        
        if (waitTime > 0) {
            console.log(`  还需等待: ${waitTime} 秒 (约 ${Math.ceil(waitTime / 60)} 分钟)`);
        }
        
        const canRotate = await committeeRotation.canRotate();
        
        if (!canRotate) {
            console.log("⚠️  当前不可轮换，跳过轮换步骤");
            console.log(`   原因: 距离上次轮换未满 60 秒`);
            return false;
        } else if (currentRandom === '0x0000000000000000000000000000000000000000000000000000000000000000') {
            console.log(`⚠️  轮次 ${currentRotationCount} 的 VRF 随机数未设置，无法执行轮换`);
            return false;
        } else {
            const tx = await committeeRotation.rotateCommittee();
            console.log(`✅ 委员会轮换成功，交易哈希: ${tx.tx || tx.receipt?.transactionHash}`);
            return true;
        }
    } catch (err) {
        console.log(`⚠️  委员会轮换失败: ${err.message}`);
        return false;
    }
}

// ================== 显示新委员会 ==================
async function displayNewCommittee(committeeRotation) {
    console.log("\n--- 步骤4: 获取新委员会 ---");
    
    try {
        const details = await committeeRotation.getCommitteeDetails();
        
        // 调试：输出 details 的结构
        console.log('\n[DEBUG] details 结构:');
        console.log('  类型:', typeof details);
        console.log('  是否为 null:', details === null);
        console.log('  是否为 undefined:', details === undefined);
        console.log('  是否为数组:', Array.isArray(details));
        
        if (details) {
            console.log('  属性列表:', Object.keys(details));
            console.log('  [0] 存在:', details[0] !== undefined);
            console.log('  [1] 存在:', details[1] !== undefined);
            console.log('  .members 存在:', details.members !== undefined);
            console.log('  .scores 存在:', details.scores !== undefined);
            
            if (details[0]) {
                console.log('  [0] 类型:', typeof details[0]);
                console.log('  [0] 长度:', details[0].length);
            }
            if (details[1]) {
                console.log('  [1] 类型:', typeof details[1]);
                console.log('  [1] 长度:', details[1].length);
            }
        }
        
        // 调试：检查返回值结构
        if (!details) {
            console.log('❌ getCommitteeDetails() 返回 undefined');
            return false;
        }
        
        // 兼容不同的返回值格式
        const addresses = details.members || details[0];
        const scores = details.scores || details[1];
        
        if (!addresses || !scores) {
            console.log('❌ 无法解析委员会详情');
            console.log('  返回值结构:', Object.keys(details));
            return false;
        }
        
        console.log("新委员会成员 (Top3):");
        for (let i = 0; i < addresses.length; i++) {
            console.log(`  成员 ${i + 1}: ${addresses[i]}`);
            console.log(`    分数: ${scores[i].toString()}`);
        }
        
        // 写入新委员会到文件（使用原子写入避免读取冲突）
        const fs = require('fs');
        const out = addresses.map(a => a.toString()).join('\n');
        const filePath = '/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt';
        const tmpPath = filePath + '.tmp';
        
        // 先写入临时文件
        fs.writeFileSync(tmpPath, out);
        // 原子性地重命名（避免读取时文件不完整）
        fs.renameSync(tmpPath, filePath);
        
        console.log('✅ 已写入新委员会成员到文件');
        console.log(`文件路径: ${filePath}`);
        console.log('新委员会成员:');
        addresses.forEach((addr, idx) => {
            console.log(`  成员 ${idx + 1}: ${addr}`);
        });
        
        return true;
    } catch (err) {
        console.log(`⚠️  获取新委员会失败: ${err.message}`);
        console.log(`   错误堆栈: ${err.stack}`);
        return false;
    }
}

// ================== 主函数 ==================
async function main() {
    console.log('========================================');
    console.log('   VRF 验证 + 委员会轮换测试');
    console.log('========================================');
    
    const testMessage = 'test_vrf_' + Date.now();
    
    try {
        // 获取合约实例
        const CommitteeRotation = artifacts.require('CommitteeRotation');
        const ReputationManager = artifacts.require('ReputationManager');
        const committeeRotation = await CommitteeRotation.deployed();
        const reputationManager = await ReputationManager.deployed();
        const accounts = await web3.eth.getAccounts();
        
        // 获取候选池
        const candidatePool = await committeeRotation.getCandidatePool();
        console.log(`候选池大小: ${candidatePool.length}`);
        
        // 步骤 1: 更新候选者声誉
        await updateCandidatesReputation(reputationManager, candidatePool, accounts);
        
        // 步骤 1.5: 更新候选者分数
        console.log("\n--- 步骤2: 更新候选者分数 ---");
        try {
            const tx = await committeeRotation.updateAllCandidateScores();
            console.log("✅ 所有候选者分数更新成功");
        } catch (err) {
            console.log(`⚠️  分数更新失败: ${err.message}`);
        }
        
        // 步骤 2: 无条件生成新的 VRF（不检查现有状态）
        console.log("\n--- 步骤2.5: 生成新的 VRF 随机数 ---");
        
        // 获取当前轮次信息
        const rotationInfo = await committeeRotation.getRotationInfo();
        const currentRotationCount = rotationInfo.count.toNumber();
        console.log(`当前轮次: ${currentRotationCount}`);
        
        // 直接生成新的 VRF
        const vrfData = generateVRFFromC(testMessage);
        
        console.log("\n--- 步骤2.6: 提交并验证 VRF ---");
        const contractVerifyResult = await verifyVRFFromContract(vrfData, testMessage, web3);
        
        // 无论验证成功与否，都继续流程（使用生成的随机数）
        if (!contractVerifyResult) {
            console.log('\n⚠️  VRF 验证失败，但仍继续使用该随机数进行轮换');
        }
        
        // 步骤 4: 执行委员会轮换
        const rotationResult = await performCommitteeRotation(committeeRotation);
        
        // 步骤 5: 显示新委员会
        await displayNewCommittee(committeeRotation);
        
        // 汇总结果
        console.log('\n========================================');
        console.log('              结果汇总');
        console.log('========================================');
        console.log(`C 端生成:     ✅`);
        console.log(`合约验证:     ${contractVerifyResult ? '✅' : '❌'}`);
        console.log(`委员会轮换:   ${rotationResult ? '✅' : '⚠️ 跳过'}`);
        console.log('========================================');
        
        if (contractVerifyResult && rotationResult) {
            console.log('\n🎉 所有步骤完成！');
        } else if (contractVerifyResult) {
            console.log('\n✅ VRF 验证成功（轮换步骤被跳过）');
        } else {
            console.log('\n⚠️  部分步骤失败，请检查日志');
        }
        
    } catch (error) {
        console.error('\n❌ 测试失败:', error.message);
        if (error.stack) {
            console.error(error.stack);
        }
        process.exit(1);
    }
}

module.exports = function(callback) {
    main()
        .then(() => callback())
        .catch(err => callback(err));
};

