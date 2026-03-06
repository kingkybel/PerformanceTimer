/*
 * File:		performance_timer_tests.cc
 * Description: Unit tests for perfomance test utilities
 *
 * Copyright (C) 2023 Dieter J Kybelksties <github@kybelksties.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * @date: 2023-08-28
 * @author: Dieter J Kybelksties
 */

#define DO_PERFORMANCE_
#include "performance_timer.h"

#include <cmath>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <thread>

using namespace std;
using namespace util;

class TimerTest : public ::testing::Test
{
    protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

#ifdef DO_PERFORMANCE_
TEST_F(TimerTest, correct_performance_measurement)
#else
TEST_F(TimerTest, DISABLED_correct_performance_measurement)
#endif
{
    RESET_PERF;
    auto& tmr = util::performance_timer::instance();
    ASSERT_EQ(tmr.get_stats().size(), 0UL);
    size_t num_outer_loop = 30;
    try
    {
        START_PERF;
        for(size_t j = 0; j < num_outer_loop; j++)
        {
            START_NAMED_PERF(outer_loop);
            for(int i = 0; i < 1000; i++)
            {
                int x = i * j;
                x     = x * x;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            END_PERF
        }
        END_PERF;
    }
    catch(const std::exception& e)
    {
        FAIL() << "Well formed performance measurement should not have thrown";
    }
    ASSERT_EQ(tmr.get_stats().size(), 2UL);
    auto outer_stats = tmr.get_stat("outer_loop");
    ASSERT_EQ(outer_stats.times_entered_, num_outer_loop);
    ASSERT_TRUE(tmr.empty());
}

#ifdef DO_PERFORMANCE_
TEST_F(TimerTest, incorrect_performance_measurement)
#else
TEST_F(TimerTest, DISABLED_incorrect_performance_measurement)
#endif
{
    RESET_PERF;
    auto& tmr = util::performance_timer::instance();
    try
    {
        END_PERF;
        FAIL() << "Incorrectly formed performance measurement should have thrown";
    }
    catch(const std::exception& e)
    {
        // all ok
    }
    ASSERT_EQ(tmr.get_stats().size(), 0UL);
    ASSERT_TRUE(tmr.empty());
}

void simVsMeasure(std::chrono::nanoseconds delay_ns, bool doSimulation)
{
    START_NAMED_PERF(simVsMeasure)
    if(doSimulation)
    {
        auto& tmr = util::performance_timer::instance();
        tmr.simulate_time(delay_ns);
    }
    else
    {
        std::this_thread::sleep_for(delay_ns);
    }
    END_PERF
};

#ifdef DO_PERFORMANCE_
TEST_F(TimerTest, simulation_vs_measured_delay_test)
#else
TEST_F(TimerTest, DISABLED_simulation_vs_measured_delay_test)
#endif
{
    RESET_PERF;
    auto& tmr = util::performance_timer::instance();
    for(size_t i = 0UL; i < 100000UL; i++)
        simVsMeasure(std::chrono::nanoseconds{50}, true);
    auto simStat = tmr.get_stat("simVsMeasure");
    RESET_PERF;
    for(size_t i = 0UL; i < 100000UL; i++)
        simVsMeasure(std::chrono::nanoseconds{50}, false);
    auto msrStat = tmr.get_stat("simVsMeasure");

    auto simTotal = simStat.aggregate_time_;
    auto msrTotal = msrStat.aggregate_time_;
    auto toleranceFactor = 1.0/6.3;
    auto tolerance = msrTotal * toleranceFactor;
    ASSERT_LE(std::abs(simTotal-msrTotal), tolerance) << "Tolerance factor " << toleranceFactor << " is too big. Try adjusting.";
}
