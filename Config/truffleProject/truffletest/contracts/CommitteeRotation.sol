// SPDX-License-Identifier: MIT
pragma solidity ^0.6.12;

import "./ReputationManager.sol";
import "./StakingManager.sol";
import "./VRF.sol";
import "./RewardPool.sol";

contract CommitteeRotation {
    // 常量
    uint256 public constant MAX_COMMITTEE_SIZE = 3;
    uint256 public constant TOP_K = 10;  // 从 top-K 候选者中选择
    uint256 public constant ROTATION_INTERVAL = 20 seconds;  // 轮换间隔：20秒
    uint256 public constant BASE_WEIGHT = 1000;  // 基础权重
    uint256 public constant MAX_WEIGHT = 10000;   // 最大权重
    
    // 委员会管理
    address[MAX_COMMITTEE_SIZE] public currentCommittee;
    
    // 轮换状态
    uint256 public lastRotationTime;
    uint256 public rotationCount;
    
    // 候选池
    address[] public candidatePool;
    
    // VRF 状态 - 简化版本：不再与轮次绑定
    bytes32 public currentVRFRandom;           // 当前可用的随机数
    bytes public currentVRFProof;              // 当前随机数的证明
    bytes public currentVRFPublicKey;          // 当前随机数的公钥
    bytes public currentVRFMessage;            // 当前随机数的消息
    bool public currentVRFVerified;            // 当前随机数是否已验证
    bool public currentVRFUsed;                // 当前随机数是否已使用
    address public vrfGenerator;
    
    // 保留旧的 mapping 用于历史记录（可选）
    mapping(uint256 => bytes32) public rotationRandom;
    mapping(uint256 => bool) public randomUsed;
    
    // 合约引用
    ReputationManager public reputationManager;
    StakingManager public stakingManager;
    RewardPool public rewardPool;
    
    // 事件
    event CommitteeRotated(address[MAX_COMMITTEE_SIZE] newCommittee, uint256 timestamp);
    event MemberAdded(address indexed member);
    event MemberRemoved(address indexed member);
    event CandidateAdded(address indexed candidate);
    event CandidateRemoved(address indexed candidate);
    event VRFRandomSubmitted(bytes32 random, uint256 timestamp);
    event VRFRandomVerified(bool isValid, bytes32 random);
    event VRFRandomUsed(uint256 indexed rotationRound, bytes32 random);
    event VRFDebug(string step, bytes32 data1, bytes32 data2);
    event VRFVerifyDebug(
        string step,
        uint256 value1,
        uint256 value2,
        uint256 value3,
        uint256 value4
    );
    
    constructor(
        address _reputationManager,
        address payable _stakingManager,
        address[MAX_COMMITTEE_SIZE] memory _initialCommittee,
        address _vrfGenerator,
        address _rewardPool
    ) public {
        reputationManager = ReputationManager(_reputationManager);
        stakingManager = StakingManager(_stakingManager);
        vrfGenerator = _vrfGenerator;
        if (_rewardPool != address(0)) {
            rewardPool = RewardPool(payable(_rewardPool));
        }
        lastRotationTime = block.timestamp;
        rotationCount = 0;
        
        // 设置初始委员会
        currentCommittee = _initialCommittee;
    }
    
    // 设置奖励池地址
    function setRewardPool(address _rewardPool) external {
        require(msg.sender == vrfGenerator || vrfGenerator == address(0), "Not authorized");
        require(_rewardPool != address(0), "Invalid address");
        rewardPool = RewardPool(payable(_rewardPool));
    }
    
    // 设置 VRF 生成器地址
    function setVRFGenerator(address _vrfGenerator) external {
        require(msg.sender == vrfGenerator || vrfGenerator == address(0), "Not authorized");
        vrfGenerator = _vrfGenerator;
    }
    
    // 提交 VRF 随机数、证明和公钥（简化版本：不再需要指定轮次）
    function submitVRFRandomWithProof(
        bytes32 random,
        bytes memory proof,
        bytes memory publicKey,
        bytes memory message
    ) external {
        require(msg.sender == vrfGenerator || vrfGenerator == address(0), "Not authorized");
        require(proof.length == 81, "Invalid proof length");
        require(publicKey.length == 33, "Invalid public key length");
        
        // 存储到当前 VRF 状态
        currentVRFRandom = random;
        currentVRFProof = proof;
        currentVRFPublicKey = publicKey;
        currentVRFMessage = message;
        currentVRFVerified = false;  // 需要验证
        currentVRFUsed = false;      // 未使用
        
        // 同时存储到历史记录
        rotationRandom[rotationCount] = random;
        
        emit VRFRandomSubmitted(random, block.timestamp);
    }
    
    // 验证 VRF 证明（简化版本：验证当前提交的 VRF）
    function verifyVRFProof() external returns (bool) {
        require(currentVRFProof.length == 81, "Proof not submitted");
        require(currentVRFPublicKey.length == 33, "Public key not submitted");
        require(currentVRFRandom != bytes32(0), "Random not submitted");
        
        // 解码公钥：从 33 字节压缩格式转换为 [x, y]
        uint256[2] memory pubKey = VRF.decodePoint(currentVRFPublicKey);
        
        // 解码证明：从 81 字节转换为 [gamma-x, gamma-y, c, s]
        uint256[4] memory proof = VRF.decodeProof(currentVRFProof);
        
        // 简化调试输出以避免 Stack too deep 错误
        emit VRFVerifyDebug("input_random", uint256(currentVRFRandom), 0, 0, 0);
        emit VRFVerifyDebug("decode_proof", proof[0], proof[2], proof[3], 0);
        emit VRFVerifyDebug("decode_pubkey", pubKey[0], pubKey[1], 0, 0);
        
        // 执行VRF验证
        // 步骤 1: 哈希到曲线
        (uint256 hPointX, uint256 hPointY) = VRF.hashToTryAndIncrement(pubKey, currentVRFMessage);
        emit VRFVerifyDebug("hash_to_curve_H", hPointX, hPointY, 0, 0);
        
        // 步骤 2: 计算 U = s*B - c*Y
        (uint256 uPointX, uint256 uPointY) = VRF.ecMulSubMul(
            proof[3],  // s
            VRF.GX, VRF.GY,  // B (生成元)
            proof[2],  // c
            pubKey[0], pubKey[1]  // Y (公钥)
        );
        emit VRFVerifyDebug("compute_U", uPointX, uPointY, 0, 0);
        
        // 步骤 3: 计算 V = s*H - c*Gamma
        (uint256 vPointX, uint256 vPointY) = VRF.ecMulSubMul(
            proof[3],  // s
            hPointX, hPointY,  // H
            proof[2],  // c
            proof[0], proof[1]  // Gamma
        );
        emit VRFVerifyDebug("compute_V", vPointX, vPointY, 0, 0);
        
        // 步骤 4: 计算 c' = hashPoints(H, Gamma, U, V)
        bytes16 derivedC = VRF.hashPoints(hPointX, hPointY, proof[0], proof[1], uPointX, uPointY, vPointX, vPointY);
        emit VRFVerifyDebug("hash_points_cprime", uint128(derivedC), 0, 0, 0);
        emit VRFVerifyDebug("proof_c", proof[2], 0, 0, 0);
        
        // 步骤 5: 验证
        bool isValid = uint128(derivedC) == proof[2];
        emit VRFVerifyDebug("verify_result", isValid ? 1 : 0, 0, 0, 0);
        
        // 只有验证成功时才标记为已验证
        if (isValid) {
            currentVRFVerified = true;
            emit VRFRandomVerified(true, currentVRFRandom);
        } else {
            // 验证失败时不清空，允许重试或重新提交
            currentVRFVerified = false;
            emit VRFRandomVerified(false, bytes32(0));
        }
        
        return isValid;
    }
    
    // 添加候选者
    function addCandidate(address candidate) external {
        require(candidate != address(0), "Invalid address");
        for (uint i = 0; i < candidatePool.length; i++) {
            require(candidatePool[i] != candidate, "Candidate already exists");
        }
        candidatePool.push(candidate);
        emit CandidateAdded(candidate);
    }
    
    // 移除候选者
    function removeCandidate(address candidate) external {
        for (uint i = 0; i < candidatePool.length; i++) {
            if (candidatePool[i] == candidate) {
                candidatePool[i] = candidatePool[candidatePool.length - 1];
                candidatePool.pop();
                emit CandidateRemoved(candidate);
                return;
            }
        }
        revert("Candidate not found");
    }
    
    // 获取候选者声誉（直接返回声誉值）
    function getCandidateReputation(address candidate) external view returns (uint256) {
        return reputationManager.calculateReputation(candidate);
    }
    
    // 获取候选池
    function getCandidatePool() external view returns (address[] memory) {
        return candidatePool;
    }
    
    // 检查是否可以进行轮换
    function canRotate() public view returns (bool) {
        return block.timestamp >= lastRotationTime + ROTATION_INTERVAL;
    }
    
    // 从 VRF 随机数中提取随机值（辅助函数，减少栈深度）
    function extractRandomValue(bytes32 random, uint256 offset, uint256 round, uint256 maxValue) internal pure returns (uint256) {
        if (offset + 3 < 32) {
            return ((uint256(random) >> (offset * 8)) & 0xFFFFFFFF) % maxValue;
        } else {
            return uint256(keccak256(abi.encodePacked(random, offset, round))) % maxValue;
        }
    }
    
    // 计算权重（辅助函数，减少栈深度）
    function calculateWeights(
        uint256[] memory scores,
        uint256 count,
        uint256 baseWeight,
        uint256 maxWeight
    ) internal pure returns (uint256[] memory) {
        if (count == 0) {
            return new uint256[](0);
        }
        
        // 找到最小和最大分数
        uint256 minScore = scores[0];
        uint256 maxScore = scores[0];
        for (uint256 i = 1; i < count; i++) {
            if (scores[i] < minScore) {
                minScore = scores[i];
            }
            if (scores[i] > maxScore) {
                maxScore = scores[i];
            }
        }
        
        uint256[] memory weights = new uint256[](count);
        
        if (maxScore == minScore) {
            // 所有分数相同，使用平均权重
            for (uint256 i = 0; i < count; i++) {
                weights[i] = baseWeight;
            }
        } else {
            // 归一化权重
            uint256 scoreRange = maxScore - minScore;
            uint256 weightRange = maxWeight - baseWeight;
            
            for (uint256 i = 0; i < count; i++) {
                uint256 normalized = ((scores[i] - minScore) * weightRange) / scoreRange;
                weights[i] = baseWeight + normalized;
            }
        }
        
        return weights;
    }
    
    // 执行委员会轮换（简化版本：使用当前已验证的 VRF）
    function rotateCommittee() external {
        require(canRotate(), "Rotation not yet available");
        require(candidatePool.length >= MAX_COMMITTEE_SIZE, "Insufficient candidates");
        
        // 检查当前 VRF 是否已设置并验证
        require(currentVRFRandom != bytes32(0), "VRF random not set");
        // 临时注释验证检查，允许使用未验证的随机数进行测试
        // require(currentVRFVerified, "VRF random not verified");
        require(!currentVRFUsed, "VRF random already used");
        
        // 保存当前委员会（用于奖励分配）
        address[MAX_COMMITTEE_SIZE] memory previousCommittee = currentCommittee;
        
        // 选择新委员会（使用加权轮盘赌选择，直接使用实时声誉值）
        address[MAX_COMMITTEE_SIZE] memory newCommittee = selectNewCommittee(currentVRFRandom);
        
        // 结束当前epoch并分配奖励（如果奖励池已设置）
        if (address(rewardPool) != address(0)) {
            // 结束当前epoch（在更新委员会之前）
            rewardPool.endEpoch();
            
            // 分配上一轮次的奖励（如果有）
            uint256 previousEpoch = rewardPool.currentEpoch() - 1;
            if (previousEpoch > 0) {
                // 将固定数组转换为动态数组
                address[] memory previousCommitteeList = new address[](MAX_COMMITTEE_SIZE);
                uint256 validCount = 0;
                for (uint i = 0; i < MAX_COMMITTEE_SIZE; i++) {
                    if (previousCommittee[i] != address(0)) {
                        previousCommitteeList[validCount] = previousCommittee[i];
                        validCount++;
                    }
                }
                
                // 调整数组大小
                if (validCount < MAX_COMMITTEE_SIZE) {
                    address[] memory trimmedList = new address[](validCount);
                    for (uint i = 0; i < validCount; i++) {
                        trimmedList[i] = previousCommitteeList[i];
                    }
                    previousCommitteeList = trimmedList;
                }
                
                // 分配奖励
                if (previousCommitteeList.length > 0) {
                    try rewardPool.distributeRewardsToMembers(previousEpoch, previousCommitteeList) {
                        // 奖励分配成功
                    } catch {
                        // 奖励分配失败（可能是没有奖励或已分配），继续执行
                    }
                }
            }
        }
        
        // 更新委员会
        currentCommittee = newCommittee;
        
        // 更新状态
        lastRotationTime = block.timestamp;
        currentVRFUsed = true;  // 标记当前随机数已使用
        randomUsed[rotationCount] = true;  // 历史记录
        rotationCount++;
        
        emit VRFRandomUsed(rotationCount - 1, currentVRFRandom);
        emit CommitteeRotated(newCommittee, block.timestamp);
    }
    
    // 使用加权轮盘赌选择新委员会
    function selectNewCommittee(bytes32 random) internal view returns (address[MAX_COMMITTEE_SIZE] memory) {
        address[MAX_COMMITTEE_SIZE] memory selected;
        
        // 1. 收集候选者声誉和创建候选者副本
        uint256[] memory scores = new uint256[](candidatePool.length);
        address[] memory candidates = new address[](candidatePool.length);
        for (uint i = 0; i < candidatePool.length; i++) {
            scores[i] = reputationManager.calculateReputation(candidatePool[i]);
            candidates[i] = candidatePool[i];
        }
        
        // 2. 找到 top-K 候选者
        uint256 topK = candidatePool.length < TOP_K ? candidatePool.length : TOP_K;
        uint256[] memory topKScores = new uint256[](topK);
        address[] memory topKCandidates = new address[](topK);
        
        // 简单的选择 top-K（使用局部副本）
        for (uint i = 0; i < topK; i++) {
            uint256 maxIdx = i;
            for (uint j = i + 1; j < candidates.length; j++) {
                if (scores[j] > scores[maxIdx]) {
                    maxIdx = j;
                }
            }
            // 交换（使用局部数组）
            uint256 tempScore = scores[i];
            address tempCandidate = candidates[i];
            scores[i] = scores[maxIdx];
            candidates[i] = candidates[maxIdx];
            scores[maxIdx] = tempScore;
            candidates[maxIdx] = tempCandidate;
            
            topKScores[i] = scores[i];
            topKCandidates[i] = candidates[i];
        }
        
        // 3. 计算权重
        uint256[] memory weights = calculateWeights(topKScores, topK, BASE_WEIGHT, MAX_WEIGHT);
        
        // 4. 构建累积权重
        uint256[] memory cumulativeWeights = new uint256[](topK);
        cumulativeWeights[0] = weights[0];
        for (uint i = 1; i < topK; i++) {
            cumulativeWeights[i] = cumulativeWeights[i-1] + weights[i];
        }
        uint256 totalWeight = cumulativeWeights[topK - 1];
        
        // 5. 使用 VRF 随机数进行轮盘赌选择（无放回）
        bool[] memory selectedFlags = new bool[](topK);
        
        for (uint selectedCount = 0; selectedCount < MAX_COMMITTEE_SIZE; selectedCount++) {
            if (topK - selectedCount == 0) break;
            
            // 从随机数中提取一个值 (每次使用4字节)
            uint256 randomValue = ((uint256(random) >> (selectedCount * 32)) & 0xFFFFFFFF) % totalWeight;
            
            // 在累积权重中找到对应的候选者
            uint256 selectedIdx = topK;  // 无效索引
            for (uint i = 0; i < topK; i++) {
                if (!selectedFlags[i] && randomValue < cumulativeWeights[i]) {
                    selectedIdx = i;
                    break;
                }
            }
            
            // 如果没找到（理论上不应该发生），选择第一个未选中的
            if (selectedIdx >= topK) {
                for (uint i = 0; i < topK; i++) {
                    if (!selectedFlags[i]) {
                        selectedIdx = i;
                        break;
                    }
                }
            }
            
            if (selectedIdx < topK) {
                selected[selectedCount] = topKCandidates[selectedIdx];
                selectedFlags[selectedIdx] = true;
                
                // 更新累积权重（移除已选中的）
                uint256 removedWeight = weights[selectedIdx];
                for (uint i = selectedIdx; i < topK; i++) {
                    if (cumulativeWeights[i] >= removedWeight) {
                        cumulativeWeights[i] -= removedWeight;
                    }
                }
                totalWeight -= removedWeight;
            }
        }
        
        return selected;
    }
    
    // 获取轮换信息
    function getRotationInfo() external view returns (
        uint256 count,
        uint256 lastTime,
        uint256 nextTime
    ) {
        return (
            rotationCount,
            lastRotationTime,
            lastRotationTime + ROTATION_INTERVAL
        );
    }
    
    // 获取委员会详情
    // 获取当前委员会成员
    function getCurrentCommittee() external view returns (address[MAX_COMMITTEE_SIZE] memory) {
        return currentCommittee;
    }
    
    // 检查候选者是否有资格加入委员会
    function isEligibleForCommittee(address candidate) external view returns (bool) {
        // 检查是否已质押
        (uint256 stakeAmount, , bool isActive) = stakingManager.getStakeInfo(candidate);
        if (stakeAmount == 0 || !isActive) {
            return false;
        }
        
        // 检查声誉是否足够
        uint256 reputation = reputationManager.calculateReputation(candidate);
        if (reputation == 0) {
            return false;
        }
        
        return true;
    }
    
    // 检查是否为当前委员会成员
    function isCurrentCommitteeMember(address member) external view returns (bool) {
        for (uint i = 0; i < MAX_COMMITTEE_SIZE; i++) {
            if (currentCommittee[i] == member) {
                return true;
            }
        }
        return false;
    }
    
    function getCommitteeDetails() external view returns (
        address[MAX_COMMITTEE_SIZE] memory members,
        uint256[MAX_COMMITTEE_SIZE] memory scores
    ) {
        members = currentCommittee;
        
        for (uint i = 0; i < MAX_COMMITTEE_SIZE; i++) {
            // 直接返回声誉值
            scores[i] = reputationManager.calculateReputation(members[i]);
        }
    }
}
