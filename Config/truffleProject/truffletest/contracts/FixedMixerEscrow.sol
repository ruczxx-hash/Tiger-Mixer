// SPDX-License-Identifier: MIT
pragma solidity ^0.6.12;

import "./A2LECDSAVerifier.sol";
import "./RewardPool.sol";
import "./MerkleTreeWithHistory.sol";
import "./ReentrancyGuard.sol";
import "./IHasher.sol";

contract FixedMixerEscrow is MerkleTreeWithHistory, ReentrancyGuard {
  struct Escrow {
    address party1;
    address party2;
    uint256 amount;
    uint256 deadline;
    bool released;
    bytes32 metaHash;
    // 为后续链上签名校验预留的元数据（当前未在流程中使用）
    // party1 与 party2 的椭圆曲线公钥（建议使用未压缩公钥 65 字节，0x04||X||Y）
    bytes party1Key;   // 可存放 65 字节未压缩公钥，或其他格式
    bytes party2Key;   // 可存放 65 字节未压缩公钥，或其他格式
    // 业务侧冗余保存 escrowId（虽然 mapping key 已是 escrowId，但为了签名原文拼装方便）
    bytes32 escrowIdCopy;
    // 业务自定义 hash（例如对业务负载/证明数据做 sha256/keccak256），后续 confirm 可校验对 escrowId||hash 的签名
    bytes32 dataHash;
  }

  // 每个合约实例对应一个固定面额（以 Wei 表示）
  uint256 public denomination;
  
  // A2L ECDSA 验证器合约地址
  A2LECDSAVerifier public verifier;
  
  // 奖励池合约引用
  RewardPool public rewardPool;
  uint256 public constant SERVICE_FEE_PERCENTAGE = 0.2e18;  // 20%服务费
  uint256 public constant PRECISION = 1e18;
  
  mapping(bytes32 => Escrow) public escrows; // key = 业务侧自定义的 escrowId
  
  // Tornado Cash 相关：存储所有 commitments 以防止重复存款
  mapping(bytes32 => bool) public commitments;

  event Opened(bytes32 indexed escrowId, address party1, address party2, uint256 amount, uint256 deadline, bytes32 metaHash);
  event Confirmed(bytes32 indexed escrowId, address confirmer);
  event Released(bytes32 indexed escrowId, address to, uint256 amount);
  event Refunded(bytes32 indexed escrowId, address to, uint256 amount);
  event ServiceFeeCollected(bytes32 indexed escrowId, uint256 serviceFee, uint256 finalAmount);
  // Tornado Cash 相关事件
  event Deposit(bytes32 indexed commitment, uint32 leafIndex, uint256 timestamp);

  constructor(uint256 _denominationWei, address _verifierAddress, uint32 _merkleTreeHeight, address _hasherAddress) public MerkleTreeWithHistory(_merkleTreeHeight, IHasher(_hasherAddress)) {
    require(_denominationWei > 0, "denomination=0");
    require(_verifierAddress != address(0), "verifier=0");
    require(_hasherAddress != address(0), "hasher=0");
    denomination = _denominationWei;
    verifier = A2LECDSAVerifier(_verifierAddress);
  }

  modifier onlyParties(bytes32 escrowId) {
    require(msg.sender == escrows[escrowId].party1 || msg.sender == escrows[escrowId].party2, "not party");
    _;
  }

  // 设置奖励池地址
  function setRewardPool(address _rewardPool) external {
    require(_rewardPool != address(0), "Invalid reward pool address");
    rewardPool = RewardPool(payable(_rewardPool));
  }

  function openEscrow(
    bytes32 escrowId,
    address party1,
    address party2,
    uint256 deadline,
    bytes32 metaHash,
    bytes memory party1Key,
    bytes memory party2Key,
    bytes32 commitment
  ) external payable nonReentrant {
    // ===== Tornado Cash 流程：检查 commitment 是否已存在（防重复）=====
    require(!commitments[commitment], "The commitment has been submitted");
    
    // ===== Tornado Cash 流程：将 commitment 插入 Merkle 树 =====
    uint32 insertedIndex = _insert(commitment);
    
    // ===== Tornado Cash 流程：记录 commitment（防重复）=====
    commitments[commitment] = true;
    
    // ===== 原有 openEscrow 流程 =====
    require(escrows[escrowId].party1 == address(0), "exists");
    require(msg.value == denomination, "wrong amount");
    require(party1 != address(0) && party2 != address(0) && party1 != party2, "bad party");
    require(deadline > block.timestamp, "bad deadline");

    escrows[escrowId] = Escrow({
      party1: party1,
      party2: party2,
      amount: msg.value,
      deadline: deadline,
      released: false,
      metaHash: metaHash,
      party1Key: party1Key,
      party2Key: party2Key,
      escrowIdCopy: escrowId,
      dataHash: bytes32(0)
    });

    emit Opened(escrowId, party1, party2, msg.value, deadline, metaHash);
    // ===== Tornado Cash 事件 =====
    emit Deposit(commitment, insertedIndex, block.timestamp);
  }

  /**
   * @dev 内部函数：处理服务费和转账
   */
  function _processPayment(bytes32 escrowId, uint256 amount, address party2Address, address payable recipient) internal {
    uint256 serviceFee = 0;
    uint256 finalAmount = amount;
    
    if (address(rewardPool) != address(0)) {
      serviceFee = (amount * SERVICE_FEE_PERCENTAGE) / PRECISION;
      finalAmount = amount - serviceFee;
      
      (bool feeOk, ) = address(rewardPool).call{value: serviceFee}("");
      require(feeOk, "Service fee transfer failed");
      
      rewardPool.recordTransaction(party2Address);
      emit ServiceFeeCollected(escrowId, serviceFee, finalAmount);
    }
    
    (bool ok, ) = recipient.call{value: finalAmount}("");
    require(ok, "pay failed");
    emit Released(escrowId, recipient, finalAmount);
  }

  /**
   * @dev 确认托管并释放资金（需要两个签名验证）
   * @param escrowId 托管ID
   * @param r1 party1签名的r值
   * @param s1 party1签名的s值
   * @param v1 party1签名的v值
   * @param r2 party2签名的r值
   * @param s2 party2签名的s值
   * @param v2 party2签名的v值
   * @param party1Address party1的地址（从party1Key计算得出）
   * @param party2Address party2的地址（从party2Key计算得出）
   */
  function confirm(
    bytes32 escrowId,
    bytes32 r1,
    bytes32 s1,
    uint8 v1,
    bytes32 r2,
    bytes32 s2,
    uint8 v2,
    address party1Address,
    address party2Address
  ) external onlyParties(escrowId) {
    Escrow storage e = escrows[escrowId];
    require(!e.released, "released");
    require(e.dataHash != bytes32(0), "dataHash not set");
    
    bytes memory message = abi.encodePacked(e.escrowIdCopy, e.dataHash);
    require(
      verifier.verifyAliceSignature(message, r1, s1, v1, party1Address),
      "party1 signature invalid"
    );
    require(
      verifier.verifyAliceSignature(message, r2, s2, v2, party2Address),
      "party2 signature invalid"
    );
    
    e.released = true;
    uint256 amount = e.amount;
    e.amount = 0;
    
    address payable recipient = payable(address(uint160(e.party2)));
    _processPayment(escrowId, amount, party2Address, recipient);
    
    emit Confirmed(escrowId, msg.sender);
  }

  function refund(bytes32 escrowId) external {
    Escrow storage e = escrows[escrowId];
    require(block.timestamp >= e.deadline, "not expired");
    require(!e.released, "released");
    e.released = true;
    uint256 amount = e.amount;
    e.amount = 0;
    address payable to = payable(address(uint160(e.party1)));
    (bool ok, ) = to.call{value: amount}("");
    require(ok, "refund failed");
    emit Refunded(escrowId, to, amount);
  }

  // 为业务增加：根据托管ID设置 dataHash（仅限托管双方调用）
  function setDataHash(bytes32 escrowId, bytes32 newDataHash) external onlyParties(escrowId) {
    Escrow storage e = escrows[escrowId];
    require(e.party1 != address(0), "escrow not exist");
    require(!e.released, "released");
    // 若需要仅能设置一次，可启用以下限制
    require(e.dataHash == bytes32(0), "already set");
    e.dataHash = newDataHash;
  }
}
