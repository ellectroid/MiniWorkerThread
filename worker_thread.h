#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

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

    WorkerThread();
    ~WorkerThread();

    void start();
    void stop_detach();
    void stop_join();
    void requestWork();
    void setWorkFunction(work_handlerfunc_t func);
    void enableWorkRepeat();
    void disableWorkRepeat();

    void setWorkPointerArg(int i, void* ptr);
    void* getWorkPointerArg(int i) const;

    void setWorkUIntArg(int i, unsigned long long val);
    unsigned long long getWorkUIntArg(int i) const;

    void setWorkIntArg(int i, long long val);
    long long getWorkIntArg(int i) const;

    void setDetachOnTerminate(bool enable);
    void sendSignal(int signal);
    int getSignal();

    unsigned int getFlags();
    bool isIdle();
    bool isThreadActive();
    bool isBusy();

    void reset();

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

    void handleSignal();
    void threadLoop();
};

#endif // WORKERTHREAD_H
