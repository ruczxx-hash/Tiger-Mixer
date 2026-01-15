#!/bin/bash
# A2L 系统角色耗时分析脚本
# 分析 alice、bob、tumbler 的运行耗时，排除区块链交互和网络传输时间

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "=========================================="
echo "A2L 系统角色耗时分析"
echo "=========================================="
echo ""

# 检查是否有日志文件或输出文件
LOG_DIR="../logs"
TIMING_OUTPUT="timing_output.txt"

echo -e "${BLUE}正在查找时间测量数据...${NC}"

# 需要排除的计时器（区块链交互和网络传输）
EXCLUDE_TIMERS=(
    "alice_blockchain_escrow_interaction"
    "alice_blockchain_interaction"
    "bob_blockchain_escrow_interaction"
    "tumbler_blockchain_escrow_interaction"
    "call_http_zk_service"
    "http_zk_service"
)

# Alice 相关的计算计时器（需要保留）
ALICE_COMPUTATION_TIMERS=(
    "alice_initialization_computation"
    "alice_registration_total"
    "alice_token_share_computation"
    "alice_payment_init_computation"
    "alice_puzzle_solution_share_computation"
    "alice_second_puzzle_randomization"
    "alice_zk_proof_generation"
    "alice_ecdsa_signing"
    "alice_secret_extraction"
    "bob_to_alice_zk_verification"
)

# Bob 相关的计算计时器（需要保留）
BOB_COMPUTATION_TIMERS=(
    "bob_puzzle_randomization"
    "bob_zk_proof_generation"
    "tumbler_to_bob_zk_verification"
)

# Tumbler 相关的计算计时器（需要保留）
TUMBLER_COMPUTATION_TIMERS=(
    "tumbler_secret_share_phase2"
    "tumbler_zk_proof_generation"
    "tumbler_commitment_generation"
    "tumbler_verification"
)

echo ""
echo -e "${GREEN}分析说明：${NC}"
echo "  ✅ 包括: 所有密码学计算时间（zk 证明生成、验证、随机化、签名等）"
echo "  ❌ 不包括: 区块链交互时间（truffle exec 脚本执行）"
echo "  ❌ 不包括: 网络传输时间（HTTP curl 调用）"
echo "  ✅ 包括: ZMQ 本地进程间通信（这是本地通信，不是网络传输）"
echo ""

echo "=========================================="
echo "建议的测试方法："
echo "=========================================="
echo ""
echo "1. 运行 A2L 系统，确保所有角色都正常执行"
echo "2. 查看程序输出的 [TIMER] 日志"
echo "3. 检查是否有以下计时器："
echo ""
echo -e "${YELLOW}Alice 计算计时器：${NC}"
for timer in "${ALICE_COMPUTATION_TIMERS[@]}"; do
    echo "  - $timer"
done
echo ""
echo -e "${YELLOW}Bob 计算计时器：${NC}"
for timer in "${BOB_COMPUTATION_TIMERS[@]}"; do
    echo "  - $timer"
done
echo ""
echo -e "${YELLOW}Tumbler 计算计时器：${NC}"
for timer in "${TUMBLER_COMPUTATION_TIMERS[@]}"; do
    echo "  - $timer"
done
echo ""
echo -e "${RED}需要排除的计时器（区块链/网络）：${NC}"
for timer in "${EXCLUDE_TIMERS[@]}"; do
    echo "  - $timer"
done
echo ""
echo "=========================================="
echo ""

# 如果存在日志文件，尝试解析
if [ -f "$TIMING_OUTPUT" ]; then
    echo -e "${GREEN}找到时间输出文件: $TIMING_OUTPUT${NC}"
    echo ""
    echo "正在分析..."
    # 这里可以添加解析逻辑
fi

echo ""
echo "提示："
echo "  - 程序运行时会输出 [TIMER] 开头的日志"
echo "  - 程序结束时会有时间测量总结"
echo "  - 可以手动记录各个计时器的值"
echo ""







