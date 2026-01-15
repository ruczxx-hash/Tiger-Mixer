const CommitteeRotation = artifacts.require("CommitteeRotation");

module.exports = async function(callback) {
  try {
    console.log("=== 获取当前委员会成员 ===");

    const committeeRotation = await CommitteeRotation.deployed();

    // getCommitteeDetails() -> (addresses[], reputations[], stakes[])
    const details = await committeeRotation.getCommitteeDetails();
    const addresses = details[0];
    const reputations = details[1];
    const stakes = details[2];

    console.log("委员会成员数量:", addresses.length.toString());

    for (let i = 0; i < addresses.length; i++) {
      console.log(`成员 ${i + 1}: ${addresses[i]}`);
      console.log(`  声誉: ${reputations[i].toString()}`);
      console.log(`  质押: ${stakes[i].toString()}`);
    }

    const fs = require('fs');
    const out = addresses.map(a => a.toString()).join('\n');
    fs.writeFileSync('/tmp/committee_members.txt', out);
    console.log('✅ 已写入 /tmp/committee_members.txt');
  } catch (err) {
    console.error('❌ 获取委员会成员失败:', err.message);
    return callback(err);
  }
  callback();
};
