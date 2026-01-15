module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), { string: ['hash'] });
    const hash = String(argv.hash || argv.h || '');
    if (!/^0x[0-9a-fA-F]{64}$/.test(hash)) throw new Error('invalid --hash');

    const receipt = await web3.eth.getTransactionReceipt(hash);
    const tx = await web3.eth.getTransaction(hash);
    const latest = await web3.eth.getBlock('latest');

    let confirmations = 0;
    let mined = false;
    let status = null;
    let blockNumber = null;

    if (receipt && receipt.blockNumber != null) {
      mined = true;
      blockNumber = receipt.blockNumber;
      confirmations = Math.max(0, latest.number - receipt.blockNumber + 1);
      status = receipt.status === true ? 'success' : (receipt.status === false ? 'reverted' : 'unknown');
    }

    const result = {
      txHash: hash,
      from: tx ? tx.from : null,
      to: tx ? tx.to : null,
      value: tx ? tx.value : null,
      gasUsed: receipt ? receipt.gasUsed : null,
      blockNumber,
      mined,
      confirmations,
      status
    };

    console.log(JSON.stringify(result));
    callback();
  } catch (err) {
    console.error('[checkTxMined] ERROR:', err.message);
    callback(err);
  }
};
