// 纯部署脚本 - 不包含测试逻辑
const ReputationManager = artifacts.require("ReputationManager");
// const ReputationTest = artifacts.require("ReputationTest"); // 测试合约：暂时注释

module.exports = async function(deployer, network, accounts) {
    console.log("开始部署声誉管理合约...");
    console.log("网络:", network);
    console.log("可用账户数量:", accounts.length);
    
    // 使用前3个账户作为委员会成员
    const committeeMembers = [accounts[0], accounts[1], accounts[2]];
    
    console.log("委员会成员:", committeeMembers);
    
    try {
        // 部署声誉管理合约
        await deployer.deploy(ReputationManager, committeeMembers);
        const reputationManager = await ReputationManager.deployed();
        
        console.log("✅ ReputationManager 部署成功");
        console.log("   合约地址:", reputationManager.address);
        
        // // 部署测试合约（注释掉）
        // await deployer.deploy(ReputationTest, committeeMembers);
        // const reputationTest = await ReputationTest.deployed();
        // console.log("✅ ReputationTest 部署成功");
        // console.log("   合约地址:", reputationTest.address);
        // 
        // // 验证部署（只读取数据，不发送交易）
        // console.log("\n验证部署结果...");
        // const result = await reputationManager.getAllReputations();
        // const members = result[0];
        // const reputations = result[1];
        // console.log("委员会成员验证:", members);
        // console.log("初始声誉:", reputations);
        // console.log("\n🎉 声誉系统合约部署完成！");
        // console.log("💡 运行测试请使用: truffle exec test_reputation_server.js");
        
    } catch (error) {
        console.error("❌ 部署失败:", error);
        throw error;
    }
};
