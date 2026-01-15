# Anonymous Atomic Locks (A2L)

A2L [1] is a cryptographic construction for building secure, privacy-preserving, interoperable, and fungibility-preserving payment channel hub (PCH). A2L builds on a novel cryptographic primitive that realizes a three-party protocol for conditional transactions, where the intermediary (tumbler) pays the receiver only if the latter solves a cryptographic challenge with the help of the sender. This repository includes implementation of A2L with instantiations based on Schnorr and ECDSA signatures.

## Quick Start

For detailed build and execution instructions, see `RUNNING_INSTRUCTIONS.md` in the repository root.

**Quick build**:
```bash
cd ecdsa
mkdir build && cd build
cmake ..
make -j$(nproc)
./bin/wrapper  # Run the protocol
```

## Dependencies

* [CMake](https://cmake.org/download/) >= 3.23.0
* [ZeroMQ](https://github.com/zeromq/libzmq)
* [GMP](https://gmplib.org/) >= 6.2.1
* [RELIC](https://github.com/relic-toolkit/relic) (configured and built with `-DARITH=gmp`)
* [PARI/GP](https://pari.math.u-bordeaux.fr/) >= 2.13.4

See `RUNNING_INSTRUCTIONS.md` for detailed installation instructions.

## Repository Structure

```
A2L-master/
├── ecdsa/              # ECDSA-based implementation
│   ├── src/            # Source code (alice.c, bob.c, tumbler.c, auditor.c)
│   ├── include/        # Header files
│   ├── docs/           # Technical documentation
│   ├── scripts/        # Benchmark and analysis scripts
│   └── keys/           # Test keys (test data only)
├── schnorr/            # Schnorr-based implementation
│   ├── src/
│   └── include/
└── README.md           # This file
```

## Artifacts for Evaluation

This repository contains all artifacts needed to evaluate the paper's contributions:

### Source Code
- **Core Protocol**: `ecdsa/src/alice.c`, `bob.c`, `tumbler.c`, `auditor.c`
- **Cryptographic Primitives**: `ecdsa/src/util.c` (class group encryption, zero-knowledge proofs)
- **Committee Management**: `ecdsa/src/committee_integration.c`, `reputation_tracker.c`
- **Benchmarks**: `ecdsa/src/dkg_benchmark.c`, `secret_share_benchmark.c`, etc.

### Documentation
- **Running Instructions**: `RUNNING_INSTRUCTIONS.md` (repository root) - Complete build and execution guide
- **Technical Docs**: `ecdsa/docs/` - Detailed protocol documentation
- **Mathematical Background**: `G_g_q_explanation.md` - Class group operations

### Test Data
- **Sample Keys**: `ecdsa/keys/`, `schnorr/keys/` (test keys only)
- **Example Configurations**: `ecdsa/SliceMessage/` - Multi-receiver scenarios
- **Execution Logs**: `ecdsa/logs/`, `ecdsa/log_game/` - Sample test data

### Build System
- **CMake Configuration**: `ecdsa/CMakeLists.txt`, `schnorr/CMakeLists.txt`
- **Dependencies**: See `RUNNING_INSTRUCTIONS.md` for installation

## Access During Review

**Anonymous Repository**: `https://github.com/[ANONYMOUS_REPO_URL]`

All artifacts are available in this public repository. No credentials required.

## Building and Running

### Prerequisites
- Linux (Ubuntu 20.04+) or macOS
- CMake >= 3.23.0, GCC >= 7.0
- RELIC, PARI/GP, GMP, ZeroMQ (see `RUNNING_INSTRUCTIONS.md`)

### Build Steps
```bash
cd ecdsa
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running the Protocol
```bash
# Method 1: Using wrapper (recommended)
./bin/wrapper

# Method 2: Manual execution
# Terminal 1: ./bin/tumbler
# Terminal 2: ./bin/alice
# Terminal 3: ./bin/bob
```

### Running Benchmarks
```bash
./bin/dkg_benchmark [committee_size] [threshold]
./bin/secret_share_benchmark [n] [t]
./bin/committee_size_benchmark [min] [max] [step]
```

## Artifacts Not Included

The following are excluded but can be obtained independently:

- **Large Binary Dependencies**: `go1.15.9.linux-amd64.tar.gz`, `pari_2.15.4.orig.tar.gz`
  - Reason: Large size, available from official sources
  - Mitigation: Download instructions in `RUNNING_INSTRUCTIONS.md`

- **Build Artifacts**: `build/` directories
  - Reason: Generated files, platform-specific
  - Mitigation: Rebuild from source using provided instructions

- **Node Modules**: `Config/node_modules/`
  - Reason: Standard practice, can be regenerated
  - Mitigation: `package-lock.json` included for reproducibility

## Reproducibility

- **Environment**: Linux (Ubuntu 20.04+), GCC with C11
- **Deterministic Operations**: All cryptographic operations use deterministic algorithms
- **Seeded PRNGs**: Random operations use seeded generators for reproducibility
- **Test Vectors**: Included for verification

## Troubleshooting

See `RUNNING_INSTRUCTIONS.md` Section 7 for detailed troubleshooting guide covering:
- Dependency installation issues
- Build failures
- Runtime errors
- Performance optimization

## License

See `LICENSE` file. Dependencies have their own licenses:
- RELIC: LGPL-2.1
- PARI/GP: GPL

## Warning

This code has **not** received sufficient peer review by other qualified cryptographers to be considered in any way, shape, or form, safe. It was developed for experimentation purposes.

**USE AT YOUR OWN RISK**

## References

[1] Erkan Tairi, Pedro-Moreno Sanchez, and Matteo Maffei, "[A2L: Anonymous Atomic Locks for Scalability and Interoperability in Payment Channel Hubs](https://eprint.iacr.org/2019/589)".

## Contact
#### 1.开启私有链：

```bash
#Config/blockchain/consortium_blockchain目录下：

geth --identity "myethereum" --dev --rpc --rpcaddr "127.0.0.1" --port 30304 --rpcport "7545" --rpccorsdomain "*" --datadir ./myblockchain --rpcapi "eth,net,web3,admin,personal" --networkid 5777 console --allow-insecure-unlock

./start_chain_optimized_background.sh


#Config/blockchain/consortium_blockchain/myblockchain目录下
geth attach geth.ipc

#启动挖矿
miner.start()

#错误处理
#显示目录被进程占用
lsof | grep myblockchain 查找对应目录运行的进程
kill -9 <PID> 终止进程

#没有权限-赋予权限
chmod +x A2L/A2L-master/ecdsa/bin/setup.sh
```

#### 2.部署智能合约

```bash
#Config/truffleProject/truffletest目录下

truffle migrate --reset

#输出
Network: development (id: 5777)
No contracts deployed.

Network: private (id: 5777)
Greeter: 0x2928b1bC04f2c9640052b2998C203b1A00e785D8
Migrations: 0xfE4AEd6c630C9DF5B083f8b7DAB2C8Fe44269cdb
MixerEscrow: 0x2D43c363C184E1EB856F93A4b10b2d417029D627

#将MixerEscrow的地址在alice、bob、tumbler的合约地址赋值中进行替换。alice替换1处，bob1处，tumbler2处。
```

#### 3.重新编译

```bash
#A2L/A2L-master/ecdsa/build 目录下

make

const result = await contractInstance.escrows('0x0221e1b20e65473b418bd3523644dac34ac3d42b620d022a192495a91f3507ae');

#因为改变了合约地址，因此需要重新编译
```

#### 4.脚本运行

For issues accessing or building artifacts during review, contact the submission system administrators.
#A2L/A2L-master/ecdsa/bin目录下：

#防止进程被占用，首先kill一下
./kill.sh

#对区块链中的账号解锁并转资金
./setup.sh

your_command 2>&1 | tee /home/zxx/A2L/A2L-master/ecdsa/bin/test/auditor.log
./tumbler 2>&1 | tee /home/zxx/A2L/A2L-master/ecdsa/bin/test/tumbler.log
./secret_share_receiver 2>&1 | tee /home/zxx/A2L/A2L-master/ecdsa/bin/test/secret.log
./alice 8182 8183 0x9339c1e45f56ecf3af4ee2d976f31a12086ad506 2>&1 | tee /home/zxx/A2L/A2L-master/ecdsa/bin/test/alice.log
./bob 8183 8182 0x27766bfd44afdc46584e0550765181422c3ba080 2>&1 | tee /home/zxx/A2L/A2L-master/ecdsa/bin/test/bob.log