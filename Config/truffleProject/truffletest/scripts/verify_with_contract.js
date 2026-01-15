const A2LECDSAVerifier = artifacts.require("A2LECDSAVerifier");

function getArg(flag) {
  const i = process.argv.indexOf(flag);
  if (i === -1 || i + 1 >= process.argv.length) return null;
  return process.argv[i + 1];
}

function pad32(hex) {
  return '0x' + hex.replace(/^0x/, '').padStart(64, '0');
}

const N = BigInt('0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141');
const HALF_N = N >> 1n;
function normalizeS(sHex) {
  const s = BigInt(sHex);
  if (s > HALF_N) {
    const norm = N - s;
    return '0x' + norm.toString(16).padStart(64, '0');
  }
  return '0x' + s.toString(16).padStart(64, '0');
}

function publicKeyToAddress(uncompressed) {
  let raw = uncompressed.replace(/^0x/, '');
  if (!raw.startsWith('04')) throw new Error('pubkey must start with 0x04');
  if (raw.length === 132) raw = raw.slice(0, 130);
  const xy = '0x' + raw.slice(2);
  const hash = web3.utils.keccak256(xy);
  return web3.utils.toChecksumAddress('0x' + hash.slice(-40));
}

module.exports = async function (callback) {
  try {
    const verifier = await A2LECDSAVerifier.deployed();
    const messageHex = getArg('--message');
    const rIn = getArg('--r');
    const sIn = getArg('--s');
    const pubkey = getArg('--pubkey');

    if (!messageHex || !rIn || !sIn || !pubkey) {
      throw new Error('usage: truffle exec scripts/verify_with_contract.js --message 0x.. --r 0x.. --s 0x.. --pubkey 0x04..');
    }

    const expected = publicKeyToAddress(pubkey);
    const r32 = pad32(rIn);
    const s32 = normalizeS(sIn);

    // 使用合约的 recoverSignerFromSHA256 方法找到正确的 v
    let correctV = null;
    for (const vTry of [27, 28]) {
      try {
        const recovered = await verifier.recoverSignerFromSHA256(
          messageHex,
          vTry,
          r32,
          s32
        );
        if (recovered && recovered.toLowerCase() === expected.toLowerCase()) {
          correctV = vTry;
          break;
        }
      } catch (e) {
        // 忽略错误，继续尝试下一个 v
      }
    }

    if (correctV === null) {
      console.log('false');
      return callback();
    }

    const ok = await verifier.verifyAliceSignature(
      messageHex,
      r32,
      s32,
      correctV,
      expected
    );
    console.log(ok ? 'true' : 'false');
    callback();
  } catch (e) {
    console.error('error:', e.message);
    callback(e);
  }
};


