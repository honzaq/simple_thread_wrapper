#pragma once

#include <atomic>
#include <mutex>
#include <type_traits>
#include "helper.h"

/* Simple thread wrapper provide basic thread functionality when you need create your own thread 
 * How to use it:
    --- Sample 1:
    {
        simple_thread test;
        test.start(
            std::chrono::seconds(1), // wake after 1s
            [&](simple_thread_context_intf & ctx) {
                std::cout << " In the thread " << ctx.was_timeout() << " " << aaa << "\n";
            }
        );

        // simulate run
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // when you explicitly need to stop thread, otherwise thread will be stopped when desctructor is called
        test.stop();
    }

    --- Sample 2:
    {
        std::atomic_bool my_wakeup_value = false;
        std::string my_additional_param = "abc";

        simple_thread test;
        test.start(
            std::chrono::seconds(1),            // wake after 1s
            [&]() -> bool {                     // I want extend wake conditions, by my value
                return my_wakeup_value.load();
            },
            [&](simple_thread_context_intf & ctx, const std::string& additional_param) {
                std::cout << " In the thread " << ctx.was_timeout() << " " << additional_param << "\n";

                // Check if function was triggered by my wakeup value
                if (!ctx.was_timeout() && event1.load()) {
                    my_wakeup_value.store(false);

                    // we can set new timeout 
                    ctx.set_timeout(std::chrono::seconds(2));
                    // we can even throw exception, simple_thread handler will catch it and continue
                    throw std::bad_function_call();
                }

                // we can unlock mutex held by simple_thread, 
                auto unlock_holder = ctx.unlock();
                invode_my_external_callback();
                // put unlocked mutex back to clocked state (not need to called explicitly as destructor will handle this for us)
                unlock_holder.reset();
            },
            std::move(my_additional_param) // We can pass other parameters to thread function
        );

        // simulate run 
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // set wakeup value
        my_wakeup_value.store(true);
        // trigger thread wakeup
        test.notify();

        // simulate run
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Unlocker interface */
class simple_thread_unlock_holder_intf
{
public:
    virtual ~simple_thread_unlock_holder_intf() {};
};
/* Unlocker holder, holds unlocked mutex, for example when you need invoke callback in your thread function */
using simple_thread_unlock_holder = std::unique_ptr<simple_thread_unlock_holder_intf>;

////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Interface for update thread parameters during your thread function */
class simple_thread_context_intf
{
public:
    /* Define if thread fun was called by thread wait timeout, or by event 
     * \return  true  when timeout occur, otherwise return  false
     */
    virtual bool was_timeout() = 0;
    /* Get last used wait timeout */
    virtual std::chrono::steady_clock::duration get_timeout() = 0;
    /* Set new wait timeout */
    virtual void set_timeout(const std::chrono::steady_clock::duration & duration) = 0;
    /* Allows unlock simple_thread mutex, for example in your thread function you need invoke callback and you do not want deadlock */
    [[nodiscard]] virtual simple_thread_unlock_holder unlock() = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Internal helper classes */
namespace internal {
    class simple_thread_unlock_holder_impl : public simple_thread_unlock_holder_intf
    {
    public:
        explicit simple_thread_unlock_holder_impl(std::unique_lock<std::mutex> & lock)
            : m_lock(lock)
        {
            m_lock.unlock();
        }
        virtual ~simple_thread_unlock_holder_impl()
        {
            m_lock.lock();
        }
    private:
        simple_thread_unlock_holder_impl & operator=(const simple_thread_unlock_holder_impl &) = delete;
        simple_thread_unlock_holder_impl(const simple_thread_unlock_holder &) = delete;
    private:
        std::unique_lock<std::mutex> & m_lock;
    };

    class simple_thread_context : public simple_thread_context_intf
    {
    public:
        bool was_timeout() override {
            return m_was_timeout;
        }

        std::chrono::steady_clock::duration get_timeout() override {
            return m_duration;
        }
        void set_timeout(const std::chrono::steady_clock::duration & duration) override {
            m_duration = duration;
        };

        /* Helper function */
        template <class _Rep, class _Period>
        void set_timeout(const std::chrono::duration<_Rep, _Period> & timeout) {
            set_timeout((std::chrono::steady_clock::duration)timeout);
        };

        [[nodiscard]] simple_thread_unlock_holder unlock() override {
            return std::make_unique<simple_thread_unlock_holder_impl>(m_lock_holder);
        }

        /// Non interface functions
        simple_thread_context(std::unique_lock<std::mutex> & lock_holder)
            : m_lock_holder(lock_holder)
        {}
        void set_was_timeout(bool was_timeout) {
            m_was_timeout = was_timeout;
        }
        auto get_new_timeout() {
            return m_duration;
        }

    private:
        std::unique_lock<std::mutex> & m_lock_holder;
        bool m_was_timeout = false;
        std::chrono::steady_clock::duration m_duration = std::chrono::nanoseconds(0);
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Simple thread wrapper provide basic thread functionality when you need create your own thread 
 */
class simple_thread
{
public:
    
    /* Start thread and call your function, with additional arguments, when then thread is awakened.
     * \param[in]  timeout  define timeout when thread should awake
     * \param[in]  fx       function or lambda which will be called when thread is awakened
     * \param[in]  ...      (optional) additional variadic arguments, which will be passed to fx function
     * \note fx function has first parameter 'simple_thread_context_intf & ', example:
     *   [&](simple_thread_context_intf & ctx) {...}
     */
    template <class _Rep, class _Period, class _Fn, class... _Args>
    std::enable_if_t<std::is_invocable_r_v<void, _Fn, simple_thread_context_intf &, _Args...>>
    start(const std::chrono::duration<_Rep, _Period> & timeout, _Fn && fx, _Args&&... ax)
    {
        start(timeout, [] { return false; }, fx, std::forward<_Args>(ax)...);
    }

    /* Start thread and call your function, with additional arguments, when then thread is awakened.
     * \param[in]  timeout  define timeout when thread should awake
     * \param[in]  pred     function (return bool) which allow extend wake up condition
     * \param[in]  fx       function or lambda which will be called when thread is awakened
     * \param[in]  ...      (optional) additional variadic arguments, which will be passed to fx function
     * \note fx function has first parameter 'simple_thread_context_intf & ', example:
     *   [&](simple_thread_context_intf & ctx) {...}
     */
    template <class _Rep, class _Period, class _Predicate, class _Fn, class... _Args>
    std::enable_if_t<std::is_invocable_r_v<bool, _Predicate> && std::is_invocable_r_v<void, _Fn, simple_thread_context_intf &, _Args...>>
    start(const std::chrono::duration<_Rep, _Period> & timeout, _Predicate pred, _Fn && fx, _Args&&... ax)
    {
        m_thread = std::thread([&]() {
            std::chrono::steady_clock::duration thread_timeout = timeout;
            while (true)
            {
                std::unique_lock lck(m_thread_mutex);

                internal::simple_thread_context ctx(lck);
                auto wait_res = m_thread_cv.wait_for(lck, thread_timeout, [&]() {
                    return pred() || m_thread_stop;
                });

                // stop was signalled, end loop
                if (m_thread_stop) {
                    std::cout << log_time() << " Stop requested\n";
                    return;
                }

                ctx.set_was_timeout(!wait_res);
                ctx.set_timeout(thread_timeout);

                try {
                    fx(ctx, std::forward<_Args>(ax)...);
                }
                catch (...) {
                    std::cout << log_time() << " Exception in thread procedure...\n";
                }
                thread_timeout = ctx.get_new_timeout();
            }
        });
    }

    /* Stop thread */
    void stop()
    {
        {
            std::scoped_lock lck(m_thread_mutex);
            m_thread_stop = true;
        }
        m_thread_cv.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    /* Notify used with external  */
    void notify()
    {
        m_thread_cv.notify_one();
    }
    ~simple_thread()
    {
        stop();
    }

private:
    std::mutex                  m_thread_mutex;          // protects: m_thread_stop
    std::thread                 m_thread;
    bool                        m_thread_stop = false;
    std::condition_variable     m_thread_cv;
};
