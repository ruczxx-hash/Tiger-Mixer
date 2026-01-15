const ReputationManager = artifacts.require("ReputationManager");
const StakingManager = artifacts.require("StakingManager");
const CommitteeRotation = artifacts.require("CommitteeRotation");
const CommitteeManager = artifacts.require("CommitteeManager");

module.exports = async function(deployer, network, accounts) {
    console.log("开始部署集成委员会管理系统...");
    console.log("网络:", network);
    console.log("可用账户数量:", accounts.length);
    
    try {
        // 1. 获取已部署的合约
        console.log("\n1. 获取已部署的合约...");
        const reputationManager = await ReputationManager.deployed();
        const stakingManager = await StakingManager.deployed();
        const committeeRotation = await CommitteeRotation.deployed();
        
        console.log("ReputationManager 地址:", reputationManager.address);
        console.log("StakingManager 地址:", stakingManager.address);
        console.log("CommitteeRotation 地址:", committeeRotation.address);
        
        // 2. 部署委员会管理主合约
        console.log("\n2. 部署委员会管理主合约...");
        await deployer.deploy(CommitteeManager);
        const committeeManager = await CommitteeManager.deployed();
        console.log("✅ CommitteeManager 部署成功");
        console.log("   合约地址:", committeeManager.address);
        
        // 3. 初始化集成系统
        console.log("\n3. 初始化集成系统...");
        const initialCommittee = [accounts[0], accounts[1], accounts[2]];
        
        await committeeManager.initialize(
            reputationManager.address,
            stakingManager.address,
            committeeRotation.address,
            initialCommittee
        );
        console.log("✅ 集成系统初始化成功");
        
        // // 4. 验证与演示逻辑（暂时注释）
        // console.log("\n4. 验证集成系统...");
        // const currentCommittee = await committeeManager.getCurrentCommittee();
        // console.log("   当前委员会:", currentCommittee);
        // const systemStatus = await committeeManager.getSystemStatus();
        // console.log("   系统状态:");
        // console.log("     已初始化:", systemStatus.initialized);
        // console.log("     最后更新:", new Date(systemStatus.lastUpdate * 1000).toLocaleString());
        // console.log("     委员会数量:", systemStatus.committeeCount.toString());
        // console.log("     候选者数量:", systemStatus.candidateCount.toString());
        // const rotationInfo = await committeeManager.getRotationInfo();
        // console.log("   轮换信息:");
        // console.log("     上次轮换:", new Date(rotationInfo.lastRotation * 1000).toLocaleString());
        // console.log("     下次轮换:", new Date(rotationInfo.nextRotation * 1000).toLocaleString());
        // console.log("     轮换次数:", rotationInfo.count.toString());
        // console.log("     可以轮换:", rotationInfo.canRotateNow);
        
        // // 5. 解锁账户与设置候选者并添加（暂时注释）
        // console.log("\n5. 设置候选者数据并添加...");
        // const candidates = [accounts[3], accounts[4], accounts[5]];
        // console.log("   解锁候选者账户...");
        // for (let i = 0; i < candidates.length; i++) {
        //     try {
        //         await web3.eth.personal.unlockAccount(candidates[i], "12345678", 0);
        //         console.log(`     ✅ 候选者 ${i + 1} 账户解锁成功: ${candidates[i]}`);
        //     } catch (error) {
        //         console.log(`     ❌ 候选者 ${i + 1} 账户解锁失败: ${error.message}`);
        //     }
        // }
        // await web3.eth.personal.unlockAccount(accounts[0], "", 0);
        // console.log("   解锁其他账户...");
        // for (let i = 1; i < 3; i++) {
        //     try {
        //         await web3.eth.personal.unlockAccount(accounts[i], "12345678", 0);
        //         console.log(`     ✅ 账户 ${i + 1} 解锁成功: ${accounts[i]}`);
        //     } catch (error) {
        //         console.log(`     ❌ 账户 ${i + 1} 解锁失败: ${error.message}`);
        //     }
        // }
        // await new Promise(resolve => setTimeout(resolve, 1000));
        // console.log("   设置候选者声誉和质押...");
        // for (let i = 0; i < candidates.length; i++) {
        //     try {
        //         const accuracy = 80 + i * 5;
        //         const participation = 85 + i * 3;
        //         const consistency = 90 + i * 2;
        //         await reputationManager.updateReputation(accuracy, participation, consistency, { from: candidates[i] });
        //         const stakeAmount = web3.utils.toWei((0.6 + i * 0.2).toString(), "ether");
        //         await stakingManager.stake({ value: stakeAmount, from: candidates[i] });
        //     } catch (error) {}
        // }
        // await new Promise(resolve => setTimeout(resolve, 2000));
        // console.log("   添加候选者到候选池...");
        // for (let i = 0; i < candidates.length; i++) {
        //     try {
        //         await committeeManager.addCandidate(candidates[i]);
        //     } catch (error) {}
        // }
        // console.log("\n6. 检查最终状态...");
        // const finalCandidatePool = await committeeManager.getCandidatePool();
        // const finalSystemStatus = await committeeManager.getSystemStatus();
        // console.log("   最终候选池大小:", finalCandidatePool.length);
        // console.log("   候选者数量:", finalSystemStatus.candidateCount.toString());
        // console.log("\n🎉 集成委员会管理系统部署完成！");
        // console.log("💡 运行测试请使用: truffle exec test_integrated_system.js");
        
    } catch (error) {
        console.error("❌ 部署失败:", error);
        throw error;
    }
};
