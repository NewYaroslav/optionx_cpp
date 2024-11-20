#pragma once
#ifndef _OPTIONX_UTILS_PERIODIC_TASK_HPP_INCLUDED
#define _OPTIONX_UTILS_PERIODIC_TASK_HPP_INCLUDED

/// \file PeriodicTask.hpp
/// \brief Provides a class for periodic task execution.

#include <functional>
#include <chrono>
#include <atomic>

namespace optionx {
namespace utils {

    /// \class PeriodicTask
    /// \brief A class for executing a task periodically.
    class PeriodicTask {
    public:
        using Callback = std::function<void()>;

        /// \brief Constructor.
        PeriodicTask() = default;

        /// \brief Sets the callback to be invoked periodically.
        /// \param callback The callback function.
        void set_callback(Callback callback) {
            m_callback = std::move(callback);
        }

        /// \brief Sets the task period.
        /// \param period_ms The period in milliseconds.
        void set_period(int64_t period_ms) {
            m_period_ms = period_ms;
        }

        /// \brief Starts the periodic task.
        void start() {
            m_running = true;
            reset();
        }

        /// \brief Stops the periodic task.
        void stop() {
            m_running = false;
        }

        /// \brief Resets the periodic task timer.
        void reset() {
            m_last_time = std::chrono::steady_clock::now();
        }

        /// \brief Processes the periodic task. Should be called in a loop.
        void process() {
            if (!m_running || !m_callback) return;

            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_time).count();

            if (elapsed_ms >= m_period_ms) {
                m_callback();
                reset();
            }
        }

    private:
        Callback m_callback = nullptr; ///< The callback to be invoked periodically.
        int64_t m_period_ms = 1000; ///< The task period in milliseconds (default: 1 second).
        std::atomic<bool> m_running = false; ///< Whether the task is running.
        std::chrono::steady_clock::time_point m_last_time; ///< The last time the task was executed.
    };

} // namespace utils
} // namespace optionx

#endif // _OPTIONX_UTILS_PERIODIC_TASK_HPP_INCLUDED
