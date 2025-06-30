# MiniWorkerThread
A minimal, single-class worker thread for C++—flag-controlled, fast, and heap-free.

MiniWorkerThread is a minimal, single-class worker thread for C++. 
Features: C++11, no heap, thread-safe, sleeping idle thread, worker reusable for another task, very simple, debuggable and modifyable.

Usage:
```
WorkerThread worker;
worker.setWorkFunction(myFunction);
worker.setWorkPointerArg(0, &myData);
worker.setWorkIntArg(0, 42);
worker.enableWorkRepeat();
worker.start();
worker.requestWork();
// ...
worker.stop_join();
```

Public API overview:   
   
Control:   
- start
- stop_join
- stop_detach
- requestWork
- sendSignal
- setDetachOnTerminate
- reset
   
Work configuration:   
- setWorkFunction
- enableWorkRepeat
- disableWorkRepeat
    
Embedded argument storage:   
- setWorkPointerArg
- getWorkPointerArg
- setWorkIntArg
- getWorkIntArg
- setWorkUIntArg
- getWorkUIntArg
    
State and diagnostics:   
- isThreadActive
- isIdle
- isBusy
- getSignal
- getFlags
     
Supported signals:   
- SIGNAL_NONE (0): no signal
- SIGNAL_KILL (1): gracefully terminates the thread
    
Internal flags:   
- FLAG_THREAD_ACTIVE: thread is running
- FLAG_TERMINATE_PENDING: shutdown has been requested (kill signal sent)
- FLAG_DETACH_ON_TERMINATE: thread will detach instead of join if kill signal is sent
- FLAG_IDLE: thread is blocked (sleeping)
- FLAG_BUSY: task is executing (work)
- FLAG_WORK_PENDING: work execution is requested, not executing yet
- FLAG_WORK_REPEAT: repeat mode is active (run task repeatedly)
