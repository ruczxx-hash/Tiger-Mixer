const SignatureEscrow = artifacts.require("SignatureEscrow");

module.exports = async function (deployer, network, accounts) {
  // 面额：按需修改，这里示例 0.01 ETH
  const denominationWei = web3.utils.toWei("0.01", "ether");
  await deployer.deploy(SignatureEscrow, denominationWei);
};



























