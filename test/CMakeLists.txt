add_executable(run_tests
    run_tests.cc
    performance_timer_tests.cc
)

target_link_libraries(run_tests PRIVATE
    PerformanceTimer
    gtest
    gtest_main
)
add_test(NAME run_tests COMMAND run_tests)
