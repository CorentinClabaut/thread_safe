#include "queue.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <future>
#include <list>
#include <atomic>
#include <vector>
#include <string>
#include <boost/range/join.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/cxx11/is_permutation.hpp>
#include "gtest_timeout.hpp"
#include "strings_generator.hpp"

using testing::Eq;

namespace threadsafe
{

class test_queue: public ::testing::Test
{
public:
    test_queue() :
        gtest_timeout_(std::chrono::milliseconds{1000})
    {}

    std::vector<std::string> pop_strings(threadsafe::queue<std::string>& queue, std::atomic<int>& strings_left_to_pop_count)
    {
        std::vector<std::string> strings_popped;

        while(--strings_left_to_pop_count >= 0)
        {
            auto popped_string = queue.blocking_pop();

            strings_popped.push_back(std::move(popped_string));
        }

        return strings_popped;
    }

    std::vector<std::string> vector_from_queue(std::queue<std::string> queue)
    {
        std::vector<std::string> strings;
        strings.reserve(queue.size());

        while(!queue.empty())
        {
            strings.push_back(std::move(queue.front()));
            queue.pop();
        }

        return strings;
    }

    std::future<bool> has_async_blocking_pop_thrown()
    {
        auto has_async_blocking_pop_thrown_callback = [&]
        {
            try
            {
                queue_.blocking_pop();
                return false;
            }
            catch(std::exception&)
            {
                return true;
            }
        };

        return std::async(std::launch::async, has_async_blocking_pop_thrown_callback);
    }

    template<class T>
    void expect_future_set(std::future<T>& future, T expected_value)
    {
        const auto status = future.wait_for(std::chrono::milliseconds(1));
        EXPECT_THAT(status, Eq(std::future_status::ready));
        EXPECT_THAT(future.get(), Eq(expected_value));
    }

    template<class T>
    void expect_future_not_set(std::future<T>& future)
    {
        auto status = future.wait_for(std::chrono::milliseconds(50));
        EXPECT_THAT(status, Eq(std::future_status::timeout));
    }

    template<class R1, class R2>
    void expect_is_permutation(R1 const& lhs, R2 const& rhs)
    {
        EXPECT_THAT(boost::size(lhs), Eq(boost::size(rhs)));
        EXPECT_TRUE(boost::algorithm::is_permutation(lhs, boost::begin(rhs)));
    }

    std::string string1_ = "First";
    std::string string2_ = "Second";
    std::string string3_ = "Third";

    threadsafe::queue<std::string> queue_;

    gtest_timeout gtest_timeout_;
};

TEST_F(test_queue, push_then_pop)
{
    queue_.push(string1_);
    queue_.push(string2_);
    queue_.push(string3_);

    auto stringPopped1 = queue_.blocking_pop();
    auto stringPopped2 = queue_.blocking_pop();
    auto stringPopped3 = queue_.blocking_pop();

    EXPECT_THAT(stringPopped1, Eq(string1_));
    EXPECT_THAT(stringPopped2, Eq(string2_));
    EXPECT_THAT(stringPopped3, Eq(string3_));
}

TEST_F(test_queue, pop_block_until_push)
{
    auto future = std::async(std::launch::async, [&]{ return queue_.blocking_pop(); });

    expect_future_not_set(future);

    queue_.push(string1_);

    expect_future_set(future, string1_);
}

TEST_F(test_queue, cant_push_after_release)
{
    queue_.release();
    EXPECT_THROW(queue_.push(string1_), std::exception);
}

TEST_F(test_queue, cant_pop_after_release)
{
    queue_.release();
    EXPECT_THROW(queue_.blocking_pop(), std::exception);
}

TEST_F(test_queue, popping_threads_stopped_on_release)
{
    auto blocking_pop_has_thrown1 = has_async_blocking_pop_thrown();
    auto blocking_pop_has_thrown2 = has_async_blocking_pop_thrown();

    expect_future_not_set(blocking_pop_has_thrown1);
    expect_future_not_set(blocking_pop_has_thrown2);

    auto strings_left_in_queue = queue_.release();

    expect_future_set(blocking_pop_has_thrown1, true);
    expect_future_set(blocking_pop_has_thrown2, true);

    EXPECT_THAT(strings_left_in_queue, Eq(std::queue<std::string>{}));
}

TEST_F(test_queue, release_gives_strings_not_pulled)
{
    queue_.push(string1_);
    queue_.push(string2_);
    queue_.push(string3_);

    auto popped_string = queue_.blocking_pop();
    EXPECT_THAT(popped_string, Eq(string1_));

    auto strings_left_in_queue = queue_.release();
    std::queue<std::string> expected_strings_in_queue({string2_, string3_});
    EXPECT_THAT(strings_left_in_queue, Eq(expected_strings_in_queue));
}

TEST_F(test_queue, several_pushing_threads)
{
    auto push_strings_callback = [&](auto const& strings_to_push)
    {
        boost::for_each(strings_to_push, [&](auto const& s){ queue_.push(s); });
    };

    int strings_to_push_count = 2000;
    auto strings_pushed = generate_random_strings(strings_to_push_count);

    auto half_strings_to_push = strings_pushed.begin() + (strings_to_push_count / 2);
    auto thread1 = std::thread(push_strings_callback, boost::make_iterator_range(strings_pushed.begin(), half_strings_to_push));
    auto thread2 = std::thread(push_strings_callback, boost::make_iterator_range(half_strings_to_push, strings_pushed.end()));

    thread1.join();
    thread2.join();

    std::atomic<int> strings_left_to_pop_count{strings_to_push_count};
    auto strings_popped = pop_strings(queue_, strings_left_to_pop_count);

    expect_is_permutation(strings_pushed, strings_popped);
}

TEST_F(test_queue, several_popping_threads)
{
    int strings_to_push_count = 2000;
    std::atomic<int> strings_left_to_pop_count {strings_to_push_count};
    auto pop_strings_callback = [&]
    {
        return pop_strings(queue_, strings_left_to_pop_count);
    };

    auto strings_pushed = generate_random_strings(strings_to_push_count);

    auto future_strings_popped_thread1 = std::async(std::launch::async, pop_strings_callback);
    auto future_strings_popped_thread2 = std::async(std::launch::async, pop_strings_callback);

    boost::for_each(strings_pushed, [&](auto const& s){ queue_.push(s); });

    auto strings_popped_thread1 = future_strings_popped_thread1.get();
    auto strings_popped_thread2 = future_strings_popped_thread2.get();

    const auto strings_popped = boost::join(strings_popped_thread1, strings_popped_thread2);
    expect_is_permutation(strings_pushed, strings_popped);
}

TEST_F(test_queue, several_pushing_and_popping_threads_stopped_on_release)
{
    auto pop_strings_callback = [&]
    {
        std::list<std::string> popped_strings;

        try
        {
            while(true)
            {
                auto popped_string = queue_.blocking_pop();
                popped_strings.push_back(std::move(popped_string));
            }
        }
        catch(std::exception&)
        {}

        return popped_strings;
    };

    auto push_string_callback = [&]
    {
        std::list<std::string> pushed_strings;

        try
        {
            while(true)
            {
                auto string = generate_random_string();
                queue_.push(string);
                pushed_strings.push_back(std::move(string));
            }
        }
        catch(std::exception&)
        {}

        return pushed_strings;
    };

    std::vector<std::future<std::list<std::string>>> future_strings_popped;
    std::generate_n(std::back_inserter(future_strings_popped), 100, [&]{ return std::async(std::launch::async, pop_strings_callback); });

    std::vector<std::future<std::list<std::string>>> future_strings_pushed;
    std::generate_n(std::back_inserter(future_strings_pushed), 100, [&]{ return std::async(std::launch::async, push_string_callback); });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const auto strings_left_in_queue = queue_.release();

    std::list<std::string> strings_popped;
    boost::for_each(future_strings_popped, [&](std::future<std::list<std::string>>& f){ strings_popped.splice(strings_popped.begin(), f.get()); });

    std::list<std::string> strings_pushed;
    boost::for_each(future_strings_pushed, [&](std::future<std::list<std::string>>& f){ strings_pushed.splice(strings_pushed.begin(), f.get()); });

    auto strings_not_popped_after_release = vector_from_queue(std::move(strings_left_in_queue));
    auto strings_popped_and_still_in_queue = boost::join(strings_popped, strings_not_popped_after_release);

    expect_is_permutation(strings_pushed, strings_popped_and_still_in_queue);
}

} // namespace threadsafe 
