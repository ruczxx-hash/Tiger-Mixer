#!/bin/bash
# 启动优化后的私有链（支持日志查询）
# 这个配置启用了日志索引，支持高效的 getPastEvents/getPastLogs 查询

echo "=== 启动优化后的私有链（支持日志查询） ==="
echo ""

# 链的配置
CHAIN_DIR="./myblockchain"
CHAIN_PORT=30304
CHAIN_RPC_PORT=7545
NETWORK_ID=5777

# 检查链是否已初始化
if [ ! -d "$CHAIN_DIR/geth" ]; then
    echo "1. 初始化链..."
    mkdir -p "$CHAIN_DIR"
    # 如果有 genesis.json，使用它初始化
    if [ -f "genesis.json" ]; then
        geth --datadir "$CHAIN_DIR" init genesis.json
    else
        echo "⚠️  未找到 genesis.json，将使用默认配置"
    fi
    echo "✅ 链已初始化"
else
    echo "链已存在，跳过初始化"
fi

echo ""
echo "2. 启动优化后的链..."
echo "   RPC 端口: $CHAIN_RPC_PORT"
echo "   数据目录: $CHAIN_DIR"
echo "   网络 ID: $NETWORK_ID"
echo ""

# 启动链（优化配置）
# 关键配置说明：
# --gcmode archive: 保留所有历史数据（包括日志），不进行状态修剪
#                   这是最重要的配置，确保可以查询所有历史日志
# --rpcapi: 启用所有必要的 RPC API，包括 eth_getLogs
# --dev: 开发模式，自动挖矿
# --dev.period 1: 每 1 秒挖一个区块
geth \
    --identity "myethereum" \
    --dev \
    --dev.period 1 \
    --gcmode archive \
    --rpc \
    --rpcaddr 127.0.0.1 \
    --port $CHAIN_PORT \
    --rpcport $CHAIN_RPC_PORT \
    --rpccorsdomain "*" \
    --datadir "$CHAIN_DIR" \
    --rpcapi eth,net,web3,admin,personal,miner,debug \
    --mine \
    --miner.threads 1 \
    --networkid $NETWORK_ID \
    --allow-insecure-unlock \
    console

echo ""
echo "✅ 链已启动"
echo ""
echo "配置说明："
echo "  --gcmode archive: 保留所有历史数据，支持完整日志查询"
echo "  --rpcapi: 启用了所有必要的 RPC API"
echo ""
echo "注意："
echo "  - archive 模式会占用更多磁盘空间，但支持完整的日志查询"
echo "  - 如果磁盘空间有限，可以使用 --gcmode full（保留最近的状态）"
echo "  - 但 full 模式可能无法查询很早期的日志"

