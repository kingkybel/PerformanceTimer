add_executable(run_tests
    run_tests.cc
    performance_timer_tests.cc
)

set(_dkyb_gtest_target gtest)
set(_dkyb_gtest_main_target gtest_main)
if(TARGET GTest::gtest)
    set(_dkyb_gtest_target GTest::gtest)
endif()
if(TARGET GTest::gtest_main)
    set(_dkyb_gtest_main_target GTest::gtest_main)
endif()

target_link_libraries(run_tests PRIVATE
    PerformanceTimer
    ${_dkyb_gtest_target}
    ${_dkyb_gtest_main_target}
)
add_test(NAME run_tests COMMAND run_tests)
