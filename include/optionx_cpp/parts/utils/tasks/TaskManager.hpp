#pragma once
#ifndef _OPTIONX_UTILS_TASK_MANAGER_HPP_INCLUDED
#define _OPTIONX_UTILS_TASK_MANAGER_HPP_INCLUDED

/// \file TaskManager.hpp
/// \brief Contains the definition of the TaskManager class for managing tasks.

#include "Task.hpp"
#include <list>
#include <mutex>
#include <atomic>
#include <memory>

namespace optionx {
namespace utils {

    /// \class TaskManager
    /// \brief Manages the execution and scheduling of tasks.
    class TaskManager {
    public:
        using TaskPtr = std::shared_ptr<Task>;

        /// \brief Default constructor.
        TaskManager()
            : m_force_execute(false), m_shutdown(false), m_task_count(0) {
        }

        /// \brief Default destructor.
        ~TaskManager() = default;

        /// \brief Adds a single execution task.
        /// \param callback The callback function to execute.
        bool add_single_task(Task::Callback callback) {
            if (m_shutdown) return false;
            add_task(std::make_shared<Task>(TaskType::SINGLE, std::move(callback)));
            return true;
        }

        /// \brief Adds a delayed task.
        /// \param delay_ms Delay in milliseconds before execution.
        /// \param callback The callback function to execute.
        bool add_delayed_task(int64_t delay_ms, Task::Callback callback) {
            if (m_shutdown) return false;
            add_task(std::make_shared<Task>(TaskType::DELAYED_SINGLE, std::move(callback), delay_ms));
            return true;
        }

        /// \brief Adds a periodic task.
        /// \param period_ms Period in milliseconds between executions.
        /// \param callback The callback function to execute.
        bool add_periodic_task(int64_t period_ms, Task::Callback callback) {
            if (m_shutdown) return false;
            add_task(std::make_shared<Task>(TaskType::PERIODIC, std::move(callback), 0, period_ms));
            return true;
        }

        /// \brief Adds a delayed periodic task.
        /// \param delay_ms Initial delay in milliseconds.
        /// \param period_ms Period in milliseconds between executions.
        /// \param callback The callback function to execute.
        bool add_delayed_periodic_task(int64_t delay_ms, int64_t period_ms, Task::Callback callback) {
            if (m_shutdown) return false;
            add_task(std::make_shared<Task>(TaskType::DELAYED_PERIODIC, std::move(callback), delay_ms, period_ms));
            return true;
        }

        /// \brief Adds a task to execute at a specific timestamp.
        /// \param timestamp_ms Timestamp in milliseconds for execution.
        /// \param callback The callback function to execute.
        bool add_on_date_task(int64_t timestamp_ms, Task::Callback callback) {
            if (m_shutdown) return false;
            add_task(std::make_shared<Task>(TaskType::ON_DATE, std::move(callback), 0, 0, timestamp_ms));
            return true;
        }

        /// \brief Adds a periodic task starting at a specific timestamp.
        /// \param timestamp_ms Initial timestamp in milliseconds.
        /// \param period_ms Period in milliseconds between executions.
        /// \param callback The callback function to execute.
        bool add_periodic_on_date_task(int64_t timestamp_ms, int64_t period_ms, Task::Callback callback) {
            if (m_shutdown) return false;
            add_task(std::make_shared<Task>(TaskType::PERIODIC_ON_DATE, std::move(callback), 0, period_ms, timestamp_ms));
            return true;
        }

        /// \brief Processes and executes ready tasks.
        void process() {
            std::list<TaskPtr> local_pending_tasks;

            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_pending_tasks.empty()) {
                local_pending_tasks.swap(m_pending_tasks);
            }
            lock.unlock();

            if (!local_pending_tasks.empty()) {
                m_tasks.insert(m_tasks.end(), local_pending_tasks.begin(), local_pending_tasks.end());
            }

            auto now = Task::get_current_time();

            for (auto& task : m_tasks) {
                if (m_force_execute) {
                    task->force_execute();
                }
                if (m_shutdown) {
                    task->shutdown();
                }
                if (!task->is_completed()) {
                    task->process(now, task);
                }
            }

            m_force_execute = false;

            m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(),
                [](const TaskPtr& task) {
                    return task->is_completed();
                }), m_tasks.end());
            m_task_count = m_tasks.size();
        }

        /// \brief Shuts down the task manager, preventing further additions.
        void shutdown() {
            m_shutdown = true;
            process();
        }

        /// \brief Forces the execution of all tasks.
        void force_execute() {
            while (m_force_execute) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            m_force_execute = true;
        }

        /// \brief Returns the number of active tasks.
        /// \return Number of active tasks.
        size_t active_task_count() const {
            return m_task_count;
        }

        /// \brief Checks if there are any active tasks.
        /// \return True if there are active tasks; false otherwise.
        bool has_active_tasks() const {
            return (m_task_count > 0);
        }

        /// \brief Gets the current time in milliseconds.
        /// \return The current time in milliseconds.
        static int64_t get_current_time() {
            return Task::get_current_time();
        }

    private:
        std::mutex         m_mutex;
        std::list<TaskPtr> m_pending_tasks;
        std::list<TaskPtr> m_tasks;
        std::atomic<bool>  m_force_execute;
        std::atomic<bool>  m_shutdown;
        std::atomic<size_t> m_task_count;

		/// \brief Adds a new task to the manager.
        /// \param task A unique pointer to the task.
        void add_task(TaskPtr task) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending_tasks.push_back(std::move(task));
        }
    }; // TaskManager

} // namespace utils
} // namespace optionx

#endif // _OPTIONX_UTILS_TASK_MANAGER_HPP_INCLUDED
