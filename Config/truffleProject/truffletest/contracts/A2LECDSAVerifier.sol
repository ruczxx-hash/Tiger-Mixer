// SPDX-License-Identifier: MIT
pragma solidity ^0.6.12;

/**
 * @title A2L ECDSA Verifier
 * @dev 将A2L项目中的cp_ecdsa_ver功能移植到智能合约
 */
contract A2LECDSAVerifier {
    
    // 事件定义
    event SignatureVerified(address indexed signer, bytes32 indexed messageHash, bool success);
    
    /**
     * @dev 验证ECDSA签名（对应cp_ecdsa_ver函数）
     * @param messageHash 消息的哈希值（32字节）
     * @param v 恢复ID (27或28)
     * @param r 签名的r值（32字节）
     * @param s 签名的s值（32字节）
     * @param expectedSigner 期望的签名者地址
     * @return 如果签名有效返回true
     */
    function verifySignature(
        bytes32 messageHash,
        uint8 v,
        bytes32 r,
        bytes32 s,
        address expectedSigner
    ) public pure returns (bool) {
        // 检查输入参数
        require(expectedSigner != address(0), "Invalid signer address");
        require(v == 27 || v == 28, "Invalid v value");
        require(r != bytes32(0), "Invalid r value");
        require(s != bytes32(0), "Invalid s value");
        
        // 防止签名可塑性攻击：检查s值是否在有效范围内
        require(s <= 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0, "Invalid s value");
        
        // 使用ecrecover恢复签名者地址
        address signer = ecrecover(messageHash, v, r, s);
        
        // 验证签名有效且签名者匹配
        require(signer != address(0), "Invalid signature");
        
        return signer == expectedSigner;
    }
    
    /**
     * @dev 验证原始消息的签名（对应A2L中的tx_buf验证）
     * @param message 原始消息字节
     * @param v 恢复ID
     * @param r 签名的r值
     * @param s 签名的s值
     * @param expectedSigner 期望的签名者地址
     * @return 如果签名有效返回true
     */
    function verifyRawMessage(
        bytes memory message,
        uint8 v,
        bytes32 r,
        bytes32 s,
        address expectedSigner
    ) public pure returns (bool) {
        // 使用keccak256哈希（以太坊标准）
        bytes32 messageHash = keccak256(message);
        
        return verifySignature(messageHash, v, r, s, expectedSigner);
    }
    
    /**
     * @dev 验证A2L原始消息的签名（使用SHA256哈希，对应RELIC库的md_map）
     * @param message 原始消息字节
     * @param v 恢复ID
     * @param r 签名的r值
     * @param s 签名的s值
     * @param expectedSigner 期望的签名者地址
     * @return 如果签名有效返回true
     */
    function verifyA2LRawMessage(
        bytes memory message,
        uint8 v,
        bytes32 r,
        bytes32 s,
        address expectedSigner
    ) public pure returns (bool) {
        // 使用SHA256哈希（对应RELIC库的md_map函数）
        bytes32 messageHash = sha256(message);
        
        return verifySignature(messageHash, v, r, s, expectedSigner);
    }
    
    /**
     * @dev 从签名恢复签名者地址（使用SHA256哈希）
     * @param message 原始消息
     * @param v 恢复ID
     * @param r 签名的r值
     * @param s 签名的s值
     * @return 签名者的地址
     */
    function recoverSignerFromSHA256(
        bytes memory message,
        uint8 v,
        bytes32 r,
        bytes32 s
    ) public pure returns (address) {
        bytes32 messageHash = sha256(message);
        if (v < 27) {
            v += 27;
        }
        return ecrecover(messageHash, v, r, s);
    }
    
    /**
     * @dev 验证签名并返回恢复的地址（用于调试）
     * @param message 原始消息
     * @param v 恢复ID
     * @param r 签名的r值
     * @param s 签名的s值
     * @param expectedSigner 期望的签名者地址
     * @return recoveredAddress 恢复的签名者地址
     * @return isValid 验证是否成功
     */
    function verifyAndRecoverAddress(
        bytes memory message,
        uint8 v,
        bytes32 r,
        bytes32 s,
        address expectedSigner
    ) public pure returns (address recoveredAddress, bool isValid) {
        bytes32 messageHash = sha256(message);
        if (v < 27) {
            v += 27;
        }
        recoveredAddress = ecrecover(messageHash, v, r, s);
        isValid = (recoveredAddress != address(0) && recoveredAddress == expectedSigner);
    }
    
    /**
     * @dev 验证Alice的签名（对应A2L中auditor.c的验证）
     * @param transactionData 交易数据（对应tx_buf）
     * @param r Alice签名的r值
     * @param s Alice签名的s值
     * @param v 恢复ID
     * @param aliceAddress Alice的地址
     * @return 验证结果
     */
    function verifyAliceSignature(
        bytes memory transactionData,
        bytes32 r,
        bytes32 s,
        uint8 v,
        address aliceAddress
    ) public returns (bool) {
        // 使用SHA256哈希（对应A2L的md_map函数）
        bytes32 messageHash = sha256(transactionData);
        bool success = verifySignature(messageHash, v, r, s, aliceAddress);
        
        emit SignatureVerified(aliceAddress, messageHash, success);
        return success;
    }
    
    /**
     * @dev 验证Tumbler的签名（对应A2L中auditor.c的验证）
     * @param transactionData 交易数据
     * @param r Tumbler签名的r值
     * @param s Tumbler签名的s值
     * @param v 恢复ID
     * @param tumblerAddress Tumbler的地址
     * @return 验证结果
     */
    function verifyTumblerSignature(
        bytes memory transactionData,
        bytes32 r,
        bytes32 s,
        uint8 v,
        address tumblerAddress
    ) public returns (bool) {
        bytes32 messageHash = keccak256(transactionData);
        bool success = verifySignature(messageHash, v, r, s, tumblerAddress);
        
        emit SignatureVerified(tumblerAddress, messageHash, success);
        return success;
    }
    
    /**
     * @dev 验证以太坊个人消息签名（带以太坊签名前缀）
     * @param message 原始消息
     * @param v 恢复ID
     * @param r 签名的r值
     * @param s 签名的s值
     * @param expectedSigner 期望的签名者地址
     * @return 如果签名有效返回true
     */
    function verifyPersonalMessage(
        bytes memory message,
        uint8 v,
        bytes32 r,
        bytes32 s,
        address expectedSigner
    ) public pure returns (bool) {
        // 计算带以太坊前缀的消息哈希
        bytes32 messageHash = keccak256(abi.encodePacked(
            "\x19Ethereum Signed Message:\n",
            uint2str(message.length),
            message
        ));
        
        return verifySignature(messageHash, v, r, s, expectedSigner);
    }
    
    /**
     * @dev 验证以太坊个人消息签名（使用预计算的哈希）
     * @param messageHash 带前缀的消息哈希
     * @param v 恢复ID
     * @param r 签名的r值
     * @param s 签名的s值
     * @param expectedSigner 期望的签名者地址
     * @return 如果签名有效返回true
     */
    function verifyPersonalMessageHash(
        bytes32 messageHash,
        uint8 v,
        bytes32 r,
        bytes32 s,
        address expectedSigner
    ) public pure returns (bool) {
        return verifySignature(messageHash, v, r, s, expectedSigner);
    }
    
    /**
     * @dev 将uint转换为字符串（用于以太坊签名前缀）
     * @param i 要转换的数字
     * @return 数字的字符串表示
     */
    function uint2str(uint i) internal pure returns (string memory) {
        if (i == 0) {
            return "0";
        }
        uint j = i;
        uint len;
        while (j != 0) {
            len++;
            j /= 10;
        }
        bytes memory bstr = new bytes(len);
        uint k = len - 1;
        while (i != 0) {
            bstr[k--] = bytes1(uint8(48 + i % 10));
            i /= 10;
        }
        return string(bstr);
    }
    
    /**
     * @dev 批量验证签名（gas优化版本）
     * @param messageHashes 消息哈希数组
     * @param vArray v值数组
     * @param rArray r值数组
     * @param sArray s值数组
     * @param signers 签名者地址数组
     * @return 验证结果数组
     */
    function batchVerifySignatures(
        bytes32[] memory messageHashes,
        uint8[] memory vArray,
        bytes32[] memory rArray,
        bytes32[] memory sArray,
        address[] memory signers
    ) public pure returns (bool[] memory) {
        require(
            messageHashes.length == vArray.length &&
            vArray.length == rArray.length &&
            rArray.length == sArray.length &&
            sArray.length == signers.length,
            "Array length mismatch"
        );
        
        bool[] memory results = new bool[](messageHashes.length);
        
        for (uint i = 0; i < messageHashes.length; i++) {
            results[i] = verifySignature(
                messageHashes[i],
                vArray[i],
                rArray[i],
                sArray[i],
                signers[i]
            );
        }
        
        return results;
    }
    
    // secp256k1 曲线的阶（对应 cp_ecdsa_ver 中的 ec_curve_get_ord(n)）
    uint256 private constant SECP256K1_ORDER = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141;
    
     /**
      * @dev 简化的ECDSA签名验证（不需要v值）
      * 直接使用ecrecover进行验证，避免复杂的数学运算
      * 
      * @param r 签名的 r 值
      * @param s 签名的 s 值
      * @param message 原始消息
      * @param hash 是否已哈希（false表示需要SHA256哈希）
      * @param publicKeyX 公钥的 X 坐标
      * @param publicKeyY 公钥的 Y 坐标
      * @return 如果签名有效返回 true
      */
     function verifySignatureManual(
         bytes32 r,
         bytes32 s,
         bytes memory message,
         bool hash,
         bytes32 publicKeyX,
         bytes32 publicKeyY
     ) public pure returns (bool) {
         // 基本验证
         require(r != bytes32(0), "r is zero");
         require(s != bytes32(0), "s is zero");
         require(publicKeyX != bytes32(0), "invalid public key");
         require(publicKeyY != bytes32(0), "invalid public key");
         
         // 计算消息哈希
         bytes32 messageHash;
         if (!hash) {
             messageHash = sha256(message);
         } else {
             require(message.length == 32, "invalid hash length");
             assembly {
                 messageHash := mload(add(message, 32))
             }
         }
         
         // 将公钥转换为地址
         address publicKeyAddress = publicKeyToAddress(publicKeyX, publicKeyY);
         
         // 尝试两个可能的 v 值
         return tryRecoverAddresses(messageHash, r, s, publicKeyAddress);
     }
     
     /**
      * @dev 尝试恢复地址（避免栈深度问题）
      */
     function tryRecoverAddresses(
         bytes32 messageHash,
         bytes32 r,
         bytes32 s,
         address expectedAddress
     ) internal pure returns (bool) {
         address recoveredAddress1 = ecrecover(messageHash, 27, r, s);
         address recoveredAddress2 = ecrecover(messageHash, 28, r, s);
         
         return (recoveredAddress1 == expectedAddress || recoveredAddress2 == expectedAddress);
     }
    
    /**
     * @dev 按照 cp_ecdsa_ver 的步骤验证签名（使用 SHA256 哈希）
     * 这是 cp_ecdsa_ver(msg, len, hash=0, q) 的直接对应
     * 
     * @param r 签名的 r 值
     * @param s 签名的 s 值
     * @param message 原始消息
     * @param publicKeyX 公钥的 X 坐标
     * @param publicKeyY 公钥的 Y 坐标
     * @return 如果签名有效返回 true
     */
    function verifyA2LSignatureManual(
        bytes32 r,
        bytes32 s,
        bytes memory message,
        bytes32 publicKeyX,
        bytes32 publicKeyY
    ) public pure returns (bool) {
        // hash=false 表示需要对消息进行哈希（对应 cp_ecdsa_ver 中的 hash=0）
        return verifySignatureManual(r, s, message, false, publicKeyX, publicKeyY);
    }
    
    /**
     * @dev 计算模逆（使用扩展欧几里得算法）
     * 对应 cp_ecdsa_ver 中的 bn_mod_inv(k, s, n)
     * 
     * @param a 要计算模逆的数
     * @param m 模数（secp256k1 的阶）
     * @return a 关于 m 的模逆
     */
    function modInverse(uint256 a, uint256 m) internal pure returns (uint256) {
        // 使用费马小定理：a^(-1) ≡ a^(m-2) mod m（当 m 是质数时）
        // 但由于 m 是 secp256k1 的阶（也是质数），可以使用费马小定理
        return modExp(a, m - 2, m);
    }
    
    /**
     * @dev 模幂运算（快速幂算法）
     * 
     * @param base 底数
     * @param exponent 指数
     * @param modulus 模数
     * @return base^exponent mod modulus
     */
    function modExp(uint256 base, uint256 exponent, uint256 modulus) internal pure returns (uint256) {
        if (modulus == 0) return 0;
        if (exponent == 0) return 1;
        
        uint256 result = 1;
        base = base % modulus;
        
        while (exponent > 0) {
            if (exponent % 2 == 1) {
                result = mulmod(result, base, modulus);
            }
            exponent = exponent >> 1;
            base = mulmod(base, base, modulus);
        }
        
        return result;
    }
    
    /**
     * @dev 从公钥坐标计算以太坊地址
     * 
     * @param publicKeyX 公钥的 X 坐标
     * @param publicKeyY 公钥的 Y 坐标
     * @return 对应的以太坊地址
     */
    function publicKeyToAddress(bytes32 publicKeyX, bytes32 publicKeyY) internal pure returns (address) {
        // 以太坊地址 = keccak256(未压缩公钥)[12:32]
        // 未压缩公钥格式：0x04 + X坐标(32字节) + Y坐标(32字节)
        bytes memory publicKey = abi.encodePacked(
            bytes1(0x04),
            publicKeyX,
            publicKeyY
        );
        
        bytes32 hash = keccak256(publicKey);
        
        // 取后20字节作为地址（简化版本）
        return address(uint160(uint256(hash)));
    }
}
