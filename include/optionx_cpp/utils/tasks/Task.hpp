#pragma once
#ifndef _OPTIONX_UTILS_TASK_HPP_INCLUDED
#define _OPTIONX_UTILS_TASK_HPP_INCLUDED

/// \file Task.hpp
/// \brief Contains the definition of the Task class for managing scheduled tasks.

#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <time_shield_cpp/time_shield.hpp>
#include <log-it/LogIt.hpp>

namespace optionx::utils {

    /// \enum TaskType
    /// \brief Defines the types of tasks.
    enum class TaskType {
        SINGLE,             ///< Single execution task.
        DELAYED_SINGLE,     ///< Single execution task with a delay.
        PERIODIC,           ///< Periodic task.
        DELAYED_PERIODIC,   ///< Periodic task with an initial delay.
        ON_DATE,            ///< Single task scheduled to execute at a specific timestamp.
        PERIODIC_ON_DATE    ///< Periodic task starting at a specific timestamp.
    };

    class TaskManager;

    /// \class Task
    /// \brief Represents a task with parameters for scheduling and execution.
    class Task {
    public:
        using Callback = std::function<void(std::shared_ptr<Task>)>;

        /// \brief Constructs a Task object.
        /// \param type The type of the task.
        /// \param callback The callback function to execute for the task.
        /// \param delay_ms Delay in milliseconds before execution (optional).
        /// \param period_ms Period in milliseconds for periodic tasks (optional).
        /// \param timestamp_ms Specific timestamp in milliseconds for tasks on a date (optional).
        Task(TaskType type,
             Callback callback,
             int64_t delay_ms = 0,
             int64_t period_ms = 0,
             int64_t timestamp_ms = 0)
            : m_type(type), m_callback(std::move(callback)),
              m_delay_ms(delay_ms), m_period_ms(period_ms),
              m_timestamp_ms(timestamp_ms),
              m_start_time(get_current_time() + m_period_ms),
              m_next_execution_time(get_current_time() + delay_ms),
              m_reschedule_time(0),
              m_completed(false), m_force_execute(false), m_shutdown(false) {
		}

		~Task() = default;

		/// \brief Reschedules the task to execute at a specific time.
        /// \param new_time_ms The new timestamp in milliseconds.
        void reschedule_at(int64_t new_time_ms) {
            if (m_shutdown) return;
            std::lock_guard<std::mutex> lock(m_mutex);
            m_next_execution_time = new_time_ms;
            m_start_time = new_time_ms;
            m_timestamp_ms = new_time_ms;
            m_reschedule_time = new_time_ms;
            m_completed = false;
        }

        /// \brief Reschedules the task to execute after a specific delay.
        /// \param new_delay_ms The new delay in milliseconds.
        void reschedule_in(int64_t new_delay_ms) {
            if (m_shutdown) return;
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = get_current_time();
            m_next_execution_time = now + new_delay_ms;
            m_start_time = m_next_execution_time;
            m_timestamp_ms = m_next_execution_time;
            m_reschedule_time = m_next_execution_time;
            m_completed = false;
        }

        /// \brief Sets a new period for periodic tasks.
        /// \param new_period_ms The new period in milliseconds.
        void set_period(int64_t new_period_ms) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_start_time -= m_period_ms;
            m_period_ms = new_period_ms;
            m_start_time += m_period_ms;
        }

        /// \brief Resets the timer for periodic tasks.
        void reset_timer() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_next_execution_time += m_period_ms;
            m_start_time += m_period_ms;
            m_timestamp_ms += m_period_ms;
            m_reschedule_time = 0;
        }

        /// \brief Checks if the task is ready to execute.
        /// \return True if the task is ready, false otherwise.
        bool is_ready() const {
            if (m_completed) return false;
            if (m_force_execute || m_shutdown) return true;
            int64_t current_time_ms = get_current_time();
            std::lock_guard<std::mutex> lock(m_mutex);
            switch (m_type) {
            case TaskType::SINGLE:
                return (current_time_ms >= m_reschedule_time);
            case TaskType::DELAYED_SINGLE:
                return (current_time_ms >= m_next_execution_time);
            case TaskType::PERIODIC:
                return (current_time_ms >= m_start_time);
            case TaskType::DELAYED_PERIODIC:
                return (current_time_ms >= m_next_execution_time);
            case TaskType::ON_DATE:
                return (current_time_ms >= m_timestamp_ms);
            case TaskType::PERIODIC_ON_DATE:
                return (current_time_ms >= m_timestamp_ms);
            default:
                LOGIT_ERROR("Unknown task type detected.");
                break;
            };
            return false;
        }

        /// \brief Marks the task for forced execution.
        void force_execute() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_force_execute = true;
        }

        /// \brief Checks if the task is periodic.
        /// \return True if the task is periodic, false otherwise.
        bool is_periodic() const {
            return m_type == TaskType::PERIODIC ||
                   m_type == TaskType::DELAYED_PERIODIC ||
                   m_type == TaskType::PERIODIC_ON_DATE;
        }

        /// \brief Checks if the task is completed.
        /// \return True if the task is completed, false otherwise.
        bool is_completed() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_completed;
        }

        /// \brief Checks if the force execute flag is set.
        /// \details Indicates whether all tasks are currently forced to execute immediately,
        ///          regardless of their schedule or readiness state.
        /// \return True if the force execute flag is set; false otherwise.
        bool is_force_execute() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_force_execute;
        }

        /// \brief Checks if the shutdown flag is set.
        /// \details Indicates whether the task manager is in the process of shutting down,
        ///          which prevents new tasks from being added and ensures all tasks are finalized.
        /// \return True if the shutdown flag is set; false otherwise.
        bool is_shutdown() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_shutdown;
        }

        /// \brief Gets the current time in milliseconds.
        /// \return The current time in milliseconds.
        static int64_t get_current_time() {
#           ifndef OPTIONX_TIMESTAMP_MS
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
#           else
            return OPTIONX_TIMESTAMP_MS;
#           endif
        }

        /// \brief Gets the task's next execution time.
        int64_t get_next_execution_time() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_execution_time;
        }

        /// \brief Calculates the delay between the scheduled execution time and the actual execution time.
        /// \return The delay in milliseconds.
        int64_t get_delay() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return get_current_time() - m_execution_time;
        }

        /// \brief Shuts down the task, preventing further execution.
        void shutdown() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_shutdown = true;
        }

	protected:

        /// \brief Marks the task as completed.
        void complete() {
            m_completed = true;
        }

		/// \brief Executes the task's callback.
        void process(int64_t current_time_ms, std::shared_ptr<Task> task) {
            if (m_completed) return;

            switch (m_type) {
            case TaskType::SINGLE: {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_execution_time = m_reschedule_time ? m_reschedule_time : m_start_time;
                if (current_time_ms >= m_reschedule_time || m_force_execute || m_shutdown) {
                    m_reschedule_time = 0;
                    lock.unlock();
                    m_completed = true;
                    m_callback(std::move(task));
                }
                break;
            }
            case TaskType::DELAYED_SINGLE: {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_execution_time = m_next_execution_time;
                if (current_time_ms >= m_next_execution_time || m_force_execute || m_shutdown) {
                    lock.unlock();
                    m_completed = true;
                    m_callback(std::move(task));
                }
                break;
            }
            case TaskType::PERIODIC: {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_execution_time = m_start_time;
                if (current_time_ms >= m_start_time || m_force_execute || m_shutdown) {
                    while (current_time_ms >= m_start_time) {
                        m_start_time += m_period_ms;
                    }
                    lock.unlock();
                    m_callback(std::move(task));
                }
                break;
            }
            case TaskType::DELAYED_PERIODIC: {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_execution_time = m_next_execution_time;
                if (current_time_ms >= m_next_execution_time || m_force_execute || m_shutdown) {
                    while (current_time_ms >= m_next_execution_time) {
                        m_next_execution_time += m_period_ms;
                    }
                    lock.unlock();
                    m_callback(std::move(task));
                }
                break;
            }
            case TaskType::ON_DATE: {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_execution_time = m_timestamp_ms;
                if (current_time_ms >= m_timestamp_ms || m_force_execute || m_shutdown) {
                    lock.unlock();
                    m_completed = true;
                    m_callback(std::move(task));
                }
                break;
            }
            case TaskType::PERIODIC_ON_DATE: {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_execution_time = m_timestamp_ms;
                if (current_time_ms >= m_timestamp_ms || m_force_execute || m_shutdown) {
                    while (current_time_ms >= m_timestamp_ms) {
                        m_timestamp_ms += m_period_ms;
                    }
                    lock.unlock();
                    m_callback(std::move(task));
                }
                break;
            }
            default:
                LOGIT_ERROR("Unknown task type in process.");
                break;
            };
            m_force_execute = false;
            if (m_shutdown) m_completed = true;
        }

		friend class TaskManager;

    private:
        mutable std::mutex m_mutex;
        TaskType m_type;
        Callback m_callback;
        int64_t  m_delay_ms;
        int64_t  m_period_ms;
        int64_t  m_timestamp_ms;
        int64_t  m_start_time;
        int64_t  m_next_execution_time;
        int64_t  m_reschedule_time;
        int64_t  m_execution_time;
        std::atomic<bool> m_completed;
        std::atomic<bool> m_force_execute;
        std::atomic<bool> m_shutdown;
    }; // Task

} // namespace optionx::utils

#endif // _OPTIONX_UTILS_TASK_HPP_INCLUDED
