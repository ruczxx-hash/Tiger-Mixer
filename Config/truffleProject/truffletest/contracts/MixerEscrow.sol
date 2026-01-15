// SPDX-License-Identifier: MIT  no use
pragma solidity ^0.6.12;

import "./RewardPool.sol";

contract MixerEscrow {
  struct Escrow {
    address party1;                 // 例如 Sender 或 Hub
    address party2;                 // 例如 Tumbler 或 Receiver
    address payable beneficiary;    // 确认成功后资金接收者（用于 M1 或 M2）
    address payable refundTo;       // 超时退款地址（通常是存入方）
    uint256 amount;
    uint256 deadline;               // 到期时间戳
    bool p1Confirmed;
    bool p2Confirmed;
    bool released;
    bytes32 metaHash;               // 存承诺/会话id等的哈希(pz/t1/t2等的聚合哈希)
  }

  mapping(bytes32 => Escrow) public escrows; // key = 业务侧自定义的 escrowId

  // 奖励池合约引用
  RewardPool public rewardPool;
  address public committeeRotation;  // 委员会轮换合约（用于验证成员身份）
  uint256 public constant SERVICE_FEE_PERCENTAGE = 0.2e18;  // 20%服务费
  uint256 public constant PRECISION = 1e18;

  event Opened(bytes32 indexed escrowId, address party1, address party2, uint256 amount, uint256 deadline, bytes32 metaHash);
  event Confirmed(bytes32 indexed escrowId, address confirmer);
  event Released(bytes32 indexed escrowId, address to, uint256 amount);
  event Refunded(bytes32 indexed escrowId, address to, uint256 amount);
  event ServiceFeeCollected(bytes32 indexed escrowId, uint256 serviceFee, uint256 finalAmount);

  modifier onlyParties(bytes32 escrowId) {
    require(msg.sender == escrows[escrowId].party1 || msg.sender == escrows[escrowId].party2, "not party");
    _;
  }

  // 设置奖励池地址
  function setRewardPool(address _rewardPool) external {
    require(_rewardPool != address(0), "Invalid reward pool address");
    rewardPool = RewardPool(payable(_rewardPool));
  }
  
  // 设置委员会轮换合约地址
  function setCommitteeRotation(address _committeeRotation) external {
    require(_committeeRotation != address(0), "Invalid committee rotation address");
    committeeRotation = _committeeRotation;
  }

  function openEscrow(
    bytes32 escrowId,
    address party1,
    address party2,
    address payable beneficiary,
    address payable refundTo,
    uint256 deadline,
    bytes32 metaHash
  ) external payable {
    require(escrows[escrowId].amount == 0, "exists");
    require(msg.value > 0, "no value");
    escrows[escrowId] = Escrow({
      party1: party1,
      party2: party2,
      beneficiary: beneficiary,
      refundTo: refundTo,
      amount: msg.value,
      deadline: deadline,
      p1Confirmed: false,
      p2Confirmed: false,
      released: false,
      metaHash: metaHash
    });
    emit Opened(escrowId, party1, party2, msg.value, deadline, metaHash);
  }

  function confirm(bytes32 escrowId) external onlyParties(escrowId) {
    Escrow storage e = escrows[escrowId];
    require(!e.released, "released");
    if (msg.sender == e.party1) e.p1Confirmed = true;
    if (msg.sender == e.party2) e.p2Confirmed = true;

    emit Confirmed(escrowId, msg.sender);

    if (e.p1Confirmed && e.p2Confirmed) {
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
        
        // 记录交易参与
        // 假设 party2 是委员会成员（Tumbler/Receiver），实际应该根据业务逻辑确定
        // 如果设置了委员会轮换合约，可以通过接口验证
        rewardPool.recordTransaction(e.party2);
        
        emit ServiceFeeCollected(escrowId, serviceFee, finalAmount);
      }
      
      // 将剩余金额转给受益人
      (bool ok, ) = e.beneficiary.call{value: finalAmount}("");
      require(ok, "pay failed");
      emit Released(escrowId, e.beneficiary, finalAmount);
    }
  }

  function refund(bytes32 escrowId) external {
    Escrow storage e = escrows[escrowId];
    require(block.timestamp >= e.deadline, "not expired");
    require(!e.released, "released");
    e.released = true;
    uint256 amount = e.amount;
    e.amount = 0;
    (bool ok, ) = e.refundTo.call{value: amount}("");
    require(ok, "refund failed");
    emit Refunded(escrowId, e.refundTo, amount);
  }
}