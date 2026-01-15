const fs = require('fs');
const path = require('path');
const FixedMixerEscrow = artifacts.require("FixedMixerEscrow");

module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), {
      string: ['contract', 'pool', 'id', 'hash', 'from']
    });

    const isHex = (v, len) => /^0x[0-9a-fA-F]+$/.test(v) && (!len || v.length === len);

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
    const dataHash = String(argv.hash || '');
    const fromAddr = String(argv.from || '');

    if (!/^0x[0-9a-fA-F]{40}$/.test(contractAddr)) throw new Error('invalid contract address');
    if (!isHex(escrowId)) throw new Error('invalid escrow id');
    if (!isHex(dataHash, 66)) throw new Error('invalid data hash, expect 0x + 64 hex');
    if (!/^0x[0-9a-fA-F]{40}$/.test(fromAddr)) throw new Error('invalid from address');

    const toBytes32 = (v) => {
      const body = v.replace(/^0x/, '');
      if (body.length === 64) return '0x' + body.toLowerCase();
      if (body.length < 64) return '0x' + body.padStart(64, '0').toLowerCase();
      return '0x' + body.slice(-64).toLowerCase();
    };

    const inst = await FixedMixerEscrow.at(contractAddr);
    const tx = await inst.setDataHash(toBytes32(escrowId), toBytes32(dataHash), { from: fromAddr });
    console.log(JSON.stringify({ txHash: tx.tx, contract: contractAddr, action: 'setDataHash' }));
    callback();
  } catch (err) {
    console.error(err);
    callback(err);
  }
};


