#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace crud_example
{

// A very simple implementation of a thread pool.
//
// Shutdowner is a type of objects that can send a signal to
// finish work to all started threads of the pool.
template<class Shutdowner>
class thread_pool_t
{
	Shutdowner m_shutdowner;

	std::vector<std::thread> m_threads;

	void shutdown_then_join() noexcept
	{
		if(!m_threads.empty())
		{
			m_shutdowner();
			for(auto & t : m_threads)
			{
				if(t.joinable())
					t.join();
			}
			m_threads.clear();
		}
	}

public:
	// This class is not Copyable nor Moveable.
	thread_pool_t(const thread_pool_t &) = delete;
	thread_pool_t(thread_pool_t &&) = delete;

	template<typename... Thread_Args>
	thread_pool_t(
		std::size_t thread_count,
		Shutdowner shutdowner,
		// Those arguments will be forwarded for the constructor
		// of every std::thread object.
		Thread_Args &&... args)
		: m_shutdowner{std::move(shutdowner)}
	{
		// Start all threads and shutdown and wait all started
		// threads in a case of an exception.
		try
		{
			m_threads.reserve(thread_count);
			for(std::size_t i = 0u; i != thread_count; ++i)
				m_threads.emplace_back(std::forward<Thread_Args>(args)...);
		}
		catch(...)
		{
			shutdown_then_join();
			throw;
		}
	}

	~thread_pool_t() noexcept
	{
		shutdown_then_join();
	}

	void stop() noexcept
	{
		shutdown_then_join();
	}
};

enum class pop_result_t
{
	extracted,
	queue_closed
};

// The very first implementation of multi-producer/multi-consumer
// message queue.
//
// This queue can hold only objects of type T.
//
// If a message queue is closed all calls to pop() method will
// return pop_result_t::queue_closed.
template<typename T>
class message_queue_t
{
	std::mutex m_lock;
	std::condition_variable m_not_empty;

	std::queue<T> m_queue;

	bool m_closed{false};

public:
	void push(T what)
	{
		std::lock_guard<std::mutex> lock{m_lock};
		if(!m_closed)
		{
			const bool was_empty = m_queue.empty();
			m_queue.push(std::move(what));
			if(was_empty)
				m_not_empty.notify_one();
		}
	}

	pop_result_t pop(T & receiver)
	{
		std::unique_lock<std::mutex> lock{m_lock};
		for(;;)
		{
			if(m_closed)
				break;

			if(!m_queue.empty())
			{
				receiver = std::move(m_queue.front());
				m_queue.pop();
				return pop_result_t::extracted;
			}

			m_not_empty.wait(lock,
					[&]{ return m_closed || !m_queue.empty(); });
		}

		return pop_result_t::queue_closed;
	}

	void close() noexcept
	{
		std::unique_lock<std::mutex> lock{m_lock};
		if(!m_closed)
		{
			m_closed = true;
			m_not_empty.notify_all();
		}
	}
};

} /* namespace crud_example */

