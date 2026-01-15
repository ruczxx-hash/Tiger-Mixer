const Migrations = artifacts.require("Migrations");
const Greeter = artifacts.require("Greeter");
const MixerEscrow = artifacts.require("MixerEscrow");

module.exports = function (deployer) {
  deployer.deploy(Migrations);
  deployer.deploy(Greeter);
  deployer.deploy(MixerEscrow);
};

