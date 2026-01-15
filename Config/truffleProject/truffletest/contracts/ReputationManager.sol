// SPDX-License-Identifier: MIT
// contracts/ReputationManager.sol
pragma solidity ^0.6.12;

contract ReputationManager {
    // 固定3个委员会成员
    address[3] public committeeMembers;
    
    // 声誉权重系数（使用18位精度，1e18 = 1.0）
    // 根据论文：ΔRep = ω_c * S_consistency + ω_p * S_participation
    uint256 public constant OMEGA_CONSISTENCY = 0.6e18;      // ω_c = 0.6 (一致性权重)
    uint256 public constant OMEGA_PARTICIPATION = 0.4e18;    // ω_p = 0.4 (参与度权重)
    uint256 public constant PRECISION = 1e18;                 // 精度因子
    
    // 时间衰减因子（使用18位精度，1e18 = 1.0）
    // 根据论文：Rep_i^(t+1) = λ * Rep_i^(t) + ΔRep_i^(t)
    // λ = 0.95 表示历史声誉的权重为95%，新增量占5%
    uint256 public constant LAMBDA = 0.95e18;                 // λ = 0.95 (时间衰减因子)
    
    // 声誉数据结构
    struct MemberReputation {
        uint256 participationRate;   // 参与率 (0-100)
        uint256 consistency;         // 一致性 (0-100)
        uint256 lastUpdateTime;     // 最后更新时间
        uint256 totalDecisions;     // 总决策数
        uint256 correctDecisions;   // 正确决策数
        uint256 previousReputation; // 上一轮次的声誉值 Rep_i^(t)（用于时间衰减计算）
    }
    
    mapping(address => MemberReputation) public memberReputations;
    
    // 事件
    event ReputationUpdated(address indexed member, uint256 participation, uint256 consistency);
    
    constructor(address[3] memory _members) public {
        committeeMembers = _members;
        
        // 初始化成员声誉为默认值
        for (uint i = 0; i < 3; i++) {
            memberReputations[_members[i]] = MemberReputation({
                participationRate: 50,   // 默认50%
                consistency: 50,         // 默认50%
                lastUpdateTime: block.timestamp,
                totalDecisions: 0,
                correctDecisions: 0,
                previousReputation: 0    // 初始化为0，表示第一次计算，不使用时间衰减
            });
        }
    }
    
    // 更新声誉（任何人都可以调用）
    // 根据论文：Rep_i^(t+1) = λ * Rep_i^(t) + ΔRep_i^(t)
    // 增量声誉：ΔRep_i^(t) = ω_c * S_consistency + ω_p * S_participation
    function updateReputation(
        uint256 participation,
        uint256 consistency
    ) external {
        require(participation <= 100 && consistency <= 100, "Invalid values");
        
        // 在更新前，计算当前声誉并保存为历史声誉
        uint256 currentReputation = calculateReputation(msg.sender);
        
        memberReputations[msg.sender] = MemberReputation({
            participationRate: participation,
            consistency: consistency,
            lastUpdateTime: block.timestamp,
            totalDecisions: memberReputations[msg.sender].totalDecisions,
            correctDecisions: memberReputations[msg.sender].correctDecisions,
            previousReputation: currentReputation  // 保存当前声誉作为历史值
        });
        
        emit ReputationUpdated(msg.sender, participation, consistency);
    }
    
    // 计算增量声誉（符合论文公式）
    // 公式：ΔRep = ω_c * S_consistency + ω_p * S_participation
    function calculateDeltaReputation(address member) public view returns (uint256) {
        MemberReputation memory rep = memberReputations[member];
        
        // 计算加权后的增量声誉
        // consistency 和 participation 都是 0-100 的分数
        uint256 weightedConsistency = (rep.consistency * OMEGA_CONSISTENCY) / PRECISION;
        uint256 weightedParticipation = (rep.participationRate * OMEGA_PARTICIPATION) / PRECISION;
        
        // 增量声誉 = ω_c * S_consistency + ω_p * S_participation
        return weightedConsistency + weightedParticipation;
    }
    
    // 计算综合声誉（符合论文公式）
    // 论文公式：Rep_i^(t+1) = λ * Rep_i^(t) + ΔRep_i^(t)
    // 其中：
    //   - λ = 0.95 (时间衰减因子，历史声誉权重95%)
    //   - Rep_i^(t) = previousReputation (上一轮次的声誉值)
    //   - ΔRep_i^(t) = ω_c * S_consistency + ω_p * S_participation (本轮增量)
    // 
    // 实现说明：
    //   - 增量声誉仅由 consistency 和 participation 的加权和组成
    //   - 新声誉 = λ * 历史声誉 + 增量声誉
    function calculateReputation(address member) public view returns (uint256) {
        MemberReputation memory rep = memberReputations[member];
        
        // 计算增量声誉（基于 consistency 和 participation）
        // 公式：ΔRep_i^(t) = ω_c * S_consistency + ω_p * S_participation
        uint256 deltaReputation = calculateDeltaReputation(member);
        
        // 如果这是第一次计算（previousReputation == 0），直接返回增量声誉
        if (rep.previousReputation == 0) {
            return deltaReputation;
        }
        
        // 应用时间衰减公式：Rep_i^(t+1) = λ * Rep_i^(t) + ΔRep_i^(t)
        uint256 decayedPreviousRep = (rep.previousReputation * LAMBDA) / PRECISION;
        uint256 newReputation = decayedPreviousRep + deltaReputation;
        
        return newReputation;
    }
    
    // 检查是否为委员会成员
    function isCommitteeMember(address member) public view returns (bool) {
        for (uint i = 0; i < 3; i++) {
            if (committeeMembers[i] == member) {
                return true;
            }
        }
        return false;
    }
    
    // 获取历史声誉值（用于调试和验证）
    function getPreviousReputation(address member) public view returns (uint256) {
        return memberReputations[member].previousReputation;
    }
    
    // 获取所有成员声誉
    function getAllReputations() public view returns (
        address[3] memory members,
        uint256[3] memory reputations
    ) {
        for (uint i = 0; i < 3; i++) {
            members[i] = committeeMembers[i];
            reputations[i] = calculateReputation(committeeMembers[i]);
        }
    }
}
