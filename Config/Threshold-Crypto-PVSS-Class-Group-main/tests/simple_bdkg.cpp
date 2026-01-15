#include "bdkg.hpp"
#include "nizk_sh_ext.hpp"
#include <chrono>
#include <memory>
#include <fstream>

using namespace Application;
using namespace QCLPVSS_;
using namespace BICYCL;
using namespace std;
using namespace std::chrono;
using namespace NIZK;

int main(int argc, char* argv[]) {
    cout << "=======================================" << endl;
    cout << "  简化的 BDKG 协议 (完全基于 test_bdkg)" << endl;
    cout << "=======================================" << endl;
    
    Mpz seed;
    SecLevel seclevel(128);
    RandGen randgen;

    ECGroup ec_group_(seclevel);

    auto T = std::chrono::system_clock::now();
    seed = static_cast<unsigned long>(T.time_since_epoch().count());
    randgen.set_seed(seed);

    Mpz q(ec_group_.order());

    HashAlgo H(seclevel);

    size_t n(10);  // 10个参与方
    size_t t(5);   // 阈值5（需要6个份额重构）
    // 注意：NIZK 内部使用 t_nizk = n - t - 2 = 10 - 5 - 2 = 3

    cout << "参数设置:" << endl;
    cout << "  参与方总数 n = " << n << endl;
    cout << "  阈值 t = " << t << " (需要 " << (t + 1) << " 个份额重构)" << endl;
    cout << "  有限域阶 q = " << q << endl;
    cout << "=======================================" << endl;

    BDKG bdkg(seclevel, H, randgen, ec_group_, q, 1, n, t);

    cout << "\n初始化完成，各方密钥已生成" << endl;

    // 保存初始密钥
    ofstream init_keys("initial_keys_bdkg.txt");
    if (init_keys.is_open()) {
        init_keys << "各参与方的初始密钥对 (CL_HSMqk):\n\n";
        for (size_t i = 0; i < n; i++) {
            init_keys << "参与方 P" << i << ":\n";
            init_keys << "  私钥: " << *bdkg.sks_[i] << "\n";
            init_keys << "  公钥: " << bdkg.pks_[i]->get() << "\n\n";
        }
        init_keys.close();
        cout << "✓ 初始密钥已保存到 initial_keys_bdkg.txt" << endl;
    }

    vector<unique_ptr<EncSharesExt>> enc_sh(n);

    vector<Mpz> tsks;
    tsks.reserve(t + 1);

    vector<ECPoint> tpks;
    tpks.reserve(t + 1);
    generate_n(back_inserter(tpks), t + 1, [&] { return ECPoint(ec_group_); });

    /********************* 1 Round DKG *************************/

    cout << "\n=== 阶段 1: 分发份额 ===" << endl;
    // parties share random secret
    for (size_t j = 0; j < t + 1; j++) {
        Mpz s_j = (randgen.random_mpz(q));
        cout << "参与方 P" << j << " 生成秘密并分发份额" << endl;
        enc_sh[j] = bdkg.dist(s_j, bdkg.pks_);
    }

    auto start = std::chrono::system_clock::now();

    cout << "\n=== 阶段 2: 验证份额 ===" << endl;
    // parties verify shares
    for (size_t j = 0; j < t + 1; j++) {
        if (!(enc_sh[j]->pf_->verify(bdkg.pks_, *enc_sh[j]->Bs_,
                *enc_sh[j]->Ds_, enc_sh[j]->R_))) {
            cout << "✗ 参与方 P" << j << " 的份额验证失败！" << endl;
            return EXIT_FAILURE;
        }
        cout << "✓ 参与方 P" << j << " 的份额验证成功" << endl;
    }

    auto stop = std::chrono::system_clock::now();
    auto ms_int = duration_cast<milliseconds>(stop - start);
    cout << "验证时间: " << ms_int.count() << " ms" << endl;

    /********************* Output sharing *************************/

    vector<vector<shared_ptr<QFI>>> Bs_transpose;
    Bs_transpose.reserve(t + 1);

    vector<QFI> Rs_transpose;
    Rs_transpose.reserve(t + 1);

    // Simulate sharing of Bs and Rs between parties
    for (size_t i = 0; i < t + 1; i++) {
        Bs_transpose.emplace_back(vector<shared_ptr<QFI>>());
        Bs_transpose[i].reserve(t + 1);

        for (size_t j = 0; j < t + 1; j++)
            Bs_transpose[i].emplace_back(enc_sh[j]->Bs_->at(i));
    }

    for (size_t i = 0; i < t + 1; i++)
        Rs_transpose.emplace_back(enc_sh[i]->R_);

    /********************* GLOBAL OUTPUT *************************/

    // |Q| = n as all proofs verifies above
    size_t Q = t + 1;

    start = std::chrono::system_clock::now();

    cout << "\n=== 阶段 3: 计算公钥 ===" << endl;
    // Compute tpks[i]
    for (size_t i = 0; i < t + 1; i++) {
        for (size_t j = 0; j < Q; j++)
            ec_group_.ec_add(tpks[i], tpks[i], *enc_sh[j]->Ds_->at(i));
    }

    stop = std::chrono::system_clock::now();
    ms_int = duration_cast<milliseconds>(stop - start);
    cout << "计算 " << (t + 1) << " 个公钥份额: " << ms_int.count() << " ms" << endl;

    start = std::chrono::system_clock::now();

    // Compute global public key
    unique_ptr<ECPoint> tpk = move(bdkg.compute_tpk(tpks));

    stop = std::chrono::system_clock::now();
    ms_int = duration_cast<milliseconds>(stop - start);
    cout << "计算全局公钥: " << ms_int.count() << " ms" << endl;

    /********************* PRIVATE OUTPUT *************************/

    start = std::chrono::system_clock::now();

    cout << "\n=== 阶段 4: 计算私钥份额 ===" << endl;
    // Compute private key share 0
    tsks.emplace_back(
        bdkg.compute_tsk_i(Bs_transpose[0], Rs_transpose, *bdkg.sks_[0]));
    cout << "✓ 参与方 P0 的私钥份额已计算" << endl;

    stop = std::chrono::system_clock::now();
    ms_int = duration_cast<milliseconds>(stop - start);
    cout << "计算 1 个私钥份额: " << ms_int.count() << " ms" << endl;

    // Compute private key share
    for (size_t i = 1; i < t + 1; i++) {
        tsks.emplace_back(
            bdkg.compute_tsk_i(Bs_transpose[i], Rs_transpose, *bdkg.sks_[i]));
        cout << "✓ 参与方 P" << i << " 的私钥份额已计算" << endl;
    }

    /********************* VERIFY OUTPUT *************************/

    cout << "\n=== 阶段 5: 验证密钥对 ===" << endl;
    // Verify tsk[i] relates to tpk[i]
    if (!bdkg.verify_partial_keypairs(tsks, tpks)) {
        cout << "✗ 部分密钥对验证失败！" << endl;
        return EXIT_FAILURE;
    }
    cout << "✓ 所有部分密钥对验证成功" << endl;

    // Verify tsk relates to tpk
    Mpz tsk = bdkg.compute_tsk(tsks);
    if (!bdkg.verify_global_keypair(tsk, *tpk)) {
        cout << "✗ 全局密钥对验证失败！" << endl;
        return EXIT_FAILURE;
    }
    cout << "✓ 全局密钥对验证成功" << endl;

    /********************* SAVE TO FILES *************************/

    cout << "\n=== 保存结果到文件 ===" << endl;

    // 保存私钥份额
    for (size_t i = 0; i < t + 1; i++) {
        string filename = "party_" + to_string(i) + "_secret_share_bdkg.txt";
        ofstream file(filename);
        if (file.is_open()) {
            file << "参与方 P" << i << " 的私钥份额:\n";
            file << tsks[i] << "\n";
            file.close();
        }
    }
    cout << "✓ 私钥份额已保存" << endl;

    // 保存全局私钥
    ofstream tsk_file("global_secret_key_bdkg.txt");
    if (tsk_file.is_open()) {
        tsk_file << "全局私钥 (重构自 " << (t + 1) << " 个份额):\n";
        tsk_file << tsk << "\n";
        tsk_file.close();
    }
    cout << "✓ 全局私钥已保存" << endl;

    // 保存CL公钥（可用于A2L）
    ofstream cl_pks_file("cl_public_keys_bdkg.txt");
    if (cl_pks_file.is_open()) {
        cl_pks_file << "各参与方的 CL 公钥 (QFI 类群元素) - 可用于 A2L:\n\n";
        for (size_t i = 0; i < n; i++) {
            cl_pks_file << "参与方 P" << i << ":\n";
            cl_pks_file << bdkg.pks_[i]->get() << "\n\n";
        }
        cl_pks_file.close();
    }
    cout << "✓ CL 公钥已保存到 cl_public_keys_bdkg.txt" << endl;

    // 保存协议摘要
    ofstream summary("bdkg_summary.txt");
    if (summary.is_open()) {
        summary << "========================================\n";
        summary << "BDKG 协议执行摘要\n";
        summary << "========================================\n\n";
        summary << "参数:\n";
        summary << "  n = " << n << " (参与方总数)\n";
        summary << "  t = " << t << " (阈值)\n";
        summary << "  需要 " << (t + 1) << " 个份额重构私钥\n\n";
        
        summary << "参与 DKG 的参与方: " << (t + 1) << " 个\n";
        summary << "  (标准 DKG 中所有 n 个参与方都参与，\n";
        summary << "   这里为了测试阈值只用了 t+1 个)\n\n";
        
        summary << "私钥份额:\n";
        for (size_t i = 0; i < t + 1; i++) {
            summary << "  tsk[" << i << "] = " << tsks[i] << "\n";
        }
        summary << "\n全局私钥:\n";
        summary << "  tsk = " << tsk << "\n\n";
        
        summary << "密钥类型说明:\n";
        summary << "  初始公钥 (pk_i): QFI 类群元素 - 可用于 A2L\n";
        summary << "  全局公钥 (tpk): ECPoint 椭圆曲线点\n";
        summary << "  私钥: Mpz 大整数\n";
        
        summary.close();
    }
    cout << "✓ 协议摘要已保存到 bdkg_summary.txt" << endl;

    cout << "\n=======================================" << endl;
    cout << "  BDKG 协议执行成功！" << endl;
    cout << "=======================================" << endl;
    cout << "\n重要文件:" << endl;
    cout << "  cl_public_keys_bdkg.txt - QFI 公钥，可用于 A2L" << endl;
    cout << "  bdkg_summary.txt - 完整摘要" << endl;

    return EXIT_SUCCESS;
}
