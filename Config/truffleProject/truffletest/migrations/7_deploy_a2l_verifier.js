const A2LECDSAVerifier = artifacts.require("A2LECDSAVerifier");

module.exports = function(deployer) {
  deployer.deploy(A2LECDSAVerifier);
};
