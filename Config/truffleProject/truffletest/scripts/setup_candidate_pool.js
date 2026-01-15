const CommitteeRotation = artifacts.require("CommitteeRotation");
const ReputationManager = artifacts.require("ReputationManager");
const StakingManager = artifacts.require("StakingManager");

module.exports = async function(callback) {
  try {
    console.log("=== 设置7人候选池 ===");

    const committeeRotation = await CommitteeRotation.deployed();
    const reputationManager = await ReputationManager.deployed();
    const stakingManager = await StakingManager.deployed();

    // 获取7个候选者地址 (eth.accounts[301-307])
    const allAccounts = await web3.eth.getAccounts();
    console.log(`总账户数量: ${allAccounts.length}`);
    
    if (allAccounts.length < 308) {
      console.error(`❌ 账户数量不足，需要至少308个账户，当前只有${allAccounts.length}个`);
      return callback(new Error("账户数量不足"));
    }
    
    const candidatePool = [
      allAccounts[301], // 候选者1
      allAccounts[302], // 候选者2
      allAccounts[303], // 候选者3
      allAccounts[304], // 候选者4
      allAccounts[305], // 候选者5
      allAccounts[306], // 候选者6
      allAccounts[307]  // 候选者7
    ];

    console.log("7人候选池地址:");
    for (let i = 0; i < candidatePool.length; i++) {
      console.log(`  候选者 ${i + 1}: ${candidatePool[i]}`);
    }

    // 解锁所有候选者账户
    console.log("\n=== 解锁候选者账户 ===");
    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      try {
        await web3.eth.personal.unlockAccount(account, '12345678', 86400);
        console.log(`✅ 候选者 ${i + 1} 解锁成功: ${account}`);
      } catch (err) {
        console.log(`⚠️  候选者 ${i + 1} 解锁失败: ${err.message}`);
      }
    }

    // 为候选者设置初始声誉（模拟不同水平）
    console.log("\n=== 设置候选者初始声誉 ===");
    const initialReputations = [
      { accuracy: 85, participation: 90, consistency: 80 }, // 候选者1 - 高声誉
      { accuracy: 80, participation: 85, consistency: 85 }, // 候选者2 - 高声誉
      { accuracy: 75, participation: 80, consistency: 75 }, // 候选者3 - 中声誉
      { accuracy: 70, participation: 75, consistency: 70 }, // 候选者4 - 中声誉
      { accuracy: 65, participation: 70, consistency: 65 }, // 候选者5 - 中声誉
      { accuracy: 60, participation: 65, consistency: 60 }, // 候选者6 - 低声誉
      { accuracy: 55, participation: 60, consistency: 55 }  // 候选者7 - 低声誉
    ];

    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      const rep = initialReputations[i];
      try {
        const tx = await reputationManager.updateReputation(rep.accuracy, rep.participation, rep.consistency, { from: account });
        console.log(`✅ 候选者 ${i + 1} 声誉设置成功: ${account} (准确度:${rep.accuracy}, 参与度:${rep.participation}, 一致性:${rep.consistency})`);
      } catch (err) {
        console.log(`⚠️  候选者 ${i + 1} 声誉设置失败: ${err.message}`);
      }
    }

    // 先给候选者转账
    console.log("\n=== 给候选者转账 ===");
    const transferAmount = web3.utils.toWei('100', 'ether'); // 每个账户转100 ETH
    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      try {
        const tx = await web3.eth.sendTransaction({
          from: "0x9483ba82278fd651ed64d5b2cc2d4d2bbfa94025", // 从账户0转账
          to: account,
          value: transferAmount
        });
        console.log(`✅ 候选者 ${i + 1} 转账成功: ${account} (100 ETH)`);
      } catch (err) {
        console.log(`⚠️  候选者 ${i + 1} 转账失败: ${err.message}`);
      }
    }

    // 为候选者设置质押（固定 1 ETH）
    console.log("\n=== 设置候选者质押 ===");
    const stakeAmount = web3.utils.toWei('1', 'ether');  // 固定 1 ETH

    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      try {
        const tx = await stakingManager.stake({ from: account, value: stakeAmount });
        console.log(`✅ 候选者 ${i + 1} 质押设置成功: ${account} (1 ETH)`);
      } catch (err) {
        console.log(`⚠️  候选者 ${i + 1} 质押设置失败: ${err.message}`);
      }
    }

    // 添加所有候选者到候选池
    console.log("\n=== 添加候选者到候选池 ===");
    for (let i = 0; i < candidatePool.length; i++) {
      const account = candidatePool[i];
      try {
        const tx = await committeeRotation.addCandidate(account);
        console.log(`✅ 候选者 ${i + 1} 添加到候选池成功: ${account}`);
      } catch (err) {
        console.log(`⚠️  候选者 ${i + 1} 添加到候选池失败: ${err.message}`);
      }
    }

    // 执行委员会轮换，选择Top3
    console.log("\n=== 执行委员会轮换 (选择Top3) ===");
    try {
      const tx = await committeeRotation.rotateCommittee();
      console.log(`✅ 委员会轮换成功，交易哈希: ${tx.tx || tx.receipt?.transactionHash}`);
    } catch (err) {
      console.log(`⚠️  委员会轮换失败: ${err.message}`);
    }

    // 验证最终委员会
    console.log("\n=== 验证最终委员会 (Top3) ===");
    try {
      const details = await committeeRotation.getCommitteeDetails();
      const addresses = details[0];
      const reputations = details[1];
      const stakes = details[2];

      console.log("最终委员会成员 (Top3):");
      for (let i = 0; i < addresses.length; i++) {
        console.log(`  成员 ${i + 1}: ${addresses[i]}`);
        console.log(`    声誉: ${reputations[i].toString()}`);
        console.log(`    质押: ${web3.utils.fromWei(stakes[i].toString(), 'ether')} ETH`);
      }

      // 写入委员会成员到文件
      const fs = require('fs');
      const out = addresses.map(a => a.toString()).join('\n');
      const filePath = '/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt';
      fs.writeFileSync(filePath, out);
      console.log('✅ 已写入委员会成员到文件');
      console.log(`文件路径: ${filePath}`);
      console.log(`文件内容:`);
      console.log(out);
    } catch (err) {
      console.log(`⚠️  验证委员会失败: ${err.message}`);
    }

    console.log("\n=== 7人候选池设置完成 ===");
    console.log("现在系统将根据声誉和质押动态选择Top3作为委员会成员");
  } catch (err) {
    console.error('❌ 设置候选池失败:', err.message);
    return callback(err);
  }
  callback();
};
