#!/bin/bash

# VRF Solidity 测试运行脚本
# 作用：简化测试运行流程
# 使用方法：./run_tests.sh [选项]

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目目录
PROJECT_DIR="/home/zxx/Config/vrf-solidity-master"

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo ""
    echo "=========================================="
    echo "   VRF Solidity 测试运行脚本"
    echo "=========================================="
    echo ""
    echo "使用方法："
    echo "  ./run_tests.sh [选项]"
    echo ""
    echo "选项："
    echo "  1, simple          运行简单测试（推荐）"
    echo "  2, c-gen           运行 C 生成器集成测试"
    echo "  3, all             运行所有测试"
    echo "  4, original        运行原有测试（vrf.js）"
    echo "  5, gas             运行 Gas 分析"
    echo "  setup              初始化项目（安装依赖+编译）"
    echo "  compile            只编译合约"
    echo "  clean              清理并重新安装"
    echo "  help, -h, --help   显示此帮助信息"
    echo ""
    echo "示例："
    echo "  ./run_tests.sh simple    # 运行简单测试"
    echo "  ./run_tests.sh all       # 运行所有测试"
    echo "  ./run_tests.sh setup     # 初始化项目"
    echo ""
}

# 检查项目目录
check_project_dir() {
    if [ ! -d "$PROJECT_DIR" ]; then
        print_error "项目目录不存在: $PROJECT_DIR"
        exit 1
    fi
    cd "$PROJECT_DIR"
}

# 检查依赖是否已安装
check_dependencies() {
    if [ ! -d "node_modules" ]; then
        print_warning "依赖未安装，正在安装..."
        npm install
    fi
}

# 检查合约是否已编译
check_compiled() {
    if [ ! -d "build/contracts" ]; then
        print_warning "合约未编译，正在编译..."
        truffle compile
    fi
}

# 初始化项目
setup_project() {
    print_info "初始化项目..."
    
    print_info "1/3 安装依赖..."
    npm install
    
    print_info "2/3 编译合约..."
    truffle compile
    
    print_info "3/3 检查 C 程序..."
    C_PROGRAM="/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test"
    if [ -f "$C_PROGRAM" ]; then
        print_success "C 程序已找到: $C_PROGRAM"
        if [ -x "$C_PROGRAM" ]; then
            print_success "C 程序可执行"
        else
            print_warning "C 程序不可执行，正在添加权限..."
            chmod +x "$C_PROGRAM"
        fi
    else
        print_warning "C 程序不存在: $C_PROGRAM"
        print_warning "C 集成测试将被跳过"
    fi
    
    print_success "项目初始化完成！"
    echo ""
    print_info "现在可以运行测试了："
    echo "  ./run_tests.sh simple"
}

# 编译合约
compile_contracts() {
    print_info "编译合约..."
    truffle compile
    print_success "编译完成！"
}

# 清理并重新安装
clean_install() {
    print_info "清理项目..."
    
    if [ -d "node_modules" ]; then
        print_info "删除 node_modules..."
        rm -rf node_modules
    fi
    
    if [ -f "package-lock.json" ]; then
        print_info "删除 package-lock.json..."
        rm -f package-lock.json
    fi
    
    if [ -d "build" ]; then
        print_info "删除 build 目录..."
        rm -rf build
    fi
    
    print_info "重新安装依赖..."
    npm install
    
    print_info "重新编译合约..."
    truffle compile
    
    print_success "清理并重装完成！"
}

# 运行简单测试
run_simple_test() {
    print_info "运行简单测试..."
    echo ""
    truffle test test/test_vrf_simple.js
    echo ""
    print_success "简单测试完成！"
}

# 运行 C 生成器测试
run_c_generator_test() {
    print_info "运行 C 生成器集成测试..."
    echo ""
    
    # 检查 C 程序
    C_PROGRAM="/home/zxx/A2L/A2L-master/ecdsa/bin/vrf_test"
    if [ ! -f "$C_PROGRAM" ]; then
        print_warning "C 程序不存在: $C_PROGRAM"
        print_warning "相关测试将被跳过"
    fi
    
    truffle test test/test_vrf_with_c_generator.js
    echo ""
    print_success "C 生成器测试完成！"
}

# 运行所有测试
run_all_tests() {
    print_info "运行所有测试..."
    echo ""
    truffle test
    echo ""
    print_success "所有测试完成！"
}

# 运行原有测试
run_original_test() {
    print_info "运行原有测试 (vrf.js)..."
    echo ""
    truffle test test/vrf.js
    echo ""
    print_success "原有测试完成！"
}

# 运行 Gas 分析
run_gas_analysis() {
    print_info "运行 Gas 分析..."
    echo ""
    npm run gas-analysis
    echo ""
    print_success "Gas 分析完成！"
}

# 主函数
main() {
    # 检查项目目录
    check_project_dir
    
    # 如果没有参数，显示帮助
    if [ $# -eq 0 ]; then
        show_help
        exit 0
    fi
    
    # 处理参数
    case "$1" in
        1|simple)
            check_dependencies
            check_compiled
            run_simple_test
            ;;
        2|c-gen)
            check_dependencies
            check_compiled
            run_c_generator_test
            ;;
        3|all)
            check_dependencies
            check_compiled
            run_all_tests
            ;;
        4|original)
            check_dependencies
            check_compiled
            run_original_test
            ;;
        5|gas)
            check_dependencies
            check_compiled
            run_gas_analysis
            ;;
        setup)
            setup_project
            ;;
        compile)
            compile_contracts
            ;;
        clean)
            clean_install
            ;;
        help|-h|--help)
            show_help
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"

