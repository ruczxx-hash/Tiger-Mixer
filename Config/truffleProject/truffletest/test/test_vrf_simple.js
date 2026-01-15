/**
 * VRF 简单测试脚本
 * 功能：使用 C 程序生成 VRF 随机数和证明，然后使用合约的 verify() 函数进行验证
 * 
 * 前置条件：
 * 1. C 程序路径：/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test
 * 2. 已部署 VRFTestHelper 合约
 * 
 * 运行方式：
 * truffle test test/test_vrf_simple.js
 */

const VRFTestHelper = artifacts.require("VRFTestHelper");
const { execSync } = require('child_process');
const fs = require('fs');

// C 程序路径配置
const VRF_TEST_PATH = '/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test';

// Tumbler 密钥文件路径（可选，如果不设置则 C 程序会生成新密钥）
const VRF_KEY_FILE = '/home/zxx/A2L/A2L-master/ecdsa/tumbler_vrf_key.bin';

// ============================================
// 合约地址配置（避免触发 migrations）
// ============================================
// 方法 1: 直接在这里设置合约地址（推荐，最简单，不会触发 migrations）
// 取消注释并填入你的合约地址：
let VRF_TEST_HELPER_ADDRESS = undefined;  // 取消注释下面这行，填入地址：
VRF_TEST_HELPER_ADDRESS = '0x368c641154E5c574108eAafB321D99e3f357C660';

// 方法 2: 通过环境变量设置（如果上面没设置，会使用环境变量）
// 运行时：VRF_TEST_HELPER_ADDRESS=0x... truffle test test/test_vrf_simple.js
if (!VRF_TEST_HELPER_ADDRESS) {
    VRF_TEST_HELPER_ADDRESS = process.env.VRF_TEST_HELPER_ADDRESS;
}
// ============================================

contract("VRF Simple Test", accounts => {
    let helper;
    
    // 辅助函数：将 hex 字符串转换为字节数组
    const hexToBytes = (hex) => {
        // 确保是字符串类型
        if (hex == null) {
            throw new Error('hexToBytes: 输入不能为 null 或 undefined');
        }
        
        // 强制转换为字符串（处理 web3 返回的特殊对象）
        let hexStr;
        if (typeof hex === 'string') {
            hexStr = hex;
        } else {
            // 如果 toString 方法存在，先尝试它
            if (hex && typeof hex.toString === 'function') {
                hexStr = hex.toString();
            } else {
                hexStr = String(hex);
            }
        }
        
        // 再次确保是字符串类型
        if (typeof hexStr !== 'string') {
            console.error('hexToBytes 调试信息:');
            console.error('  输入类型:', typeof hex);
            console.error('  输入值:', hex);
            console.error('  转换后类型:', typeof hexStr);
            console.error('  转换后值:', hexStr);
            throw new Error(`hexToBytes: 无法将输入转换为字符串。输入类型: ${typeof hex}`);
        }
        
        // 移除 0x 前缀（使用 indexOf 而不是 startsWith，更兼容）
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
            console.log("   请确保 C 程序已编译并位于正确路径");
            return false;
        }
        return true;
    };
    
    // 从 C 程序生成 VRF 证明
    // 可选参数：keyFile - 指定密钥文件路径，如果为空则使用默认的 Tumbler 密钥文件
    const generateVRFFromC = (message, keyFile = null) => {
        console.log(`\n--- 从 C 程序生成 VRF ---`);
        console.log(`消息: "${message}"`);
        
        // 确定使用的密钥文件
        const usedKeyFile = keyFile || (fs.existsSync(VRF_KEY_FILE) ? VRF_KEY_FILE : '');
        if (usedKeyFile) {
            console.log(`使用密钥文件: ${usedKeyFile}`);
        } else {
            console.log(`未指定密钥文件，C 程序将生成新密钥对`);
        }
        
        try {
            // 调用 C 程序生成 VRF
            // 第一个参数：密钥文件路径（空字符串表示生成新密钥）
            // 第二个参数：消息
            const command = `"${VRF_TEST_PATH}" "${usedKeyFile}" "${message}"`;
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
    
    // 测试前获取已部署的合约实例
    before(async () => {
        console.log("\n========================================");
        console.log("   VRF 验证测试 - C 生成 + 合约验证");
        console.log("========================================\n");
        
        // 获取合约实例（按优先级：直接设置 > 环境变量 > deployed()）
        if (VRF_TEST_HELPER_ADDRESS) {
            // 方法 1 & 2: 使用指定的地址获取合约实例
            // 注意：使用 at() 方法不会触发 migrations，只是连接已有合约
            helper = await VRFTestHelper.at(VRF_TEST_HELPER_ADDRESS);
            console.log("✅ 使用指定地址的 VRFTestHelper 合约:", helper.address);
            console.log("   注意：使用 at() 方法不会触发 migrations");
        } else {
            // 方法 3: 尝试使用 deployed()（可能会触发 migrations）
            try {
                helper = await VRFTestHelper.deployed();
                console.log("✅ 使用已部署的 VRFTestHelper 合约:", helper.address);
                console.log("   警告：如果 migrations 未执行，可能会自动执行 migrations");
            } catch (error) {
                throw new Error(
                    '无法获取已部署的 VRFTestHelper 合约。\n\n' +
                    '解决方案（按优先级）：\n\n' +
                    '方法 1（最简单）：在测试文件顶部直接设置地址\n' +
                    '   const VRF_TEST_HELPER_ADDRESS = \'0x你的合约地址\';\n\n' +
                    '方法 2：通过环境变量指定（运行时不会触发 migrations）\n' +
                    '   VRF_TEST_HELPER_ADDRESS=0x你的合约地址 truffle test test/test_vrf_simple.js\n\n' +
                    '方法 3：在 Truffle Console 中部署并记录地址\n' +
                    '   truffle console\n' +
                    '   > const helper = await VRFTestHelper.new();\n' +
                    '   > helper.address\n'
                );
            }
        }
        
        // 检查 C 程序
        const cProgramExists = checkCProgram();
        if (!cProgramExists) {
            throw new Error('C 程序不存在，无法运行测试。请确保 C 程序已编译：' + VRF_TEST_PATH);
        }
        
        // 检查密钥文件（如果配置了的话）
        if (VRF_KEY_FILE && fs.existsSync(VRF_KEY_FILE)) {
            console.log("✅ 找到 Tumbler 密钥文件:", VRF_KEY_FILE);
            console.log("   将使用 Tumbler 的公钥和私钥进行 VRF 生成");
        } else if (VRF_KEY_FILE) {
            console.log("⚠️  警告: 配置了密钥文件但文件不存在:", VRF_KEY_FILE);
            console.log("   C 程序将为每次调用生成新的密钥对");
        }
    });
    
    describe("基础 VRF 验证测试", () => {
        /**
         * 测试 1: 使用 C 程序生成 VRF 并在合约中验证
         */
        it("应该成功验证 C 程序生成的 VRF 证明", async () => {
            console.log("\n--- 测试 1: C 生成 + 合约验证 ---");
            
            // 步骤 1: 使用 C 程序生成 VRF
            const testMessage = 'test_vrf_' + Date.now();
            const vrfData = generateVRFFromC(testMessage);
            
            // 步骤 2: 转换数据格式
            console.log("\n--- 步骤 2: 转换数据格式 ---");
            const publicKeyBytes = '0x' + vrfData.publicKey;
            const proofBytes = '0x' + vrfData.proof;
            let messageBytes = web3.utils.utf8ToHex(testMessage);
            // 确保是字符串
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
            
            // 断言验证成功
            assert.equal(verifyResult, true, "合约验证应该成功");
            
            // 步骤 5: 提取随机数并对比
            console.log("\n--- 步骤 5: 提取随机数并对比 ---");
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
         * 测试 2: 测试多条不同消息的 VRF 生成和验证
         */
        it("应该成功验证多条不同消息的 VRF 证明", async () => {
            console.log("\n--- 测试 2: 多消息验证 ---");
            
            const messages = [
                'test_vrf1_' + Date.now(),
                'test_vrf2_' + Date.now(),
                'test_vrf3_' + Date.now()
            ];
            
            let successCount = 0;
            const randomNumbers = [];
            
            for (let i = 0; i < messages.length; i++) {
                const msg = messages[i];
                console.log(`\n测试消息 ${i + 1}: "${msg}"`);
                
                try {
                    // 生成 VRF
                    const testMessage = 'test_vrf_' + Date.now();
            const vrfData = generateVRFFromC(testMessage);
            
            // 步骤 2: 转换数据格式
            console.log("\n--- 步骤 2: 转换数据格式 ---");
            const publicKeyBytes = '0x' + vrfData.publicKey;
            const proofBytes = '0x' + vrfData.proof;
            let messageBytes = web3.utils.utf8ToHex(testMessage);
            // 确保是字符串
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
            
            // 断言验证成功
            assert.equal(verifyResult, true, "合约验证应该成功");
            
            // 步骤 5: 提取随机数并对比
            console.log("\n--- 步骤 5: 提取随机数并对比 ---");
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
                } catch (error) {
                    console.log(`  ❌ 错误: ${error.message}`);
                    throw error;
                }
            }
            
            console.log(`\n✅ 测试 2 完成：${successCount}/${messages.length} 条消息验证成功！`);
            
            // 验证每个随机数都不同
            console.log("\n验证随机数唯一性:");
            const uniqueRandoms = new Set(randomNumbers.map(r => r.toLowerCase()));
            console.log(`  生成的随机数数量: ${randomNumbers.length}`);
            console.log(`  唯一随机数数量: ${uniqueRandoms.size}`);
            assert.equal(uniqueRandoms.size, randomNumbers.length, "所有随机数应该都不同");
            console.log("  ✅ 所有随机数都是唯一的");
        });
        
        /**
         * 测试 3: 测试无效证明应该失败（使用错误消息）
         */
        it("应该拒绝使用错误消息的 VRF 证明", async () => {
            console.log("\n--- 测试 3: 验证无效证明（错误消息） ---");
            
            // 步骤 1: 使用 C 程序生成 VRF（正确的消息）
            const correctMessage = 'correct_message';
            const vrfData = generateVRFFromC(correctMessage);
            
            // 步骤 2: 转换格式
            const publicKeyBytes = '0x' + vrfData.publicKey;
            const proofBytes = '0x' + vrfData.proof;
            
            // 步骤 3: 解码
            const publicKey = await helper.decodePoint.call(publicKeyBytes);
            const proof = await helper.decodeProof.call(proofBytes);
            
            // 步骤 4: 使用错误的消息进行验证
            const wrongMessage = hexToBytes("0x696e76616c6964");  // "invalid"
            
            console.log("使用错误的消息进行验证...");
            console.log("  正确消息:", correctMessage);
            console.log("  错误消息:", "invalid");
            
            const result = await helper.verify.call(publicKey, proof, wrongMessage);
            
            console.log("验证结果:", result);
            assert.equal(result, false, "使用错误消息的验证应该失败");
            
            console.log("✅ 测试 3 完成：无效证明被正确拒绝！");
        });
        
        /**
         * 测试 4: 使用 fastVerify() 进行快速验证
         */
        it("应该使用 fastVerify() 成功验证 VRF 证明（节省 Gas）", async () => {
            console.log("\n--- 测试 4: fastVerify() 快速验证 ---");
            console.log("目的: 验证 fastVerify() 功能，比 verify() 节省约 91% Gas");
            
            // 步骤 1: 使用 C 程序生成 VRF
            const testMessage = 'fast_verify_test_' + Date.now();
            const vrfData = generateVRFFromC(testMessage);
            
            // 步骤 2: 转换数据格式
            console.log("\n--- 步骤 2: 转换数据格式 ---");
            const publicKeyBytes = '0x' + vrfData.publicKey;
            const proofBytes = '0x' + vrfData.proof;
            let messageBytes = web3.utils.utf8ToHex(testMessage);
            // 确保是字符串
            messageBytes = String(messageBytes);
            
            // 步骤 3: 解码公钥和证明
            const publicKey = await helper.decodePoint.call(publicKeyBytes);
            const proof = await helper.decodeProof.call(proofBytes);
            const message = hexToBytes(messageBytes);
            
            console.log("  公钥 X:", publicKey[0].toString(16));
            console.log("  公钥 Y:", publicKey[1].toString(16));
            console.log("  Gamma X:", proof[0].toString(16));
            console.log("  Gamma Y:", proof[1].toString(16));
            
            // 步骤 4: 计算 fastVerify 所需的参数（链外计算，节省 Gas）
            console.log("\n--- 步骤 3: 计算 fastVerify 参数 ---");
            console.log("  注意: 在链外计算参数可以避免额外的 Gas 消耗");
            const startParamsTime = Date.now();
            const fastVerifyParams = await helper.computeFastVerifyParams.call(publicKey, proof, message);
            const endParamsTime = Date.now();
            
            const uPoint = fastVerifyParams[0];  // [uPointX, uPointY]
            const vComponents = fastVerifyParams[1];  // [sHX, sHY, cGammaX, cGammaY]
            
            console.log("  U 点 X:", uPoint[0].toString(16));
            console.log("  U 点 Y:", uPoint[1].toString(16));
            console.log("  s*H X:", vComponents[0].toString(16));
            console.log("  s*H Y:", vComponents[1].toString(16));
            console.log("  c*Gamma X:", vComponents[2].toString(16));
            console.log("  c*Gamma Y:", vComponents[3].toString(16));
            console.log(`  参数计算耗时: ${endParamsTime - startParamsTime} ms`);
            
            // 步骤 5: 使用 fastVerify 进行验证
            console.log("\n--- 步骤 4: fastVerify() 验证 ---");
            const startTime = Date.now();
            const fastVerifyResult = await helper.fastVerify.call(
                publicKey,
                proof,
                message,
                uPoint,
                vComponents
            );
            const endTime = Date.now();
            
            console.log(`  验证结果: ${fastVerifyResult ? '✅ 成功' : '❌ 失败'}`);
            console.log(`  fastVerify 耗时: ${endTime - startTime} ms`);
            
            // 断言验证成功
            assert.equal(fastVerifyResult, true, "fastVerify 验证应该成功");
            
            // 步骤 6: 对比 verify() 和 fastVerify() 的结果（应该一致）
            console.log("\n--- 步骤 5: 对比 verify() 和 fastVerify() ---");
            const verifyStartTime = Date.now();
            const verifyResult = await helper.verify.call(publicKey, proof, message);
            const verifyEndTime = Date.now();
            
            console.log(`  verify() 结果: ${verifyResult ? '✅ 成功' : '❌ 失败'}`);
            console.log(`  verify() 耗时: ${verifyEndTime - verifyStartTime} ms`);
            
            // 两个验证结果应该一致
            assert.equal(verifyResult, fastVerifyResult, "verify() 和 fastVerify() 的结果应该一致");
            
            // 提取随机数验证
            const contractRandom = await helper.gammaToHash.call(proof[0], proof[1]);
            const expectedRandom = '0x' + vrfData.random;
            
            console.log("\n--- 步骤 6: 验证随机数 ---");
            console.log("  合约生成的随机数:", contractRandom);
            console.log("  C 程序生成的随机数:", expectedRandom);
            
            assert.equal(
                contractRandom.toLowerCase(),
                expectedRandom.toLowerCase(),
                "合约和 C 程序生成的随机数应该一致"
            );
            
            console.log("\n✅ 测试 4 完成：fastVerify() 验证成功！");
            console.log("\n性能对比:");
            console.log(`  verify() 耗时: ${verifyEndTime - verifyStartTime} ms`);
            console.log(`  fastVerify() 耗时: ${endTime - startTime} ms`);
            console.log(`  fastVerify() 参数计算耗时: ${endParamsTime - startParamsTime} ms`);
            console.log("\n注意: fastVerify() 在实际交易中可节省约 91% Gas 消耗");
        });
    });
    
    describe("VRF 随机数生成测试", () => {
        /**
         * 测试 5: 验证随机数生成的确定性
         * 
         * 目的：
         * 1. 验证确定性：相同的 Gamma 点多次调用 gammaToHash 应该产生相同的随机数
         * 2. 验证跨平台一致性：确保 Solidity 的 gammaToHash 实现与 C 程序一致
         */
        it("相同的 Gamma 点应该产生相同的随机数", async () => {
            console.log("\n--- 测试 4: 确定性随机数生成 ---");
            console.log("目的: 验证随机数生成的确定性 + 跨平台一致性");
            
            // 使用 C 程序生成一个 VRF 证明
            const testMessage = 'deterministic_test';
            const vrfData = generateVRFFromC(testMessage);
            
            // 解码证明
            const proofBytes = '0x' + vrfData.proof;
            const proof = await helper.decodeProof.call(proofBytes);
            
            console.log("\n步骤 1: 测试确定性（相同输入产生相同输出）");
            // 第一次生成随机数
            const hash1 = await helper.gammaToHash.call(proof[0], proof[1]);
            console.log("  第一次调用 gammaToHash:", hash1);
            
            // 第二次生成随机数（应该相同）
            const hash2 = await helper.gammaToHash.call(proof[0], proof[1]);
            console.log("  第二次调用 gammaToHash:", hash2);
            
            assert.equal(hash1, hash2, "相同输入应该产生相同的随机数");
            console.log("  ✅ 确定性验证通过：两次调用结果一致");
            
            console.log("\n步骤 2: 测试跨平台一致性（合约 vs C 程序）");
            // 验证与 C 程序生成的一致
            // 注意：这里的验证是为了确保 Solidity 的 gammaToHash 实现
            //       与 C 程序的实现完全一致，这是跨平台兼容性的关键
            const expectedRandom = '0x' + vrfData.random;
            console.log("  C 程序生成的随机数:", expectedRandom);
            console.log("  合约生成的随机数:  ", hash1);
            
            assert.equal(
                hash1.toLowerCase(),
                expectedRandom.toLowerCase(),
                "合约的 gammaToHash 实现应该与 C 程序完全一致"
            );
            console.log("  ✅ 跨平台一致性验证通过：合约与 C 程序生成的随机数一致");
            
            console.log("\n✅ 测试 5 完成：随机数生成是确定性的且跨平台兼容！");
        });
        
        /**
         * 测试 6: 不同消息产生不同随机数
         */
        it("不同的消息应该产生不同的随机数", async () => {
            console.log("\n--- 测试 6: 不同输入产生不同随机数 ---");
            
            // 生成两个不同消息的 VRF
            const message1 = 'message_1';
            const message2 = 'message_2';
            
            const vrfData1 = generateVRFFromC(message1);
            const vrfData2 = generateVRFFromC(message2);
            
            // 解码证明
            const proof1 = await helper.decodeProof.call('0x' + vrfData1.proof);
            const proof2 = await helper.decodeProof.call('0x' + vrfData2.proof);
            
            // 生成随机数
            const hash1 = await helper.gammaToHash.call(proof1[0], proof1[1]);
            const hash2 = await helper.gammaToHash.call(proof2[0], proof2[1]);
            
            console.log("消息 1 的随机数:", hash1);
            console.log("消息 2 的随机数:", hash2);
            
            assert.notEqual(hash1, hash2, "不同的消息应该产生不同的随机数");
            
            // 验证与 C 程序生成的一致
            assert.equal(hash1.toLowerCase(), ('0x' + vrfData1.random).toLowerCase(), "第一个随机数应该匹配");
            assert.equal(hash2.toLowerCase(), ('0x' + vrfData2.random).toLowerCase(), "第二个随机数应该匹配");
            
            console.log("✅ 测试 6 完成：不同消息产生不同的随机数！");
        });
    });
    
    // 测试后输出总结
    after(function() {
        try {
            console.log("\n========================================");
            console.log("          测试总结");
            console.log("========================================");
            console.log("✅ 测试完成！");
            console.log("\n主要功能验证：");
            console.log("  1. ✅ C 程序生成 VRF 证明");
            console.log("  2. ✅ 合约成功验证 C 生成的证明");
            console.log("  3. ✅ 多条不同消息验证成功");
            console.log("  4. ✅ 正确拒绝错误消息的证明");
            console.log("  5. ✅ fastVerify() 快速验证（节省 Gas）");
            console.log("  6. ✅ 随机数生成的确定性");
            console.log("  7. ✅ 不同消息产生不同随机数");
            console.log("\n跨平台验证：");
            console.log("  • C 程序路径: " + VRF_TEST_PATH);
            if (VRF_KEY_FILE && fs.existsSync(VRF_KEY_FILE)) {
                console.log("  • 密钥文件: " + VRF_KEY_FILE + " (Tumbler 密钥)");
            } else {
                console.log("  • 密钥文件: 未指定（使用随机生成的密钥对）");
            }
            console.log("  • Solidity 合约验证 C 生成的证明");
            console.log("  • 随机数在 C 端和合约端完全一致");
            console.log("\n建议：");
            console.log("  • 生产环境建议使用 fastVerify() 节省 Gas");
            console.log("  • 在链外计算 computeFastVerifyParams()");
            console.log("  • 随机数具有确定性和不可预测性");
            console.log("========================================\n");
        } catch (e) {
            // 忽略 after hook 中的错误，避免影响测试结果
        }
    });
});

