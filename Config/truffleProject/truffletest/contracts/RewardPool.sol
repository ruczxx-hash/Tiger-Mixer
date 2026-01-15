// SPDX-License-Identifier: MIT
// contracts/RewardPool.sol
pragma solidity ^0.6.12;

/**
 * 奖励池合约
 * 功能：
 * 1. 接收服务费（20%的交易费用）
 * 2. 接收被slash的质押资金
 * 3. 记录每个epoch中每个成员参与的交易数
 * 4. 在每个epoch结束时按交易数量比例分配奖励
 */
contract RewardPool {
    // 服务费比例（20% = 0.2，使用18位精度）
    uint256 public constant SERVICE_FEE_PERCENTAGE = 0.2e18;  // 20%
    uint256 public constant PRECISION = 1e18;
    
    // Epoch 数据结构
    struct EpochReward {
        uint256 totalReward;              // 该epoch的总奖励
        uint256 totalTransactions;        // 该epoch的总交易数
        bool distributed;                 // 是否已分配
        mapping(address => uint256) memberTransactions;  // 成员参与的交易数
        mapping(address => uint256) memberRewards;       // 成员获得的奖励
    }
    
    // Epoch 管理
    uint256 public currentEpoch;
    mapping(uint256 => EpochReward) public epochs;
    
    // 总奖励池余额
    uint256 public totalPoolBalance;
    
    // 权限管理
    address public owner;
    address public committeeRotation;  // 委员会轮换合约（可以触发epoch结束）
    address public signatureEscrow;    // 签名托管合约（可以记录交易参与）
    address public fixedMixerEscrow;   // 固定面额混币托管合约（可以记录交易参与）
    
    // 事件
    event ServiceFeeDeposited(uint256 epoch, uint256 amount, address indexed from);
    event SlashedFundsDeposited(uint256 epoch, uint256 amount, address indexed slashedMember);
    event TransactionRecorded(uint256 epoch, address indexed member);
    event EpochEnded(uint256 epoch, uint256 totalReward, uint256 totalTransactions);
    event RewardDistributed(uint256 epoch, address indexed member, uint256 amount);
    event EpochRewardClaimed(uint256 epoch, address indexed member, uint256 amount);
    
    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner");
        _;
    }
    
    modifier onlyAuthorized() {
        require(
            msg.sender == owner || 
            msg.sender == committeeRotation || 
            msg.sender == signatureEscrow ||
            msg.sender == fixedMixerEscrow,
            "Not authorized"
        );
        _;
    }
    
    constructor() public {
        owner = msg.sender;
        currentEpoch = 1;
        // 初始化第一个epoch
        epochs[currentEpoch].totalReward = 0;
        epochs[currentEpoch].totalTransactions = 0;
        epochs[currentEpoch].distributed = false;
    }
    
    // 设置授权地址
    function setCommitteeRotation(address _committeeRotation) external onlyOwner {
        committeeRotation = _committeeRotation;
    }
    
    function setSignatureEscrow(address _signatureEscrow) external onlyOwner {
        signatureEscrow = _signatureEscrow;
    }
    
    function setFixedMixerEscrow(address _fixedMixerEscrow) external onlyOwner {
        fixedMixerEscrow = _fixedMixerEscrow;
    }
    
    // 接收被slash的资金（从StakingManager调用）
    // 注意：资金通过 receive() 函数接收，这里只是记录事件
    function depositSlashedFunds(address slashedMember, uint256 amount) external {
        require(amount > 0, "Amount must be positive");
        // 资金应该已经通过 receive() 接收，这里记录事件
        emit SlashedFundsDeposited(currentEpoch, amount, slashedMember);
    }
    
    // 记录成员参与的交易（从交易合约调用）
    function recordTransaction(address member) external onlyAuthorized {
        require(member != address(0), "Invalid member");
        
        epochs[currentEpoch].memberTransactions[member]++;
        epochs[currentEpoch].totalTransactions++;
        
        emit TransactionRecorded(currentEpoch, member);
    }
    
    // 结束当前epoch并开始新epoch（从CommitteeRotation调用）
    function endEpoch() external {
        require(msg.sender == committeeRotation || msg.sender == owner, "Not authorized");
        
        uint256 epoch = currentEpoch;
        uint256 totalReward = epochs[epoch].totalReward;
        uint256 totalTransactions = epochs[epoch].totalTransactions;
        
        emit EpochEnded(epoch, totalReward, totalTransactions);
        
        // 开始新epoch
        currentEpoch++;
        epochs[currentEpoch].totalReward = 0;
        epochs[currentEpoch].totalTransactions = 0;
        epochs[currentEpoch].distributed = false;
    }
    
   
    
    // 分配奖励给指定成员列表（由CommitteeRotation调用）
    function distributeRewardsToMembers(uint256 epoch, address[] memory members) external {
        require(epoch <= currentEpoch, "Invalid epoch");
        require(!epochs[epoch].distributed, "Already distributed");
        require(epochs[epoch].totalReward > 0, "No reward to distribute");
        require(epochs[epoch].totalTransactions > 0, "No transactions");
        require(msg.sender == committeeRotation || msg.sender == owner, "Not authorized");
        
        EpochReward storage epochReward = epochs[epoch];
        uint256 totalReward = epochReward.totalReward;
        uint256 totalTransactions = epochReward.totalTransactions;
        
        // 计算每个成员的奖励
        for (uint i = 0; i < members.length; i++) {
            address member = members[i];
            uint256 memberTxCount = epochReward.memberTransactions[member];
            
            if (memberTxCount > 0) {
                // 按交易数量比例分配
                uint256 memberReward = (totalReward * memberTxCount) / totalTransactions;
                epochReward.memberRewards[member] = memberReward;
                
                emit RewardDistributed(epoch, member, memberReward);
            }
        }
        
        epochReward.distributed = true;
    }
    
    // 成员领取奖励
    function claimReward(uint256 epoch) external {
        require(epoch < currentEpoch, "Epoch not ended");
        require(epochs[epoch].distributed, "Rewards not distributed");
        
        uint256 reward = epochs[epoch].memberRewards[msg.sender];
        require(reward > 0, "No reward to claim");
        require(address(this).balance >= reward, "Insufficient balance");
        
        // 清零奖励（防止重复领取）
        epochs[epoch].memberRewards[msg.sender] = 0;
        totalPoolBalance -= reward;
        
        // 转账奖励
        msg.sender.transfer(reward);
        
        emit EpochRewardClaimed(epoch, msg.sender, reward);
    }
    
    // 查询函数
    function getCurrentEpoch() external view returns (uint256) {
        return currentEpoch;
    }
    
    function getEpochInfo(uint256 epoch) external view returns (
        uint256 totalReward,
        uint256 totalTransactions,
        bool distributed
    ) {
        EpochReward storage epochReward = epochs[epoch];
        return (
            epochReward.totalReward,
            epochReward.totalTransactions,
            epochReward.distributed
        );
    }
    
    function getMemberTransactions(uint256 epoch, address member) external view returns (uint256) {
        return epochs[epoch].memberTransactions[member];
    }
    
    function getMemberReward(uint256 epoch, address member) external view returns (uint256) {
        return epochs[epoch].memberRewards[member];
    }
    
    // 接收ETH（服务费或slash资金）
    // 服务费通过此函数接收（从SignatureEscrow或FixedMixerEscrow等交易合约调用）
    // slash资金也通过此函数接收，但会额外调用 depositSlashedFunds 记录事件
    receive() external payable {
        require(msg.value > 0, "No value sent");
        
        epochs[currentEpoch].totalReward += msg.value;
        totalPoolBalance += msg.value;
        
        // 默认记录为服务费，如果是slash资金会有单独的事件（通过 depositSlashedFunds）
        emit ServiceFeeDeposited(currentEpoch, msg.value, msg.sender);
    }
}

