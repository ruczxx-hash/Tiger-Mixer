const ReputationManager = artifacts.require("ReputationManager");
const StakingManager = artifacts.require("StakingManager");
const CommitteeRotation = artifacts.require("CommitteeRotation");
const RewardPool = artifacts.require("RewardPool");
// const CommitteeRotationTest = artifacts.require("CommitteeRotationTest"); // 测试合约：暂时注释

module.exports = async function(deployer, network, accounts) {
    console.log("开始部署委员会轮换系统...");
    console.log("网络:", network);
    console.log("可用账户数量:", accounts.length);
    
    try {
        // 1. 获取已部署的合约
        console.log("\n1. 获取已部署的合约...");
        const reputationManager = await ReputationManager.deployed();
        const stakingManager = await StakingManager.deployed();
        let rewardPool;
        
        // 尝试获取 RewardPool（如果已部署）
        try {
            rewardPool = await RewardPool.deployed();
            console.log("RewardPool 地址:", rewardPool.address);
        } catch (error) {
            console.log("⚠️  RewardPool 未部署，将使用 address(0)");
            rewardPool = null;
        }
        
        console.log("ReputationManager 地址:", reputationManager.address);
        console.log("StakingManager 地址:", stakingManager.address);
        
        // 2. 设置初始委员会（使用前3个账户）
        const initialCommittee = [accounts[0], accounts[1], accounts[2]];
        console.log("初始委员会:", initialCommittee);
        
        // 3. 部署委员会轮换合约
        console.log("\n2. 部署委员会轮换合约...");
        // VRF 生成器地址：设置为 address(0) 表示允许任何地址提交（或后续通过 setVRFGenerator 设置）
        const vrfGenerator = "0x0000000000000000000000000000000000000000";
        // RewardPool 地址
        const rewardPoolAddress = rewardPool ? rewardPool.address : "0x0000000000000000000000000000000000000000";
        
        await deployer.deploy(
            CommitteeRotation, 
            reputationManager.address, 
            stakingManager.address, 
            initialCommittee,
            vrfGenerator,
            rewardPoolAddress,
            { 
                gas: 8000000,  // 进一步增加 gas limit（块限制是 8000000）
                gasPrice: 1    // 私有链可以用很低的 gas price
            }
        );
        const committeeRotation = await CommitteeRotation.deployed();
        console.log("✅ CommitteeRotation 部署成功");
        console.log("   合约地址:", committeeRotation.address);
        
        // 4. 设置合约之间的引用关系（如果 RewardPool 已部署）
        if (rewardPool) {
            console.log("\n3. 设置合约引用关系...");
            try {
                // 设置 RewardPool 的 CommitteeRotation 引用
                await rewardPool.setCommitteeRotation(committeeRotation.address);
                console.log("   ✅ RewardPool.setCommitteeRotation() 成功");
                
                // 设置 StakingManager 的 RewardPool 和 CommitteeRotation 引用
                await stakingManager.setRewardPool(rewardPool.address);
                console.log("   ✅ StakingManager.setRewardPool() 成功");
                await stakingManager.setCommitteeRotation(committeeRotation.address);
                console.log("   ✅ StakingManager.setCommitteeRotation() 成功");
            } catch (error) {
                console.log("   ⚠️  设置合约引用失败:", error.message);
                console.log("      可以后续手动设置");
            }
        }
        
        // // 4. 部署测试合约（注释掉）
        // console.log("\n3. 部署测试合约...");
        // await deployer.deploy(CommitteeRotationTest,
        //     reputationManager.address,
        //     stakingManager.address,
        //     initialCommittee
        // );
        // const committeeRotationTest = await CommitteeRotationTest.deployed();
        // console.log("✅ CommitteeRotationTest 部署成功");
        // console.log("   合约地址:", committeeRotationTest.address);
        // 
        // // 5. 验证部署（注释掉）
        // console.log("\n4. 验证部署结果...");
        // // 检查轮换参数
        // const rotationInterval = await committeeRotation.ROTATION_INTERVAL();
        // const minReputation = await committeeRotation.MIN_REPUTATION();
        // const minStakeWeight = await committeeRotation.MIN_STAKE_WEIGHT();
        // const maxCommitteeSize = await committeeRotation.MAX_COMMITTEE_SIZE();
        // console.log("轮换参数:");
        // console.log("   轮换间隔:", rotationInterval.toString(), "秒");
        // console.log("   最小声誉要求:", minReputation.toString());
        // console.log("   最小质押权重要求:", minStakeWeight.toString());
        // console.log("   最大委员会规模:", maxCommitteeSize.toString());
        // // 检查初始委员会
        // const currentCommittee = await committeeRotation.getCurrentCommittee();
        // console.log("\n当前委员会:");
        // for (let i = 0; i < 3; i++) {
        //     console.log(`   成员 ${i}: ${currentCommittee[i]}`);
        // }
        // // 检查轮换信息
        // const rotationInfo = await committeeRotation.getRotationInfo();
        // console.log("\n轮换信息:");
        // console.log("   上次轮换时间:", new Date(rotationInfo.lastRotation * 1000).toLocaleString());
        // console.log("   下次轮换时间:", new Date(rotationInfo.nextRotation * 1000).toLocaleString());
        // console.log("   轮换次数:", rotationInfo.count.toString());
        // console.log("   是否可以轮换:", rotationInfo.canRotateNow);
        // 
        // // 6. 添加一些候选者（注释掉）
        // console.log("\n5. 添加候选者...");
        // const candidates = [accounts[3], accounts[4], accounts[5], accounts[6], accounts[7]];
        // for (let i = 0; i < candidates.length; i++) {
        //     try {
        //         await committeeRotation.addCandidate(candidates[i]);
        //         console.log(`   ✅ 添加候选者 ${i + 3}: ${candidates[i]}`);
        //     } catch (error) {
        //         console.log(`   ❌ 添加候选者 ${i + 3} 失败: ${error.message}`);
        //     }
        // }
        // // 检查候选池
        // const candidatePool = await committeeRotation.getCandidatePool();
        // console.log("\n候选池大小:", candidatePool.length);
        // for (let i = 0; i < candidatePool.length; i++) {
        //     console.log(`   候选者 ${i}: ${candidatePool[i]}`);
        // }
        // 
        console.log("\n🎉 委员会轮换合约部署完成！");
        
    } catch (error) {
        console.error("❌ 部署失败:", error);
        throw error;
    }
};
