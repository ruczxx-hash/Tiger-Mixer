"""主程序入口"""
from simulation import SimulationInitializer, DetectionLayers, StatisticsCollector, Simulator
from config.system_config import NUM_SMALL_TASK_ATTACKERS, NUM_LARGE_TASK_ATTACKERS


def main():
    """主函数"""
    # 初始化所有组件
    print("=" * 60)
    print("初始化模拟系统...")
    print("=" * 60)
    
    initializer = SimulationInitializer()
    init_data = initializer.initialize_all()
    
    # 初始化统计收集器
    total_attackers = NUM_SMALL_TASK_ATTACKERS + NUM_LARGE_TASK_ATTACKERS
    statistics_collector = StatisticsCollector(total_attackers)
    
    # 初始化检测层
    detection_layers = DetectionLayers(
        init_data['committees'],
        init_data['reference_illegal_samples'],
        init_data['reference_legitimate_samples']
    )
    
    # 创建模拟器
    simulator = Simulator(initializer, detection_layers, statistics_collector)
    
    # 运行模拟
    simulator.run(max_time=4320)
    
    # 保存结果
    simulator.save_results()


if __name__ == "__main__":
    main()
