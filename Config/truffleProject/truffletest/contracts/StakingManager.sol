// SPDX-License-Identifier: MIT
// contracts/StakingManager.sol
pragma solidity ^0.6.12;

import "./RewardPool.sol";

contract StakingManager {
    // 质押参数
    uint256 public constant REQUIRED_STAKE = 1 ether;     // 固定质押量：1 ETH
    
    constructor() public {
        owner = msg.sender;
    }
    
    // 质押数据结构（简化版）
    struct StakeInfo {
        uint256 amount;           // 质押数量（固定为1 ETH）
        uint256 stakeTime;        // 质押时间
        bool isActive;            // 是否活跃
    }
    
    mapping(address => StakeInfo) public stakes;
    
    uint256 public totalStakePool;    // 总质押池
    
    // 奖励池合约引用（用于接收slash资金）
    RewardPool public rewardPool;
    
    // 权限管理
    address public owner;
    address public committeeRotation;  // 委员会轮换合约（可以slash成员）
    
    // Slash 记录
    mapping(address => uint256) public slashedAmounts;  // 被slash的总金额
    
    // 事件
    event Staked(address indexed user, uint256 amount);
    event Unstaked(address indexed user, uint256 amount);
    event Slashed(address indexed member, uint256 amount, string reason);
    
    // 质押ETH（固定1 ETH）
    function stake() external payable {
        require(msg.value == REQUIRED_STAKE, "Must stake exactly 1 ETH");
        require(!stakes[msg.sender].isActive, "Already staked");
        
        stakes[msg.sender] = StakeInfo({
            amount: msg.value,
            stakeTime: block.timestamp,
            isActive: true
        });
        
        totalStakePool += msg.value;
        
        emit Staked(msg.sender, msg.value);
    }
    
    // 解锁质押（无锁定期，立即解锁）
    function unstake() external {
        StakeInfo storage stakeInfo = stakes[msg.sender];
        require(stakeInfo.isActive, "No active stake");
        
        uint256 amount = stakeInfo.amount;
        
        // 更新状态
        stakeInfo.isActive = false;
        totalStakePool -= amount;
        
        // 转账（只返还质押金额，无奖励）
        require(address(this).balance >= amount, "Insufficient contract balance");
        msg.sender.transfer(amount);
        
        emit Unstaked(msg.sender, amount);
    }
    
    // 计算质押权重（用于兼容性，但不再使用）
    // 注意：CommitteeRotation 已改为只使用声誉，此函数保留但返回固定值
    function calculateStakeWeight(address user) external view returns (uint256) {
        // 如果已质押，返回固定权重 100（仅用于满足接口要求）
        // 实际上 CommitteeRotation 不再使用此权重
        if (stakes[user].isActive) {
            return 100;
        }
        return 0;
    }
    
    // 检查是否已质押
    function hasStake(address user) external view returns (bool) {
        return stakes[user].isActive;
    }
    
    // 获取质押信息
    function getStakeInfo(address user) external view returns (
        uint256 amount,
        uint256 stakeTime,
        bool isActive
    ) {
        StakeInfo memory stakeInfo = stakes[user];
        return (
            stakeInfo.amount,
            stakeInfo.stakeTime,
            stakeInfo.isActive
        );
    }
    
    // 获取总质押池信息
    function getPoolInfo() external view returns (
        uint256 totalPool,
        uint256 contractBalance
    ) {
        return (
            totalStakePool,
            address(this).balance
        );
    }
    
    // 设置奖励池地址
    function setRewardPool(address _rewardPool) external {
        require(msg.sender == owner, "Only owner");
        require(_rewardPool != address(0), "Invalid address");
        rewardPool = RewardPool(payable(_rewardPool));
    }
    
    // 设置委员会轮换合约地址
    function setCommitteeRotation(address _committeeRotation) external {
        require(msg.sender == owner, "Only owner");
        require(_committeeRotation != address(0), "Invalid address");
        committeeRotation = _committeeRotation;
    }
    
    // Slash 恶意成员（只有授权地址可以调用）
    function slash(address maliciousMember, uint256 penaltyAmount, string memory reason) external {
        require(
            msg.sender == owner || msg.sender == committeeRotation,
            "Not authorized to slash"
        );
        require(maliciousMember != address(0), "Invalid member");
        
        StakeInfo storage stakeInfo = stakes[maliciousMember];
        require(stakeInfo.isActive, "Member has no active stake");
        require(penaltyAmount > 0, "Penalty amount must be positive");
        require(penaltyAmount <= stakeInfo.amount, "Penalty exceeds stake amount");
        
        // 扣除惩罚金额
        stakeInfo.amount -= penaltyAmount;
        totalStakePool -= penaltyAmount;
        slashedAmounts[maliciousMember] += penaltyAmount;
        
        // 如果质押金额为0，标记为非活跃
        if (stakeInfo.amount == 0) {
            stakeInfo.isActive = false;
        }
        
        // 将被slash的资金转入奖励池
        if (address(rewardPool) != address(0)) {
            RewardPool pool = RewardPool(payable(address(rewardPool)));
            (bool success, ) = address(pool).call{value: penaltyAmount}("");
            require(success, "Failed to transfer slashed funds to reward pool");
            // 记录slash事件（资金已通过receive()接收）
            pool.depositSlashedFunds(maliciousMember, penaltyAmount);
        } else {
            // 如果没有奖励池，资金留在合约中（可以后续提取）
            // 或者可以选择销毁
        }
        
        emit Slashed(maliciousMember, penaltyAmount, reason);
    }
    
    // 获取slash信息
    function getSlashedAmount(address member) external view returns (uint256) {
        return slashedAmounts[member];
    }
    
    // 接收ETH
    receive() external payable {
        // 允许合约接收ETH
    }
}
