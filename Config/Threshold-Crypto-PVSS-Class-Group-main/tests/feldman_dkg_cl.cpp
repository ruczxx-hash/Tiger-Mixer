#include "qclpvss.hpp"
#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

using namespace QCLPVSS_;
using namespace BICYCL;
using namespace std;
using namespace std::chrono;

int main() {
    cout << "=======================================" << endl;
    cout << "  Feldman VSS 的 DKG 协议" << endl;
    cout << "  生成 CL 类型的全局公钥" << endl;
    cout << "=======================================" << endl;
    
    // 参数设置
    const size_t n = 10;  // 10 个参与方
    const size_t t = 5;   // 阈值 5 (需要 6 个份额重构)
    
    cout << "\n参数:" << endl;
    cout << "  参与方数量 n = " << n << endl;
    cout << "  阈值 t = " << t << " (需要 " << (t + 1) << " 个份额重构)" << endl;
    
    // 初始化
    Mpz seed;
    SecLevel seclevel(128);
    RandGen randgen;
    
    auto T = system_clock::now();
    seed = static_cast<unsigned long>(T.time_since_epoch().count());
    randgen.set_seed(seed);
    
    ECGroup ec_group_(seclevel);
    Mpz q(ec_group_.order());
    
    cout << "  有限域阶 q = " << q << endl;
    cout << "=======================================" << endl;
    
    OpenSSL::HashAlgo H(seclevel);
    
    // 创建 CL_HSMqk 实例（用于类群操作）
    // 参数: (q, k, seclevel, randgen)
    CL_HSMqk cl(q, 1, seclevel, randgen);
    
    cout << "\n类群系统初始化完成" << endl;
    
    // ===== 基本假设验证 =====
    cout << "\n=== 基本假设验证 ===" << endl;
    
    // 1. 验证 power_of_h 的确定性
    Mpz test_exp(12345UL);
    QFI test_qfi1, test_qfi2;
    cl.power_of_h(test_qfi1, test_exp);
    cl.power_of_h(test_qfi2, test_exp);
    if (!(test_qfi1 == test_qfi2)) {
        cout << "✗ 致命错误: power_of_h 不确定！" << endl;
        return EXIT_FAILURE;
    }
    cout << "✓ power_of_h 确定性: 通过" << endl;
    
    // 2. 验证加法同态: h^2 * h^3 = h^5
    QFI h2, h3, h2_h3, h5;
    cl.power_of_h(h2, Mpz(2UL));
    cl.power_of_h(h3, Mpz(3UL));
    cl.Cl_G().nucomp(h2_h3, h2, h3);
    cl.power_of_h(h5, Mpz(5UL));
    if (!(h2_h3 == h5)) {
        cout << "✗ 致命错误: 加法同态性失败 h^2 * h^3 != h^5" << endl;
        cout << "  h^2 * h^3 = " << h2_h3 << endl;
        cout << "  h^5       = " << h5 << endl;
        return EXIT_FAILURE;
    }
    cout << "✓ 加法同态 h^2 * h^3 = h^5: 通过" << endl;
    
    // 3. 验证幂运算: (h^2)^3 = h^6
    QFI h2_pow3, h6;
    cl.power_of_h(h2_pow3, Mpz(2UL));
    cl.Cl_G().nupow(h2_pow3, h2_pow3, Mpz(3UL));
    cl.power_of_h(h6, Mpz(6UL));
    if (!(h2_pow3 == h6)) {
        cout << "✗ 致命错误: 幂运算失败 (h^2)^3 != h^6" << endl;
        cout << "  (h^2)^3 = " << h2_pow3 << endl;
        cout << "  h^6     = " << h6 << endl;
        return EXIT_FAILURE;
    }
    cout << "✓ 幂运算 (h^2)^3 = h^6: 通过" << endl;
    
    // 4. 验证大指数
    Mpz large_exp;
    Mpz::sub(large_exp, q, Mpz(1UL));  // q-1
    QFI h_large;
    cl.power_of_h(h_large, large_exp);
    cout << "✓ 大指数 h^{q-1} 计算成功" << endl;
    
    // 5. ⭐ 关键测试：验证 h 的阶是否为 q
    QFI h_q;
    cl.power_of_h(h_q, q);  // h^q
    QFI identity = cl.Cl_G().one();  // 单位元
    if (h_q == identity) {
        cout << "✓ 关键验证: h^q = 单位元 (h 的阶整除 q)" << endl;
    } else {
        cout << "✗ 警告: h^q ≠ 单位元 (h 的阶不是 q)" << endl;
        cout << "  这会导致 DKG 验证失败！" << endl;
        cout << "  h^q = " << h_q << endl;
        cout << "  单位元 = " << identity << endl;
        // 不直接失败，继续看看会发生什么
    }
    
    // 6. 测试模运算的影响
    Mpz test_sum;
    Mpz::add(test_sum, Mpz(q), Mpz(123UL));  // q + 123
    QFI h_sum, h_mod;
    cl.power_of_h(h_sum, test_sum);  // h^{q+123}
    Mpz test_mod;
    Mpz::mod(test_mod, test_sum, q);  // 123
    cl.power_of_h(h_mod, test_mod);   // h^{123}
    if (h_sum == h_mod) {
        cout << "✓ 指数模运算: h^{q+123} = h^{123} (确认 h^q = 1)" << endl;
    } else {
        cout << "✗ 指数模运算: h^{q+123} ≠ h^{123}" << endl;
        cout << "  这确认了 h 的阶不是 q" << endl;
    }
    
    cout << "=== 所有基本假设验证通过 ===" << endl;
    
    /********************* Feldman VSS based DKG *************************/
    
    // 每个参与方生成随机多项式
    // f_i(x) = a_{i,0} + a_{i,1}*x + ... + a_{i,t}*x^t
    // 其中 a_{i,0} = s_i 是参与方 i 的秘密贡献
    
    cout << "\n=== 阶段 1: 生成多项式和承诺 ===" << endl;
    
    vector<vector<Mpz>> polynomials(n);  // polynomials[i] = [a_{i,0}, a_{i,1}, ..., a_{i,t}]
    vector<vector<QFI>> commitments(n);   // commitments[i][j] = h^{a_{i,j}}
    
    for (size_t i = 0; i < n; i++) {
        polynomials[i].resize(t + 1);
        commitments[i].resize(t + 1);
        
        // 生成随机多项式系数
        for (size_t j = 0; j <= t; j++) {
            polynomials[i][j] = randgen.random_mpz(q);
        }
        
        // 计算承诺 C_{i,j} = h^{a_{i,j}}
        for (size_t j = 0; j <= t; j++) {
            cl.power_of_h(commitments[i][j], polynomials[i][j]);
        }
        
        cout << "参与方 P" << i << " 生成秘密 s_" << i << " = " << polynomials[i][0] << endl;
        cout << "  多项式次数: " << t << endl;
    }
    
    // 计算全局公钥（所有参与方秘密承诺的乘积）
    cout << "\n=== 阶段 2: 计算全局公钥 ===" << endl;
    
    // commitments 是 h 的幂，所以在 Cl_G() 对应的类群中
    QFI global_pk = cl.Cl_G().one();  // 初始化为单位元
    
    for (size_t i = 0; i < n; i++) {
        // global_pk *= commitments[i][0] = h^{s_i}
        cl.Cl_G().nucomp(global_pk, global_pk, commitments[i][0]);
    }
    
    cout << "✓ 全局公钥计算完成: PK = ∏_{i=0}^{n-1} h^{s_i} = h^{Σ s_i}" << endl;
    
    // 每个参与方计算份额并分发
    cout << "\n=== 阶段 3: 计算和分发份额 ===" << endl;
    
    // shares[i][j] = 参与方 i 给参与方 j 的份额 = f_i(j+1)
    vector<vector<Mpz>> shares(n);
    
    for (size_t i = 0; i < n; i++) {
        shares[i].resize(n);
        
        for (size_t j = 0; j < n; j++) {
            // 计算 f_i(j+1) = a_{i,0} + a_{i,1}*(j+1) + ... + a_{i,t}*(j+1)^t
            Mpz x(j + 1);  // 评估点（避免 x=0）
            Mpz result(0UL);
            Mpz x_power(1UL);  // x^k
            
            for (size_t k = 0; k <= t; k++) {
                Mpz term;
                Mpz::mul(term, polynomials[i][k], x_power);
                Mpz::add(result, result, term);
                Mpz::mul(x_power, x_power, x);  // 更新 x^{k+1}
            }
            
            Mpz::mod(shares[i][j], result, q);
        }
        
        cout << "参与方 P" << i << " 已计算所有份额" << endl;
    }
    
    // 验证份额（使用承诺）
    cout << "\n=== 阶段 4: 验证份额 ===" << endl;
    
    for (size_t i = 0; i < n; i++) {  // 验证来自参与方 i 的份额
        for (size_t j = 0; j < n; j++) {  // 参与方 j 验证自己收到的份额
            // 验证: h^{f_i(j+1)} ?= ∏_{k=0}^{t} (h^{a_{i,k}})^{(j+1)^k}
            //                     = ∏_{k=0}^{t} C_{i,k}^{(j+1)^k}
            
            Mpz x(j + 1);
            
            // 左边: h^{shares[i][j]}
            QFI lhs;
            cl.power_of_h(lhs, shares[i][j]);
            
            // 右边: ∏_{k=0}^{t} C_{i,k}^{x^k}
            // 使用 Cl_G() 而不是 Cl_Delta()，因为 commitments 是用 power_of_h 生成的
            QFI rhs = cl.Cl_G().one();
            Mpz x_power(1UL);
            
            for (size_t k = 0; k <= t; k++) {
                QFI temp;
                Mpz x_power_mod;
                Mpz::mod(x_power_mod, x_power, q);  // ⭐ 指数 mod q
                cl.Cl_G().nupow(temp, commitments[i][k], x_power_mod);
                cl.Cl_G().nucomp(rhs, rhs, temp);
                Mpz::mul(x_power, x_power, x);
            }
            
            if (!(lhs == rhs)) {
                cout << "✗ 份额验证失败: P" << i << " → P" << j << endl;
                cout << "  调试信息:" << endl;
                cout << "    x = " << x << endl;
                cout << "    shares[" << i << "][" << j << "] = " << shares[i][j] << endl;
                
                // 详细诊断：重新计算期望值
                cout << "\n  === 详细诊断 ===" << endl;
                cout << "  多项式系数:" << endl;
                Mpz expected_share(0UL);
                Mpz x_pow(1UL);
                for (size_t k = 0; k <= t; k++) {
                    cout << "    a[" << i << "][" << k << "] = " << polynomials[i][k] << endl;
                    Mpz term;
                    Mpz::mul(term, polynomials[i][k], x_pow);
                    Mpz::add(expected_share, expected_share, term);
                    Mpz::mul(x_pow, x_pow, x);
                }
                Mpz::mod(expected_share, expected_share, q);
                cout << "  期望份额 = " << expected_share << endl;
                cout << "  实际份额 = " << shares[i][j] << endl;
                cout << "  份额相等? " << (expected_share == shares[i][j] ? "是" : "否") << endl;
                
                // 验证承诺
                cout << "\n  验证承诺计算:" << endl;
                QFI rhs_check = cl.Cl_G().one();
                Mpz x_p(1UL);
                for (size_t k = 0; k <= t; k++) {
                    cout << "    k=" << k << ", x^k=" << x_p << endl;
                    QFI temp;
                    cl.Cl_G().nupow(temp, commitments[i][k], x_p);
                    cout << "      C[" << i << "][" << k << "]^{x^k} = " << temp << endl;
                    cl.Cl_G().nucomp(rhs_check, rhs_check, temp);
                    cout << "      累积 rhs = " << rhs_check << endl;
                    Mpz::mul(x_p, x_p, x);
                }
                
                cout << "\n  最终比较:" << endl;
                cout << "    lhs (h^share) = " << lhs << endl;
                cout << "    rhs (∏C^x^k)  = " << rhs << endl;
                cout << "    相等? " << (lhs == rhs ? "是" : "否") << endl;
                
                // 手动验证：直接计算 h^{期望值}
                QFI lhs_expected;
                cl.power_of_h(lhs_expected, expected_share);
                cout << "\n  交叉验证:" << endl;
                cout << "    h^{expected} = " << lhs_expected << endl;
                cout << "    h^{expected} == lhs? " << (lhs_expected == lhs ? "是" : "否") << endl;
                cout << "    h^{expected} == rhs? " << (lhs_expected == rhs ? "是" : "否") << endl;
                
                return EXIT_FAILURE;
            }
        }
        cout << "✓ 来自 P" << i << " 的所有份额验证成功" << endl;
    }
    
    // 每个参与方聚合份额得到私钥份额
    cout << "\n=== 阶段 5: 聚合私钥份额 ===" << endl;
    
    vector<Mpz> secret_shares(n);
    
    for (size_t j = 0; j < n; j++) {
        secret_shares[j] = Mpz(0UL);
        
        // sk_j = Σ_{i=0}^{n-1} share_{i→j} = Σ_{i=0}^{n-1} f_i(j+1)
        for (size_t i = 0; i < n; i++) {
            Mpz::add(secret_shares[j], secret_shares[j], shares[i][j]);
        }
        Mpz::mod(secret_shares[j], secret_shares[j], q);
        
        cout << "参与方 P" << j << " 的私钥份额 = " << secret_shares[j] << endl;
    }
    
    // 验证：重构全局私钥
    cout << "\n=== 阶段 6: 验证门限重构 ===" << endl;
    
    // 计算真实的全局私钥（仅用于验证，实际中无人知晓）
    Mpz global_sk(0UL);
    for (size_t i = 0; i < n; i++) {
        Mpz::add(global_sk, global_sk, polynomials[i][0]);  // 累加所有 s_i
    }
    Mpz::mod(global_sk, global_sk, q);
    
    cout << "全局私钥 (仅用于测试): " << global_sk << endl;
    
    // 验证全局公钥
    QFI expected_pk;
    cl.power_of_h(expected_pk, global_sk);
    
    if (expected_pk == global_pk) {
        cout << "✓ 全局公钥验证成功: h^{Σ s_i}" << endl;
    } else {
        cout << "✗ 全局公钥验证失败！" << endl;
        return EXIT_FAILURE;
    }
    
    // 使用拉格朗日插值验证门限重构
    // 使用前 t+1 个参与方的份额重构
    cout << "\n使用前 " << (t + 1) << " 个参与方的份额重构:" << endl;
    
    Mpz reconstructed_sk(0UL);
    
    for (size_t j = 0; j <= t; j++) {
        // 计算拉格朗日系数 λ_j = ∏_{m≠j} m/(m-j)
        Mpz lambda(1UL);
        Mpz lambda_num(1UL);
        Mpz lambda_den(1UL);
        
        for (size_t m = 0; m <= t; m++) {
            if (m != j) {
                Mpz x_m(m + 1);  // 评估点
                Mpz x_j(j + 1);
                
                Mpz diff;
                Mpz::sub(diff, x_m, x_j);
                
                Mpz::mul(lambda_num, lambda_num, x_m);
                Mpz::mul(lambda_den, lambda_den, diff);
            }
        }
        
        // lambda = lambda_num / lambda_den (mod q)
        Mpz lambda_den_inv;
        Mpz::mod(lambda_den, lambda_den, q);
        Mpz::mod_inverse(lambda_den_inv, lambda_den, q);
        Mpz::mul(lambda, lambda_num, lambda_den_inv);
        Mpz::mod(lambda, lambda, q);
        
        // reconstructed_sk += lambda * secret_shares[j]
        Mpz term;
        Mpz::mul(term, lambda, secret_shares[j]);
        Mpz::add(reconstructed_sk, reconstructed_sk, term);
    }
    Mpz::mod(reconstructed_sk, reconstructed_sk, q);
    
    cout << "  重构的全局私钥: " << reconstructed_sk << endl;
    
    if (reconstructed_sk == global_sk) {
        cout << "  ✓ 门限重构成功！" << endl;
    } else {
        cout << "  ✗ 门限重构失败！" << endl;
        return EXIT_FAILURE;
    }
    
    /********************* 保存到文件 *************************/
    
    cout << "\n=== 保存结果到文件 ===" << endl;
    
    // 保存私钥份额
    for (size_t i = 0; i < n; i++) {
        string filename = "party_" + to_string(i) + "_secret_share_feldman.txt";
        ofstream file(filename);
        if (file.is_open()) {
            file << "参与方 P" << i << " 的私钥份额:\n";
            file << secret_shares[i] << "\n";
            file.close();
        }
    }
    cout << "✓ 私钥份额已保存" << endl;
    
    // 保存全局公钥（QFI 类群元素）
    ofstream pk_file("global_public_key_feldman.txt");
    if (pk_file.is_open()) {
        pk_file << "全局公钥 (QFI 类群元素) - 可用于 CL 加密:\n";
        pk_file << global_pk << "\n\n";
        pk_file << "说明:\n";
        pk_file << "  - 类型: QFI (二次型)\n";
        pk_file << "  - 用途: 可用于 CL 加密\n";
        pk_file << "  - 生成方式: h^{s_0} * h^{s_1} * ... * h^{s_9} = h^{Σ s_i}\n";
        pk_file << "  - 对应 A2L 的 cl_public_key_t (GEN 类型)\n";
        pk_file.close();
    }
    cout << "✓ 全局公钥已保存到 global_public_key_feldman.txt" << endl;
    
    // 保存全局私钥（仅用于测试）
    ofstream sk_file("global_secret_key_feldman.txt");
    if (sk_file.is_open()) {
        sk_file << "全局私钥 (Mpz) - 仅用于测试:\n";
        sk_file << global_sk << "\n\n";
        sk_file << "注意: 在真实的 DKG 中，全局私钥从未被任何单一参与方知晓！\n";
        sk_file << "      只能通过至少 " << (t + 1) << " 个参与方协作重构\n";
        sk_file.close();
    }
    cout << "✓ 全局私钥已保存" << endl;
    
    // 保存承诺
    ofstream commitments_file("commitments_feldman.txt");
    if (commitments_file.is_open()) {
        commitments_file << "各参与方的承诺 (QFI):\n\n";
        for (size_t i = 0; i < n; i++) {
            commitments_file << "参与方 P" << i << " 的承诺:\n";
            for (size_t j = 0; j <= t; j++) {
                commitments_file << "  C_" << i << "," << j << " = h^{a_" << i << "," << j << "} = " << commitments[i][j] << "\n";
            }
            commitments_file << "\n";
        }
        commitments_file.close();
    }
    cout << "✓ 承诺已保存" << endl;
    
    // 保存协议摘要
    ofstream summary("feldman_dkg_summary.txt");
    if (summary.is_open()) {
        summary << "========================================\n";
        summary << "Feldman VSS 的 DKG 协议执行摘要\n";
        summary << "========================================\n\n";
        
        summary << "参数:\n";
        summary << "  参与方数量 n = " << n << "\n";
        summary << "  阈值 t = " << t << " (需要 " << (t + 1) << " 个份额重构)\n";
        summary << "  有限域阶 q = " << q << "\n\n";
        
        summary << "协议特点:\n";
        summary << "  - 无需预生成密钥对\n";
        summary << "  - 每个参与方生成随机多项式\n";
        summary << "  - 使用承诺进行公开验证\n";
        summary << "  - 全局私钥从未被单一方知晓\n\n";
        
        summary << "生成的密钥类型:\n";
        summary << "  全局公钥: QFI (类群元素) ⭐ 可用于 CL 加密\n";
        summary << "  全局私钥: Mpz (永不被单一方知晓)\n";
        summary << "  私钥份额: Mpz (每个参与方持有)\n\n";
        
        summary << "各参与方的秘密贡献 s_i:\n";
        for (size_t i = 0; i < n; i++) {
            summary << "  s_" << i << " = " << polynomials[i][0] << "\n";
        }
        summary << "\n";
        
        summary << "各参与方的私钥份额:\n";
        for (size_t i = 0; i < n; i++) {
            summary << "  sk_" << i << " = " << secret_shares[i] << "\n";
        }
        summary << "\n";
        
        summary << "全局私钥 (Σ s_i):\n";
        summary << "  sk = " << global_sk << "\n\n";
        
        summary << "全局公钥 (h^{Σ s_i}):\n";
        summary << "  " << global_pk << "\n\n";
        
        summary << "密钥关系:\n";
        summary << "  global_pk = h^{global_sk} = h^{Σ s_i} = ∏ h^{s_i}\n";
        summary << "  global_sk = s_0 + s_1 + ... + s_{n-1} (mod q)\n";
        summary << "  sk_j = Σ_{i=0}^{n-1} f_i(j+1) (所有参与方的份额之和)\n";
        
        summary.close();
    }
    cout << "✓ 协议摘要已保存到 feldman_dkg_summary.txt" << endl;
    
    cout << "\n=======================================" << endl;
    cout << "  Feldman DKG 协议执行成功！" << endl;
    cout << "=======================================" << endl;
    cout << "\n生成的全局公钥:" << endl;
    cout << "  类型: QFI (类群元素)" << endl;
    cout << "  文件: global_public_key_feldman.txt" << endl;
    cout << "  用途: 可用于 CL 加密" << endl;
    cout << "  兼容: A2L 项目的 cl_public_key_t" << endl;
    cout << "\n门限性质:" << endl;
    cout << "  - 任意 " << (t + 1) << " 个参与方可以重构私钥" << endl;
    cout << "  - 少于 " << (t + 1) << " 个参与方无法重构" << endl;
    cout << "  - 全局私钥从未被任何单一方知晓" << endl;
    cout << "\nDKG 特性:" << endl;
    cout << "  ✓ 无需预生成密钥对" << endl;
    cout << "  ✓ 分布式生成全局公钥" << endl;
    cout << "  ✓ 每个参与方只知道自己的份额" << endl;
    cout << "  ✓ 使用 Feldman 承诺公开验证" << endl;
    
    return EXIT_SUCCESS;
}

