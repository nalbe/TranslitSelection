// AsyncWorker.hpp
#pragma once

// Implementation-specific headers
#include "cst/ErrorCollector/ErrorCollector.hpp"

// Standard library headers
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <utility>
#include <exception>
#include <functional>
#include <atomic>
#include <memory>



/// AsyncWorker
namespace cst
{

	// =================================================================
	//  AsyncWorker - thread with job queue
	// =================================================================
	class AsyncWorker final :
		public ErrorCollectorMixin
	{
		// -- type aliases -------------------------------------------------
	public:
		using job_type = std::function<void()>;

		// -- members ------------------------------------------------------
	private:
		mutable std::mutex           m_mutex;
		mutable std::mutex           m_syncMutex;
		std::condition_variable      m_queueCv;
		std::condition_variable      m_syncCv;
		std::queue<job_type>         m_job;
		std::atomic<bool>            m_sync;
		std::atomic<bool>            m_done;
		std::thread                  m_thread;

		// -- non-modifiers ------------------------------------------------
	public:
		bool active()                         const noexcept;

		// -- modifiers ----------------------------------------------------
	public:
		bool start()                                noexcept;
		bool enqueue(job_type)                      noexcept;
		void stop()                                 noexcept;
		bool sync()                                 noexcept;

		// -- internals ----------------------------------------------------
	private:
		void loop()                                 noexcept;

		// -- lifecycle ----------------------------------------------------
	public:
		AsyncWorker(bool = false)                   noexcept;
		~AsyncWorker()                              noexcept;
		AsyncWorker(const AsyncWorker&)             = delete;
		AsyncWorker& operator=(const AsyncWorker&)  = delete;
		AsyncWorker(AsyncWorker&&)                  = delete;
		AsyncWorker& operator=(AsyncWorker&&)       = delete;

	};  // class AsyncWorker




	/// -- non-modifiers ------------------------------------------------

	// check active
	bool AsyncWorker::active() const noexcept
	{
		return !m_done.load(std::memory_order_acquire);
	}


	/// -- modifiers ----------------------------------------------------

	// start
	bool AsyncWorker::start() noexcept
	{
		if (m_thread.joinable()) { return true; }
		clear_errors();

		std::lock_guard<std::mutex> lock(m_mutex);
		try {
			m_done.store(false, std::memory_order_release);
			m_sync.store(false, std::memory_order_release);
			m_thread = std::thread(&AsyncWorker::loop, this);
		}
		catch (const std::exception& e) {
			emplace_error(
				FormattedRecord{ "Failed to start thread." }, e
			);
			return false;
		}
		catch (...) {
			emplace_error(
				FormattedRecord{ "Failed to start thread." }
			);
			return false;
		}
		if (!m_thread.joinable()) {
			emplace_error(
				FormattedRecord{ "Failed to join thread." }
			);
			return false;
		}
		return true;
	}

	// enqueue
	bool AsyncWorker::enqueue(job_type job) noexcept
	{
		try {
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_done.load(std::memory_order_acquire)) { return false; }
			m_job.push(std::move(job));
			m_queueCv.notify_one();
			return true;
		}
		catch (const std::exception& e) {
			emplace_error(
				FormattedRecord{ "Failed to enqueue job." }, e
			);
		}
		catch (...) {
			emplace_error(
				FormattedRecord{ "Failed to enqueue job." }
			);
		}
		return false;
	}

	// stop
	void AsyncWorker::stop() noexcept
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_done.load(std::memory_order_acquire)) { return; }
			m_done.store(true, std::memory_order_release);
			m_sync.store(false, std::memory_order_release);
		}
		m_queueCv.notify_all();
		m_syncCv.notify_all();

		if (m_thread.joinable()) {
			m_thread.join();
		}
	}

	// sync
	bool AsyncWorker::sync() noexcept
	{
		if (m_done.load(std::memory_order_acquire)) { return false; }
		if (m_sync.load(std::memory_order_acquire)) { return false; }  // in progress

		m_sync.store(true, std::memory_order_release);
		if (!enqueue([&] {
			m_sync.store(false, std::memory_order_release);
			m_syncCv.notify_all();
		})) {
			m_sync.store(false, std::memory_order_release);
			return false;
		}
		m_queueCv.notify_all();

		std::unique_lock<std::mutex> syncLock(m_syncMutex);
		m_syncCv.wait(syncLock, [this] {
			return !m_sync.load(std::memory_order_acquire);
			});

		return true;
	}


	/// -- internals ----------------------------------------------------

	// loop
	void AsyncWorker::loop() noexcept
	{
		std::queue<job_type> processing;

		while (true) {
			{
				std::unique_lock<std::mutex> queueLock(m_mutex);
				m_queueCv.wait(queueLock, [this] {
					return m_done.load(std::memory_order_acquire) or
						(!has_errors() and !m_job.empty() );
					}
				);

				// drain remaining tasks on shutdown, then exit
				if (m_done.load(std::memory_order_acquire) and
					(has_errors() or m_job.empty()) )
				{
					break;
				}

				// swap queues in O(1) to minimise lock hold time
				if (processing.empty()) {
					std::swap(m_job, processing);
				}
			}

			while (!processing.empty()) {
				job_type job{ std::move(processing.front()) };

				try {
					job();
				}
				catch (const std::exception& e) {
					emplace_error(
						FormattedRecord{ "Failed to execute job." }, e
					);
				}
				catch (...) {
					emplace_error(
						FormattedRecord{ "Failed to execute job." }
					);
				}

				processing.pop();
			}
		}
	}


	/// -- lifecycle ----------------------------------------------------

	// destructor
	AsyncWorker::~AsyncWorker() noexcept
	{
		stop();
	}

	// constructor
	AsyncWorker::AsyncWorker(bool start) noexcept :
		m_mutex(),
		m_syncMutex(),
		m_queueCv(),
		m_syncCv(),
		m_job(),
		m_sync(),
		m_done(!start),
		m_thread(start ? std::thread(&AsyncWorker::loop, this) : std::thread())
	{}


}  // namespace cst



/// AsyncWorkerMixin
namespace cst
{

	// =====================================================================
	//  AsyncWorkerMixin - mixin thread with job queue
	// =====================================================================
	class AsyncWorkerMixin
	{
		// -- type aliases -------------------------------------------------
	private:
		using job_type = typename AsyncWorker::job_type;

		// -- protected members --------------------------------------------
	protected:
		std::unique_ptr<AsyncWorker> m_worker;

		// -- protected methods --------------------------------------------
	protected:
		bool start_worker()                                    noexcept;
		bool enqueue_job(job_type)                             noexcept;
		void stop_worker()                                     noexcept;
		bool sync_worker()                                     noexcept;

		// -- lifecycle ----------------------------------------------------
	protected:
		~AsyncWorkerMixin()                                   = default;
		AsyncWorkerMixin(bool = false)                         noexcept;
		AsyncWorkerMixin(const AsyncWorkerMixin&)              = delete;
		AsyncWorkerMixin& operator=(const AsyncWorkerMixin&)   = delete;
		AsyncWorkerMixin(AsyncWorkerMixin&&)                  = default;
		AsyncWorkerMixin& operator=(AsyncWorkerMixin&&)       = default;

	};  // class AsyncWorkerMixin




	/// -- protected methods --------------------------------------------

	bool AsyncWorkerMixin::start_worker() noexcept
	{
		return m_worker->start();
	}

	bool AsyncWorkerMixin::enqueue_job(job_type job) noexcept
	{
		return m_worker->enqueue(std::move(job));
	}

	void AsyncWorkerMixin::stop_worker() noexcept
	{
		m_worker->stop();
	}

	bool AsyncWorkerMixin::sync_worker() noexcept
	{
		return m_worker->sync();
	}


	/// -- lifecycle ----------------------------------------------------

	AsyncWorkerMixin::AsyncWorkerMixin(bool start) noexcept :
		m_worker(std::make_unique<AsyncWorker>(start))
	{}


}  // namespace cst



