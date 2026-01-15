const StakingManager = artifacts.require("StakingManager");
// const StakingTest = artifacts.require("StakingTest"); // 测试合约：暂时注释

module.exports = async function(deployer, network, accounts) {
    console.log("开始部署质押系统...");
    console.log("网络:", network);
    console.log("可用账户数量:", accounts.length);
    
    try {
        // 1. 部署质押管理合约
        console.log("\n1. 部署质押管理合约...");
        await deployer.deploy(StakingManager);
        const stakingManager = await StakingManager.deployed();
        console.log("✅ StakingManager 部署成功");
        console.log("   合约地址:", stakingManager.address);
        
        // // 2. 部署测试合约（注释掉）
        // console.log("\n2. 部署测试合约...");
        // await deployer.deploy(StakingTest);
        // const stakingTest = await StakingTest.deployed();
        // console.log("✅ StakingTest 部署成功");
        // console.log("   合约地址:", stakingTest.address);
        // 
        // // 3. 验证部署（注释掉）
        // console.log("\n3. 验证部署结果...");
        // // 检查质押参数
        // const minStake = await stakingManager.MIN_STAKE();
        // const maxStake = await stakingManager.MAX_STAKE();
        // const lockPeriod = await stakingManager.LOCK_PERIOD();
        // const rewardRate = await stakingManager.REWARD_RATE();
        // console.log("质押参数:");
        // console.log("   最小质押量:", web3.utils.fromWei(minStake.toString(), 'ether'), "ETH");
        // console.log("   最大质押量:", web3.utils.fromWei(maxStake.toString(), 'ether'), "ETH");
        // console.log("   锁定期:", lockPeriod.toString(), "秒");
        // console.log("   奖励率:", rewardRate.toString(), "%");
        // // 检查初始池子状态
        // const poolInfo = await stakingManager.getPoolInfo();
        // console.log("\n初始池子状态:");
        // console.log("   总质押池:", web3.utils.fromWei(poolInfo.totalPool.toString(), 'ether'), "ETH");
        // console.log("   总奖励池:", web3.utils.fromWei(poolInfo.totalRewardsAmount.toString(), 'ether'), "ETH");
        // console.log("   合约余额:", web3.utils.fromWei(poolInfo.contractBalance.toString(), 'ether'), "ETH");
        // // 4. 添加一些奖励到池子
        // console.log("\n4. 添加奖励到池子...");
        // const rewardAmount = web3.utils.toWei("1", "ether");
        // await stakingManager.addReward({ value: rewardAmount, from: accounts[0] });
        // console.log("✅ 添加奖励成功:", web3.utils.fromWei(rewardAmount, 'ether'), "ETH");
        // // 验证奖励添加
        // const updatedPoolInfo = await stakingManager.getPoolInfo();
        // console.log("   更新后总奖励池:", web3.utils.fromWei(updatedPoolInfo.totalRewardsAmount.toString(), 'ether'), "ETH");
        // console.log("\n🎉 质押系统部署完成！");
        // console.log("💡 运行测试请使用: truffle exec test_staking_system.js");
        
    } catch (error) {
        console.error("❌ 部署失败:", error);
        throw error;
    }
};
