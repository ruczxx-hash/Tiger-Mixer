# VRF Solidity 测试运行指南

本指南将帮助你运行 VRF (Verifiable Random Function) 的测试代码，包括基础测试和与 C 程序集成的测试。

---

## 📋 目录

1. [环境准备](#环境准备)
2. [快速开始](#快速开始)
3. [测试文件说明](#测试文件说明)
4. [运行测试](#运行测试)
5. [常见问题](#常见问题)

---

## 🔧 环境准备

### 1. 安装依赖

在项目根目录下运行：

```bash
cd /home/zxx/Config/vrf-solidity-master
npm install
```

这会安装所有必要的依赖，包括：
- `truffle`: Solidity 开发框架
- `ganache-cli`: 本地以太坊测试网络
- `truffle-assertions`: 测试断言库
- `elliptic-curve-solidity`: 椭圆曲线运算库

### 2. 编译合约

```bash
npm run compile-contracts
# 或者
truffle compile
```

### 3. 启动本地测试网络（可选）

**选项 A：使用 Truffle 内置网络（推荐，更简单）**
```bash
# 不需要额外操作，直接运行测试即可
truffle test
```

**选项 B：使用 Ganache CLI（如需要持久化网络）**
```bash
# 在新终端窗口运行
ganache-cli -p 8545
```

---

## 🚀 快速开始

### 最简单的测试方式

```bash
# 进入项目目录
cd /home/zxx/Config/vrf-solidity-master

# 运行所有测试
npm test

# 或者只运行新创建的简单测试
truffle test test/test_vrf_simple.js
```

---

## 📁 测试文件说明

### 1. `test_vrf_simple.js` ⭐ **推荐新手**

**功能：** 使用预定义的测试向量验证 VRF 功能

**特点：**
- ✅ 不依赖外部程序
- ✅ 快速执行
- ✅ 覆盖所有核心功能

**测试内容：**
- 验证有效的 VRF 证明
- 验证多个不同的证明
- 拒绝无效的证明
- 确定性随机数生成
- 不同输入产生不同随机数
- Gas 消耗分析

### 2. `test_vrf_with_c_generator.js` ⭐ **高级集成测试**

**功能：** 集成 C 程序生成器，测试跨平台兼容性

**特点：**
- 🔗 需要 C 程序支持
- 🌐 测试跨平台兼容性
- 🔄 实时生成 VRF 证明

**前置条件：**
- C 程序路径：`/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test`
- C 程序必须可执行

**测试内容：**
- C 生成 + Solidity 验证
- 多消息验证
- 确定性验证
- 预定义测试向量（备用）

### 3. `vrf.js` （原有测试）

**功能：** 完整的 VRF 库测试套件

**测试内容：**
- 辅助函数测试（decodeProof, decodePoint, computeFastVerifyParams）
- 证明验证测试（verify, fastVerify）
- VRF 哈希输出测试（gammaToHash）

---

## ▶️ 运行测试

### 方式 1: 运行所有测试

```bash
# 运行项目中所有测试文件
npm test

# 或者
truffle test
```

**预期输出：**
```
Contract: VRF
  ✓ should decode a VRF proof from bytes (1)
  ✓ should decode a VRF proof from bytes (2)
  ...
Contract: VRF Simple Test
  ✓ 应该成功验证有效的 VRF 证明
  ✓ 应该成功验证多个不同的有效 VRF 证明
  ...
```

### 方式 2: 运行单个测试文件

**运行简单测试（推荐）：**
```bash
truffle test test/test_vrf_simple.js
```

**运行 C 集成测试：**
```bash
truffle test test/test_vrf_with_c_generator.js
```

**运行原有测试：**
```bash
truffle test test/vrf.js
```

### 方式 3: 运行特定测试用例

```bash
# 使用 grep 过滤特定测试
truffle test test/test_vrf_simple.js --grep "应该成功验证有效"
```

### 方式 4: 使用本地网络运行

```bash
# 先启动 ganache-cli（在另一个终端）
ganache-cli -p 8545

# 然后运行测试
npm run test:local
# 或者
truffle test --network local
```

---

## 📊 测试输出示例

### 简单测试输出示例

```
========================================
   VRF 验证测试 - 使用 verify()
========================================

✅ TestHelperVRF 合约已部署: 0x1234...

Contract: VRF Simple Test
  基础 VRF 验证测试

    --- 测试 1: 验证有效的 VRF 证明 ---
    输入数据:
      公钥: 0x03e30118c907034baf1456063bf7b423...
      证明: 0x03e30118c907034baf1456063bf7b423...
      消息: 0x73616d706c65

    步骤 1: 解码公钥
      公钥 X: e30118c907034baf1456063bf7b42397...
      公钥 Y: 52229bb0b81d4955bff53d8315c24a03...

    步骤 4: 执行 VRF 验证...

    验证结果:
      验证通过: true
      耗时: 124 ms

    ✅ 测试 1 完成：验证成功！
    ✓ 应该成功验证有效的 VRF 证明 (2156ms)

  VRF 随机数生成测试
    ✓ 相同的输入应该产生相同的随机数 (89ms)
    ✓ 不同的证明应该产生不同的随机数 (91ms)

  6 passing (12s)

========================================
          测试总结
========================================
✅ 所有测试通过！
```

---

## 🔍 测试详解

### 测试 1: 基础验证流程

```javascript
// 1. 解码公钥（从压缩格式 33 字节）
const publicKey = await helper.decodePoint.call(publicKeyBytes);
// 返回: [x坐标, y坐标]

// 2. 解码证明（从 81 字节）
const proof = await helper.decodeProof.call(proofBytes);
// 返回: [gamma_x, gamma_y, c, s]

// 3. 准备消息
const message = web3.utils.hexToBytes(messageHex);

// 4. 验证证明
const result = await helper.verify.call(publicKey, proof, message);
// 返回: true/false

// 5. 提取随机数
const random = await helper.gammaToHash.call(proof[0], proof[1]);
// 返回: bytes32 随机数哈希
```

### 测试 2: C 程序集成

```javascript
// 1. 调用 C 程序生成 VRF
const vrfData = generateVRFFromC(message);
// 返回: { publicKey, proof, random, message }

// 2. 在 Solidity 中验证
const result = await helper.verify.call(publicKey, proof, message);

// 3. 对比随机数
const contractRandom = await helper.gammaToHash.call(proof[0], proof[1]);
assert.equal(contractRandom, vrfData.random);
```

---

## 🎯 C 程序集成测试的额外步骤

如果要运行 `test_vrf_with_c_generator.js`，需要确保：

### 1. 检查 C 程序是否存在

```bash
ls -la /home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test
```

### 2. 确保 C 程序可执行

```bash
chmod +x /home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test
```

### 3. 测试 C 程序

```bash
/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test "" "test_message"
```

**预期输出：**
```
序列化公钥: 03xxxxxxxxxxxx...
证明: 03xxxxxxxxxxxx...
随机数输出: xxxxxxxxxxxxxxxx...
```

### 4. 运行集成测试

```bash
truffle test test/test_vrf_with_c_generator.js
```

**注意：** 如果 C 程序不存在，测试会自动跳过 C 相关测试，只运行预定义测试向量。

---

## 📈 Gas 消耗分析

### 运行 Gas 分析

```bash
npm run gas-analysis
```

### 预期 Gas 消耗（参考值）

| 函数 | Gas 消耗 (平均) | 说明 |
|------|----------------|------|
| `verify()` | ~1,643,712 | 完整验证（高 Gas） |
| `fastVerify()` | ~150,715 | 快速验证（推荐） |
| `decodeProof()` | ~56,851 | 解码证明 |
| `decodePoint()` | ~55,867 | 解码公钥 |
| `computeFastVerifyParams()` | ~1,611,989 | 计算快速验证参数 |
| `gammaToHash()` | ~24,198 | 生成随机数哈希 |

**建议：**
- ✅ 在链外计算 `computeFastVerifyParams()`
- ✅ 在合约中使用 `fastVerify()` 节省 91% Gas

---

## ❓ 常见问题

### 问题 1: `Error: Cannot find module 'truffle'`

**解决方案：**
```bash
npm install
# 或者全局安装
npm install -g truffle
```

### 问题 2: `Error: CompileError: contracts/VRF.sol:5:1`

**原因：** 合约未编译或依赖缺失

**解决方案：**
```bash
npm install
npm run compile-contracts
```

### 问题 3: `Error: No network specified`

**解决方案：**
```bash
# 方式 1: 使用默认网络
truffle test

# 方式 2: 指定网络
truffle test --network local
```

### 问题 4: C 程序测试失败

**可能原因：**
- C 程序路径不正确
- C 程序未编译
- C 程序无执行权限

**解决方案：**
```bash
# 检查文件是否存在
ls -la /home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test

# 编译 C 程序（如果需要）
cd /home/zxx/A2L/A2L-master/ecdsa
make

# 添加执行权限
chmod +x /home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test

# 测试运行
/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test "" "test"
```

### 问题 5: `Error: Timeout of 2000ms exceeded`

**原因：** 测试超时（椭圆曲线运算耗时较长）

**解决方案：**
在 `truffle-config.js` 中增加超时时间：
```javascript
mocha: {
  timeout: 30000  // 30 秒
}
```

---

## 🎓 测试最佳实践

### 1. 开发流程

```bash
# 1. 修改代码
vim contracts/VRF.sol

# 2. 编译
truffle compile

# 3. 快速测试
truffle test test/test_vrf_simple.js

# 4. 完整测试
npm test

# 5. Gas 分析
npm run gas-analysis
```

### 2. 持续集成

如果设置了 CI/CD，可以在 `.travis.yml` 或 GitHub Actions 中：

```yaml
script:
  - npm install
  - npm run compile-contracts
  - npm test
```

### 3. 调试技巧

**启用详细日志：**
```bash
# 查看详细的 Truffle 输出
truffle test --show-events

# 查看详细的 Solidity 堆栈跟踪
truffle test --stacktrace
```

**使用 Truffle Console 交互调试：**
```bash
truffle console
> const helper = await TestHelperVRF.deployed()
> const proof = await helper.decodeProof.call("0x03...")
> console.log(proof)
```

---

## 📚 参考资源

- **VRF 标准：** [VRF-draft-04](https://tools.ietf.org/pdf/draft-irtf-cfrg-vrf-04)
- **Truffle 文档：** [https://trufflesuite.com/docs/](https://trufflesuite.com/docs/)
- **项目仓库：** [https://github.com/witnet/vrf-solidity](https://github.com/witnet/vrf-solidity)
- **测试示例：** `test/vrf-example.md`

---

## ✅ 快速检查清单

运行测试前，确保：

- [ ] 已安装 Node.js (v12+)
- [ ] 已运行 `npm install`
- [ ] 已编译合约 `truffle compile`
- [ ] （可选）C 程序可执行
- [ ] 端口 8545 未被占用（如使用 Ganache）

---

## 🆘 获取帮助

如果遇到问题：

1. **查看日志：** 仔细阅读错误信息
2. **检查版本：** `node --version`, `npm --version`
3. **清理重装：**
   ```bash
   rm -rf node_modules package-lock.json
   npm install
   ```
4. **查看原有测试：** 参考 `test/vrf.js` 的实现

---

## 🎉 开始测试

现在你可以开始测试了！

```bash
cd /home/zxx/Config/vrf-solidity-master
npm install
truffle compile
truffle test test/test_vrf_simple.js
```

祝测试顺利！🚀

















