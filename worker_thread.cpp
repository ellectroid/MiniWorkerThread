#include "worker_thread.h"

WorkerThread::WorkerThread()
    : m_thread(),
      m_mutex(),
      m_cond(),
      m_signal(SIGNAL_NONE),
      m_workFunc(nullptr),
      m_flags(0)
{
    for (void*& ptr : m_work_arg_ptr) ptr = nullptr;
    for (long long& i : m_work_arg_int) i = 0;
}

WorkerThread::~WorkerThread() {
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

void WorkerThread::start() {
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

void WorkerThread::stop_detach() {
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

void WorkerThread::stop_join() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_flags.fetch_and(~FLAG_DETACH_ON_TERMINATE);
        m_signal = SIGNAL_KILL;
    }
    m_cond.notify_one();
}

void WorkerThread::requestWork() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_flags.fetch_or(FLAG_WORK_PENDING);
    }
    m_cond.notify_one();
}

void WorkerThread::setWorkFunction(work_handlerfunc_t func) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_workFunc = func;
}

void WorkerThread::enableWorkRepeat() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_flags.fetch_or(FLAG_WORK_REPEAT);
}

void WorkerThread::disableWorkRepeat() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_flags.fetch_and(~FLAG_WORK_REPEAT);
}

void WorkerThread::setWorkPointerArg(int i, void* ptr) {
    if (i >= 0 && i < 2) m_work_arg_ptr[i] = ptr;
}

void* WorkerThread::getWorkPointerArg(int i) const {
    return (i >= 0 && i < 2) ? m_work_arg_ptr[i] : nullptr;
}

void WorkerThread::setWorkUIntArg(int i, unsigned long long val) {
    if (i >= 0 && i < 2) m_work_arg_uint[i] = val;
}

unsigned long long WorkerThread::getWorkUIntArg(int i) const {
    return (i >= 0 && i < 2) ? m_work_arg_uint[i] : 0;
}

void WorkerThread::setWorkIntArg(int i, long long val) {
    if (i >= 0 && i < 2) m_work_arg_int[i] = val;
}

long long WorkerThread::getWorkIntArg(int i) const {
    return (i >= 0 && i < 2) ? m_work_arg_int[i] : 0;
}

void WorkerThread::setDetachOnTerminate(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (enable)
        m_flags.fetch_or(FLAG_DETACH_ON_TERMINATE);
    else
        m_flags.fetch_and(~FLAG_DETACH_ON_TERMINATE);
}

void WorkerThread::sendSignal(int signal) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_signal = signal;
    }
    m_cond.notify_one();
}

int WorkerThread::getSignal() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_signal;
}

unsigned int WorkerThread::getFlags() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_flags.load();
}

bool WorkerThread::isIdle() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_flags.load() & FLAG_IDLE) != 0;
}

bool WorkerThread::isThreadActive() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_flags.load() & FLAG_THREAD_ACTIVE) != 0;
}

bool WorkerThread::isBusy() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_flags.load() & FLAG_BUSY) != 0;
}

void WorkerThread::reset() {
    stop_detach();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signal = SIGNAL_NONE;
    m_workFunc = nullptr;
    m_flags.store(0);
    for (void*& ptr : m_work_arg_ptr) ptr = nullptr;
    for (long long& i : m_work_arg_int) i = 0;
}

void WorkerThread::handleSignal() {
    if (m_signal == SIGNAL_KILL) {
        m_flags.fetch_or(FLAG_TERMINATE_PENDING);
    }
    m_signal = SIGNAL_NONE;
}

void WorkerThread::threadLoop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_flags.fetch_or(FLAG_THREAD_ACTIVE);
    }

    while (true) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_flags.fetch_or(FLAG_IDLE);

            m_cond.wait(lock, [this] {
                return (m_flags.load() & FLAG_WORK_PENDING) || (m_signal != SIGNAL_NONE);
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
