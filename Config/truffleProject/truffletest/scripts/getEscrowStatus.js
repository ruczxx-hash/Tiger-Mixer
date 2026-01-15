const fs = require('fs');
const path = require('path');
const FixedMixerEscrow = artifacts.require("FixedMixerEscrow");

module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), { string: ['id'] });

    const isHex = (v, len) => /^0x[0-9a-fA-F]+$/.test(v) && (!len || v.length === len);
    const isAddr = (v) => /^0x[0-9a-fA-F]{40}$/.test(v);

    const escrowId = String(argv.id || '');
    if (!escrowId || !isHex(escrowId)) throw new Error('invalid --id (bytes32 hex)');

    const abPath = path.join(__dirname, 'deployed-addresses.json');
    if (!fs.existsSync(abPath)) throw new Error('address book not found: ' + abPath);
    const book = JSON.parse(fs.readFileSync(abPath, 'utf8'));
    const addresses = book.addresses || {};

    const matches = [];
    for (const [label, addr] of Object.entries(addresses)) {
      if (!isAddr(addr)) continue;
      try {
        const inst = await FixedMixerEscrow.at(addr);
        const e = await inst.escrows(escrowId);
        const amountStr = e.amount ? e.amount.toString() : '0';
        const exists = amountStr !== '0' || (e.party1 && e.party1 !== '0x0000000000000000000000000000000000000000');
        if (exists) {
          matches.push({
            pool: label,
            contract: addr,
            party1: e.party1,
            party2: e.party2,
            beneficiary: e.beneficiary,
            refundTo: e.refundTo,
            amount: amountStr,
            deadline: e.deadline ? e.deadline.toString() : '0',
            p1Confirmed: !!e.p1Confirmed,
            p2Confirmed: !!e.p2Confirmed,
            released: !!e.released,
            metaHash: e.metaHash
          });
        }
      } catch (e) {
        // 忽略单池查询异常
      }
    }

    const out = { escrowId, found: matches.length > 0, matches };
    console.log(JSON.stringify(out));
    callback();
  } catch (err) {
    console.error('[getEscrowStatus] ERROR:', err.message);
    callback(err);
  }
};
