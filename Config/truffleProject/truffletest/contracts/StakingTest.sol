// SPDX-License-Identifier: MIT
// contracts/StakingTest.sol
pragma solidity ^0.6.12;

import "./StakingManager.sol";

contract StakingTest {
    StakingManager public stakingManager;
    
    constructor() public {
        stakingManager = new StakingManager();
    }
    
    // 测试质押
    function testStake() external payable {
        stakingManager.stake{value: msg.value}();
    }
    
    // 测试解锁
    function testUnstake() external {
        stakingManager.unstake();
    }
    
    // 获取质押信息
    function getStakeInfo(address user) external view returns (
        uint256 amount,
        uint256 stakeTime,
        bool isActive
    ) {
        return stakingManager.getStakeInfo(user);
    }
    
    // 计算质押权重
    function calculateStakeWeight(address user) external view returns (uint256) {
        return stakingManager.calculateStakeWeight(user);
    }
    
    // 检查是否已质押
    function hasStake(address user) external view returns (bool) {
        return stakingManager.hasStake(user);
    }
    
    // 获取池子信息
    function getPoolInfo() external view returns (
        uint256 totalPool,
        uint256 contractBalance
    ) {
        return stakingManager.getPoolInfo();
    }
    
    // 接收ETH
    receive() external payable {
        // 允许合约接收ETH
    }
}
