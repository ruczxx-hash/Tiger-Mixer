// SPDX-License-Identifier: MIT
// contracts/CommitteeManager.sol
pragma solidity ^0.6.12;

import "./ReputationManager.sol";
import "./StakingManager.sol";
import "./CommitteeRotation.sol";

/**
 * 委员会管理主合约
 * 集成声誉系统、质押系统和轮换系统
 * 提供统一的委员会管理接口
 */
contract CommitteeManager {
    // 合约引用
    ReputationManager public reputationManager;
    StakingManager public stakingManager;
    CommitteeRotation public committeeRotation;
    
    // 委员会状态
    address[3] public currentCommittee;
    bool public isInitialized;
    uint256 public lastUpdateTime;
    bool public paused;  // 系统暂停状态
    
    // 事件
    event CommitteeUpdated(address[3] newCommittee, uint256 timestamp);
    event SystemInitialized(address reputationManager, address stakingManager, address committeeRotation);
    event SystemPaused(uint256 timestamp);
    event SystemResumed(uint256 timestamp);
    
    constructor() public {
        isInitialized = false;
        lastUpdateTime = block.timestamp;
        paused = false;
    }
    
    /**
     * 初始化委员会管理系统
     * @param _reputationManager 声誉管理合约地址
     * @param _stakingManager 质押管理合约地址
     * @param _committeeRotation 委员会轮换合约地址
     * @param _initialCommittee 初始委员会成员
     */
    function initialize(
        address _reputationManager,
        address payable _stakingManager,
        address _committeeRotation,
        address[3] memory _initialCommittee
    ) external {
        require(!isInitialized, "Already initialized");
        require(_reputationManager != address(0), "Invalid reputation manager");
        require(_stakingManager != address(0), "Invalid staking manager");
        require(_committeeRotation != address(0), "Invalid committee rotation");
        
        reputationManager = ReputationManager(_reputationManager);
        stakingManager = StakingManager(_stakingManager);
        committeeRotation = CommitteeRotation(_committeeRotation);
        
        currentCommittee = _initialCommittee;
        isInitialized = true;
        lastUpdateTime = block.timestamp;
        
        emit SystemInitialized(_reputationManager, _stakingManager, _committeeRotation);
    }
    
    /**
     * 获取当前委员会
     */
    function getCurrentCommittee() external view returns (address[3] memory) {
        return currentCommittee;
    }
    
    /**
     * 检查是否为当前委员会成员
     */
    function isCommitteeMember(address member) external view returns (bool) {
        for (uint i = 0; i < 3; i++) {
            if (currentCommittee[i] == member) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * 获取委员会成员详细信息
     */
    function getCommitteeDetails() external view returns (
        address[3] memory members,
        uint256[3] memory reputations,
        uint256[3] memory stakeWeights,
        uint256[3] memory scores
    ) {
        (members, scores) = committeeRotation.getCommitteeDetails();
        // 填充声誉和质押权重（现在分数只基于声誉）
        for (uint i = 0; i < 3; i++) {
            reputations[i] = scores[i];
            stakeWeights[i] = 100; // 固定权重
        }
    }
    
    /**
     * 更新委员会（从轮换系统获取）
     */
    function updateCommittee() public {
        require(isInitialized, "Not initialized");
        
        address[3] memory newCommittee = committeeRotation.getCurrentCommittee();
        
        // 检查是否有变化
        bool hasChanged = false;
        for (uint i = 0; i < 3; i++) {
            if (currentCommittee[i] != newCommittee[i]) {
                hasChanged = true;
                break;
            }
        }
        
        if (hasChanged) {
            currentCommittee = newCommittee;
            lastUpdateTime = block.timestamp;
            emit CommitteeUpdated(newCommittee, block.timestamp);
        }
    }
    
    /**
     * 执行委员会轮换
     */
    function rotateCommittee() external {
        require(isInitialized, "Not initialized");
        require(!paused, "System is paused");
        
        // 执行轮换
        committeeRotation.rotateCommittee();
        
        // 更新委员会
        updateCommittee();
    }
    
    /**
     * 添加候选者
     */
    function addCandidate(address candidate) external {
        require(isInitialized, "Not initialized");
        require(!paused, "System is paused");
        committeeRotation.addCandidate(candidate);
    }
    
    /**
     * 移除候选者
     */
    function removeCandidate(address candidate) external {
        require(isInitialized, "Not initialized");
        require(!paused, "System is paused");
        committeeRotation.removeCandidate(candidate);
    }
    
    /**
     * 获取候选池
     */
    function getCandidatePool() external view returns (address[] memory) {
        return committeeRotation.getCandidatePool();
    }
    
    /**
     * 获取轮换信息
     */
    function getRotationInfo() external view returns (
        uint256 lastRotation,
        uint256 nextRotation,
        uint256 count,
        bool canRotateNow
    ) {
        uint256 rotationCount;
        uint256 lastTime;
        uint256 nextTime;
        (rotationCount, lastTime, nextTime) = committeeRotation.getRotationInfo();
        return (lastTime, nextTime, rotationCount, committeeRotation.canRotate());
    }
    
    /**
     * 获取系统状态
     */
    function getSystemStatus() external view returns (
        bool initialized,
        uint256 lastUpdate,
        uint256 committeeCount,
        uint256 candidateCount
    ) {
        return (
            isInitialized,
            lastUpdateTime,
            3, // 固定3个成员
            committeeRotation.getCandidatePool().length
        );
    }
    
    /**
     * 紧急暂停（管理员功能）
     */
    function emergencyPause() external {
        require(isInitialized, "Not initialized");
        require(!paused, "System already paused");
        
        paused = true;
        emit SystemPaused(block.timestamp);
    }
    
    /**
     * 恢复系统（管理员功能）
     */
    function resumeSystem() external {
        require(isInitialized, "Not initialized");
        require(paused, "System not paused");
        
        paused = false;
        emit SystemResumed(block.timestamp);
    }
}
