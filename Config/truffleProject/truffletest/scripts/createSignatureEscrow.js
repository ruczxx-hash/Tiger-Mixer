const fs = require('fs');
const path = require('path');
const SignatureEscrow = artifacts.require("SignatureEscrow");

module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), {
      string: ['contract', 'pool', 'id', 'tumbler', 'beneficiary', 'deadline', 'alicePubKey', 'tumblerPubKey', 'from']
    });

    const isHex = (v, len) => /^0x[0-9a-fA-F]+$/.test(v) && (!len || v.length === len);
    const isAddr = (v) => /^0x[0-9a-fA-F]{40}$/.test(v);

    let contractAddr = argv.contract ? String(argv.contract) : '';
    let pool = argv.pool ? String(argv.pool) : '';

    if (!contractAddr && pool) {
      if (pool === '0_1') pool = '0.1';
      const abPath = path.join(__dirname, 'deployed-addresses.json');
      if (!fs.existsSync(abPath)) throw new Error('address book not found: ' + abPath);
      const book = JSON.parse(fs.readFileSync(abPath, 'utf8'));
      const pools = book.pools || {};
      contractAddr = pools[pool] || pools['0_1'];
      if (!contractAddr) throw new Error('pool not found in address book: ' + pool);
    }

    const escrowId = String(argv.id || '');
    const tumbler = String(argv.tumbler || '');
    const beneficiary = String(argv.beneficiary || '');
    let deadline = argv.deadline ? Number(argv.deadline) : Math.floor(Date.now()/1000) + 3600;
    const alicePubKey = String(argv.alicePubKey || '');
    const tumblerPubKey = String(argv.tumblerPubKey || '');
    const fromAddr = String(argv.from || '');

    if (!isAddr(contractAddr)) throw new Error('invalid contract address');
    if (!isHex(escrowId)) throw new Error('invalid escrow id');
    if (![tumbler, beneficiary, fromAddr].every(isAddr)) throw new Error('invalid address in args');
    if (!alicePubKey || !tumblerPubKey) throw new Error('public keys required');

    const toBytes32 = (v) => {
      const body = v.replace(/^0x/, '');
      if (body.length === 64) return '0x' + body.toLowerCase();
      if (body.length < 64) return '0x' + body.padStart(64, '0').toLowerCase();
      return '0x' + body.slice(-64).toLowerCase();
    };

    // 校验并调整 deadline
    const latest = await web3.eth.getBlock('latest');
    const chainNow = Number(latest.timestamp);
    if (!(deadline > chainNow + 5)) {
      const old = deadline;
      deadline = chainNow + 3600;
      console.log(`[DEBUG] Adjust deadline: input=${old}, chainNow=${chainNow} -> use ${deadline}`);
    }

    const inst = await SignatureEscrow.at(contractAddr);
    const denom = await inst.denomination();

    // 预检查：是否已存在该 escrowId
    let existingAlice = '0x0000000000000000000000000000000000000000';
    try {
      const info = await inst.getEscrowInfo(toBytes32(escrowId));
      existingAlice = String(info[0]);
    } catch (_) {}

    console.log('[DEBUG] createSignatureEscrow params:', {
      contractAddr,
      escrowId: toBytes32(escrowId),
      tumbler,
      beneficiary,
      deadline,
      alicePubKeyLength: alicePubKey.length,
      tumblerPubKeyLength: tumblerPubKey.length,
      denom: denom.toString(),
      fromAddr
    });

    // 预估 Gas，尽早发现 revert
    try {
      await inst.createEscrow.estimateGas(
        toBytes32(escrowId),
        tumbler,
        beneficiary,
        deadline,
        alicePubKey,
        tumblerPubKey,
        { from: fromAddr, value: denom.toString() }
      );
    } catch (e) {
      console.error('[PRECHECK] estimateGas failed. Possible reasons:');
      if (existingAlice && existingAlice !== '0x0000000000000000000000000000000000000000') {
        console.error(' - escrowId already exists on-chain');
      }
      console.error(' - msg.value must equal denomination');
      console.error(' - deadline must be in the future');
      console.error(' - tumbler must be a valid address and not equal to from');
      console.error(' - public keys must be non-empty hex bytes');
      throw e;
    }

    const tx = await inst.createEscrow(
      toBytes32(escrowId),
      tumbler,
      beneficiary,
      deadline,
      alicePubKey,
      tumblerPubKey,
      { from: fromAddr, value: denom.toString() }
    );
    console.log(JSON.stringify({ txHash: tx.tx, contract: contractAddr }));
    callback();
  } catch (err) {
    console.error(err);
    callback(err);
  }
};
