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
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#if __has_include(<source_location>)
    #include <source_location>
    #if defined(__cpp_lib_source_location) && __cpp_lib_source_location >= 201'907L
        #define DKYB_HAS_SOURCE_LOCATION 1 // NOSONAR
    #endif
#endif

#ifndef DKYB_HAS_SOURCE_LOCATION
    #if __has_include(<experimental/source_location>)
        #include <experimental/source_location>
        #define DKYB_HAS_SOURCE_LOCATION          1 // NOSONAR
        #define DKYB_SOURCE_LOCATION_EXPERIMENTAL 1 // NOSONAR
    #endif
#endif

#ifndef NS_UTIL_TIMER_H_INCLUDED
    #define NS_UTIL_TIMER_H_INCLUDED

namespace util
{
struct no_such_key : std::runtime_error
{
    explicit no_such_key(std::string const& key = "<NO OPEN KEY>")
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
    struct transparent_string_hash
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept
        {
            return std::hash<std::string_view>{}(value);
        }

        std::size_t operator()(std::string const& value) const noexcept
        {
            return (*this)(std::string_view{value});
        }

        std::size_t operator()(char const* value) const noexcept
        {
            return (*this)(std::string_view{value});
        }
    };

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
    performance_timer()                                          = default;
    ~performance_timer()                                         = default;
    performance_timer(performance_timer const&)                  = delete;
    performance_timer(performance_timer&&)                       = delete;
    performance_timer&       operator=(performance_timer const&) = delete;
    performance_timer&       operator=(performance_timer&&)      = delete;
    static performance_timer the_instance_;

    std::unordered_map<std::string, stats, transparent_string_hash, std::equal_to<>>       stat_map_{};
    std::unordered_map<std::string, std::string, transparent_string_hash, std::equal_to<>> alias_{};
    std::deque<std::string>                                                                marker_stack_{};

  public:
    /**
     * @brief Singleton instance.
     *
     * @return performance_timer& the one and only instance
     */
    static performance_timer& instance() noexcept
    {
        return the_instance_;
    }

    /**
     * @brief Reset the recording structures.
     */
    void reset()
    {
        stat_map_.clear();
        alias_.clear();
        marker_stack_.clear();
    }

    /**
     * @brief Start the recording of time.
     *
     * @param key unique string to identify the section of code to measuer.
     * @param start_line line in the code where recording starts
     * @param alias an optional alis to make it easier to find the statistics structure where perfomance is recorded.
     */
    void start(std::string_view key, int32_t start_line, std::optional<std::string_view> alias = std::nullopt)
    {
        auto [found, _inserted] = stat_map_.try_emplace(std::string{key});
        if (alias)
        {
            alias_.insert_or_assign(std::string{*alias}, found->first);
        }

        auto& entry       = found->second;
        entry.start_line_ = start_line;
        entry.start_      = clock_t::now();
        entry.times_entered_++;
        marker_stack_.push_front(found->first);
    }

    /**
     * @brief End the recording of a code section,
     *
     * @param end_line line in the code where recording ends
     */
    void end(int32_t end_line)
    {
        if (marker_stack_.empty())
        {
            throw util::no_such_key();
        }

        auto key = marker_stack_.front();
        marker_stack_.pop_front();
        if (auto found = stat_map_.find(key); found != stat_map_.end())
        {
            auto& entry     = found->second;
            entry.end_line_ = end_line;
            entry.end_      = clock_t::now();
            entry.aggregate_time_ += std::chrono::duration_cast<nanosecond_t>(entry.end_ - entry.start_).count();
            return;
        }
        throw util::no_such_key(key);
    }

    /**
     * @brief Add the given time in nanoseconds to every recording frame on the stack.
     *
     * @param time_ns time in nanoseconds
     */
    void simulate_time(std::chrono::nanoseconds time_ns)
    {
        // increase the times for every timing frame on the stack by given nano-seconds
        for (auto const& key: marker_stack_)
        {
            if (auto found = stat_map_.find(key); found != stat_map_.end())
            {
                found->second.aggregate_time_ += static_cast<double>(time_ns.count());
            }
        }
    }

    /**
     * @brief Retrieve all recorded statistics.
     *
     * @return the (iterable) container with the statistics
     */
    [[nodiscard]] auto const& get_stats() const noexcept
    {
        return stat_map_;
    }

    /**
     * @brief Get the stat object for a given key.
     *
     * @param key string-key or alias
     * @return util::performance_timer::stats the statistics for the given key, or empty stats if key cannot be found
     */
    [[nodiscard]] auto get_stat(std::string_view key) const
    {
        auto const owned_key = std::string{key};
        auto       found     = stat_map_.find(owned_key);
        if (found != stat_map_.end())
        {
            return found->second;
        }

        if (auto found_alias = alias_.find(owned_key); found_alias != alias_.end())
        {
            found = stat_map_.find(found_alias->second);
        }
        if (found != stat_map_.end())
        {
            return found->second;
        }
        return stats{};
    }

    /**
     * @brief Get the stack object
     *
     * @return auto
     */
    [[nodiscard]] bool empty() const noexcept
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
    friend std::ostream& operator<<(std::ostream& os, util::performance_timer const& tmr)
    {
        for (auto const& [key, vals]: tmr.get_stats())
        {
            os << key << std::endl;
            os << "\tlines:          " << vals.start_line_ << "->" << vals.end_line_ << std::endl;
            os << "\tnum entered:    " << vals.times_entered_ << std::endl;
            os << "\taggregate time: " << vals.aggregate_time_ << std::endl;
            os << "\taverage_time:   " << vals.aggregate_time_ / static_cast<double>(vals.times_entered_) << std::endl;
        }
        return os;
    }
};

inline performance_timer performance_timer::the_instance_{};
    #if defined DO_PERFORMANCE_
        #define RESET_PERF                                                                                             \
            {                                                                                                          \
                auto& the_timer = util::performance_timer::instance();                                                 \
                the_timer.reset();                                                                                     \
            }

        #if defined(DKYB_HAS_SOURCE_LOCATION)
            #if defined(DKYB_SOURCE_LOCATION_EXPERIMENTAL)
                #define DKYB_SOURCE_LOCATION_T std::experimental::source_location
            #else
                #define DKYB_SOURCE_LOCATION_T std::source_location
            #endif

            #define START_PERF                                                                                         \
                {                                                                                                      \
                    auto&             the_timer = util::performance_timer::instance();                                 \
                    auto const        loc       = DKYB_SOURCE_LOCATION_T::current();                                   \
                    std::stringstream ss;                                                                              \
                    ss << loc.file_name() << ":" << loc.line() << " (" << loc.function_name() << ")";                  \
                    the_timer.start(ss.str(), static_cast<int32_t>(loc.line()));                                       \
                }

            #define START_NAMED_PERF(name)                                                                             \
                {                                                                                                      \
                    auto&             the_timer = util::performance_timer::instance();                                 \
                    auto const        loc       = DKYB_SOURCE_LOCATION_T::current();                                   \
                    std::stringstream ss;                                                                              \
                    ss << loc.file_name() << ":" << loc.line() << " (" << loc.function_name() << ")";                  \
                    the_timer.start(ss.str(), static_cast<int32_t>(loc.line()), #name);                                \
                }
            #define END_PERF                                                                                           \
                {                                                                                                      \
                    auto&      the_timer = util::performance_timer::instance();                                        \
                    auto const loc       = DKYB_SOURCE_LOCATION_T::current();                                          \
                    the_timer.end(loc.line());                                                                         \
                }
        #else // not defined(DKYB_HAS_SOURCE_LOCATION)
            #define START_PERF                                                                                         \
                {                                                                                                      \
                    auto&             the_timer = util::performance_timer::instance();                                 \
                    std::stringstream ss;                                                                              \
                    ss << __FILE__ << ":" << __LINE__ << "(" << __PRETTY_FUNCTION__ << ")";                            \
                    the_timer.start(ss.str(), __LINE__);                                                               \
                }

            #define START_NAMED_PERF(name)                                                                             \
                {                                                                                                      \
                    auto&             the_timer = util::performance_timer::instance();                                 \
                    std::stringstream ss;                                                                              \
                    ss << __FILE__ << ":" << __LINE__ << "(" << __PRETTY_FUNCTION__ << ")";                            \
                    the_timer.start(ss.str(), __LINE__, #name);                                                        \
                }
            #define END_PERF                                                                                           \
                {                                                                                                      \
                    auto& the_timer = util::performance_timer::instance();                                             \
                    the_timer.end(__LINE__);                                                                           \
                }
        #endif

    #else
        #define RESET_PERF
        #define START_PERF
        #define START_NAMED_PERF(name)
        #define SIMULATE_TIME(time_ns)
        #define END_PERF
    #endif // defined DO_PERFORMANCE_

}; // namespace util

#endif // NS_UTIL_TIMER_H_INCLUDED
