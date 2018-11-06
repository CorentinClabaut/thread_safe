#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <future>
#include <chrono>

namespace threadsafe
{

class gtest_timeout
{
public:
    gtest_timeout(std::chrono::milliseconds timeout) :
        timeout_checker_thread_([&, timeout]{ expect_test_completed_before(timeout); })
    {}

    ~gtest_timeout()
    {
        signal_test_completed();
    }

private:
    void expect_test_completed_before(std::chrono::milliseconds timeout)
    {
        auto finished = test_completed_.get_future();
        auto status = finished.wait_for(timeout);
        if (status == std::future_status::timeout)
            FAIL() << "Test took long than " << timeout.count() << "ms to terminate";
    }

    void signal_test_completed()
    {
        test_completed_.set_value();
        timeout_checker_thread_.join();
    }

    std::promise<void> test_completed_;
    std::thread timeout_checker_thread_;
};

} // namespace threadsafe 
