// SPDX-License-Identifier: MIT   no use
pragma solidity ^0.6.12;

import "./RewardPool.sol";

contract SignatureEscrow {
    struct Escrow {
        address alice;                 // Alice 地址
        address tumbler;              // Tumbler 地址
        address payable beneficiary;  // 受益人
        uint256 amount;              // 托管金额
        uint256 deadline;             // 截止时间
        bytes32 txHash;               // 交易哈希
        bytes alicePublicKey;         // Alice 公钥
        bytes tumblerPublicKey;       // Tumbler 公钥
        bytes tumblerSignature1;      // Tumbler 第一个签名
        bytes tumblerSignature2;      // Tumbler 第二个签名
        bool released;                // 是否已释放
        bool txHashStored;            // 交易哈希是否已存储
    }

    // 每个合约实例对应一个固定面额
    uint256 public denomination;
    
    // 奖励池合约引用
    RewardPool public rewardPool;
    uint256 public constant SERVICE_FEE_PERCENTAGE = 0.2e18;  // 20%服务费
    uint256 public constant PRECISION = 1e18;
    
    mapping(bytes32 => Escrow) private escrows; // key = escrowId

    event EscrowCreated(bytes32 indexed escrowId, address alice, address tumbler, uint256 amount, uint256 deadline);
    event TxHashStored(bytes32 indexed escrowId, bytes32 txHash);
    event TumblerSignature1Provided(bytes32 indexed escrowId, bytes tumblerSignature1);
    event TumblerSignature2Provided(bytes32 indexed escrowId, bytes tumblerSignature2);
    event EscrowReleased(bytes32 indexed escrowId, address to, uint256 amount);
    event EscrowRefunded(bytes32 indexed escrowId, address to, uint256 amount);
    event ServiceFeeCollected(bytes32 indexed escrowId, uint256 serviceFee, uint256 finalAmount);

    constructor(uint256 _denominationWei) public {
        require(_denominationWei > 0, "denomination=0");
        denomination = _denominationWei;
    }

    modifier onlyAlice(bytes32 escrowId) {
        require(msg.sender == escrows[escrowId].alice, "not alice");
        _;
    }

    modifier onlyTumbler(bytes32 escrowId) {
        require(msg.sender == escrows[escrowId].tumbler, "not tumbler");
        _;
    }

    // 设置奖励池地址
    function setRewardPool(address _rewardPool) external {
        require(_rewardPool != address(0), "Invalid reward pool address");
        rewardPool = RewardPool(payable(_rewardPool));
    }

    // Alice 创建托管
    function createEscrow(
        bytes32 escrowId,
        address tumbler,
        address payable beneficiary,
        uint256 deadline,
        bytes calldata alicePublicKey,
        bytes calldata tumblerPublicKey
    ) external payable {
        require(escrows[escrowId].alice == address(0), "escrow exists");
        require(msg.value == denomination, "wrong amount");
        require(beneficiary != address(0), "zero beneficiary");
        require(tumbler != address(0) && tumbler != msg.sender, "invalid tumbler");
        require(deadline > block.timestamp, "bad deadline");
        require(alicePublicKey.length > 0 && tumblerPublicKey.length > 0, "empty public keys");

        // 逐字段写入，避免结构体字面量导致的 Stack too deep
        Escrow storage e = escrows[escrowId];
        e.alice = msg.sender;
        e.tumbler = tumbler;
        e.beneficiary = beneficiary;
        e.amount = msg.value;
        e.deadline = deadline;
        e.txHash = bytes32(0);
        e.alicePublicKey = alicePublicKey;
        e.tumblerPublicKey = tumblerPublicKey;
        e.tumblerSignature1 = "";
        e.tumblerSignature2 = "";
        e.released = false;
        e.txHashStored = false;

        emit EscrowCreated(escrowId, msg.sender, tumbler, msg.value, deadline);
    }

    // 存储交易哈希（由 Alice 调用）
    function storeTxHash(bytes32 escrowId, bytes32 txHash) external onlyAlice(escrowId) {
        require(!escrows[escrowId].released, "already released");
        require(!escrows[escrowId].txHashStored, "txHash already stored");
        
        escrows[escrowId].txHash = txHash;
        escrows[escrowId].txHashStored = true;
        
        emit TxHashStored(escrowId, txHash);
    }

    // Tumbler 提供第一个签名
    function provideTumblerSignature1(bytes32 escrowId, bytes calldata signature) external onlyTumbler(escrowId) {
        require(!escrows[escrowId].released, "already released");
        require(escrows[escrowId].txHashStored, "txHash not stored");
        require(signature.length > 0, "empty signature");
        
        escrows[escrowId].tumblerSignature1 = signature;
        
        emit TumblerSignature1Provided(escrowId, signature);
    }

    // Tumbler 提供第二个签名并释放资金
    function provideTumblerSignature2AndRelease(
        bytes32 escrowId,
        bytes calldata tumblerSignature2
    ) external onlyTumbler(escrowId) {
        _validateBeforeRelease(escrowId, tumblerSignature2);
        _release(escrowId, tumblerSignature2);
    }

    function _validateBeforeRelease(bytes32 escrowId, bytes calldata tumblerSignature2) internal view {
        Escrow storage e = escrows[escrowId];
        require(!e.released, "already released");
        require(e.txHashStored, "txHash not stored");
        require(e.tumblerSignature1.length > 0, "tumbler signature1 missing");
        require(tumblerSignature2.length > 0, "empty tumbler signature2");

        // 简化的签名验证：只检查签名长度和是否不同
        require(e.tumblerSignature1.length == 65, "invalid signature1 length");
        require(tumblerSignature2.length == 65, "invalid signature2 length");
        require(keccak256(e.tumblerSignature1) != keccak256(tumblerSignature2), "signatures must differ");
    }

    function _release(bytes32 escrowId, bytes calldata tumblerSignature2) internal {
        Escrow storage e = escrows[escrowId];
        e.tumblerSignature2 = tumblerSignature2;
        e.released = true;
        uint256 amount = e.amount;
        e.amount = 0;
        
        // 计算服务费（20%）
        uint256 serviceFee = 0;
        uint256 finalAmount = amount;
        
        if (address(rewardPool) != address(0)) {
            serviceFee = (amount * SERVICE_FEE_PERCENTAGE) / PRECISION;
            finalAmount = amount - serviceFee;
            
            // 将服务费转入奖励池
            (bool feeOk, ) = address(rewardPool).call{value: serviceFee}("");
            require(feeOk, "Service fee transfer failed");
            
            // 记录交易参与（Tumbler 是委员会成员）
            rewardPool.recordTransaction(e.tumbler);
            
            emit ServiceFeeCollected(escrowId, serviceFee, finalAmount);
        }
        
        // 将剩余金额转给受益人
        (bool success, ) = e.beneficiary.call{value: finalAmount}("");
        require(success, "transfer failed");
        emit TumblerSignature2Provided(escrowId, tumblerSignature2);
        emit EscrowReleased(escrowId, e.beneficiary, finalAmount);
    }

    // 退款功能（超时后）
    function refund(bytes32 escrowId) external {
        Escrow storage escrow = escrows[escrowId];
        require(block.timestamp >= escrow.deadline, "not expired");
        require(!escrow.released, "already released");
        
        escrow.released = true;
        uint256 amount = escrow.amount;
        escrow.amount = 0;
        
        (bool success, ) = escrow.alice.call{value: amount}("");
        require(success, "refund failed");
        
        emit EscrowRefunded(escrowId, escrow.alice, amount);
    }

    // 获取托管信息
    function getEscrowInfo(bytes32 escrowId) external view returns (
        address alice,
        address tumbler,
        address beneficiary,
        uint256 amount,
        uint256 deadline,
        bytes32 txHash,
        bool released,
        bool txHashStored
    ) {
        Escrow storage escrow = escrows[escrowId];
        return (
            escrow.alice,
            escrow.tumbler,
            escrow.beneficiary,
            escrow.amount,
            escrow.deadline,
            escrow.txHash,
            escrow.released,
            escrow.txHashStored
        );
    }
}