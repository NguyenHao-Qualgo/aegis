
#include "FunctionScheduler.h"

#include <iostream>

using namespace std;
namespace {
struct ConstIntervalFunctor {
    const chrono::microseconds constInterval;

    explicit ConstIntervalFunctor(chrono::microseconds interval) : constInterval(interval) {
        if (interval < chrono::microseconds::zero()) {
            LOG_E("FunctionScheduler:ConstIntervalFunctor(): time interval must be non-negative");
        }
    }

    chrono::microseconds operator()() const {
        return constInterval;
    }
};

string microsecondsToString(chrono::microseconds duration) {
    auto count = duration.count();
    string unit;
    /*
    if (count >= 1000'000) {  // Convert to milliseconds if the duration is equal to or greater than 1 second
        count /= 1000'000;
        unit = "s";
    } else if (count >= 1000) {  // Convert to microseconds if the duration is equal to or greater than 1 millisecond
        count /= 1000;
        unit = "ms";
    } else {  // Duration is in microseconds
        unit = "us";
    }
    */
    unit = "us";
    return to_string(count) + unit;
}

}  // namespace
FunctionScheduler::FunctionScheduler() = default;
FunctionScheduler::~FunctionScheduler() {
    if (tearDown()) {
        _thread.join();
    }
}

void FunctionScheduler::addFunction(function<void()>&& cb, chrono::microseconds interval, const string& nameID,
    chrono::microseconds startDelay, bool runOnce) {
    addFunctionInternal(
        move(cb), ConstIntervalFunctor(interval), nameID, microsecondsToString(interval), startDelay, runOnce);
}

void FunctionScheduler::addFunctionInternal(function<void()>&& cb, IntervalDistributionFunc&& fn, const string& nameID,
    const string& intervalDescr, chrono::microseconds startDelay, bool runOnce) {
    addFunctionToHeapChecked(move(cb), move(fn), nameID, intervalDescr, startDelay, runOnce);
}

void FunctionScheduler::addFunctionToHeapChecked(function<void()>&& cb, IntervalDistributionFunc&& fn,
    const string& nameID, const string& intervalDescr, chrono::microseconds startDelay, bool runOnce) {
    if (!cb) {
        LOG_E("Scheduled function must be set");
        return;
    }
    if (!fn) {
        LOG_E("Interval distribution must be set");
        return;
    }
    if (startDelay < chrono::microseconds::zero()) {
        LOG_E("Start delay must be positive");
        return;
    }

    auto it = _functionMap.find(nameID);
    if (it != _functionMap.end() && it->second->isValid()) {
        // exists
        LOG_E("function named {} already exists", nameID.c_str());
        return;
    }

    // TODO: Add lock in case add function in runtime
    this_thread::sleep_for(chrono::seconds(2));
    addFunctionToHeap(make_unique<RepeatFunc>(move(cb), move(fn), nameID, intervalDescr, startDelay, runOnce));
}

void FunctionScheduler::addFunctionToHeap(
    /*const unique_lock<mutex>& lock,*/  // TODO
    unique_ptr<RepeatFunc> func) {

    _functions.push_back(move(func));
    _functionMap[_functions.back()->name] = _functions.back().get();

    if (_running) {
        _functions.back()->resetNextRunTime(chrono::steady_clock::now());
        push_heap(_functions.begin(), _functions.end(), compareRepeatFunc);
        _runningCondvar.notify_one();
    }
}

bool FunctionScheduler::start() {
    if (_running) {
        return false;
    }

    LOG_D("start with {} functions", _functions.size());
    auto now = chrono::steady_clock::now();

    // reset the next run time for all functions
    for (const auto& f : _functions) {
        f->resetNextRunTime(now);
        LOG_I("funcion named: {}, period: {}, delay: {} us", f->name.c_str(), f->intervalDescr.c_str(),
            f->startDelay.count());
    }

    make_heap(_functions.begin(), _functions.end(), compareRepeatFunc);
    _thread = thread([&] { this->run(); });
    _running = true;
    return true;
}

void FunctionScheduler::run() {
    unique_lock<mutex> lock(_mutex);
    pthread_setname_np(_thread.native_handle(), "FuncSched");

    while (_running) {
        // If we have nothing to run, wait until a function is added (TODO) or until we
        // are stopped.
        if (_functions.empty()) {
            _runningCondvar.wait(lock, [this] { return !_functions.empty(); });
            continue;
        }

        auto now = chrono::steady_clock::now();
        // Check to see if the function was cancelled.
        // If so, just remove it and continue around the loop.
        const auto& top = _functions.front();
        if (!top->isValid()) {
            pop_heap(_functions.begin(), _functions.end(), compareRepeatFunc);
            _functions.pop_back();
            continue;
        }

        auto sleepTime = top->getNextRunTime() - now;
        if (sleepTime <= chrono::steady_clock::duration::zero()) {
            // run callback function
            runOneFunction(lock, now);
            // TODO notify for cancel function
            _runningCondvar.notify_all();
        } else {
            _runningCondvar.wait_for(lock, sleepTime);
        }
    }
}

void FunctionScheduler::runOneFunction(unique_lock<mutex>& lock, chrono::steady_clock::time_point now) {
    pop_heap(_functions.begin(), _functions.end(), compareRepeatFunc);
    auto func = move(_functions.back());
    _functions.pop_back();

    if (!func->cb) {
        LOG_W("FunctionScheduler {} function has been cancelled", func->name.c_str());
        return;
    }
    _currentFunction = func.get();
    // update the next run time
    func->setNextRunTime(now);

    // release the lock while invoke user's function
    lock.unlock();

    // invoke the function
    try {
        LOG_D("{} now is running", func->name.c_str());
        func->cb();
    } catch (const exception& e) {
        LOG_E("Erorr running scheduled function {}", func->name.c_str());
    }

    // acquire the lock
    lock.lock();

    // TODO add cancel
    // check the function was cancelled while we're running it
    // shouldn't reschedule it
    if (!_currentFunction) {
        return;
    }
    if (_currentFunction->runOnce) {
        _functionMap.erase(_currentFunction->name);
        _currentFunction = nullptr;
        return;
    }

    // re-insert the function to heap
    LOG_D("Re-insert {}", func->name.c_str());
    _functions.push_back(move(func));
    _currentFunction = nullptr;
    push_heap(_functions.begin(), _functions.end(), compareRepeatFunc);
}

bool FunctionScheduler::tearDown() {
    scoped_lock<mutex> g(_mutex);
    if (!_running) {
        return false;
    }
    _running = false;
    _runningCondvar.notify_one();
    return true;
}

bool FunctionScheduler::cancelFunction(const string& nameID) {
    lock_guard<mutex> g(_mutex);
    auto it = _functionMap.find(nameID);
    if (it == _functionMap.end()) {
        LOG_D("function named {} not found", nameID.c_str());
        return false;
    }
    it->second->cancel();
    _functionMap.erase(it);
    return true;
}