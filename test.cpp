#include <iostream>
#include <chrono>
#include <thread>
#include "worker_thread.h"

void printFlags(const char* tag, unsigned int flags) {
    std::cout << "[" << tag << "] Flags: ";
    if (flags & WorkerThread::FLAG_THREAD_ACTIVE)       std::cout << "ACTIVE ";
    if (flags & WorkerThread::FLAG_TERMINATE_PENDING)   std::cout << "TERM_PENDING ";
    if (flags & WorkerThread::FLAG_DETACH_ON_TERMINATE) std::cout << "DETACH ";
    if (flags & WorkerThread::FLAG_IDLE)                std::cout << "IDLE ";
    if (flags & WorkerThread::FLAG_BUSY)                std::cout << "BUSY ";
    if (flags & WorkerThread::FLAG_WORK_PENDING)        std::cout << "WORK_PENDING ";
    if (flags == 0) std::cout << "(none)";
    std::cout << "\n";
}

int test_worker_function(WorkerThread* self) {
    std::cout << "[Worker] Started work\n";
    printFlags("Worker", self->getFlags());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "[Worker] Work completed\n";
    printFlags("Worker", self->getFlags());
    return 0;
}

int main() {
    WorkerThread worker;

    std::cout << "[Main] Setting work handler\n";
    worker.setWorkFunction(test_worker_function);

    std::cout << "[Main] Starting worker thread\n";
    worker.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "[Main] Flags after start:\n";
    printFlags("Main", worker.getFlags());

    worker.enableWorkRepeat();

    std::cout << "[Main] Requesting work\n";
    worker.requestWork();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[Main] Flags during work:\n";
    printFlags("Main", worker.getFlags());

    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::cout << "[Main] Flags after work finished:\n";
    printFlags("Main", worker.getFlags());

    std::cout << "[Main] Sending kill signal\n";
    worker.sendSignal(WorkerThread::SIGNAL_KILL);
    while (worker.isThreadActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "[Main] Final flags before reusing object:\n";
    printFlags("Main", worker.getFlags());

    // =========================================================================
    std::cout << "\n[Main] === Starting Detached Mode Test with Reused Object ===\n\n";

    worker.setWorkFunction(test_worker_function);
    worker.setDetachOnTerminate(true);
    worker.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "[Main] Flags after restart (detached):\n";
    printFlags("Main", worker.getFlags());

    std::cout << "[Main] Requesting work\n";
    worker.requestWork();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    printFlags("Main", worker.getFlags());

    std::cout << "[Main] Sending kill signal\n";
    worker.sendSignal(WorkerThread::SIGNAL_KILL);
    while (worker.isThreadActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    printFlags("Main", worker.getFlags());
    std::cout << "[Main] Detached test complete. Worker exited cleanly again.\n";

    return 0;
}
