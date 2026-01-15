// 验证步骤1：从未压缩公钥计算以太坊地址（去掉0x04，仅对 X||Y 做 keccak256）

// 运行方式（在truffle项目根目录）:
//   truffle exec scripts/check_pubkey_address.js

module.exports = async function (callback) {
  try {
    // 1) 将你的未压缩公钥填到这里（来自审计器输出，65字节，0x04 开头）
    const uncompressedPubKey =
      '0x04af42dc95f0ebd5e9ce98a70803d25a77a1e40a74c0e5a7da5d20a1a284e650185127b62ed4fbb0282597fc6f9186095596524d9e9cda72c2a846664899751ec400';

    // 2) 去掉前缀 0x04，仅对 X||Y 做 keccak256
    let raw = uncompressedPubKey.replace(/^0x/, '');
    if (!raw.startsWith('04')) {
      throw new Error('未压缩公钥应以 0x04 开头');
    }
    // 允许 130 或 132 个十六进制字符；若为 132，截断到标准的 130
    if (raw.length === 132) {
      raw = raw.slice(0, 130);
    }
    if (raw.length !== 130) {
      throw new Error(`未压缩公钥长度异常，期望130个hex字符(65字节)，当前: ${raw.length}`);
    }

    const xyHex = '0x' + raw.slice(2); // 去掉 '04'
    const hash = web3.utils.keccak256(xyHex);
    const addr = '0x' + hash.slice(-40);
    const checksum = web3.utils.toChecksumAddress(addr);

    console.log('未压缩公钥:', uncompressedPubKey);
    console.log('去掉前缀后的XY(hex):', xyHex);
    console.log('keccak256(X||Y):', hash);
    console.log('计算得到的以太坊地址:', checksum);

    // 3) 如需对比期望地址，可在此填写 expectedAddress
    // const expectedAddress = '0x6Bd1000af374A61819C5728b73c3D875086c7325';
    // console.log('是否匹配:', checksum.toLowerCase() === expectedAddress.toLowerCase());

    callback();
  } catch (e) {
    console.error('校验失败:', e.message);
    callback(e);
  }
};


