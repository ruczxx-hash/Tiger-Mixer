#!/bin/bash
# 启动新链用于 Tornado Cash 测试

echo "=== 启动新链（Tornado Cash 测试） ==="
echo ""

# 新链的配置
NEW_CHAIN_DIR="./tornado_chain"
NEW_CHAIN_PORT=8546  # 使用不同的端口
NEW_CHAIN_RPC_PORT=8546

# 检查链是否已初始化
if [ ! -d "$NEW_CHAIN_DIR/geth" ]; then
    echo "1. 初始化新链..."
    mkdir -p "$NEW_CHAIN_DIR"
    geth --datadir "$NEW_CHAIN_DIR" init genesis.json
    if [ $? -ne 0 ]; then
        echo "❌ 初始化失败"
        exit 1
    fi
    echo "✅ 链已初始化"
else
    echo "链已存在，跳过初始化"
fi

echo ""
echo "2. 启动新链..."
echo "   RPC 端口: $NEW_CHAIN_RPC_PORT"
echo "   数据目录: $NEW_CHAIN_DIR"
echo ""

# 启动链（dev 模式，自动挖矿）
# 注意：dev 模式应该自动挖矿，但如果账户余额仍为 0，需要在 console 中执行 miner.start()
geth \
    --identity "tornado-test" \
    --dev \
    --dev.period 1 \
    --rpc \
    --rpcaddr 127.0.0.1 \
    --port 30305 \
    --rpcport $NEW_CHAIN_RPC_PORT \
    --rpccorsdomain "*" \
    --datadir "$NEW_CHAIN_DIR" \
    --rpcapi eth,net,web3,admin,personal,miner \
    --mine \
    --miner.threads 1 \
    --networkid 5778 \
    console \
    --allow-insecure-unlock

