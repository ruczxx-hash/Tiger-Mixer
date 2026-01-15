# 私有链日志查询优化配置

## 问题

私有链默认配置可能不支持高效的日志查询（`getPastEvents`/`getPastLogs`），导致：
- 查询超时
- 查询失败
- 无法重建 Merkle 树

## 解决方案

### 1. 使用 Archive 模式（推荐）

**关键配置：`--gcmode archive`**

```bash
geth \
    --gcmode archive \
    --rpc \
    --rpcapi eth,net,web3,admin,personal,miner,debug \
    ...
```

**优点：**
- 保留所有历史数据（包括所有日志）
- 支持查询任意历史区块的日志
- 与以太坊主网行为一致

**缺点：**
- 占用更多磁盘空间（可能增长很快）
- 启动时间稍长

### 2. 使用 Full 模式（折中方案）

```bash
geth \
    --gcmode full \
    --rpc \
    --rpcapi eth,net,web3,admin,personal,miner,debug \
    ...
```

**优点：**
- 保留最近的状态数据
- 磁盘占用比 archive 模式小

**缺点：**
- 可能无法查询很早期的日志（取决于状态修剪策略）

### 3. 启用必要的 RPC API

确保启用以下 RPC API：
- `eth`: 基本以太坊 API
- `net`: 网络 API
- `web3`: Web3 API
- `debug`: 调试 API（可选，但有助于排查问题）

```bash
--rpcapi eth,net,web3,admin,personal,miner,debug
```

## 配置对比

| 配置项 | 默认（dev模式） | 优化后 |
|--------|----------------|--------|
| `--gcmode` | 未设置（可能使用默认修剪） | `archive` |
| `--rpcapi` | `eth,net,web3,admin,personal` | `eth,net,web3,admin,personal,miner,debug` |
| 日志查询 | 可能失败/超时 | 支持完整查询 |
| 磁盘占用 | 较小 | 较大（archive模式） |

## 使用方法

### 方法1：使用优化后的启动脚本

```bash
cd /home/zxx/Config/blockchain/consortium_blockchain
./start_chain_optimized.sh          # 前台运行
# 或
./start_chain_optimized_background.sh  # 后台运行
```

### 方法2：手动启动

```bash
geth \
    --identity "myethereum" \
    --dev \
    --dev.period 1 \
    --gcmode archive \
    --rpc \
    --rpcaddr 127.0.0.1 \
    --port 30304 \
    --rpcport 7545 \
    --rpccorsdomain "*" \
    --datadir ./myblockchain \
    --rpcapi eth,net,web3,admin,personal,miner,debug \
    --mine \
    --miner.threads 1 \
    --networkid 5777 \
    --allow-insecure-unlock \
    console
```

## 验证配置

启动后，测试日志查询：

```bash
# 测试 getPastEvents
cd /home/zxx/tornado-core-master
node scripts/query_deposit_events.js --contract <合约地址> --fromBlock 0
```

如果查询成功且快速，说明配置正确。

## 注意事项

1. **磁盘空间**：Archive 模式会占用大量磁盘空间，确保有足够的空间
2. **性能**：Archive 模式可能影响同步速度，但对于私有链通常不是问题
3. **迁移**：如果已有链数据，需要重新初始化才能启用 archive 模式（或使用 `geth export/import`）

## 如果无法使用 Archive 模式

如果磁盘空间有限，可以：
1. 使用 Full 模式（`--gcmode full`）
2. 在代码中使用分块查询策略（已实现）
3. 维护交易哈希列表，通过交易收据查询（已实现）

## 参考

- Geth 文档：https://geth.ethereum.org/docs/
- Archive 模式说明：https://geth.ethereum.org/docs/interface/archival-node

