const Migrations = artifacts.require("Migrations");
const Greeter = artifacts.require("Greeter");

module.exports = function (deployer) {
  deployer.deploy(Migrations);
  deployer.deploy(Greeter);
};
