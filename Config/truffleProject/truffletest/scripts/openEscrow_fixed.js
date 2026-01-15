const fs = require('fs');
const path = require('path');
const FixedMixerEscrow = artifacts.require("FixedMixerEscrow");

module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), {
      string: ['contract', 'pool', 'id', 'p1', 'p2', 'deadline', 'meta', 'from', 'p1key', 'p2key', 'commitment']
    });

    const isHex = (v, len) => /^0x[0-9a-fA-F]+$/.test(v) && (!len || v.length === len);
    const isAddr = (v) => /^0x[0-9a-fA-F]{40}$/.test(v);

    let contractAddr = argv.contract ? String(argv.contract) : '';
    let pool = argv.pool ? String(argv.pool) : '';

    if (!contractAddr && pool) {
      // 兼容 0.1 与 0_1
      if (pool === '0_1') pool = '0.1';
      const abPath = path.join(__dirname, 'deployed-addresses.json');
      if (!fs.existsSync(abPath)) throw new Error('address book not found: ' + abPath);
      const book = JSON.parse(fs.readFileSync(abPath, 'utf8'));
      const pools = book.pools || {};
      contractAddr = pools[pool] || pools['0_1'];
      if (!contractAddr) throw new Error('pool not found in address book: ' + pool);
    }

    const escrowId = String(argv.id || '');
    const p1 = String(argv.p1 || '');
    const p2 = String(argv.p2 || '');
    let deadline = argv.deadline ? Number(argv.deadline) : Math.floor(Date.now()/1000) + 3600;
    const meta = String(argv.meta || '');
    const fromAddr = String(argv.from || '');
    const commitment = String(argv.commitment || '');

    if (!isAddr(contractAddr)) throw new Error('invalid contract address');
    if (!isHex(escrowId)) throw new Error('invalid escrow id');
    if (![p1,p2,fromAddr].every(isAddr)) throw new Error('invalid address in args');
    if (!commitment || !isHex(commitment, 66)) throw new Error('invalid commitment (must be 0x + 64 hex chars)');

    const toBytes32 = (v) => {
      const body = v.replace(/^0x/, '');
      if (body.length === 64) return '0x' + body.toLowerCase();
      if (body.length < 64) return '0x' + body.padStart(64, '0').toLowerCase();
      return '0x' + body.slice(-64).toLowerCase();
    };

    const normUncompressedKey = (k) => {
      if (!k) return '0x';
      let raw = String(k).replace(/^0x/, '');
      if (!raw.startsWith('04')) throw new Error('uncompressed pubkey must start with 0x04');
      // 不做任何长度处理，直接返回原格式（与 test_a2l_verifier.js 一致）
      // 合约中使用 bytes 类型，可以接受任意长度的公钥
      return '0x' + raw;
    };

    // 校验并调整 deadline（避免 bad deadline）
    const latest = await web3.eth.getBlock('latest');
    const chainNow = Number(latest.timestamp);
    if (!(deadline > chainNow + 5)) {
      const old = deadline;
      deadline = chainNow + 3600;
      console.log(`[DEBUG] Adjust deadline: input=${old}, chainNow=${chainNow} -> use ${deadline}`);
    }

    const inst = await FixedMixerEscrow.at(contractAddr);
    const denom = await inst.denomination();

    console.log('[DEBUG] openEscrow_fixed params:', {
      contractAddr,
      escrowId: toBytes32(escrowId),
      p1, p2,
      deadline,
      meta: toBytes32(meta),
      denom: denom.toString(),
      fromAddr,
      p1key: argv.p1key ? String(argv.p1key) : undefined,
      p2key: argv.p2key ? String(argv.p2key) : undefined,
      commitment: toBytes32(commitment)
    });

    const p1KeyBytes = argv.p1key ? normUncompressedKey(String(argv.p1key)) : '0x';
    const p2KeyBytes = argv.p2key ? normUncompressedKey(String(argv.p2key)) : '0x';

    const tx = await inst.openEscrow(
      toBytes32(escrowId), p1, p2, deadline, toBytes32(meta), p1KeyBytes, p2KeyBytes, toBytes32(commitment),
      { from: fromAddr, value: denom.toString() }
    );
    
    // 保存 deposit 交易哈希到文件（用于快速重建 Merkle 树）
    const depositTxHashFile = path.join(__dirname, '..', 'deposit_tx_hashes.json');
    let depositTxHashes = [];
    try {
      if (fs.existsSync(depositTxHashFile)) {
        depositTxHashes = JSON.parse(fs.readFileSync(depositTxHashFile, 'utf8'));
      }
    } catch (e) {
      console.error(`Warning: Failed to read deposit tx hashes file: ${e.message}`);
    }
    
    // 添加新的交易哈希（如果不存在）
    if (!depositTxHashes.includes(tx.tx)) {
      depositTxHashes.push(tx.tx);
      try {
        fs.writeFileSync(depositTxHashFile, JSON.stringify(depositTxHashes, null, 2), 'utf8');
        console.error(`[DEPOSIT] Saved txHash to deposit_tx_hashes.json: ${tx.tx}`);
      } catch (e) {
        console.error(`Warning: Failed to write deposit tx hashes file: ${e.message}`);
      }
    }
    
    console.log(JSON.stringify({ txHash: tx.tx, contract: contractAddr }));
    callback();
  } catch (err) {
    console.error(err);
    callback(err);
  }
};
