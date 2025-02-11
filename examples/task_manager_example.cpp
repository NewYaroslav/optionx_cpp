#define LOGIT_BASE_PATH "E:\\_repoz\\optionx_cpp"
#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2
//#include "optionx_cpp/parts/utils/tasks.hpp"
#include "optionx_cpp/parts/data.hpp"
#include "optionx_cpp/parts/utils.hpp"
#include <iostream>
#include <thread>

int main() {
    // Add default logging settings
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();

    {
        optionx::utils::TaskManager task_manager;

        // Add a delayed task that will execute after 3 seconds
        task_manager.add_delayed_task(3000, [](std::shared_ptr<optionx::utils::Task> task) {
            LOGIT_STREAM_TRACE() << "Delayed single task executed! Delay: " << task->get_delay() << " ms";
        });

        // Add a periodic task that executes every 2 seconds
        task_manager.add_periodic_task(2000, [](std::shared_ptr<optionx::utils::Task> task) {
            static int counter = 0;
            LOGIT_STREAM_TRACE() << "Periodic task executed: " << ++counter
                      << " | Delay: " << task->get_delay() << " ms";
            if (counter >= 5) {
                task->shutdown();
            }
        });

        // Add a task scheduled to execute at a specific time
        auto future_time = optionx::utils::Task::get_current_time() + 5000; // Executes in 5 seconds
        task_manager.add_on_date_task(future_time, [](std::shared_ptr<optionx::utils::Task> task) {
            LOGIT_STREAM_TRACE() << "Task executed at specified date! Delay: " << task->get_delay() << " ms";
        });

        // Add a periodic task that starts executing at a specific time
        auto start_time = optionx::utils::Task::get_current_time() + 6000; // Starts in 6 seconds
        task_manager.add_periodic_on_date_task(start_time, 1000, [](std::shared_ptr<optionx::utils::Task> task) {
            static int periodic_counter = 0;
            LOGIT_STREAM_TRACE() << "Periodic on-date task executed: " << ++periodic_counter
                      << " | Delay: " << task->get_delay() << " ms";
            if (periodic_counter >= 3) {
                task->shutdown();
            }
        });

        // Run the TaskManager in a loop
        while (true) {
            task_manager.process(); // Process ready tasks
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Avoid busy-waiting
            if (!task_manager.has_active_tasks()) break; // Exit the loop if there are no active tasks
        }
        task_manager.shutdown(); // Gracefully shutdown the TaskManager
    }

    {
        LOGIT_TRACE0();
        optionx::utils::TaskManager task_manager;

        // Add a delayed task that will execute after 3 seconds
        task_manager.add_delayed_task(3000, [](std::shared_ptr<optionx::utils::Task> task) {
            LOGIT_STREAM_TRACE() << "Delayed single task executed! Delay: " << task->get_delay() << " ms";
        });

        // Add a periodic task that executes every 2 seconds
        task_manager.add_periodic_task(2000, [](std::shared_ptr<optionx::utils::Task> task) {
            static int counter = 0;
            LOGIT_STREAM_TRACE() << "Periodic task executed: " << ++counter
                      << " | Delay: " << task->get_delay() << " ms";
            if (counter >= 5) {
                task->shutdown();
            }
        });

        // Add a task scheduled to execute at a specific time
        auto future_time = optionx::utils::Task::get_current_time() + 5000; // Executes in 5 seconds
        task_manager.add_on_date_task(future_time, [](std::shared_ptr<optionx::utils::Task> task) {
            LOGIT_STREAM_TRACE() << "Task executed at specified date! Delay: " << task->get_delay() << " ms";
        });

        // Add a periodic task that starts executing at a specific time
        auto start_time = optionx::utils::Task::get_current_time() + 6000; // Starts in 6 seconds
        task_manager.add_periodic_on_date_task(start_time, 1000, [](std::shared_ptr<optionx::utils::Task> task) {
            static int periodic_counter = 0;
            LOGIT_STREAM_TRACE() << "Periodic on-date task executed: " << ++periodic_counter
                      << " | Delay: " << task->get_delay() << " ms";
            if (periodic_counter >= 3) {
                task->shutdown();
            }
        });

        task_manager.run();
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        task_manager.shutdown();
    }

    LOGIT_WAIT();
    return 0;
}
