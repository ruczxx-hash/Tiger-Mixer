include (CTest)
if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
  set (_cmd ${CMAKE_CTEST_COMMAND} --output-on-failure $(ARGS))
else ()
  set (_cmd ${CMAKE_CTEST_COMMAND} --output-on-failure)
endif ()
add_custom_target (tests COMMAND ${_cmd} COMMENT "Running the tests")

### List of tests
set (TESTS_LIST #test_qclpvss
                #test_sss
                #test_nizkpok_dl
                test_bdkg
                #test_nizk_linear_cl
                #test_pvss_reshare
                )

### Automatically create target and test from TESTS_LIST
foreach (test ${TESTS_LIST})
  # /!\ The test and the target building the executable have the same name
  add_executable (${test} ${test}.cpp)
  target_link_libraries (${test} PUBLIC bicycl project)
  add_dependencies (tests ${test})
  add_test (NAME ${test} COMMAND ${test}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach ()

### Simple DKG demonstration (standalone executable)
add_executable (simple_dkg simple_dkg.cpp)
target_link_libraries (simple_dkg PUBLIC bicycl)

### Simple DKG with Class Group (for A2L project)
add_executable (simple_dkg_cl simple_dkg_cl.cpp)
target_link_libraries (simple_dkg_cl PUBLIC bicycl)

### Simple BDKG (based on test_bdkg, with file output)
add_executable (simple_bdkg simple_bdkg.cpp)
target_link_libraries (simple_bdkg PUBLIC project)

### Simple CL DKG (pure Class Group DKG, generates CL public key)
add_executable (simple_cl_dkg simple_cl_dkg.cpp)
target_link_libraries (simple_cl_dkg PUBLIC project)

add_executable (feldman_dkg_cl feldman_dkg_cl.cpp)
target_link_libraries (feldman_dkg_cl PUBLIC project)
