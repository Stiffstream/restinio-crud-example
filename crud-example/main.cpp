#include <restinio/all.hpp>

#include "multithreading.hpp"
#include "request_processor.hpp"

namespace crud_example
{

// Type of message for pushing tasks to a pool of worker threads.
struct task_t
{
	std::function<void()> m_task;

	task_t() = default;

	template<typename F>
	task_t(F && task) : m_task{std::forward<F>(task)} {}
};

// Type of message queue of task_t objects.
using task_queue_t = message_queue_t<task_t>;

// Short alias for express-like router.
using router_t = restinio::router::express_router_t<>;

auto make_router(
	task_queue_t & queue,
	request_processor_t & processor)
{
	auto router = std::make_unique<router_t>();

	//
	// Every handler just delegates the actual work to `processor`.
	//

	router->http_get("/all/v1/pets",
			[&queue, &processor](const auto & req, const auto &) {
				queue.push(
					task_t{
						[req, &processor] {
							processor.on_get_all_pets(req);
						}
					});
				return restinio::request_accepted();
			});

	router->http_post("/all/v1/pets",
			[&queue, &processor](const auto & req, const auto &) {
				queue.push(
					task_t{
						[req, &processor] {
							processor.on_create_new_pet(req);
						}
					});
				return restinio::request_accepted();
			});

	router->http_get(R"--(/all/v1/pets/:id(\d+))--",
			[&queue, &processor](const auto & req, const auto & params) {
				const auto id = restinio::cast_to<pet_id_t>(params["id"]);
				queue.push(
					task_t{
						[req, &processor, id] {
							processor.on_get_specific_pet(req, id);
						}
					});
				return restinio::request_accepted();
			});

	router->add_handler(
			restinio::http_method_patch(),
			R"--(/all/v1/pets/:id(\d+))--",
			[&queue, &processor](const auto & req, const auto & params) {
				const auto id = restinio::cast_to<pet_id_t>(params["id"]);
				queue.push(
					task_t{
						[req, &processor, id] {
							processor.on_patch_specific_pet(req, id);
						}
					});
				return restinio::request_accepted();
			});

	router->http_delete(R"--(/all/v1/pets/:id(\d+))--",
			[&queue, &processor](const auto & req, const auto & params) {
				const auto id = restinio::cast_to<pet_id_t>(params["id"]);
				queue.push(
					task_t{
						[req, &processor, id] {
							processor.on_delete_specific_pet(req, id);
						}
					});
				return restinio::request_accepted();
			});

	return router;
}

void worker_thread_func(
	task_queue_t & queue)	
{
	for(;;)
	{
		// Try to extract next message to process.
		task_t msg;
		const auto pop_result = queue.pop(msg);
		if(pop_result_t::queue_closed == pop_result)
			break;

		// Extracted task should be executed.
		// NOTE: because this is just example we don't handle
		// exceptions from the task.
		// In production code there should be try-catch blocks with
		// some reaction to an exception: logging of the exception and
		// maybe the correct shutdown of the server.
		msg.m_task();
	}
}

// Type of shutdowner to be used with thread-pool.
// Closes the specified task-queue. This gives a signal to
// worker threads to finish their work.
class my_shutdowner_t
{
	task_queue_t & m_queue;
public:
	my_shutdowner_t(task_queue_t & queue) : m_queue{queue} {}

	void operator()() noexcept
	{
		m_queue.close();
	}
};

using my_thread_pool_t = thread_pool_t<my_shutdowner_t>;

} /* namespace crud_example */

void run_application()
{
	using namespace crud_example;

	db_layer_t db{ "pets.db3" };
	request_processor_t processor{ db };

	task_queue_t queue;
	my_thread_pool_t worker_threads_pool{
			3,
			my_shutdowner_t{queue},
			worker_thread_func, std::ref(queue)
	};

	// Default traits are used as a base because they are thread-safe.
	struct my_traits_t : public restinio::default_traits_t
	{
		using request_handler_t = router_t;
	};

	restinio::run(
		restinio::on_this_thread<my_traits_t>()
			.port(8080)
			.address("localhost")
			.request_handler(make_router(queue, processor))
			.cleanup_func([&worker_threads_pool] {
				worker_threads_pool.stop();
			}));
}

int main()
{
	try
	{
		run_application();
	}
	catch(const std::exception & x)
	{
		std::cerr << "Exception caught: " << x.what() << std::endl;
		return 2;
	}
	catch(...)
	{
		std::cerr << "Unknown exception" << std::endl;
		return 3;
	}

	return 0;
}

