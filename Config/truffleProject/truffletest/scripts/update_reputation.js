const CommitteeRotation = artifacts.require("CommitteeRotation");
const ReputationManager = artifacts.require("ReputationManager");

module.exports = async function(callback) {
  try {
    console.log("=== 更新候选者声誉 ===");

    const committeeRotation = await CommitteeRotation.deployed();
    const reputationManager = await ReputationManager.deployed();

    // 获取所有候选者
    const candidatePool = await committeeRotation.getCandidatePool();
    console.log(`候选池中有 ${candidatePool.length} 个候选者`);

    if (candidatePool.length === 0) {
      console.log("❌ 候选池为空，请先运行 setup_candidate_pool.js");
      return callback();
    }

    // 模拟声誉更新（实际应用中这里会根据真实表现数据更新）
    console.log("\n=== 模拟声誉更新 ===");
    
    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      
      // 模拟不同的表现（随机变化）
      const baseAccuracy = 60 + Math.random() * 30; // 60-90
      const baseParticipation = 65 + Math.random() * 25; // 65-90
      const baseConsistency = 55 + Math.random() * 35; // 55-90
      
      const accuracy = Math.round(baseAccuracy);
      const participation = Math.round(baseParticipation);
      const consistency = Math.round(baseConsistency);
      
      try {
        // 解锁账户
        await web3.eth.personal.unlockAccount(account, '12345678', 86400);
        
        // 更新声誉
        const tx = await reputationManager.updateReputation(accuracy, participation, consistency, { from: account });
        console.log(`✅ 候选者 ${i + 1} 声誉更新成功: ${account}`);
        console.log(`    准确度: ${accuracy}, 参与度: ${participation}, 一致性: ${consistency}`);
      } catch (err) {
        console.log(`⚠️  候选者 ${i + 1} 声誉更新失败: ${err.message}`);
      }
    }

    // 更新所有候选者分数
    console.log("\n=== 更新候选者分数 ===");
    try {
      const tx = await committeeRotation.updateAllCandidateScores();
      console.log("✅ 所有候选者分数更新成功");
    } catch (err) {
      console.log(`⚠️  分数更新失败: ${err.message}`);
    }

    // 显示当前排名
    console.log("\n=== 当前候选者排名 ===");
    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      try {
        const reputation = await reputationManager.calculateReputation(account);
        const score = await committeeRotation.getCandidateScore(account);
        console.log(`候选者 ${i + 1}: ${account}`);
        console.log(`  声誉: ${reputation}, 综合分数: ${score}`);
      } catch (err) {
        console.log(`候选者 ${i + 1}: ${account} - 获取信息失败`);
      }
    }

    console.log("\n=== 声誉更新完成 ===");
    console.log("可以运行 rotate_committee.js 来重新选择Top3委员会成员");
  } catch (err) {
    console.error('❌ 更新声誉失败:', err.message);
    return callback(err);
  }
  callback();
};
