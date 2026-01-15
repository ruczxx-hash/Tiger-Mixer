/* global artifacts, web3 */
const fs = require('fs');
const path = require('path');

module.exports = async function (deployer, network, accounts) {
  // Load Hasher contract from generated JSON
  const HasherJSONPath = path.join(__dirname, '..', 'build', 'Hasher.json');
  
  if (!fs.existsSync(HasherJSONPath)) {
    console.error('[Hasher] Hasher.json not found. Please copy it from tornado-core-master/build/Hasher.json');
    throw new Error('Hasher.json not found');
  }
  
  const HasherJSON = require(HasherJSONPath);
  
  // Deploy Hasher using web3 directly since it's a generated contract
  const contract = new web3.eth.Contract(HasherJSON.abi);
  const deployTx = contract.deploy({ data: HasherJSON.bytecode });
  
  const instance = await deployTx.send({
    from: accounts[0],
    gas: 5000000
  });
  
  const deployedAddress = instance.options.address;
  console.log('[Hasher] Deployed at:', deployedAddress);
  
  // Save to artifacts for Truffle
  const artifactsPath = path.join(__dirname, '..', 'build', 'contracts', 'Hasher.json');
  const artifacts = {
    contractName: 'Hasher',
    abi: HasherJSON.abi,
    bytecode: HasherJSON.bytecode,
    networks: {}
  };
  
  // Get network ID
  const networkId = await web3.eth.net.getId();
  artifacts.networks[networkId] = {
    address: deployedAddress,
    transactionHash: instance.transactionHash || instance.options.transactionHash || '0x0'
  };
  
  // Ensure directory exists
  const artifactsDir = path.dirname(artifactsPath);
  if (!fs.existsSync(artifactsDir)) {
    fs.mkdirSync(artifactsDir, { recursive: true });
  }
  
  fs.writeFileSync(artifactsPath, JSON.stringify(artifacts, null, 2));
  console.log('[Hasher] Artifact saved to:', artifactsPath);
};
