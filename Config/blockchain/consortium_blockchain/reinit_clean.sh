#!/bin/bash
echo "=== 清理并重新初始化链 ==="
echo ""
echo "⚠️  警告：这会删除链上所有数据！"
echo ""
read -p "确认要继续吗？(yes/no): " confirm
if [ "$confirm" != "yes" ]; then
    echo "已取消"
    exit 1
fi

echo ""
echo "1. 停止 geth 进程..."
pkill -f 'geth.*myblockchain' || echo "   (没有运行的 geth 进程)"

echo ""
echo "2. 备份链数据（可选）..."
if [ -d "myblockchain/geth" ]; then
    BACKUP_DIR="myblockchain_backup_$(date +%Y%m%d_%H%M%S)"
    echo "   备份到: $BACKUP_DIR"
    cp -r myblockchain/geth "$BACKUP_DIR" 2>/dev/null || echo "   (备份失败，继续执行)"
fi

echo ""
echo "3. 清理链数据..."
rm -rf myblockchain/geth/chaindata/*
rm -rf myblockchain/geth/lightchaindata/*
echo "   ✅ 链数据已清理"

echo ""
echo "4. 重新初始化链..."
geth --datadir ./myblockchain init genesis.json

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 链重新初始化成功！"
    echo ""
    echo "下一步：重新启动链"
else
    echo ""
    echo "❌ 初始化失败"
    exit 1
fi
