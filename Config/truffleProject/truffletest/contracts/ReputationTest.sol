// SPDX-License-Identifier: MIT
// contracts/ReputationTest.sol
pragma solidity ^0.6.12;

import "./ReputationManager.sol";

contract ReputationTest {
    ReputationManager public reputationManager;
    
    constructor(address[3] memory _members) public {
        reputationManager = new ReputationManager(_members);
    }
    
    // 测试声誉更新 - 直接调用
    function testUpdateReputation(
        uint256 participation,
        uint256 consistency
    ) external {
        // 直接调用声誉更新（updateReputation 只需要 participation 和 consistency）
        reputationManager.updateReputation(participation, consistency);
    }
    
    // 获取成员声誉
    function getMemberReputation(address member) external view returns (uint256) {
        return reputationManager.calculateReputation(member);
    }
}
