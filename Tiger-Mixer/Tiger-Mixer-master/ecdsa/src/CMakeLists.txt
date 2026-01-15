# # find_library(RELIC relic HINTS /home/zxx/Config/relic/build/lib)
# # find_library(PARI pari HINTS /usr/local/lib)
# # find_library(GMP gmp HINTS /usr/lib/x86_64-linux-gnu/)
# # find_library(ZMQ zmq HINTS /usr/lib/x86_64-linux-gnu)
# # add_executable(alice alice.c util.c secret_share.c)
# # target_link_libraries(alice ${RELIC} ${PARI} ${GMP} ${ZMQ})
# # add_executable(bob bob.c util.c secret_share.c)
# # target_link_libraries(bob ${RELIC} ${PARI} ${GMP} ${ZMQ})
# # add_executable(tumbler tumbler.c util.c secret_share.c)
# # target_link_libraries(tumbler ${RELIC} ${PARI} ${GMP} ${ZMQ})
# # add_executable(wrapper wrapper.c)
# # add_executable(secret_share_receiver secret_share_receiver.c util.c secret_share.c)
# # target_link_libraries(secret_share_receiver ${RELIC} ${PARI} ${GMP} ${ZMQ} pthread)
# # add_executable(auditor auditor.c util.c secret_share.c)
# # target_link_libraries(auditor ${RELIC} ${PARI} ${GMP} ${ZMQ} )
# # add_executable(keygen keygen.c util.c)
# # target_link_libraries(keygen ${RELIC} ${PARI} ${GMP} ${ZMQ})

# # 使用 pkg-config 的更可靠方法
# # find_package(PkgConfig REQUIRED)

# # # 使用 pkg-config 查找 PARI
# # pkg_check_modules(PARI REQUIRED pari)
# # if(PARI_FOUND)
# #     message(STATUS "Found PARI via pkg-config")
# #     message(STATUS "PARI_LIBRARIES: ${PARI_LIBRARIES}")
# #     message(STATUS "PARI_LIBRARY_DIRS: ${PARI_LIBRARY_DIRS}")
# #     message(STATUS "PARI_INCLUDE_DIRS: ${PARI_INCLUDE_DIRS}")
# # endif()

# # 手动查找其他库（保持原有方式）
# find_library(RELIC relic HINTS /home/zxx/Config/relic/build/lib)
# find_library(PARI pari HINTS /usr/local/lib)
# find_library(GMP gmp HINTS /usr/lib/x86_64-linux-gnu)
# find_library(ZMQ zmq HINTS /usr/lib/x86_64-linux-gnu)

# # 验证所有库
# if(NOT RELIC)
#     message(FATAL_ERROR "RELIC library not found")
# endif()
# if(NOT PARI)
#     message(FATAL_ERROR "PARI library not found")
# endif()
# if(NOT GMP)
#     message(FATAL_ERROR "GMP library not found")
# endif()
# if(NOT ZMQ)
#     message(FATAL_ERROR "ZMQ library not found")
# endif()

# # 添加包含目录
# include_directories(${PARI_INCLUDE_DIRS})
# link_directories(${PARI_LIBRARY_DIRS})

# # 构建可执行文件
# add_executable(alice alice.c util.c secret_share.c)
# target_link_libraries(alice ${RELIC} ${PARI_LIBRARIES} ${GMP} ${ZMQ} -lm)

# add_executable(bob bob.c util.c secret_share.c)
# target_link_libraries(bob ${RELIC} ${PARI_LIBRARIES} ${GMP} ${ZMQ} -lm)

# add_executable(tumbler tumbler.c util.c secret_share.c)
# target_link_libraries(tumbler ${RELIC} ${PARI_LIBRARIES} ${GMP} ${ZMQ} -lm)

# add_executable(wrapper wrapper.c)

# add_executable(secret_share_receiver secret_share_receiver.c util.c secret_share.c)
# target_link_libraries(secret_share_receiver ${RELIC} ${PARI_LIBRARIES} ${GMP} ${ZMQ} pthread -lm)

# add_executable(auditor auditor.c util.c secret_share.c)
# target_link_libraries(auditor ${RELIC} ${PARI_LIBRARIES} ${GMP} ${ZMQ} -lm)

# add_executable(keygen keygen.c util.c)
# target_link_libraries(keygen ${RELIC} ${PARI_LIBRARIES} ${GMP} ${ZMQ} -lm)

# 查找库文件
find_library(RELIC relic HINTS /home/zxx/Config/relic/build/lib)
find_library(PARI pari HINTS /usr/local/lib)
find_library(GMP gmp HINTS /usr/lib/x86_64-linux-gnu)
find_library(ZMQ zmq HINTS /usr/lib/x86_64-linux-gnu)
find_library(CURL curl HINTS /usr/lib/x86_64-linux-gnu)

# 验证所有库
if(NOT RELIC)
    message(FATAL_ERROR "RELIC library not found")
endif()
if(NOT PARI)
    message(FATAL_ERROR "PARI library not found")
endif()
if(NOT GMP)
    message(FATAL_ERROR "GMP library not found")
endif()
if(NOT ZMQ)
    message(FATAL_ERROR "ZMQ library not found")
endif()
if(NOT CURL)
    message(FATAL_ERROR "CURL library not found")
endif()

# 添加PARI头文件目录（根据你的PARI安装位置调整）
include_directories(/usr/local/include/pari)
# 或者如果PARI头文件在标准位置：
# include_directories(/usr/include/pari)

# 创建共享库
set(LIBRARY_SOURCES 
    util.c 
    secret_share.c 
    gs.c 
    gs_serial.c 
    malleable_proof.c
    composite_malleable_proof.c
    http_zk_client.c
    pedersen_dkg.c
    dkg_integration.c
    cl_canonical.c
    committee_integration.c  # 恢复委员会集成
    reputation_tracker.c     # 声誉跟踪系统
    reputation_tracker_util.c # 声誉跟踪工具函数
)

add_library(a2l_ecdsa STATIC ${LIBRARY_SOURCES})
target_link_libraries(a2l_ecdsa ${RELIC} ${PARI} ${GMP} ${CURL} -lm)

# 设置库变量供主CMakeLists.txt使用
set(RELIC_LIBRARY ${RELIC} PARENT_SCOPE)
set(CL_LIBRARY ${PARI} ${GMP} -lm PARENT_SCOPE)

# 构建可执行文件
add_executable(alice alice.c)
target_link_libraries(alice a2l_ecdsa ${ZMQ})

add_executable(bob bob.c)
target_link_libraries(bob a2l_ecdsa ${ZMQ})

add_executable(tumbler tumbler.c)
target_link_libraries(tumbler a2l_ecdsa ${ZMQ})

add_executable(wrapper wrapper.c)

add_executable(secret_share_receiver secret_share_receiver.c)
target_link_libraries(secret_share_receiver a2l_ecdsa ${ZMQ} pthread)

# 声誉统计数据计算程序
add_executable(calculate_reputation_stats calculate_reputation_stats.c)
target_link_libraries(calculate_reputation_stats a2l_ecdsa)

add_executable(auditor auditor.c)
target_link_libraries(auditor a2l_ecdsa ${ZMQ})

# auditor_detection只做检测，不需要复杂的库依赖
add_executable(auditor_detection auditor_detection.c)

add_executable(keygen keygen.c)
target_link_libraries(keygen a2l_ecdsa ${ZMQ})

# 性能测试程序
add_executable(secret_share_benchmark secret_share_benchmark.c)
target_link_libraries(secret_share_benchmark a2l_ecdsa ${ZMQ})

# DKG性能测试程序
add_executable(dkg_benchmark dkg_benchmark.c)
target_link_libraries(dkg_benchmark a2l_ecdsa ${ZMQ})

# 委员会大小性能测试程序
add_executable(committee_size_benchmark committee_size_benchmark.c)
target_link_libraries(committee_size_benchmark a2l_ecdsa ${ZMQ})

# THRESHOLD重构性能测试程序
add_executable(threshold_benchmark threshold_benchmark.c)
target_link_libraries(threshold_benchmark a2l_ecdsa ${ZMQ})

# 委员会大小性能测试程序（固定阈值=2）
add_executable(committee_size_threshold2_benchmark committee_size_threshold2_benchmark.c)
target_link_libraries(committee_size_threshold2_benchmark a2l_ecdsa ${ZMQ})

# DKG委员会大小性能测试程序
add_executable(dkg_committee_benchmark dkg_committee_benchmark.c)
target_link_libraries(dkg_committee_benchmark a2l_ecdsa ${ZMQ})


add_executable(dkg_threshold_benchmark dkg_threshold_benchmark.c)
target_link_libraries(dkg_threshold_benchmark a2l_ecdsa ${ZMQ})

# DKG私钥重构性能测试程序
add_executable(dkg_reconstruction_benchmark dkg_reconstruction_benchmark.c)
target_link_libraries(dkg_reconstruction_benchmark a2l_ecdsa ${ZMQ})

# 秘密分享阈值性能测试程序
add_executable(secret_share_threshold_benchmark secret_share_threshold_benchmark.c)
target_link_libraries(secret_share_threshold_benchmark a2l_ecdsa ${ZMQ})

# 委员会交互测试程序（文件不存在，已注释）
# add_executable(test_committee_interaction test_committee_interaction.c)
# target_link_libraries(test_committee_interaction a2l_ecdsa ${ZMQ})

# VRF 生成器库和测试程序
# 查找 secp256k1-vrf 库
find_library(SECP256K1_VRF secp256k1-vrf HINTS /home/zxx/Config/secp256k1-vrf-master/.libs)

if(NOT SECP256K1_VRF)
    message(WARNING "secp256k1-vrf library not found, VRF features will be disabled")
else()
    message(STATUS "Found secp256k1-vrf library: ${SECP256K1_VRF}")
    
    # 添加 VRF 生成器到静态库
    target_sources(a2l_ecdsa PRIVATE vrf_generator.c)
    
    # VRF 测试程序
    add_executable(vrf_test vrf_test.c)
    target_link_libraries(vrf_test a2l_ecdsa ${SECP256K1_VRF} ${ZMQ})
    
    # VRF 命令行工具（用于从 JS 脚本调用）
    # 注意：vrf_generator_debug.c 不存在，但 vrf_cli.c 可以单独编译（debug功能可选）
    add_executable(vrf_cli vrf_cli.c)
    target_link_libraries(vrf_cli a2l_ecdsa ${SECP256K1_VRF} ${ZMQ})
    
    # VRF 验证命令行工具（文件不存在，已注释）
    # add_executable(vrf_verify_cli vrf_verify_cli.c)
    # target_link_libraries(vrf_verify_cli a2l_ecdsa ${SECP256K1_VRF} ${ZMQ})
    
    # 添加 secp256k1-vrf 头文件目录
    target_include_directories(vrf_test PRIVATE /home/zxx/Config/secp256k1-vrf-master/include)
    target_include_directories(vrf_cli PRIVATE /home/zxx/Config/secp256k1-vrf-master/include)
    # target_include_directories(vrf_verify_cli PRIVATE /home/zxx/Config/secp256k1-vrf-master/include)
    target_include_directories(a2l_ecdsa PRIVATE /home/zxx/Config/secp256k1-vrf-master/include)
endif()