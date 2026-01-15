import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from collections import Counter

from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import precision_recall_fscore_support, classification_report
from sklearn.metrics import f1_score, accuracy_score, confusion_matrix
from sklearn.preprocessing import MinMaxScaler
import warnings

warnings.filterwarnings('ignore')

# 加载第一个代码生成的特征文件
print("正在加载特征数据...")
df_features = pd.read_csv(
    "/home/zxx/Config/comprehensive_transaction_statistics.csv")

print(f"原始数据shape: {df_features.shape}")
print(f"原始数据列名: {df_features.columns.tolist()}")

# 检查标签分布
print("\n=== 标签分布 ===")
print("Label值统计:")
print(df_features['label'].value_counts())
print(f"空值数量: {df_features['label'].isnull().sum()}")

# 数据预处理
print("\n正在进行数据预处理...")

# 1. 处理标签：空值标记为合法(0)，'heist'标记为非法(1)
df_features['binary_label'] = df_features['label'].apply(
    lambda x: 1 if x == 'heist' else 0
)

print("处理后的标签分布:")
print(f"合法用户 (0): {sum(df_features['binary_label'] == 0)}")
print(f"非法用户 (1): {sum(df_features['binary_label'] == 1)}")

# 2. 选择特征列（排除地址和标签列）
feature_columns = [col for col in df_features.columns
                   if col not in ['address', 'label', 'binary_label']]

print(f"\n特征数量: {len(feature_columns)}")

# 3. 处理缺失值
print(f"\n缺失值检查:")
missing_counts = df_features[feature_columns].isnull().sum()
if missing_counts.sum() > 0:
    print("存在缺失值的列:")
    print(missing_counts[missing_counts > 0])
    # 用0填充缺失值
    df_features[feature_columns] = df_features[feature_columns].fillna(0)
    print("已用0填充缺失值")
else:
    print("无缺失值")

# 4. 处理无穷值
print("\n处理无穷值...")
for col in feature_columns:
    # 将无穷值替换为该列的最大有限值
    if df_features[col].dtype in ['float64', 'int64']:
        finite_values = df_features[col][np.isfinite(df_features[col])]
        if len(finite_values) > 0:
            max_finite = finite_values.max()
            min_finite = finite_values.min()
            df_features[col] = df_features[col].replace([np.inf, -np.inf], [max_finite, min_finite])

# 5. 特征标准化 将所有特征缩放到 [0,1] 区间，避免特征量纲影响模型
print("\n正在进行特征标准化...")
scaler = MinMaxScaler()
df_features_scaled = df_features.copy()
df_features_scaled[feature_columns] = scaler.fit_transform(df_features[feature_columns])

print("特征标准化完成")

# 6. 准备训练数据  x为特征 y为二分类标签
X = df_features_scaled[feature_columns]
y = df_features_scaled['binary_label']

print(f"\n最终数据集形状: {X.shape}")
print(f"标签分布: {y.value_counts().to_dict()}")

# 检查数据平衡性
if y.sum() == 0:
    print("警告: 数据集中没有非法用户样本!")
    print("无法进行有效的机器学习训练")
    exit()
elif y.sum() == len(y):
    print("警告: 数据集中没有合法用户样本!")
    print("无法进行有效的机器学习训练")
    exit()

# 7. 数据分割 按照7:3的比例分割数据集 并保持标签分布
print("\n正在分割数据集...") 
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.30, random_state=42, stratify=y
)

# 保存测试集的地址信息
test_addresses = df_features_scaled.loc[X_test.index, 'address'].copy()
test_labels = df_features_scaled.loc[X_test.index, 'label'].copy()

print(f"训练集形状: {X_train.shape}")
print(f"测试集形状: {X_test.shape}")
print(f"训练集标签分布: {y_train.value_counts().to_dict()}")
print(f"测试集标签分布: {y_test.value_counts().to_dict()}")

# 8. 随机森林训练
print("\n正在训练随机森林模型...")
rf_classifier = RandomForestClassifier(
    n_estimators=100,
    random_state=42,
    max_depth=10,
    min_samples_split=5,
    min_samples_leaf=2,
    class_weight='balanced'  # 处理类别不平衡
)

rf_classifier.fit(X_train, y_train)

# 9. 预测 得到类别和概率
print("正在进行预测...") 
y_pred = rf_classifier.predict(X_test)
y_proba = rf_classifier.predict_proba(X_test)

# 10. 性能评估  准确率是所有预测正确的样本数占总样本数的比例
print("\n=== 模型性能评估 ===")
accuracy = accuracy_score(y_test, y_pred)
print(f"准确率: {accuracy:.4f}")

# 详细的分类报告
print("\n分类报告:")
print(classification_report(y_test, y_pred, target_names=['合法用户', '非法用户']))

# 混淆矩阵
cm = confusion_matrix(y_test, y_pred)
print("\n混淆矩阵:")
print("          预测")
print("         合法  非法")
print(f"实际 合法   {cm[0, 0]:4d}  {cm[0, 1]:4d}")
print(f"    非法   {cm[1, 0]:4d}  {cm[1, 1]:4d}")

# 11. 结果分析
print("\n=== 可疑地址检测结果 ===")

# 创建结果DataFrame
results_df = pd.DataFrame({
    'address': test_addresses,
    'original_label': test_labels,
    'true_label': y_test,  # 0: 合法, 1: 非法
    'predicted_label': y_pred,  # 0: 合法, 1: 非法
    'illegal_probability': y_proba[:, 1],  # 预测为非法的概率
    'legal_probability': y_proba[:, 0]  # 预测为合法的概率
})

# 添加标签说明
results_df['true_label_desc'] = results_df['true_label'].apply(lambda x: '非法用户' if x == 1 else '合法用户')
results_df['predicted_label_desc'] = results_df['predicted_label'].apply(lambda x: '非法用户' if x == 1 else '合法用户')

# 分析预测为非法的地址
suspicious_addresses = results_df[results_df['predicted_label'] == 1].copy()
suspicious_addresses = suspicious_addresses.sort_values('illegal_probability', ascending=False)

print(f"检测到 {len(suspicious_addresses)} 个可疑地址")

if len(suspicious_addresses) > 0:
    # 分析预测准确性
    correct_predictions = suspicious_addresses[suspicious_addresses['true_label'] == 1]
    false_positives = suspicious_addresses[suspicious_addresses['true_label'] == 0]

    print(f"\n预测准确性分析：")
    print(f"正确识别的非法地址: {len(correct_predictions)} 个")
    print(f"误判的合法地址: {len(false_positives)} 个")

# 12. 新增功能：保存假阴性地址（预测为合法但实际非法的地址）
false_negatives = results_df[(results_df['predicted_label'] == 0) & (results_df['true_label'] == 1)].copy()
false_negatives = false_negatives.sort_values('illegal_probability', ascending=False)

print(f"\n假阴性地址（漏检的非法地址）: {len(false_negatives)} 个")
if len(false_negatives) > 0:
    print("这些地址实际是非法但被模型预测为合法：")
    print(false_negatives[['address', 'original_label', 'illegal_probability']].to_string(index=False))

    # 分析这些地址的特征
    print("\n假阴性地址的特征分析：")
    false_negatives_features = false_negatives.merge(
        df_features_scaled[['address'] + feature_columns],
        on='address',
        how='left'
    )

    # 计算平均特征值
    avg_features = false_negatives_features[feature_columns].mean().sort_values(ascending=False)
    print("\n假阴性地址的平均特征值（前10个最重要的特征）：")
    print(avg_features.head(10))

    # 与整体非法地址的特征比较
    all_illegal_features = df_features_scaled[df_features_scaled['binary_label'] == 1][feature_columns].mean()
    comparison = pd.DataFrame({
        'false_negatives': avg_features,
        'all_illegal': all_illegal_features
    })
    comparison['difference'] = comparison['false_negatives'] - comparison['all_illegal']

    print("\n假阴性地址与所有非法地址的特征差异（前10个差异最大的特征）：")
    print(comparison.sort_values('difference', ascending=False).head(10))

# 13. 特征重要性分析（完整输出）
print("\n=== 特征重要性分析（完整）===")
feature_importance = pd.DataFrame({
    'feature': feature_columns,
    'importance': rf_classifier.feature_importances_
}).sort_values('importance', ascending=False)

print("所有特征按重要性排序：")
pd.set_option('display.max_rows', None)  # 显示所有行
print(feature_importance.to_string(index=False))
pd.reset_option('display.max_rows')  # 重置显示设置

# 14. 保存结果
results_df.to_csv('wallet_classification_results.csv', index=False)
if len(suspicious_addresses) > 0:
    suspicious_addresses.to_csv('suspicious_addresses.csv', index=False)
feature_importance.to_csv('feature_importance.csv', index=False)

# 新增：保存假阴性地址
if len(false_negatives) > 0:
    false_negatives.to_csv('false_negatives.csv', index=False)
    print("\n假阴性地址已保存到 false_negatives.csv")

print(f"\n结果已保存到：")
print("- wallet_classification_results.csv (完整结果)")
if len(suspicious_addresses) > 0:
    print("- suspicious_addresses.csv (可疑地址)")
if len(false_negatives) > 0:
    print("- false_negatives.csv (漏检的非法地址)")
print("- feature_importance.csv (特征重要性)")

# 15. 可视化所有特征重要性
plt.figure(figsize=(15, 10))
plt.barh(np.arange(len(feature_importance)),
         feature_importance['importance'],
         tick_label=feature_importance['feature'])
plt.gca().invert_yaxis()  # 反转Y轴，使最重要的特征在顶部
plt.title('所有特征重要性')
plt.xlabel('特征重要性分数')
plt.ylabel('特征')
plt.tight_layout()
plt.savefig('feature_importance_all.png', dpi=300)
plt.show()

# 16. 整体统计信息
print("\n=== 整体统计信息 ===")
print(f"总测试样本数: {len(results_df)}")
print(f"实际非法地址数: {len(results_df[results_df['true_label'] == 1])}")
print(f"实际合法地址数: {len(results_df[results_df['true_label'] == 0])}")
print(f"预测非法地址数: {len(results_df[results_df['predicted_label'] == 1])}")
print(f"预测合法地址数: {len(results_df[results_df['predicted_label'] == 0])}")

# 新增：假阴性统计
print(f"\n假阴性（漏检的非法地址）: {len(false_negatives)}")
print(
    f"假阳性（误判的合法地址）: {len(results_df[(results_df['predicted_label'] == 1) & (results_df['true_label'] == 0)])}")

print("\n=== 钱包分类检测完成 ===")

# 17. 保存训练好的模型和标准化器
print("\n=== 保存模型 ===")
try:
    import joblib
    
    # 保存模型
    joblib.dump(rf_classifier, 'rf_model.pkl')
    print("✅ 随机森林模型已保存到: rf_model.pkl")
    
    # 保存标准化器
    joblib.dump(scaler, 'scaler.pkl')
    print("✅ 特征标准化器已保存到: scaler.pkl")
    
    # 保存特征列名
    import json
    with open('feature_columns.json', 'w') as f:
        json.dump(feature_columns, f)
    print("✅ 特征列名已保存到: feature_columns.json")
    
    print("\n模型保存完成！现在可以使用 detect_address.py 进行单地址检测")
    
except ImportError:
    print("❌ 错误：未安装joblib库，请运行: pip install joblib")
    print("模型保存失败")
except Exception as e:
    print(f"❌ 模型保存失败: {e}")