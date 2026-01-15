#include <bicycl.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>

using namespace BICYCL;
using namespace BICYCL::OpenSSL;
using namespace std;

// 简化的 Shamir 秘密共享类
class SimplifiedSSS {
private:
    RandGen& randgen_;

public:
    SimplifiedSSS(RandGen& randgen) : randgen_(randgen) {}

    // 生成 t 次多项式: f(x) = a_0 + a_1*x + ... + a_t*x^t
    // coefficients[0] = secret, coefficients[1..t] = random
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

// Joint-Feldman DKG 协议
class SimpleJointFeldmanDKG {
private:
    const size_t n_;        // 参与方总数
    const size_t t_;        // 阈值 (需要 t+1 个份额重构)
    const Mpz& q_;          // 素数阶
    ECGroup& ec_group_;     // 椭圆曲线群
    RandGen& randgen_;
    SimplifiedSSS sss_;

    // 每个参与方的数据
    vector<Mpz> secrets_;                           // 每个参与方的随机秘密 z_i
    vector<vector<Mpz>> polynomials_;               // 每个参与方的多项式系数
    vector<vector<unique_ptr<ECPoint>>> commitments_;  // 每个参与方的承诺 A_ij = g^{a_ij}
    vector<vector<Mpz>> shares_matrix_;             // shares_matrix_[i][j] = f_i(j+1)
    
    // 最终结果
    vector<Mpz> secret_key_shares_;                 // 每个参与方的私钥份额
    vector<unique_ptr<ECPoint>> public_key_shares_; // 每个参与方的公钥份额
    unique_ptr<ECPoint> global_public_key_;         // 全局公钥

public:
    SimpleJointFeldmanDKG(size_t n, size_t t, const Mpz& q, ECGroup& ec_group, RandGen& randgen)
        : n_(n), t_(t), q_(q), ec_group_(ec_group), randgen_(randgen), sss_(randgen) {
        
        secrets_.resize(n_);
        polynomials_.resize(n_);
        commitments_.resize(n_);
        shares_matrix_.resize(n_);
        secret_key_shares_.resize(n_, Mpz(0UL));
        
        global_public_key_ = unique_ptr<ECPoint>(new ECPoint(ec_group_));
        
        for (size_t i = 0; i < n_; i++) {
            shares_matrix_[i].resize(n_);
            public_key_shares_.push_back(unique_ptr<ECPoint>(new ECPoint(ec_group_)));
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
            
            // 计算并广播承诺 A_ij = g^{a_ij}
            commitments_[i].clear();
            cout << "参与方 P" << i << " 的承诺: ";
            for (size_t j = 0; j <= t_; j++) {
                commitments_[i].push_back(unique_ptr<ECPoint>(new ECPoint(ec_group_)));
                ec_group_.scal_mul_gen(*commitments_[i][j], BN(polynomials_[i][j]));
                cout << "A_" << i << j << " = g^{a_" << i << j << "}";
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
                // 验证 g^{s_ij} = ∏_{k=0}^{t} (A_ik)^{j^k}
                
                // 左边: g^{s_ij}
                unique_ptr<ECPoint> lhs(new ECPoint(ec_group_));
                ec_group_.scal_mul_gen(*lhs, BN(shares_matrix_[i][j]));
                
                // 右边: ∏_{k=0}^{t} (A_ik)^{j^k}
                unique_ptr<ECPoint> rhs(new ECPoint(ec_group_));
                Mpz j_power;
                unique_ptr<ECPoint> temp(new ECPoint(ec_group_));
                
                for (size_t k = 0; k <= t_; k++) {
                    Mpz::pow_mod(j_power, Mpz(j + 1), Mpz(k), q_);  // (j+1)^k
                    ec_group_.scal_mul(*temp, BN(j_power), *commitments_[i][k]);  // A_ik^{j^k}
                    ec_group_.ec_add(*rhs, *rhs, *temp);
                }
                
                // 验证
                if (ec_group_.ec_point_eq(*lhs, *rhs)) {
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

    // 步骤 3: 计算最终的密钥
    void phase3_compute_keys() {
        cout << "\n=== 阶段 3: 计算密钥 ===" << endl;
        
        // 假设所有参与方都是合格的 (Qual = {0, 1, 2})
        vector<size_t> qual;
        for (size_t i = 0; i < n_; i++) {
            qual.push_back(i);
        }
        
        // 计算全局公钥 pk = ∏_{i∈Qual} y_i = ∏_{i∈Qual} A_i0
        cout << "\n计算全局公钥 pk = ∏_{i∈Qual} A_i0:" << endl;
        // global_public_key_ 已经在构造函数中初始化
        for (size_t i : qual) {
            ec_group_.ec_add(*global_public_key_, *global_public_key_, *commitments_[i][0]);
            cout << "  累加 A_" << i << "0" << endl;
        }
        cout << "全局公钥计算完成" << endl;
        
        // 计算每个参与方的公钥份额 pk_j = ∏_{i∈Qual} A_ij
        cout << "\n计算公钥份额 pk_j = ∏_{i∈Qual} A_ij:" << endl;
        for (size_t j = 0; j < n_; j++) {
            // public_key_shares_[j] 已经在构造函数中初始化
            for (size_t i : qual) {
                // 计算 A_ij = ∏_{k=0}^{t} (A_ik)^{j^k}
                unique_ptr<ECPoint> A_ij(new ECPoint(ec_group_));
                Mpz j_power;
                unique_ptr<ECPoint> temp(new ECPoint(ec_group_));
                
                for (size_t k = 0; k <= t_; k++) {
                    Mpz::pow_mod(j_power, Mpz(j + 1), Mpz(k), q_);
                    ec_group_.scal_mul(*temp, BN(j_power), *commitments_[i][k]);
                    ec_group_.ec_add(*A_ij, *A_ij, *temp);
                }
                
                ec_group_.ec_add(*public_key_shares_[j], *public_key_shares_[j], *A_ij);
            }
            cout << "  pk_" << j << " 计算完成" << endl;
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
        
        // 验证每个参与方的密钥对 g^{sk_j} = pk_j
        for (size_t j = 0; j < n_; j++) {
            unique_ptr<ECPoint> computed_pk(new ECPoint(ec_group_));
            ec_group_.scal_mul_gen(*computed_pk, BN(secret_key_shares_[j]));
            
            if (ec_group_.ec_point_eq(*computed_pk, *public_key_shares_[j])) {
                cout << "  ✓ 参与方 P" << j << " 的密钥对验证成功: g^{sk_" << j << "} = pk_" << j << endl;
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
        
        // 验证 g^{sk} = pk
        unique_ptr<ECPoint> computed_global_pk(new ECPoint(ec_group_));
        ec_group_.scal_mul_gen(*computed_global_pk, BN(reconstructed_sk));
        
        if (ec_group_.ec_point_eq(*computed_global_pk, *global_public_key_)) {
            cout << "  ✓ 全局密钥对验证成功: g^{sk} = pk" << endl;
            
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
        
        // 保存全局公钥
        ofstream pk_file("global_public_key.txt");
        if (pk_file.is_open()) {
            pk_file << "全局公钥 (pk):\n";
            pk_file << "椭圆曲线点 (已生成)\n";
            pk_file.close();
            cout << "  ✓ 已保存到 global_public_key.txt" << endl;
        }
        
        // 保存所有公钥份额
        ofstream pks_file("party_public_keys.txt");
        if (pks_file.is_open()) {
            pks_file << "各参与方的公钥份额:\n\n";
            for (size_t i = 0; i < n_; i++) {
                pks_file << "参与方 P" << i << " 的公钥份额 (pk_" << i << "):\n";
                pks_file << "椭圆曲线点 (已生成)\n\n";
            }
            pks_file.close();
            cout << "  ✓ 已保存到 party_public_keys.txt" << endl;
        }
        
        // 保存完整的协议摘要
        ofstream summary("dkg_summary.txt");
        if (summary.is_open()) {
            summary << "========================================\n";
            summary << "Joint-Feldman DKG 协议执行摘要\n";
            summary << "========================================\n\n";
            summary << "参数设置:\n";
            summary << "  参与方总数 n = " << n_ << "\n";
            summary << "  阈值 t = " << t_ << " (需要 " << (t_ + 1) << " 个份额重构)\n";
            summary << "  有限域阶 q = " << q_ << "\n\n";
            
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
            
            summary << "全局公钥:\n";
            summary << "  椭圆曲线点 (已生成)\n";
            
            summary.close();
            cout << "  ✓ 已保存协议摘要到 dkg_summary.txt" << endl;
        }
    }

    // 运行完整的 DKG 协议
    bool run() {
        cout << "\n=======================================" << endl;
        cout << "开始执行 Joint-Feldman DKG 协议" << endl;
        cout << "=======================================" << endl;
        cout << "参与方数量: " << n_ << endl;
        cout << "阈值: " << t_ << " (需要 " << (t_ + 1) << " 个份额重构)" << endl;
        cout << "=======================================" << endl;
        
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
    
    ECGroup ec_group(seclevel);
    Mpz q(ec_group.order());
    
    cout << "使用椭圆曲线群，阶为: " << q << endl;
    
    // 创建并运行 DKG
    SimpleJointFeldmanDKG dkg(n, t, q, ec_group, randgen);
    
    if (dkg.run()) {
        cout << "\n所有数据已保存到当前目录！" << endl;
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}

