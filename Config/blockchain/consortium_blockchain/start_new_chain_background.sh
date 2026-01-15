#!/bin/bash
# 在后台启动新链

echo "=== 在后台启动新链 ==="
echo ""

# 停止旧进程
pkill -f "geth.*tornado_chain" 2>/dev/null
sleep 2

# 启动链
cd "$(dirname "$0")"
nohup geth \
    --identity "tornado-test" \
    --dev \
    --dev.period 1 \
    --rpc \
    --rpcaddr 127.0.0.1 \
    --port 30305 \
    --rpcport 8546 \
    --rpccorsdomain "*" \
    --datadir ./tornado_chain \
    --rpcapi eth,net,web3,admin,personal,miner \
    --mine \
    --miner.threads 1 \
    --networkid 5778 \
    --allow-insecure-unlock \
    > tornado_chain.log 2>&1 &

echo "✅ 链已在后台启动"
echo "日志文件: tornado_chain.log"
echo ""
echo "等待几秒钟后检查状态："
echo "  cd /home/zxx/tornado-core-master"
echo "  RPC_URL=http://127.0.0.1:8546 node scripts/check_chain_status.js"
