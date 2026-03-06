/*
 * File:        performance_timer.h
 * Description: High-precision timer
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
#include <chrono>
#include <deque>
#include <exception>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

#ifndef NS_UTIL_TIMER_H_INCLUDED
    #define NS_UTIL_TIMER_H_INCLUDED

namespace util
{
struct no_such_key : std::runtime_error
{
    no_such_key(const std::string& key = "<NO OPEN KEY>")
    : std::runtime_error(std::string("cannot find stats for for key '" + key + "'"))
    {
    }
};

/**
 * @brief Simple timer class for performance tests.
 * Add
 * <ul>
 *      <li> RESET_PERF: reset the perfomance recording structures </li>
 *      <li> START_PERF: Add this to start the recording of time </li>
 *      <li> START_NAMED_PERF(name): Add this to start the recording of time with an alias </li>
 *      <li> SIMULATE_TIME(time_ns): simulate time to speed up otherwise lengthy operations/tests </li>
 *      <li> END_PERF: Stop the recording of time </li>
 * </ul
 *
 */
class performance_timer
{
    public:
    using clock_t      = std::chrono::high_resolution_clock;
    using second_t     = std::chrono::duration<double, std::ratio<1>>;
    using nanosecond_t = std::chrono::duration<double, std::nano>;
    struct stats
    {
        int32_t                          start_line_     = -1;
        int32_t                          end_line_       = -1;
        std::chrono::time_point<clock_t> start_          = clock_t::now();
        std::chrono::time_point<clock_t> end_            = clock_t::now();
        size_t                           times_entered_  = 0UL;
        double                           aggregate_time_ = 0.0;
    };

    private:
    performance_timer()                              = default;
    performance_timer(performance_timer&)            = delete;
    performance_timer& operator=(performance_timer&) = delete;

    std::unordered_map<std::string, stats>       stat_map_{};
    std::unordered_map<std::string, std::string> alias_{};
    std::deque<std::string>                      marker_stack_{};

    public:
    /**
     * @brief Singleton instance.
     *
     * @return performance_timer& the one and only instance
     */
    static performance_timer& instance()
    {
        static auto the_instance = performance_timer{};
        return the_instance;
    }

    /**
     * @brief Reset the recording structures.
     */
    void reset()
    {
        stat_map_.clear();
        marker_stack_.clear();
    }

    /**
     * @brief Start the recording of time.
     * 
     * @param key unique string to identify the section of code to measuer.
     * @param start_line line in the code where recording starts
     * @param alias an optional alis to make it easier to find the statistics structure where perfomance is recorded.
     */
    void start(const std::string& key, int32_t start_line, std::optional<std::string> alias = {})
    {
        auto found = stat_map_.find(key);
        if(found == stat_map_.end())
        {
            // insert an element
            auto emplaced = stat_map_.emplace(key, stats{});
            found         = emplaced.first;
        }
        if(alias)
            alias_[alias.value()] = key;

        found->second.start_line_ = start_line;
        found->second.start_      = clock_t::now();
        found->second.times_entered_++;
        marker_stack_.push_front(key);
    }

    /**
     * @brief End the recording of a code section,
     * 
     * @param end_line line in the code where recording ends
     */
    void end(int32_t end_line)
    {
        if(marker_stack_.empty())
            throw util::no_such_key();

        auto key = marker_stack_[0];
        marker_stack_.pop_front();
        auto found = stat_map_.find(key);
        if(found == stat_map_.end())
            throw util::no_such_key(key);
        found->second.end_line_ = end_line;
        found->second.end_      = clock_t::now();
        found->second.aggregate_time_ +=
         (std::chrono::duration_cast<nanosecond_t>(found->second.end_ - found->second.start_).count());
        stat_map_[key];
    }

    /**
     * @brief Add the given time in nanoseconds to every recording frame on the stack.
     * 
     * @param time_ns time in nanoseconds
     */
    void simulate_time(std::chrono::nanoseconds time_ns)
    {
        // increase the times for every timing frame on the stack by given nano-seconds
        for(const auto& key: marker_stack_)
        {
            auto found = stat_map_.find(key);
            found->second.aggregate_time_ += static_cast<double>(time_ns.count());
        }
    }

    /**
     * @brief Retrieve all recorded statistics.
     * 
     * @return the (iterable) container with the statistics 
     */
    auto get_stats() const
    {
        return stat_map_;
    }

    /**
     * @brief Get the stat object for a given key.
     * 
     * @param key string-key or alias
     * @return util::performance_timer::stats the statistics for the given key, or empty stats if key cannot be found
     */
    auto get_stat(const std::string& key) const
    {
        auto found = stat_map_.find(key);
        if(found != stat_map_.end())
            return found->second;
        auto found_alias = alias_.find(key);
        if(found_alias != alias_.end())
            found = stat_map_.find(found_alias->second);
        if(found != stat_map_.end())
            return found->second;
        return stats{};
    }

    /**
     * @brief Get the stack object
     * 
     * @return auto 
     */
    bool empty() const
    {
        return marker_stack_.empty();
    }

    /**
     * @brief ostream operator
     * 
     * @param os outstream to be modified
     * @param tmr perfomance timer object
     * @return std::ostream& the modified stream
     */
    friend std::ostream& operator<<(std::ostream& os, const util::performance_timer& tmr)
    {
        for(const auto& stat: tmr.get_stats())
        {
            os << stat.first << std::endl;
            os << "\tlines:          " << stat.second.start_line_ << "->" << stat.second.end_line_ << std::endl;
            os << "\tnum entered:    " << stat.second.times_entered_ << std::endl;
            os << "\taggregate time: " << stat.second.aggregate_time_ << std::endl;
            os << "\taverage_time:   " << stat.second.aggregate_time_ / static_cast<double>(stat.second.times_entered_)
               << std::endl;
        }
        return os;
    }
};
    #if defined DO_PERFORMANCE_
        #define RESET_PERF                                             \
            {                                                          \
                auto& the_timer = util::performance_timer::instance(); \
                the_timer.reset();                                     \
            }

        #define START_PERF                                                              \
            {                                                                           \
                auto&             the_timer = util::performance_timer::instance();      \
                std::stringstream ss;                                                   \
                ss << __FILE__ << ":" << __LINE__ << "(" << __PRETTY_FUNCTION__ << ")"; \
                the_timer.start(ss.str(), __LINE__);                                    \
            }

        #define START_NAMED_PERF(name)                                                  \
            {                                                                           \
                auto&             the_timer = util::performance_timer::instance();      \
                std::stringstream ss;                                                   \
                ss << __FILE__ << ":" << __LINE__ << "(" << __PRETTY_FUNCTION__ << ")"; \
                the_timer.start(ss.str(), __LINE__, #name);                             \
            }

        #define END_PERF                                               \
            {                                                          \
                auto& the_timer = util::performance_timer::instance(); \
                the_timer.end(__LINE__);                               \
            }

    #else
        #define RESET_PERF
        #define START_PERF
        #define START_NAMED_PERF(name)
        #define SIMULATE_TIME(time_ns)
        #define END_PERF
    #endif  // defined DO_PERFORMANCE_

};  // namespace util

#endif  // NS_UTIL_TIMER_H_INCLUDED
