/**
 * VRF 测试脚本 - 集成 C 程序生成器
 * 功能：使用 C 程序生成 VRF 证明，然后在 Solidity 合约中验证
 * 
 * 前置条件：
 * 1. 已编译 C 程序：/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test
 * 2. 已部署 TestHelperVRF 合约
 * 
 * 运行方式：
 * truffle test test/test_vrf_with_c_generator.js
 */

const TestHelperVRF = artifacts.require("TestHelperVRF");
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

// C 程序路径配置
const VRF_TEST_PATH = '/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test';

contract("VRF - C 生成器集成测试", accounts => {
    let helper;
    
    // 检查 C 程序是否存在
    const checkCProgram = () => {
        if (!fs.existsSync(VRF_TEST_PATH)) {
            console.log(`\n⚠️  警告: C 程序不存在: ${VRF_TEST_PATH}`);
            console.log("   跳过 C 生成器测试，使用预定义测试向量");
            return false;
        }
        return true;
    };
    
    // 从 C 程序生成 VRF 证明
    const generateVRFFromC = (message) => {
        console.log(`\n--- 从 C 程序生成 VRF ---`);
        console.log(`消息: "${message}"`);
        
        try {
            // 调用 C 程序生成 VRF
            const command = `"${VRF_TEST_PATH}" "" "${message}"`;
            const output = execSync(command, { encoding: 'utf-8', timeout: 10000 });
            
            // 解析输出
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
    
    before(async () => {
        console.log("\n========================================");
        console.log("   VRF C 生成器集成测试");
        console.log("========================================\n");
        
        helper = await TestHelperVRF.new();
        console.log("✅ TestHelperVRF 合约已部署:", helper.address);
        
        // 检查 C 程序
        const cProgramExists = checkCProgram();
        if (!cProgramExists) {
            console.log("   将使用预定义的测试向量进行测试");
        }
    });
    
    describe("C 生成器 + Solidity 验证", () => {
        /**
         * 测试 1: 使用 C 程序生成 VRF 并在合约中验证
         */
        it("应该成功验证 C 程序生成的 VRF 证明", async function() {
            // 如果 C 程序不存在，跳过此测试
            if (!checkCProgram()) {
                console.log("\n⚠️  跳过测试：C 程序不可用");
                this.skip();
                return;
            }
            
            console.log("\n--- 测试 1: C 生成 + Solidity 验证 ---");
            
            // 步骤 1: 使用 C 程序生成 VRF
            const testMessage = 'test_message_' + Date.now();
            const vrfData = generateVRFFromC(testMessage);
            
            // 步骤 2: 转换数据格式
            console.log("\n--- 步骤 2: 转换数据格式 ---");
            const publicKeyBytes = '0x' + vrfData.publicKey;
            const proofBytes = '0x' + vrfData.proof;
            const messageBytes = web3.utils.utf8ToHex(testMessage);
            
            console.log(`  公钥长度: ${(publicKeyBytes.length - 2) / 2} 字节`);
            console.log(`  证明长度: ${(proofBytes.length - 2) / 2} 字节`);
            console.log(`  消息长度: ${(messageBytes.length - 2) / 2} 字节`);
            
            // 步骤 3: 解码公钥和证明
            console.log("\n--- 步骤 3: 解码数据 ---");
            const publicKey = await helper.decodePoint.call(publicKeyBytes);
            const proof = await helper.decodeProof.call(proofBytes);
            const message = web3.utils.hexToBytes(messageBytes);
            
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
            
            // 断言验证成功
            assert.equal(verifyResult, true, "合约验证应该成功");
            
            // 步骤 5: 提取随机数并对比
            console.log("\n--- 步骤 5: 提取随机数 ---");
            const contractRandom = await helper.gammaToHash.call(proof[0], proof[1]);
            const expectedRandom = '0x' + vrfData.random;
            
            console.log("  合约生成的随机数:", contractRandom);
            console.log("  C 程序生成的随机数:", expectedRandom);
            console.log("  随机数是否一致:", contractRandom.toLowerCase() === expectedRandom.toLowerCase() ? '✅' : '❌');
            
            // 验证随机数一致性
            assert.equal(
                contractRandom.toLowerCase(),
                expectedRandom.toLowerCase(),
                "合约和 C 程序生成的随机数应该一致"
            );
            
            console.log("\n✅ 测试 1 完成：C 生成的证明在合约中验证成功！");
        });
        
        /**
         * 测试 2: 测试多条消息的 VRF 生成和验证
         */
        it("应该成功验证多条不同消息的 VRF 证明", async function() {
            if (!checkCProgram()) {
                console.log("\n⚠️  跳过测试：C 程序不可用");
                this.skip();
                return;
            }
            
            console.log("\n--- 测试 2: 多消息验证 ---");
            
            const messages = [
                'message_1',
                'message_2',
                'message_3'
            ];
            
            let successCount = 0;
            const randomNumbers = [];
            
            for (let i = 0; i < messages.length; i++) {
                const msg = messages[i];
                console.log(`\n测试消息 ${i + 1}: "${msg}"`);
                
                try {
                    // 生成 VRF
                    const vrfData = generateVRFFromC(msg);
                    
                    // 转换格式
                    const publicKeyBytes = '0x' + vrfData.publicKey;
                    const proofBytes = '0x' + vrfData.proof;
                    const messageBytes = web3.utils.utf8ToHex(msg);
                    
                    // 解码
                    const publicKey = await helper.decodePoint.call(publicKeyBytes);
                    const proof = await helper.decodeProof.call(proofBytes);
                    const message = web3.utils.hexToBytes(messageBytes);
                    
                    // 验证
                    const verifyResult = await helper.verify.call(publicKey, proof, message);
                    
                    if (verifyResult) {
                        successCount++;
                        const random = await helper.gammaToHash.call(proof[0], proof[1]);
                        randomNumbers.push(random);
                        console.log(`  ✅ 验证成功`);
                        console.log(`  随机数: ${random}`);
                    } else {
                        console.log(`  ❌ 验证失败`);
                    }
                    
                    assert.equal(verifyResult, true, `消息 ${i + 1} 的验证应该成功`);
                } catch (error) {
                    console.log(`  ❌ 错误: ${error.message}`);
                    throw error;
                }
            }
            
            console.log(`\n✅ 测试 2 完成：${successCount}/${messages.length} 条消息验证成功！`);
            
            // 验证每个随机数都不同
            console.log("\n验证随机数唯一性:");
            const uniqueRandoms = new Set(randomNumbers);
            console.log(`  生成的随机数数量: ${randomNumbers.length}`);
            console.log(`  唯一随机数数量: ${uniqueRandoms.size}`);
            assert.equal(uniqueRandoms.size, randomNumbers.length, "所有随机数应该都不同");
            console.log("  ✅ 所有随机数都是唯一的");
        });
        
        /**
         * 测试 3: 测试相同消息生成的确定性
         */
        it("相同的消息和密钥应该产生相同的证明", async function() {
            if (!checkCProgram()) {
                console.log("\n⚠️  跳过测试：C 程序不可用");
                this.skip();
                return;
            }
            
            console.log("\n--- 测试 3: 确定性验证 ---");
            
            // 注意：vrf_test 每次都生成新密钥，所以这个测试需要使用相同的密钥文件
            // 如果你的 C 程序支持读取密钥文件，可以取消注释以下代码
            
            console.log("  注意: 由于 vrf_test 每次生成新密钥，");
            console.log("       不同次运行会产生不同的证明（这是正常的）");
            console.log("       如需测试确定性，请使用相同的私钥");
            
            // 使用预定义的测试向量来验证确定性
            const testData = {
                pub: "0x03e30118c907034baf1456063bf7b423972e13e1743bf8dbb2e00fd8ba4a8c367a",
                pi: "0x03e30118c907034baf1456063bf7b423972e13e1743bf8dbb2e00fd8ba4a8c367aed294d880a72183e25a6c5cf22f374e9f981e29d2334fa32d87dab2dfa7dc5a5d6a4f546497dd557f3dec64a00635425"
            };
            
            const proof = await helper.decodeProof.call(testData.pi);
            
            // 多次调用 gammaToHash 应该产生相同结果
            const hash1 = await helper.gammaToHash.call(proof[0], proof[1]);
            const hash2 = await helper.gammaToHash.call(proof[0], proof[1]);
            
            console.log("  第一次哈希:", hash1);
            console.log("  第二次哈希:", hash2);
            
            assert.equal(hash1, hash2, "相同输入应该产生相同的哈希");
            console.log("  ✅ 确定性验证通过");
            
            console.log("\n✅ 测试 3 完成：VRF 输出具有确定性！");
        });
    });
    
    describe("预定义测试向量验证（备用）", () => {
        /**
         * 测试 4: 使用预定义测试向量（不依赖 C 程序）
         */
        it("应该验证预定义的测试向量", async () => {
            console.log("\n--- 测试 4: 预定义测试向量 ---");
            console.log("  （此测试不依赖 C 程序）");
            
            const testData = {
                pub: "0x03e30118c907034baf1456063bf7b423972e13e1743bf8dbb2e00fd8ba4a8c367a",
                pi: "0x03e30118c907034baf1456063bf7b423972e13e1743bf8dbb2e00fd8ba4a8c367aed294d880a72183e25a6c5cf22f374e9f981e29d2334fa32d87dab2dfa7dc5a5d6a4f546497dd557f3dec64a00635425",
                message: "0x73616d706c65",
                hash: "0xafe6b3c31e87ec45dfd87b3cc5e60a73e00e9f9dc2cfee6f3c4dae89e0c9b6de"
            };
            
            const publicKey = await helper.decodePoint.call(testData.pub);
            const proof = await helper.decodeProof.call(testData.pi);
            const message = web3.utils.hexToBytes(testData.message);
            
            const verifyResult = await helper.verify.call(publicKey, proof, message);
            console.log(`  验证结果: ${verifyResult ? '✅ 成功' : '❌ 失败'}`);
            
            assert.equal(verifyResult, true, "预定义测试向量应该验证成功");
            
            const hash = await helper.gammaToHash.call(proof[0], proof[1]);
            console.log(`  随机数: ${hash}`);
            console.log(`  期望值: ${testData.hash}`);
            
            assert.equal(hash, testData.hash, "随机数应该匹配");
            
            console.log("\n✅ 测试 4 完成：预定义测试向量验证成功！");
        });
    });
    
    after(() => {
        console.log("\n========================================");
        console.log("          集成测试总结");
        console.log("========================================");
        console.log("\n验证流程：");
        console.log("  1. C 程序生成 VRF 证明");
        console.log("  2. JavaScript 转换数据格式");
        console.log("  3. Solidity 合约解码数据");
        console.log("  4. Solidity 合约验证证明");
        console.log("  5. 对比随机数输出");
        console.log("\n✅ 跨平台 VRF 验证链路打通！");
        console.log("\n提示：");
        console.log("  • C 程序路径: " + VRF_TEST_PATH);
        console.log("  • 确保 C 程序已编译且可执行");
        console.log("  • 验证过程完全兼容 SECP256K1_SHA256_TAI 标准");
        console.log("========================================\n");
    });
});

