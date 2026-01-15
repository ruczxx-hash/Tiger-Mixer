# 一、Install the required libraries

### Cmake

```
sudo apt-get install cmake
```

### Zeromq     

Two methods: The first one is to install using the git clone package.

gitclone command：

```
git clone https://github.com/zeromq/libzmq.git
```


The second method is to install directly using the following command.

```
apt-get install libzmq3-dev
```

### GMP

```
sudo apt install libgmp-dev
```

### RELIC

```cpp
git clone https://github.com/relic-toolkit/relic.git
cd relic
mkdir build
cd build
cmake .. -DARITH=gmp
make
```

Test

```cpp
make test
```

If all the tests are passed, then there will be no problem with the installation.

### PARI/GP

```cpp
wget https://pari.math.u-bordeaux.fr/pub/pari/unix/pari-2.17.2.tar.gz
tar zxvf pari-2.17.2.tar.gz
cd pari-2.17.2.tar.gz
./Configure
sudo make install

gp --version
```



# 二、编译



在以上的库都成功安装后，在代码的/ecdsa文件夹下执行：



```
mkdir build
cd build
cmake..
make
```

# 三、运行前准备工作

```bash
#Config/blockchain/consortium_blockchain

geth --identity "myethereum" --dev --rpc --rpcaddr "127.0.0.1" --port 30304 --rpcport "7545" --rpccorsdomain "*" --datadir ./myblockchain --rpcapi "eth,net,web3,admin,personal" --networkid 5777 console --allow-insecure-unlock

#Config/blockchain/consortium_blockchain/myblockchain目录下
geth attach geth.ipc

#启动挖矿
miner.start()

##Config/truffleProject/truffletest目录下 部署智能合约

truffle migrate --reset
npx truffle networks | cat

#A2L/A2L-master/ecdsa/bin目录下：解锁账户并从账户0转账
./setup.sh 

#Config/truffleProject/truffletest/scripts目录下
truffle exec ./setup_candidate_pool.js 提前设置委员会成员

```

# 四、运行

```bash
#/Agent
python3 service3.py

#/Tiger-Mixer/Tiger-Mixer-master/ecdsa/bin
#1、开启委员会自动轮换 日志文件输出在/Tiger-Mixer/Tiger-Mixer-master/ecdsa/logs/auto_rotation_simple.log
./auto_rotation_simple.sh

#2、开启委员会成员、
./start_secret_share_receiver.sh

./tumbler 
./alice 8182 8183 0x9339c1e45f56ecf3af4ee2d976f31a12086ad506 0.1 8181
./bob 8183 8182 0x27766bfd44afdc46584e0550765181422c3ba080 8181
```

# 代码介绍

#### 混币相关及脚本文件：

- /Tiger-Mixer/Tiger-Mixer-master/ecdsa/src：其中包含了所有c文件，包括alice.c bob.c tumbler.c auditor.c secret_share_receiver.c等文件，主要的混币逻辑均在此执行

- /Tiger-Mixer/Tiger-Mixer-master/ecdsa/build：进行cmake以及make的目录。生成的可执行文件保存在bin目录下

- /Tiger-Mixer/Tiger-Mixer-master/ecdsa/bin：可执行文件保存目录。

  

#### ETH区块链相关：

- /Config/blockchain：geth创建私有链相关文件
- /Config/blockchain/consortium_blockchain：私有链路径，开启私有链的命令在这里执行
- /Config/truffleProject：与geth交互，创建智能合约、上链部署相关文件
- /Config/truffleProject/truffletest：目前使用该文件夹，与本项目有关的智能合约都包含在此处



# 实验运行
## Performance Evaluation
**auditor_benchmark.csv**

figc，用来测量auditor负责的工作：VSS验证、重构transaction information、重构私钥。

auditor_benchmark.csv 由 **threshold_benchmark.c**生成

**dkg_threshold_benchmark.csv**

生成文件：/Tiger-mixer/Tiger-mixer-master/ecdsa/src/dkg_threshold_benchmark.c

运行方式：./dkg_threshold_benchmark.c    (Tiger-Mixer/Tiger-Mixer-master/ecdsa/bin目录下)

功能：测试 DKG（分布式密钥生成）的性能

主要工作：

- 使用 Class Group 承诺（2048 位判别式）

- 测试不同委员会大小（3-20）和多数阈值 (n+1)/2

- 测量 4 个阶段的性能：

ShareTime：份额生成（多项式系数生成 + 份额计算）

CommitmentTime：承诺生成（Class Group 幂运算 g_q^{a_ij}）

VerificationTime：分片验证（验证其他 n-1 个参与者的分片）

AggregationTime：私钥分片合成（合成完整的 DKG 私钥）

- 每个配置测试 100 次，计算平均时间和成功率

**secret_share_threshold_benchmark.csv**

生成文件：/A2L/A2L-master/ecdsa/src/secret_share_threshold_benchmark.c

功能：测试秘密分享（VSS - Verifiable Secret Sharing）的性能

主要工作：

- 使用椭圆曲线 Feldman VSS

- 支持分块处理（固定消息大小 15767 字节）

- 测试不同委员会大小（3-20）和多数阈值 (n+1)/2

- 测量 2 个阶段的性能：

1. ShareTime：分片生成（为每个块生成多项式系数和分片）

1. CommitmentTime：承诺生成（生成 VSS 承诺）

- 每个配置测试 100 次，计算平均时间和成功率
## Effectiveness Evaluation