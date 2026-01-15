const VRFTestHelper = artifacts.require("VRFTestHelper");

module.exports = function(deployer) {
    return deployer.deploy(VRFTestHelper).then(() => {
        console.log("VRFTestHelper deployed at:", VRFTestHelper.address);
    });
};