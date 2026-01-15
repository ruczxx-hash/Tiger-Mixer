# 轮换间隔设置说明

## 📋 当前状态

### 源代码设置
**文件**: `/home/zxx/Config/truffleProject/truffletest/contracts/CommitteeRotation.sol`

```solidity
uint256 public constant ROTATION_INTERVAL = 20 seconds;  // 轮换间隔：20秒
```

✅ **源代码已设置为 20 秒**

### 已部署合约
**合约地址**: `0x7422B6F8bd5b4d72e3071e6E3166a223a0000f02`

⚠️ **需要检查已部署合约中的实际值**

## 🔧 如何设置轮换间隔为20秒

### 方法1: 重新编译和部署（推荐）

如果已部署的合约中 `ROTATION_INTERVAL` 不是20秒，需要重新部署：

```bash
cd /home/zxx/Config/truffleProject/truffletest

# 1. 确保源代码中设置为20秒
# 检查 contracts/CommitteeRotation.sol 第13行
grep "ROTATION_INTERVAL" contracts/CommitteeRotation.sol

# 2. 重新编译
truffle compile

# 3. 重新部署（注意：这会重置合约状态）
truffle migrate --reset --network private
```

### 方法2: 检查当前部署的合约值

```bash
cd /home/zxx/Config/truffleProject/truffletest

# 使用truffle console检查
truffle console --network private
> const CommitteeRotation = artifacts.require("CommitteeRotation");
> const cr = await CommitteeRotation.at("0x7422B6F8bd5b4d72e3071e6E3166a223a0000f02");
> const interval = await cr.ROTATION_INTERVAL();
> interval.toString()
```

## 📊 轮换间隔的作用

`ROTATION_INTERVAL` 控制两次轮换之间的最小时间间隔：

```solidity
function canRotate() public view returns (bool) {
    return block.timestamp >= lastRotationTime + ROTATION_INTERVAL;
}
```

- **如果设置为 20 秒**：上次轮换后至少需要等待 20 秒才能再次轮换
- **如果设置为 60 秒**：上次轮换后至少需要等待 60 秒才能再次轮换

## 🔍 为什么显示"轮换时间未到"

从日志可以看到：
```
是否可以进行轮换: ❌ 否
⏰ 轮换时间未到，还需等待 238 秒
```

这说明：
1. 上次轮换时间 + 轮换间隔 > 当前时间
2. 如果 `ROTATION_INTERVAL = 60秒`，需要等待60秒
3. 如果 `ROTATION_INTERVAL = 20秒`，需要等待20秒

**如果显示需要等待238秒，说明合约中可能仍然是60秒，或者上次轮换时间较久。**

## ✅ 验证设置

### 1. 检查源代码
```bash
grep "ROTATION_INTERVAL" /home/zxx/Config/truffleProject/truffletest/contracts/CommitteeRotation.sol
```

应该显示：`ROTATION_INTERVAL = 20 seconds`

### 2. 检查编译后的合约
```bash
cd /home/zxx/Config/truffleProject/truffletest
truffle compile
```

### 3. 检查已部署的合约
```bash
cd /home/zxx/Config/truffleProject/truffletest
truffle exec -e "const CommitteeRotation = artifacts.require('CommitteeRotation'); module.exports = async function(callback) { const cr = await CommitteeRotation.at('0x7422B6F8bd5b4d72e3071e6E3166a223a0000f02'); const interval = await cr.ROTATION_INTERVAL(); console.log('ROTATION_INTERVAL =', interval.toString(), '秒'); callback(); }" --network private
```

## ⚠️ 注意事项

1. **重新部署会重置合约状态**：
   - 委员会成员会重置为初始值
   - 轮换次数会重置为0
   - 候选池会清空

2. **如果不想重置状态**：
   - 只能等待当前合约的轮换间隔到期
   - 或者修改合约代码，添加一个管理员函数来修改间隔（需要修改合约）

3. **当前部署的合约地址**：
   - `CommitteeRotation`: `0x7422B6F8bd5b4d72e3071e6E3166a223a0000f02`
   - 如果重新部署，地址会改变，需要更新 `update_and_rotate_new.js` 中的地址

## 🎯 快速解决方案

如果只是想快速测试20秒的轮换间隔：

1. **确认源代码是20秒**（已完成）
2. **重新编译**（已完成）
3. **重新部署**（需要执行）：
   ```bash
   cd /home/zxx/Config/truffleProject/truffletest
   truffle migrate --reset --network private
   ```
4. **更新脚本中的合约地址**（如果地址改变）

## 📝 总结

- ✅ 源代码已设置为 20 秒
- ⚠️ 需要检查已部署合约中的实际值
- 🔧 如果需要应用20秒设置，需要重新部署合约




