#!/bin/bash
# 在后台启动优化后的私有链（支持日志查询）

echo "=== 在后台启动优化后的私有链 ==="
echo ""

# 停止旧进程
pkill -f "geth.*myblockchain" 2>/dev/null
sleep 2

# 链的配置
CHAIN_DIR="./myblockchain"
CHAIN_PORT=30304
CHAIN_RPC_PORT=7545
NETWORK_ID=5777

# 启动链（优化配置）
cd "$(dirname "$0")"
nohup geth \
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
    > myblockchain.log 2>&1 &

echo "✅ 链已在后台启动"
echo "日志文件: myblockchain.log"
echo ""
echo "配置说明："
echo "  --gcmode archive: 保留所有历史数据，支持完整日志查询"
echo "  --rpcapi: 启用了所有必要的 RPC API（包括 eth_getLogs）"
echo ""
echo "等待几秒钟后检查状态："
echo "  cd /home/zxx/tornado-core-master"
echo "  RPC_URL=http://127.0.0.1:7545 node scripts/check_chain_status.js"
echo ""
echo "查看日志："
echo "  tail -f myblockchain.log"

