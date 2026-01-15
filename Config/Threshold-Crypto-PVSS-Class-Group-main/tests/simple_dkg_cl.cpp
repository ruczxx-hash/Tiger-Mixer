#include <bicycl.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>

using namespace BICYCL;
using namespace std;

// 简化的 Shamir 秘密共享类
class SimplifiedSSS {
private:
    RandGen& randgen_;

public:
    SimplifiedSSS(RandGen& randgen) : randgen_(randgen) {}

    // 生成 t 次多项式: f(x) = a_0 + a_1*x + ... + a_t*x^t
    void generatePolynomial(const Mpz& secret, vector<Mpz>& coefficients, 
                           const size_t t, const Mpz& q) const {
        coefficients.clear();
        coefficients.push_back(secret);  // a_0 = secret
        
        for (size_t i = 0; i < t; i++) {
            coefficients.push_back(randgen_.random_mpz(q));  // a_1, ..., a_t
        }
    }

    // 计算 f(x) = a_0 + a_1*x + ... + a_t*x^t mod q
    Mpz evaluatePolynomial(const vector<Mpz>& coefficients, size_t x, const Mpz& q) const {
        Mpz result = coefficients[0];
        Mpz temp, x_power;
        
        for (size_t i = 1; i < coefficients.size(); i++) {
            Mpz::pow_mod(x_power, Mpz(x), Mpz(i), q);  // x^i
            Mpz::mul(temp, coefficients[i], x_power);   // a_i * x^i
            Mpz::add(result, result, temp);             // 累加
        }
        
        Mpz::mod(result, result, q);
        return result;
    }

    // 使用拉格朗日插值重构秘密
    Mpz reconstructSecret(const vector<pair<size_t, Mpz>>& shares, 
                         const size_t k, const Mpz& q) const {
        if (shares.size() < k) {
            throw std::invalid_argument("份额数量不足，无法重构秘密");
        }

        Mpz secret(0UL);

        // 拉格朗日插值
        for (size_t j = 0; j < k; j++) {
            Mpz numerator(1UL);
            Mpz denominator(1UL);
            Mpz xj(shares[j].first);

            for (size_t m = 0; m < k; m++) {
                if (m == j) continue;

                Mpz xm(shares[m].first);
                Mpz::mul(numerator, numerator, xm);           // ∏ x_m
                Mpz::sub(xm, xm, xj);                         // x_m - x_j
                Mpz::mul(denominator, denominator, xm);       // ∏ (x_m - x_j)
            }

            Mpz::mod_inverse(denominator, denominator, q);
            Mpz::mul(numerator, numerator, denominator);      // λ_j
            Mpz::addmul(secret, numerator, shares[j].second); // += λ_j * y_j
        }

        Mpz::mod(secret, secret, q);
        return secret;
    }
};

// 基于类群的 Joint-Feldman DKG 协议
class SimpleJointFeldmanDKG_CL {
private:
    const size_t n_;        // 参与方总数
    const size_t t_;        // 阈值 (需要 t+1 个份额重构)
    const Mpz& q_;          // 素数阶
    CL_HSMqk& cl_;          // 类群系统
    RandGen& randgen_;
    SimplifiedSSS sss_;

    // 每个参与方的数据
    vector<Mpz> secrets_;                           // 每个参与方的随机秘密 z_i (私钥)
    vector<Mpz> secret_keys_;                       // 每个参与方的私钥 sk_i
    vector<QFI> public_keys_;                       // 每个参与方的公钥 pk_i = h^{sk_i}
    vector<vector<Mpz>> polynomials_;               // 每个参与方的多项式系数
    vector<vector<QFI>> commitments_;               // 每个参与方的承诺 A_ij = h^{a_ij}
    vector<vector<Mpz>> shares_matrix_;             // shares_matrix_[i][j] = f_i(j+1)
    
    // 最终结果
    vector<Mpz> secret_key_shares_;                 // 每个参与方的私钥份额
    vector<QFI> public_key_shares_;                 // 每个参与方的公钥份额
    QFI global_public_key_;                         // 全局公钥

public:
    SimpleJointFeldmanDKG_CL(size_t n, size_t t, const Mpz& q, CL_HSMqk& cl, RandGen& randgen)
        : n_(n), t_(t), q_(q), cl_(cl), randgen_(randgen), sss_(randgen),
          global_public_key_() {
        
        secrets_.resize(n_);
        secret_keys_.resize(n_);
        public_keys_.resize(n_, QFI());
        polynomials_.resize(n_);
        commitments_.resize(n_);
        shares_matrix_.resize(n_);
        secret_key_shares_.resize(n_, Mpz(0UL));
        public_key_shares_.resize(n_, QFI());
        
        for (size_t i = 0; i < n_; i++) {
            shares_matrix_[i].resize(n_);
        }
    }

    // 步骤 0: 密钥生成
    void phase0_keygen() {
        cout << "\n=== 阶段 0: 密钥生成 ===" << endl;
        
        for (size_t i = 0; i < n_; i++) {
            // 生成私钥 sk_i
            secret_keys_[i] = randgen_.random_mpz(q_);
            
            // 计算公钥 pk_i = h^{sk_i} (类群元素)
            cl_.power_of_h(public_keys_[i], secret_keys_[i]);
            
            cout << "参与方 P" << i << ":" << endl;
            cout << "  私钥 sk_" << i << " = " << secret_keys_[i] << endl;
            cout << "  公钥 pk_" << i << " = h^{sk_" << i << "} (类群元素)" << endl;
        }
    }

    // 步骤 1: 每个参与方 P_i 生成多项式并广播承诺
    void phase1_distribute() {
        cout << "\n=== 阶段 1: 生成多项式和承诺 ===" << endl;
        
        for (size_t i = 0; i < n_; i++) {
            // 生成随机秘密 z_i
            secrets_[i] = randgen_.random_mpz(q_);
            cout << "\n参与方 P" << i << " 生成秘密 z_" << i << " = " 
                 << secrets_[i] << endl;
            
            // 生成 t 次多项式 f_i(X)
            sss_.generatePolynomial(secrets_[i], polynomials_[i], t_, q_);
            
            cout << "参与方 P" << i << " 的多项式系数: ";
            for (size_t j = 0; j <= t_; j++) {
                cout << "a_" << i << j << " = " << polynomials_[i][j];
                if (j < t_) cout << ", ";
            }
            cout << endl;
            
            // 计算并广播承诺 A_ij = h^{a_ij} (类群元素)
            commitments_[i].resize(t_ + 1, QFI());
            cout << "参与方 P" << i << " 的承诺 (类群元素): ";
            for (size_t j = 0; j <= t_; j++) {
                cl_.power_of_h(commitments_[i][j], polynomials_[i][j]);
                cout << "A_" << i << j << " = h^{a_" << i << j << "}";
                if (j < t_) cout << ", ";
            }
            cout << endl;
            
            // 计算并私下发送份额 s_ij = f_i(j) 给每个参与方 P_j
            for (size_t j = 0; j < n_; j++) {
                shares_matrix_[i][j] = sss_.evaluatePolynomial(polynomials_[i], j + 1, q_);
                cout << "  发送给 P" << j << " 的份额: s_" << i << j 
                     << " = f_" << i << "(" << (j + 1) << ") = " 
                     << shares_matrix_[i][j] << endl;
            }
        }
    }

    // 步骤 2: 每个参与方验证收到的份额
    bool phase2_verify() {
        cout << "\n=== 阶段 2: 验证份额 ===" << endl;
        
        for (size_t j = 0; j < n_; j++) {  // 接收者
            cout << "\n参与方 P" << j << " 验证收到的份额:" << endl;
            
            for (size_t i = 0; i < n_; i++) {  // 发送者
                // 验证 h^{s_ij} = ∏_{k=0}^{t} (A_ik)^{(j+1)^k}
                
                // 左边: h^{s_ij}
                QFI lhs;
                cl_.power_of_h(lhs, shares_matrix_[i][j]);
                
                // 右边: ∏_{k=0}^{t} (A_ik)^{(j+1)^k}
                QFI rhs = cl_.Cl_Delta().one();  // 初始化为单位元（使用Cl_Delta）
                Mpz j_power;
                QFI temp;
                
                for (size_t k = 0; k <= t_; k++) {
                    Mpz::pow_mod(j_power, Mpz(j + 1), Mpz(k), q_);  // (j+1)^k
                    cl_.Cl_Delta().nupow(temp, commitments_[i][k], j_power);  // A_ik^{j^k}（使用Cl_Delta）
                    cl_.Cl_Delta().nucomp(rhs, rhs, temp);  // 使用Cl_Delta
                }
                
                // 验证
                if (lhs == rhs) {
                    cout << "  ✓ 来自 P" << i << " 的份额验证成功" << endl;
                } else {
                    cout << "  ✗ 来自 P" << i << " 的份额验证失败！" << endl;
                    return false;
                }
            }
        }
        
        cout << "\n所有份额验证通过！" << endl;
        return true;
    }

    // 步骤 3: 计算密钥
    void phase3_compute_keys() {
        cout << "\n=== 阶段 3: 计算密钥 ===" << endl;
        
        // 假设所有参与方都是合格的 (Qual = {0, 1, 2})
        vector<size_t> qual;
        for (size_t i = 0; i < n_; i++) {
            qual.push_back(i);
        }
        
        // 计算全局公钥 pk = ∏_{i∈Qual} A_i0 (类群乘法)
        cout << "\n计算全局公钥 pk = ∏_{i∈Qual} A_i0:" << endl;
        global_public_key_ = cl_.Cl_Delta().one();  // 单位元（使用Cl_Delta）
        for (size_t i : qual) {
            cl_.Cl_Delta().nucomp(global_public_key_, global_public_key_, commitments_[i][0]);
            cout << "  累加 A_" << i << "0" << endl;
        }
        cout << "全局公钥计算完成 (类群元素)" << endl;
        
        // 计算每个参与方的公钥份额 pk_j = ∏_{i∈Qual} A_ij
        cout << "\n计算公钥份额 pk_j = ∏_{i∈Qual} A_ij:" << endl;
        for (size_t j = 0; j < n_; j++) {
            public_key_shares_[j] = cl_.Cl_Delta().one();  // 单位元（使用Cl_Delta）
            for (size_t i : qual) {
                // 计算 A_ij = ∏_{k=0}^{t} (A_ik)^{(j+1)^k}
                QFI A_ij = cl_.Cl_Delta().one();  // 单位元（使用Cl_Delta）
                Mpz j_power;
                QFI temp;
                
                for (size_t k = 0; k <= t_; k++) {
                    Mpz::pow_mod(j_power, Mpz(j + 1), Mpz(k), q_);
                    cl_.Cl_Delta().nupow(temp, commitments_[i][k], j_power);  // 使用Cl_Delta
                    cl_.Cl_Delta().nucomp(A_ij, A_ij, temp);  // 使用Cl_Delta
                }
                
                cl_.Cl_Delta().nucomp(public_key_shares_[j], public_key_shares_[j], A_ij);
            }
            cout << "  pk_" << j << " 计算完成 (类群元素)" << endl;
        }
        
        // 计算每个参与方的私钥份额 sk_j = ∑_{i∈Qual} s_ij
        cout << "\n计算私钥份额 sk_j = ∑_{i∈Qual} s_ij:" << endl;
        for (size_t j = 0; j < n_; j++) {
            secret_key_shares_[j] = Mpz(0UL);
            for (size_t i : qual) {
                Mpz::add(secret_key_shares_[j], secret_key_shares_[j], shares_matrix_[i][j]);
            }
            Mpz::mod(secret_key_shares_[j], secret_key_shares_[j], q_);
            cout << "  sk_" << j << " = " << secret_key_shares_[j] << endl;
        }
    }

    // 步骤 4: 验证密钥对的正确性
    bool phase4_verify_keys() {
        cout << "\n=== 阶段 4: 验证密钥对 ===" << endl;
        
        // 验证每个参与方的密钥对 h^{sk_j} = pk_j
        for (size_t j = 0; j < n_; j++) {
            QFI computed_pk;
            cl_.power_of_h(computed_pk, secret_key_shares_[j]);
            
            if (computed_pk == public_key_shares_[j]) {
                cout << "  ✓ 参与方 P" << j << " 的密钥对验证成功: h^{sk_" << j << "} = pk_" << j << endl;
            } else {
                cout << "  ✗ 参与方 P" << j << " 的密钥对验证失败！" << endl;
                return false;
            }
        }
        
        // 验证可以用任意 t+1 个份额重构全局私钥
        cout << "\n验证秘密重构 (使用前 " << (t_ + 1) << " 个份额):" << endl;
        vector<pair<size_t, Mpz>> shares_for_reconstruction;
        for (size_t i = 0; i <= t_; i++) {
            shares_for_reconstruction.push_back({i + 1, secret_key_shares_[i]});
            cout << "  使用份额: (" << (i + 1) << ", sk_" << i << ")" << endl;
        }
        
        Mpz reconstructed_sk = sss_.reconstructSecret(shares_for_reconstruction, t_ + 1, q_);
        cout << "  重构的全局私钥 sk = " << reconstructed_sk << endl;
        
        // 验证 h^{sk} = pk
        QFI computed_global_pk;
        cl_.power_of_h(computed_global_pk, reconstructed_sk);
        
        if (computed_global_pk == global_public_key_) {
            cout << "  ✓ 全局密钥对验证成功: h^{sk} = pk" << endl;
            
            // 验证 sk = ∑ z_i
            Mpz expected_sk(0UL);
            for (size_t i = 0; i < n_; i++) {
                Mpz::add(expected_sk, expected_sk, secrets_[i]);
            }
            Mpz::mod(expected_sk, expected_sk, q_);
            cout << "  期望的全局私钥 sk = ∑ z_i = " << expected_sk << endl;
            
            if (reconstructed_sk == expected_sk) {
                cout << "  ✓ 重构的私钥与期望值一致" << endl;
            }
            
            return true;
        } else {
            cout << "  ✗ 全局密钥对验证失败！" << endl;
            return false;
        }
    }

    // 保存结果到文件
    void save_to_files() {
        cout << "\n=== 保存结果到文件 ===" << endl;
        
        // 保存每个参与方的私钥份额
        for (size_t i = 0; i < n_; i++) {
            string filename = "party_" + to_string(i) + "_secret_share.txt";
            ofstream file(filename);
            if (file.is_open()) {
                file << "参与方 P" << i << " 的私钥份额 (sk_" << i << "):\n";
                file << secret_key_shares_[i] << "\n";
                file.close();
                cout << "  ✓ 已保存到 " << filename << endl;
            }
        }
        
        // 保存全局公钥 (类群元素)
        ofstream pk_file("global_public_key_cl.txt");
        if (pk_file.is_open()) {
            pk_file << "全局公钥 (pk) - 类群元素 (QFI):\n";
            pk_file << global_public_key_ << "\n";
            pk_file.close();
            cout << "  ✓ 已保存到 global_public_key_cl.txt" << endl;
        }
        
        // 保存所有公钥份额
        ofstream pks_file("party_public_keys_cl.txt");
        if (pks_file.is_open()) {
            pks_file << "各参与方的公钥份额 - 类群元素 (QFI):\n\n";
            for (size_t i = 0; i < n_; i++) {
                pks_file << "参与方 P" << i << " 的公钥份额 (pk_" << i << "):\n";
                pks_file << public_key_shares_[i] << "\n\n";
            }
            pks_file.close();
            cout << "  ✓ 已保存到 party_public_keys_cl.txt" << endl;
        }
        
        // 保存初始密钥对
        ofstream keys_file("initial_keypairs_cl.txt");
        if (keys_file.is_open()) {
            keys_file << "各参与方的初始密钥对:\n\n";
            for (size_t i = 0; i < n_; i++) {
                keys_file << "参与方 P" << i << ":\n";
                keys_file << "  私钥 sk_" << i << " = " << secret_keys_[i] << "\n";
                keys_file << "  公钥 pk_" << i << " = " << public_keys_[i] << "\n\n";
            }
            keys_file.close();
            cout << "  ✓ 已保存到 initial_keypairs_cl.txt" << endl;
        }
        
        // 保存完整的协议摘要
        ofstream summary("dkg_summary_cl.txt");
        if (summary.is_open()) {
            summary << "========================================\n";
            summary << "Joint-Feldman DKG 协议执行摘要 (类群版本)\n";
            summary << "========================================\n\n";
            summary << "参数设置:\n";
            summary << "  参与方总数 n = " << n_ << "\n";
            summary << "  阈值 t = " << t_ << " (需要 " << (t_ + 1) << " 个份额重构)\n";
            summary << "  有限域阶 q = " << q_ << "\n\n";
            
            summary << "使用群类型: 类群 (Class Group)\n";
            summary << "  公钥类型: QFI (二次型)\n";
            summary << "  私钥类型: Mpz (大整数)\n\n";
            
            summary << "各参与方的初始秘密 (z_i):\n";
            for (size_t i = 0; i < n_; i++) {
                summary << "  z_" << i << " = " << secrets_[i] << "\n";
            }
            summary << "\n";
            
            summary << "各参与方的私钥份额 (sk_i):\n";
            for (size_t i = 0; i < n_; i++) {
                summary << "  sk_" << i << " = " << secret_key_shares_[i] << "\n";
            }
            summary << "\n";
            
            summary << "全局公钥 (类群元素):\n";
            summary << "  " << global_public_key_ << "\n";
            
            summary.close();
            cout << "  ✓ 已保存协议摘要到 dkg_summary_cl.txt" << endl;
        }
    }

    // 运行完整的 DKG 协议
    bool run() {
        cout << "\n=======================================" << endl;
        cout << "开始执行 Joint-Feldman DKG 协议 (类群版本)" << endl;
        cout << "=======================================" << endl;
        cout << "参与方数量: " << n_ << endl;
        cout << "阈值: " << t_ << " (需要 " << (t_ + 1) << " 个份额重构)" << endl;
        cout << "群类型: 类群 (Class Group)" << endl;
        cout << "=======================================" << endl;
        
        phase0_keygen();
        phase1_distribute();
        
        if (!phase2_verify()) {
            cout << "\n协议失败：份额验证未通过！" << endl;
            return false;
        }
        
        phase3_compute_keys();
        
        if (!phase4_verify_keys()) {
            cout << "\n协议失败：密钥验证未通过！" << endl;
            return false;
        }
        
        save_to_files();
        
        cout << "\n=======================================" << endl;
        cout << "DKG 协议执行成功！" << endl;
        cout << "=======================================" << endl;
        
        return true;
    }

    // 获取公钥（用于与 A2L 项目对接）
    const QFI& get_global_public_key() const {
        return global_public_key_;
    }

    const vector<QFI>& get_public_key_shares() const {
        return public_key_shares_;
    }

    const vector<Mpz>& get_secret_key_shares() const {
        return secret_key_shares_;
    }
};

int main() {
    // 设置参数
    const size_t n = 3;  // 3 个参与方
    const size_t t = 1;  // 阈值为 1 (需要 2 个份额重构)
    
    // 初始化
    SecLevel seclevel(128);
    RandGen randgen;
    
    auto T = chrono::system_clock::now();
    Mpz seed(static_cast<unsigned long>(T.time_since_epoch().count()));
    randgen.set_seed(seed);
    
    // 初始化类群系统 (与 test_bdkg 一致)
    Mpz q("115792089237316195423570985008687907852837564279074904382605163141518161494337");  // secp256k1 的阶
    
    cout << "初始化类群系统..." << endl;
    cout << "使用素数阶 q = " << q << endl;
    
    // 创建 CL_HSMqk 对象 (类群系统)
    CL_HSMqk cl(q, 1, seclevel, randgen, false);
    
    cout << "类群初始化完成" << endl;
    cout << "判别式: " << cl.Cl_Delta().discriminant() << endl;
    
    // 创建并运行 DKG
    SimpleJointFeldmanDKG_CL dkg(n, t, q, cl, randgen);
    
    if (dkg.run()) {
        cout << "\n所有数据已保存到当前目录！" << endl;
        cout << "\n生成的公钥是类群元素 (QFI 类型)" << endl;
        cout << "可以用于 A2L 项目的 cl_public_key_t" << endl;
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}

