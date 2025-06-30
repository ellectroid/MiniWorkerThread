#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class WorkerThread {
public:
	typedef int (*work_handlerfunc_t)(WorkerThread*);

	enum Signal {
		SIGNAL_NONE = 0, SIGNAL_KILL = 1
	};

	enum FlagBits {
		FLAG_THREAD_ACTIVE = 1 << 0,
		FLAG_TERMINATE_PENDING = 1 << 1,
		FLAG_DETACH_ON_TERMINATE = 1 << 2,
		FLAG_IDLE = 1 << 3,
		FLAG_BUSY = 1 << 4,
		FLAG_WORK_PENDING = 1 << 5,
		FLAG_WORK_REPEAT = 1 << 6
	};

private:
	std::thread m_thread;
	std::mutex m_mutex;
	std::condition_variable m_cond;
	int m_signal;
	work_handlerfunc_t m_workFunc;
	std::atomic<unsigned int> m_flags;
	void* m_work_arg_ptr[2];
	union {
		unsigned long long m_work_arg_uint[2];
		long long m_work_arg_int[2];
	};

public:
	WorkerThread() :
			m_thread(),
			m_mutex(),
			m_cond(),
			m_signal(SIGNAL_NONE),
			m_workFunc(nullptr),
			m_flags(0) {
		for (size_t i = 0;
				i < sizeof(m_work_arg_ptr) / sizeof(m_work_arg_ptr[0]); ++i)
			m_work_arg_ptr[i] = nullptr;
		for (size_t i = 0;
				i < sizeof(m_work_arg_int) / sizeof(m_work_arg_int[0]); ++i)
			m_work_arg_int[i] = 0;
	}

	~WorkerThread() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_flags.fetch_or(FLAG_TERMINATE_PENDING);
			m_signal = SIGNAL_KILL;
		}
		m_cond.notify_one();
		if (m_thread.joinable()) {
			if (m_flags.load() & FLAG_DETACH_ON_TERMINATE)
				m_thread.detach();
			else
				m_thread.join();
		}
	}

	void start() {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_thread.joinable()) {
			if (m_flags.load() & FLAG_DETACH_ON_TERMINATE)
				m_thread.detach();
			else
				m_thread.join();
		}
		if (!(m_flags.load() & FLAG_THREAD_ACTIVE)) {
			m_thread = std::thread(&WorkerThread::threadLoop, this);
		}
	}

	void stop_detach() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_flags.fetch_or(FLAG_DETACH_ON_TERMINATE);
			m_signal = SIGNAL_KILL;
		}
		m_cond.notify_one();
		if ((m_flags.load() & FLAG_THREAD_ACTIVE) && m_thread.joinable()) {
			m_thread.detach();
		}
	}

	void stop_join() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_flags.fetch_and(~FLAG_DETACH_ON_TERMINATE);
			m_signal = SIGNAL_KILL;
		}
		m_cond.notify_one();
	}

	void requestWork() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_flags.fetch_or(FLAG_WORK_PENDING);
		}
		m_cond.notify_one();
	}

	void setWorkFunction(work_handlerfunc_t func) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_workFunc = func;
	}

	void enableWorkRepeat() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_flags.fetch_or(FLAG_WORK_REPEAT);
	}

	void disableWorkRepeat() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_flags.fetch_and(~FLAG_WORK_REPEAT);
	}

	void setWorkPointerArg(int i, void* ptr) {
		if (i >= 0
				&& i < static_cast<int>(sizeof(m_work_arg_ptr) / sizeof(m_work_arg_ptr[0])))
			m_work_arg_ptr[i] = ptr;
	}

	void* getWorkPointerArg(int i) const {
		return (i >= 0
				&& i < static_cast<int>(sizeof(m_work_arg_ptr) / sizeof(m_work_arg_ptr[0])))
				? m_work_arg_ptr[i] : nullptr;
	}

	void setWorkUIntArg(int i, unsigned long long val) {
		if (i >= 0
				&& i < static_cast<int>(sizeof(m_work_arg_uint) / sizeof(m_work_arg_uint[0])))
			m_work_arg_uint[i] = val;
	}

	unsigned long long getWorkUIntArg(int i) const {
		return (i >= 0
				&& i < static_cast<int>(sizeof(m_work_arg_uint) / sizeof(m_work_arg_uint[0])))
				? m_work_arg_uint[i] : 0;
	}

	void setWorkIntArg(int i, long long val) {
		if (i >= 0
				&& i < static_cast<int>(sizeof(m_work_arg_int) / sizeof(m_work_arg_int[0])))
			m_work_arg_int[i] = val;
	}

	long long getWorkIntArg(int i) const {
		return (i >= 0
				&& i < static_cast<int>(sizeof(m_work_arg_int) / sizeof(m_work_arg_int[0])))
				? m_work_arg_int[i] : 0;
	}

	void setDetachOnTerminate(bool enable) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (enable)
			m_flags.fetch_or(FLAG_DETACH_ON_TERMINATE);
		else
			m_flags.fetch_and(~FLAG_DETACH_ON_TERMINATE);
	}

	void sendSignal(int signal) {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_signal = signal;
		}
		m_cond.notify_one();
	}

	int getSignal() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_signal;
	}

	unsigned int getFlags() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_flags.load();
	}

	bool isIdle() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return (m_flags.load() & FLAG_IDLE) != 0;
	}

	bool isThreadActive() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return (m_flags.load() & FLAG_THREAD_ACTIVE) != 0;
	}

	bool isBusy() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return (m_flags.load() & FLAG_BUSY) != 0;
	}

	void reset() {
		stop_detach();
		std::lock_guard<std::mutex> lock(m_mutex);
		m_signal = SIGNAL_NONE;
		m_workFunc = nullptr;
		m_flags.store(0);
		for (size_t i = 0; i < sizeof(m_work_arg_ptr) / sizeof(m_work_arg_ptr[0]); ++i)
			m_work_arg_ptr[i] = nullptr;
		for (size_t i = 0; i < sizeof(m_work_arg_int) / sizeof(m_work_arg_int[0]); ++i)
			m_work_arg_int[i] = 0;
	}

private:
	void handleSignal() {
		if (m_signal == SIGNAL_KILL) {
			m_flags.fetch_or(FLAG_TERMINATE_PENDING);
		}
		m_signal = SIGNAL_NONE;
	}

	void threadLoop() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_flags.fetch_or(FLAG_THREAD_ACTIVE);
		}

		while (true) {
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_flags.fetch_or(FLAG_IDLE);

				m_cond.wait(lock,
						[this] {
							return (m_flags.load() & FLAG_WORK_PENDING)
									|| (m_signal != SIGNAL_NONE);
						});

				m_flags.fetch_and(~FLAG_IDLE);

				handleSignal();
				if (m_flags.load() & FLAG_TERMINATE_PENDING)
					break;

				if ((m_flags.load() & FLAG_WORK_PENDING) && m_workFunc) {
					if (!(m_flags.load() & FLAG_WORK_REPEAT))
						m_flags.fetch_and(~FLAG_WORK_PENDING);

					m_flags.fetch_or(FLAG_BUSY);
					lock.unlock();
					m_workFunc(this);
					lock.lock();
					m_flags.fetch_and(~FLAG_BUSY);
				}
			}
		}

		std::lock_guard<std::mutex> lock(m_mutex);
		m_flags.fetch_and(~FLAG_TERMINATE_PENDING);
		m_flags.fetch_and(~FLAG_WORK_REPEAT);
		m_flags.fetch_and(~FLAG_WORK_PENDING);
		m_flags.fetch_and(~FLAG_THREAD_ACTIVE);
	}
};
