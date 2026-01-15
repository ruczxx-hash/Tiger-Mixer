const fs = require('fs');
const path = require('path');
const SignatureEscrow = artifacts.require("SignatureEscrow");

module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), {
      string: ['contract', 'pool', 'id', 'signature', 'from']
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
    const signature = String(argv.signature || '');
    const fromAddr = String(argv.from || '');

    if (!isAddr(contractAddr)) throw new Error('invalid contract address');
    if (!isHex(escrowId)) throw new Error('invalid escrow id');
    if (!isAddr(fromAddr)) throw new Error('invalid from address');
    if (!signature) throw new Error('signature required');

    const toBytes32 = (v) => {
      const body = v.replace(/^0x/, '');
      if (body.length === 64) return '0x' + body.toLowerCase();
      if (body.length < 64) return '0x' + body.padStart(64, '0').toLowerCase();
      return '0x' + body.slice(-64).toLowerCase();
    };

    console.log('[DEBUG] provideTumblerSignature1 params:', {
      contractAddr,
      escrowId: toBytes32(escrowId),
      signatureLength: signature.length,
      fromAddr
    });

    const inst = await SignatureEscrow.at(contractAddr);
    const tx = await inst.provideTumblerSignature1(toBytes32(escrowId), signature, { from: fromAddr });

    console.log(JSON.stringify({ txHash: tx.tx, contract: contractAddr }));
    callback();
  } catch (err) {
    console.error(err);
    callback(err);
  }
};
