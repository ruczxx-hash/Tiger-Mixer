// 从命令行参数接收 VRF 数据并调用合约验证
// 用法: truffle exec scripts/verify_vrf_with_contract.js <proof> <publicKey> <message> <random>

module.exports = async function(callback) {
    try {
        const args = process.argv.slice(4); // 跳过 truffle exec script.js
        
        if (args.length < 4) {
            console.error('用法: truffle exec scripts/verify_vrf_with_contract.js <proof> <publicKey> <message> <random>');
            console.error('参数:');
            console.error('  proof:     81字节的VRF证明 (hex, 带或不带0x前缀)');
            console.error('  publicKey: 33字节的公钥 (hex, 带或不带0x前缀)');
            console.error('  message:   原始消息 (hex, 带或不带0x前缀)');
            console.error('  random:    32字节的随机数 (hex, 带或不带0x前缀)');
            callback(new Error('参数不足'));
            return;
        }
        
        let [proofHex, publicKeyHex, messageHex, randomHex] = args;
        
        // 确保都有 0x 前缀
        const ensureHex = (str) => str.startsWith('0x') ? str : '0x' + str;
        proofHex = ensureHex(proofHex);
        publicKeyHex = ensureHex(publicKeyHex);
        messageHex = ensureHex(messageHex);
        randomHex = ensureHex(randomHex);
        
        console.log('\n=== 合约端 VRF 验证 ===\n');
        console.log('输入数据:');
        console.log(`  证明长度: ${(proofHex.length - 2) / 2} 字节`);
        console.log(`  公钥长度: ${(publicKeyHex.length - 2) / 2} 字节`);
        console.log(`  消息长度: ${(messageHex.length - 2) / 2} 字节`);
        console.log(`  随机数: ${randomHex}`);
        console.log('');
        
        const CommitteeRotation = artifacts.require('CommitteeRotation');
        const committeeRotation = await CommitteeRotation.deployed();
        
        // 获取当前轮次
        const rotationInfo = await committeeRotation.getRotationInfo();
        const currentRotationCount = rotationInfo.count.toNumber();
        
        // 检查当前轮次是否可用
        const currentRandom = await committeeRotation.rotationRandom(currentRotationCount);
        const currentVerified = await committeeRotation.randomVerified(currentRotationCount);
        
        let targetRotationCount;
        if (currentRandom !== '0x0000000000000000000000000000000000000000000000000000000000000000') {
            console.log(`当前轮次 ${currentRotationCount} 状态:`);
            console.log(`  已有随机数: ${currentRandom}`);
            console.log(`  已验证: ${currentVerified}`);
            console.log(`\n使用下一轮次: ${currentRotationCount + 1}\n`);
            targetRotationCount = currentRotationCount + 1;
        } else {
            console.log(`使用当前轮次: ${currentRotationCount}\n`);
            targetRotationCount = currentRotationCount;
        }
        
        console.log(`目标轮次: ${targetRotationCount}\n`);
        
        // 检查是否已经提交过
        const existingRandom = await committeeRotation.rotationRandom(targetRotationCount);
        if (existingRandom !== '0x0000000000000000000000000000000000000000000000000000000000000000') {
            console.log(`⚠️  轮次 ${targetRotationCount} 已有随机数: ${existingRandom}`);
            
            // 检查是否已验证
            const isVerified = await committeeRotation.randomVerified(targetRotationCount);
            if (isVerified) {
                console.log(`✅ 该轮次随机数已验证\n`);
                callback();
                return;
            } else {
                console.log(`⚠️  该轮次随机数未验证，将尝试验证\n`);
                
                // 尝试验证已有数据
                try {
                    const accounts = await web3.eth.getAccounts();
                    console.log(`调用 verifyVRFProof(${targetRotationCount})...`);
                    
                    const verifyTx = await committeeRotation.verifyVRFProof(
                        targetRotationCount,
                        { from: accounts[0] }
                    );
                    
                    console.log(`✅ 验证交易已提交: ${verifyTx.tx}`);
                    
                    // 等待确认
                    await new Promise(resolve => setTimeout(resolve, 2000));
                    
                    const isNowVerified = await committeeRotation.randomVerified(targetRotationCount);
                    if (isNowVerified) {
                        console.log(`✅ 合约验证成功\n`);
                        
                        // 查询调试事件
                        await printDebugEvents(committeeRotation, targetRotationCount, verifyTx.receipt.blockNumber);
                        
                        callback();
                    } else {
                        console.log(`❌ 合约验证失败\n`);
                        callback(new Error('验证失败'));
                    }
                    return;
                } catch (error) {
                    console.log(`❌ 验证执行失败: ${error.message}\n`);
                    callback(error);
                    return;
                }
            }
        }
        
        // 提交新的 VRF 数据
        console.log('提交 VRF 数据到合约...');
        const accounts = await web3.eth.getAccounts();
        
        const submitTx = await committeeRotation.submitVRFRandomWithProof(
            targetRotationCount,
            randomHex,
            proofHex,
            publicKeyHex,
            messageHex,
            { from: accounts[0] }
        );
        
        console.log(`✅ VRF 数据已提交: ${submitTx.tx}\n`);
        
        // 等待交易确认
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        // 验证提交的数据
        const contractRandom = await committeeRotation.rotationRandom(targetRotationCount);
        const contractProof = await committeeRotation.rotationProof(targetRotationCount);
        const contractPublicKey = await committeeRotation.rotationPublicKey(targetRotationCount);
        const contractMessage = await committeeRotation.rotationMessage(targetRotationCount);
        
        console.log('合约中的数据:');
        console.log(`  随机数: ${contractRandom}`);
        console.log(`  证明长度: ${(contractProof.length - 2) / 2} 字节`);
        console.log(`  公钥长度: ${(contractPublicKey.length - 2) / 2} 字节`);
        console.log(`  消息长度: ${(contractMessage.length - 2) / 2} 字节`);
        console.log('');
        
        // 验证数据一致性
        let dataMatches = true;
        if (contractRandom.toLowerCase() !== randomHex.toLowerCase()) {
            console.log(`❌ 随机数不一致`);
            dataMatches = false;
        } else {
            console.log(`✅ 随机数一致`);
        }
        
        if (contractProof.toLowerCase() !== proofHex.toLowerCase()) {
            console.log(`❌ 证明不一致`);
            dataMatches = false;
        } else {
            console.log(`✅ 证明一致`);
        }
        
        if (contractPublicKey.toLowerCase() !== publicKeyHex.toLowerCase()) {
            console.log(`❌ 公钥不一致`);
            dataMatches = false;
        } else {
            console.log(`✅ 公钥一致`);
        }
        
        if (contractMessage.toLowerCase() !== messageHex.toLowerCase()) {
            console.log(`❌ 消息不一致`);
            dataMatches = false;
        } else {
            console.log(`✅ 消息一致`);
        }
        
        console.log('');
        
        if (!dataMatches) {
            console.log('⚠️  数据传输存在问题，跳过验证\n');
            callback(new Error('数据不一致'));
            return;
        }
        
        // 执行合约验证
        console.log('执行合约验证...');
        const verifyTx = await committeeRotation.verifyVRFProof(
            targetRotationCount,
            { from: accounts[0] }
        );
        
        console.log(`✅ 验证交易已提交: ${verifyTx.tx}\n`);
        
        // 等待交易确认
        await new Promise(resolve => setTimeout(resolve, 2000));
        
        // 检查验证结果
        const isVerified = await committeeRotation.randomVerified(targetRotationCount);
        const finalRandom = await committeeRotation.rotationRandom(targetRotationCount);
        
        if (isVerified) {
            console.log(`✅ 合约验证成功`);
            console.log(`最终随机数: ${finalRandom}\n`);
            
            // 查询调试事件
            await printDebugEvents(committeeRotation, targetRotationCount, verifyTx.receipt.blockNumber);
            
            callback();
        } else {
            console.log(`❌ 合约验证失败`);
            console.log(`最终随机数: ${finalRandom}`);
            
            if (finalRandom === '0x0000000000000000000000000000000000000000000000000000000000000000') {
                console.log(`⚠️  随机数已被清空（验证失败导致）\n`);
            }
            
            // 查询调试事件
            await printDebugEvents(committeeRotation, targetRotationCount, verifyTx.receipt.blockNumber);
            
            callback(new Error('合约验证失败'));
        }
        
    } catch (error) {
        console.error('\n❌ 错误:', error.message);
        if (error.stack) {
            console.error(error.stack);
        }
        callback(error);
    }
};

// 打印调试事件
async function printDebugEvents(committeeRotation, round, fromBlock) {
    console.log('查询调试事件...');
    
    try {
        const events = await committeeRotation.getPastEvents('VRFVerifyDebug', {
            filter: { round: round },
            fromBlock: fromBlock,
            toBlock: 'latest'
        });
        
        if (events.length > 0) {
            console.log(`\n找到 ${events.length} 个调试事件:\n`);
            for (const event of events) {
                const { step, value1, value2, value3, value4 } = event.returnValues;
                console.log(`[${step}]`);
                if (value1 !== '0') console.log(`  value1: ${value1}`);
                if (value2 !== '0') console.log(`  value2: ${value2}`);
                if (value3 !== '0') console.log(`  value3: ${value3}`);
                if (value4 !== '0') console.log(`  value4: ${value4}`);
            }
            console.log('');
        } else {
            console.log('未找到调试事件\n');
        }
    } catch (error) {
        console.log(`⚠️  无法查询调试事件: ${error.message}\n`);
    }
}

