// Usage:
// npx truffle exec scripts/openEscrow_truffle.js \
//   --network development \
//   --contract <MixerEscrow_address> \
//   --id 0x... --p1 0x... --p2 0x... --benef 0x... --refund 0x... \
//   --deadline 1735660800 --meta 0x... --valueEth 0.1

const MixerEscrow = artifacts.require("MixerEscrow");

module.exports = async function (callback) {
  try {
    // 强制将 0x 开头的参数解析为字符串，避免被转成 Number（科学计数法）
    const argv = require('minimist')(process.argv.slice(2), {
      string: [
        'contract', 'id', 'p1', 'p2', 'benef', 'refund', 'meta', 'valueEth', 'deadline', 'from'
      ]
    });

    

    const contractAddr = typeof argv.contract === 'string' ? argv.contract : String(argv.contract || '');
    const escrowId = typeof argv.id === 'string' ? argv.id : String(argv.id || '');
    const p1 = typeof argv.p1 === 'string' ? argv.p1 : String(argv.p1 || '');
    const p2 = typeof argv.p2 === 'string' ? argv.p2 : String(argv.p2 || '');
    const benef = typeof argv.benef === 'string' ? argv.benef : String(argv.benef || '');
    const refund = typeof argv.refund === 'string' ? argv.refund : String(argv.refund || '');
    const deadline = argv.deadline ? Number(argv.deadline) : Math.floor(Date.now()/1000) + 3600;
    const meta = typeof argv.meta === 'string' ? argv.meta : String(argv.meta || '');
    const valueEth = argv.valueEth ? String(argv.valueEth) : '0.1';
    const fromAddr = typeof argv.from === 'string' ? argv.from : String(argv.from || '');

    // 基础校验，尽早发现地址被误解析的情况
    const isHex = (v, len) => /^0x[0-9a-fA-F]+$/.test(v) && (!len || v.length === len);
    const isAddr = (v) => isHex(v, 42);
    if (!isAddr(contractAddr)) {
      throw new Error(`invalid contract address: ${contractAddr}`);
    }
    if (!isHex(escrowId)) {
      throw new Error(`invalid escrow id (expect 0x... hex): ${escrowId}`);
    }
    if (![p1, p2, benef, refund].every(isAddr)) {
      throw new Error(`invalid participant/beneficiary/refund address: ${[p1,p2,benef,refund].join(',')}`);
    }
    if (!isAddr(fromAddr)) {
      throw new Error(`invalid from address: ${fromAddr}`);
    }

    if (!contractAddr || !escrowId || !p1 || !p2 || !benef || !refund || !meta || !fromAddr) {
      throw new Error('missing required args');
    }

    // 规范化参数，确保传入 web3 的都是字符串
    const toAddress = (v) => {
      const s = String(v);
      if (!web3.utils.isAddress(s)) throw new Error(`invalid address: ${v}`);
      return s;
    };
    const toBytes32 = (v) => {
      const s0 = String(v);
      if (!web3.utils.isHexStrict(s0)) throw new Error(`invalid hex: ${v}`);
      const body = s0.replace(/^0x/, '');
      if (body.length === 64) return '0x' + body.toLowerCase();
      if (body.length < 64) return '0x' + body.padStart(64, '0').toLowerCase();
      return '0x' + body.slice(-64).toLowerCase();
    };

    const escrowIdHex = toBytes32(escrowId);
    const metaHex = toBytes32(meta);
    const p1Addr = toAddress(p1);
    const p2Addr = toAddress(p2);
    const benefAddr = toAddress(benef);
    const refundAddr = toAddress(refund);
    const fromAddress = toAddress(fromAddr);

    // 调试输出
    console.log('[DEBUG] openEscrow params:', {
      contractAddr,
      escrowIdHex,
      p1Addr,
      p2Addr,
      benefAddr,
      refundAddr,
      deadlineType: typeof deadline,
      deadline,
      metaHex,
      valueEth,
      fromAddress
    });

    const inst = await MixerEscrow.at(contractAddr);
    const valueWei = web3.utils.toWei(valueEth, 'ether');
    
    const tx = await inst.openEscrow(
      escrowIdHex, p1Addr, p2Addr, benefAddr, refundAddr, deadline, metaHex,
      { from: fromAddress, value: valueWei }
    );
    console.log(JSON.stringify({ txHash: tx.tx, events: Object.keys(tx.logs || {}) }));
    callback();
  } catch (err) {
    console.error(err);
    callback(err);
  }
};


