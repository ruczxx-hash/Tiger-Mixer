const fs = require('fs');
const path = require('path');
const FixedMixerEscrow = artifacts.require("FixedMixerEscrow");
const A2LECDSAVerifier = artifacts.require("A2LECDSAVerifier");

module.exports = async function (deployer, network, accounts) {
  // 获取已部署的 A2LECDSAVerifier 合约地址
  // 如果还未部署，则先部署它
  let verifierAddress;
  try {
    const verifier = await A2LECDSAVerifier.deployed();
    verifierAddress = verifier.address;
    console.log('[FixedPools] Using existing A2LECDSAVerifier at:', verifierAddress);
  } catch (e) {
    // 如果 A2LECDSAVerifier 还未部署，先部署它
    console.log('[FixedPools] Deploying A2LECDSAVerifier...');
    await deployer.deploy(A2LECDSAVerifier);
    const verifier = await A2LECDSAVerifier.deployed();
    verifierAddress = verifier.address;
    console.log('[FixedPools] Deployed A2LECDSAVerifier at:', verifierAddress);
  }

  // 获取已部署的 Hasher 合约地址
  let hasherAddress;
  try {
    // Try to load from artifacts
    const HasherJSON = require('../build/contracts/Hasher.json');
    const networkId = await web3.eth.net.getId();
    if (HasherJSON.networks && HasherJSON.networks[networkId]) {
      hasherAddress = HasherJSON.networks[networkId].address;
      console.log('[FixedPools] Using existing Hasher at:', hasherAddress);
    } else {
      throw new Error('Hasher not deployed');
    }
  } catch (e) {
    console.error('[FixedPools] Hasher not found. Please run migration 1_deploy_hasher.js first.');
    throw e;
  }

  const denoms = [
    web3.utils.toWei('0.1', 'ether'),
    web3.utils.toWei('1', 'ether'),
    web3.utils.toWei('10', 'ether'),
    web3.utils.toWei('100', 'ether'),
  ];
  // 主标签改为小数形式；同时保留历史别名 0_1
  const labels = ['0.1', '1', '10', '100'];
  const addresses = {};

  // Tornado Cash Merkle Tree 高度（20层，支持最多 2^20 = 1,048,576 个存款）
  const merkleTreeHeight = 20;
  
  for (let i = 0; i < denoms.length; i++) {
    await deployer.deploy(FixedMixerEscrow, denoms[i], verifierAddress, merkleTreeHeight, hasherAddress);
    const instance = await FixedMixerEscrow.deployed();
    addresses[labels[i]] = instance.address;
    if (labels[i] === '0.1') addresses['0_1'] = instance.address; // 兼容旧脚本
  }

  const outPath = path.join(__dirname, '..', 'scripts', 'deployed-addresses.json');
  fs.writeFileSync(outPath, JSON.stringify({ network, pools: addresses, verifier: verifierAddress }, null, 2));
  console.log('[FixedPools] deployed addresses:', addresses);
  console.log('[FixedPools] verifier address:', verifierAddress);
  console.log('[FixedPools] wrote address book to', outPath);
};






