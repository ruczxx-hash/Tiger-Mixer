#!/usr/bin/env node
/**
 * Tornado Cash 密钥生成脚本
 * 功能：
 * 1. 生成 31 字节的随机 nullifier
 * 2. 生成 31 字节的随机 secret
 * 3. 计算 commitment = PedersenHash(nullifier || secret)
 * 4. 计算 nullifierHash = PedersenHash(nullifier)
 * 
 * 输出：JSON 格式
 * {
 *   "nullifier": "0x...",      // 31 字节的十六进制字符串
 *   "secret": "0x...",         // 31 字节的十六进制字符串
 *   "commitment": "0x...",     // 32 字节的十六进制字符串
 *   "nullifierHash": "0x..."   // 32 字节的十六进制字符串
 * }
 */

const crypto = require('crypto');
const snarkjs = require('snarkjs');
const circomlib = require('circomlib');
const bigInt = snarkjs.bigInt;

/** Generate random number of specified byte length */
const rbigint = nbytes => bigInt.leBuff2int(crypto.randomBytes(nbytes));

/** Compute pedersen hash */
const pedersenHash = data => circomlib.babyJub.unpackPoint(circomlib.pedersenHash.hash(data))[0];

/** BigNumber to hex string of specified length */
function toHex(number, length = 32) {
  const str = number instanceof Buffer ? number.toString('hex') : bigInt(number).toString(16);
  return '0x' + str.padStart(length * 2, '0');
}

/**
 * Create deposit object from secret and nullifier
 */
function createDeposit({ nullifier, secret }) {
  const deposit = { nullifier, secret };
  // 计算 commitment 的 preimage：将 nullifier 和 secret 连接在一起
  deposit.preimage = Buffer.concat([deposit.nullifier.leInt2Buff(31), deposit.secret.leInt2Buff(31)]);
  // 计算 commitment = PedersenHash(nullifier || secret)
  deposit.commitment = pedersenHash(deposit.preimage);
  deposit.commitmentHex = toHex(deposit.commitment);
  // 计算 nullifierHash = PedersenHash(nullifier)
  deposit.nullifierHash = pedersenHash(deposit.nullifier.leInt2Buff(31));
  deposit.nullifierHashHex = toHex(deposit.nullifierHash);
  return deposit;
}

// 主函数
function main() {
  try {
    // 生成 31 字节的随机 nullifier
    const nullifier = rbigint(31);
    
    // 生成 31 字节的随机 secret
    const secret = rbigint(31);
    
    // 创建 deposit 对象（计算 commitment 和 nullifierHash）
    const deposit = createDeposit({ nullifier, secret });
    
    // 输出 JSON 格式的结果
    const result = {
      nullifier: toHex(nullifier, 31),
      secret: toHex(secret, 31),
      commitment: deposit.commitmentHex,
      nullifierHash: deposit.nullifierHashHex
    };
    
    console.log(JSON.stringify(result));
  } catch (error) {
    console.error(JSON.stringify({ error: error.message }));
    process.exit(1);
  }
}

main();

