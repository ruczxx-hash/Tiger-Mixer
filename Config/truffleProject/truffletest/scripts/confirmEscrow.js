const fs = require('fs');
const path = require('path');
const FixedMixerEscrow = artifacts.require("FixedMixerEscrow");
const A2LECDSAVerifier = artifacts.require("A2LECDSAVerifier");

module.exports = async function (callback) {
  try {
    const argv = require('minimist')(process.argv.slice(2), {
      string: ['contract', 'pool', 'id', 'from', 'r1', 's1', 'v1', 'r2', 's2', 'v2']
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

    const escrowId = typeof argv.id === 'string' ? argv.id : String(argv.id || '');
    const fromAddr = typeof argv.from === 'string' ? argv.from : String(argv.from || '');

    // 签名参数
    const r1 = argv.r1 ? String(argv.r1) : '';
    const s1 = argv.s1 ? String(argv.s1) : '';
    const v1 = argv.v1 ? parseInt(argv.v1) : null;
    const r2 = argv.r2 ? String(argv.r2) : '';
    const s2 = argv.s2 ? String(argv.s2) : '';
    const v2 = argv.v2 ? parseInt(argv.v2) : null;

    if (!isAddr(contractAddr)) throw new Error(`invalid contract address: ${contractAddr}`);
    if (!isHex(escrowId)) throw new Error(`invalid escrow id: ${escrowId}`);
    if (!isAddr(fromAddr)) throw new Error(`invalid from address: ${fromAddr}`);
    if (!r1 || !s1 || v1 === null || !r2 || !s2 || v2 === null) {
      throw new Error('Missing signature parameters: r1, s1, v1, r2, s2, v2 are all required');
    }

    const toBytes32 = (v) => {
      const body = v.replace(/^0x/, '');
      if (body.length === 64) return '0x' + body.toLowerCase();
      if (body.length < 64) return '0x' + body.padStart(64, '0').toLowerCase();
      return '0x' + body.slice(-64).toLowerCase();
    };

    // 工具函数：补齐到bytes32
    const pad32 = (hex) => {
      return '0x' + hex.replace(/^0x/, '').padStart(64, '0');
    };

    // 工具函数：low-s 归一化
    const SECP256K1_N = BigInt('0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141');
    const SECP256K1_HALF_N = SECP256K1_N >> 1n;
    function normalizeS(sHex) {
      const s = BigInt(sHex);
      if (s > SECP256K1_HALF_N) {
        const norm = SECP256K1_N - s;
        return '0x' + norm.toString(16).padStart(64, '0');
      }
      return '0x' + s.toString(16).padStart(64, '0');
    }

    // 从未压缩公钥计算以太坊地址（与 test_a2l_verifier.js 一致）
    function publicKeyToAddress(uncompressedPublicKey) {
      let raw = uncompressedPublicKey.replace(/^0x/, '');
      if (!raw.startsWith('04')) {
        throw new Error('未压缩公钥应以 0x04 开头');
      }
      // 允许 130 或 132 或 134 个十六进制字符；截断到标准的 130
      // 因为可能有多余的字节（如长度前缀等）
      if (raw.length >= 130) {
        raw = raw.slice(0, 130);
      }
      if (raw.length !== 130) {
        throw new Error(`未压缩公钥长度异常，期望至少130个hex字符(65字节)，当前: ${raw.length}`);
      }
      const xyHex = '0x' + raw.slice(2); // 去掉 '04'，得到 X||Y (64字节，128个hex字符)
      const hash = web3.utils.keccak256(xyHex);
      const addr = '0x' + hash.slice(-40);
      return web3.utils.toChecksumAddress(addr);
    }

    // 从签名和消息找到正确的v值
    async function findCorrectV(messageBytes, r, s, expectedAddress, verifier) {
      // 确保 r 和 s 已经格式化
      const rPadded = pad32(r);
      const sNormalized = normalizeS(s);
      
      console.log('[DEBUG findCorrectV] Trying to find v for:', {
        expectedAddress,
        rPadded: rPadded.substring(0, 20) + '...',
        sNormalized: sNormalized.substring(0, 20) + '...',
        messageBytesLength: messageBytes.length
      });
      
      for (const v of [27, 28]) {
        try {
          const recovered = await verifier.recoverSignerFromSHA256(messageBytes, v, rPadded, sNormalized);
          console.log(`[DEBUG findCorrectV] v=${v}, recovered=${recovered}, expected=${expectedAddress}`);
          if (recovered.toLowerCase() === expectedAddress.toLowerCase()) {
            console.log(`[DEBUG findCorrectV] Found correct v: ${v}`);
            return v;
          }
        } catch (e) {
          console.log(`[DEBUG findCorrectV] v=${v} failed:`, e.message);
          // 继续尝试下一个v值
        }
      }
      console.log('[DEBUG findCorrectV] Could not find valid v');
      return null;
    }

    const escrowIdHex = toBytes32(escrowId);
    const fromAddress = fromAddr;

    console.log('[DEBUG] confirmEscrow params:', { 
      contractAddr, 
      escrowIdHex, 
      fromAddress,
      r1: r1.substring(0, 20) + '...',
      s1: s1.substring(0, 20) + '...',
      v1,
      r2: r2.substring(0, 20) + '...',
      s2: s2.substring(0, 20) + '...',
      v2
    });

    const inst = await FixedMixerEscrow.at(contractAddr);
    
    // 读取托管信息
    const escrow = await inst.escrows(escrowIdHex);
    const escrowIdCopy = escrow.escrowIdCopy;
    const dataHash = escrow.dataHash;
    const party1Key = escrow.party1Key;
    const party2Key = escrow.party2Key;

    console.log('[DEBUG] Escrow info:', {
      party1: escrow.party1,
      party2: escrow.party2,
      escrowIdCopy: escrowIdCopy,
      dataHash: dataHash,
      party1KeyLen: party1Key.length,
      party2KeyLen: party2Key.length
    });

    // 构造签名消息：escrowId || dataHash
    // 使用 abi.encodePacked 的方式（与合约中一致）
    // 将两个 bytes32 直接拼接（每个都是64个十六进制字符）
    // 注意：web3.eth.abi.encodeParameter 可能和 Solidity 的 abi.encodePacked 不同
    // 所以我们手动拼接，确保格式一致
    const escrowIdHexStr = escrowIdCopy.replace(/^0x/, '').padStart(64, '0').toLowerCase();
    const dataHashHexStr = dataHash.replace(/^0x/, '').padStart(64, '0').toLowerCase();
    const messageBytes = '0x' + escrowIdHexStr + dataHashHexStr;
    
    console.log('[DEBUG] Message construction:', {
      escrowIdCopy,
      dataHash,
      escrowIdHex: escrowIdHexStr.substring(0, 20) + '...',
      dataHashHex: dataHashHexStr.substring(0, 20) + '...',
      messageBytesLength: messageBytes.length,
      messageBytesPreview: messageBytes.substring(0, 50) + '...'
    });

    // 从公钥计算地址（与 test_a2l_verifier.js 一致）
    // 因为在这个系统中，公钥是随机生成的，需要从公钥计算地址
    let party1KeyHex, party2KeyHex;
    
    if (Buffer.isBuffer(party1Key)) {
      party1KeyHex = '0x' + party1Key.toString('hex');
    } else if (typeof party1Key === 'string') {
      if (party1Key.startsWith('0x')) {
        party1KeyHex = party1Key;
      } else {
        party1KeyHex = '0x' + party1Key;
      }
    } else {
      party1KeyHex = '0x' + Buffer.from(party1Key).toString('hex');
    }
    
    if (Buffer.isBuffer(party2Key)) {
      party2KeyHex = '0x' + party2Key.toString('hex');
    } else if (typeof party2Key === 'string') {
      if (party2Key.startsWith('0x')) {
        party2KeyHex = party2Key;
      } else {
        party2KeyHex = '0x' + party2Key;
      }
    } else {
      party2KeyHex = '0x' + Buffer.from(party2Key).toString('hex');
    }
    
    console.log('[DEBUG] Party1 key type:', typeof party1Key);
    console.log('[DEBUG] Party1 key (raw, first 100 chars):', String(party1Key).substring(0, 100) + '...');
    console.log('[DEBUG] Party1 key (hex, first 100 chars):', party1KeyHex.substring(0, 100) + '...');
    console.log('[DEBUG] Party1 key length:', party1KeyHex.length);
    console.log('[DEBUG] Party2 key (hex, first 100 chars):', party2KeyHex.substring(0, 100) + '...');
    console.log('[DEBUG] Party2 key length:', party2KeyHex.length);
    
    // 从公钥计算地址
    const party1Address = publicKeyToAddress(party1KeyHex);
    const party2Address = publicKeyToAddress(party2KeyHex);

    console.log('[DEBUG] Calculated addresses from public keys:', {
      party1Address,
      party2Address,
      contractParty1: escrow.party1,
      contractParty2: escrow.party2,
      matchesParty1: party1Address.toLowerCase() === escrow.party1.toLowerCase(),
      matchesParty2: party2Address.toLowerCase() === escrow.party2.toLowerCase()
    });

    // 获取验证器合约地址
    const verifierAddr = await inst.verifier();
    const verifier = await A2LECDSAVerifier.at(verifierAddr);

    // 归一化s值
    const s1Normalized = normalizeS(s1);
    const s2Normalized = normalizeS(s2);
    const r1Padded = pad32(r1);
    const r2Padded = pad32(r2);

    // 找到正确的v值（如果需要）
    // 注意：即使传入的 v1/v2 是 0，我们也要尝试找到正确的 v 值
    let finalV1 = (v1 === 27 || v1 === 28) ? v1 : null;
    let finalV2 = (v2 === 27 || v2 === 28) ? v2 : null;

    if (finalV1 === null) {
      console.log('[DEBUG] Finding correct v1...');
      const foundV1 = await findCorrectV(messageBytes, r1, s1, party1Address, verifier);
      if (foundV1 === null) {
        throw new Error('Could not find valid v1 for party1 signature');
      }
      finalV1 = foundV1;
      console.log('[DEBUG] Found v1:', finalV1);
    }

    if (finalV2 === null) {
      console.log('[DEBUG] Finding correct v2...');
      const foundV2 = await findCorrectV(messageBytes, r2, s2, party2Address, verifier);
      if (foundV2 === null) {
        throw new Error('Could not find valid v2 for party2 signature');
      }
      finalV2 = foundV2;
      console.log('[DEBUG] Found v2:', finalV2);
    }

    console.log('[DEBUG] Final signature params:', {
      r1: r1Padded,
      s1: s1Normalized,
      v1: finalV1,
      r2: r2Padded,
      s2: s2Normalized,
      v2: finalV2,
      party1Address,
      party2Address
    });

    // 调用合约的confirm函数
    const tx = await inst.confirm(
      escrowIdHex,
      r1Padded,
      s1Normalized,
      finalV1,
      r2Padded,
      s2Normalized,
      finalV2,
      party1Address,
      party2Address,
      { from: fromAddress }
    );

    console.log(JSON.stringify({ txHash: tx.tx, events: Object.keys(tx.logs || {}), contract: contractAddr }));
    callback();
  } catch (err) {
    console.error(err);
    callback(err);
  }
};
