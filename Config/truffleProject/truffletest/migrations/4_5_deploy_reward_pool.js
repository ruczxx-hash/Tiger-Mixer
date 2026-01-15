const RewardPool = artifacts.require("RewardPool");

module.exports = async function(deployer, network, accounts) {
    console.log("开始部署奖励池合约...");
    console.log("网络:", network);
    console.log("可用账户数量:", accounts.length);
    
    try {
        // 部署 RewardPool 合约
        console.log("\n1. 部署 RewardPool 合约...");
        await deployer.deploy(RewardPool, {
            gas: 3000000,
            gasPrice: 1
        });
        const rewardPool = await RewardPool.deployed();
        console.log("✅ RewardPool 部署成功");
        console.log("   合约地址:", rewardPool.address);
        
        console.log("\n🎉 奖励池合约部署完成！");
        console.log("   注意：需要后续设置 CommitteeRotation 和 MixerEscrow 地址");
        
    } catch (error) {
        console.error("❌ 部署失败:", error);
        throw error;
    }
};














