#ifndef _FUNCTION_SCHEDULER_H_
#define _FUNCTION_SCHEDULER_H_

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "logging.h"

class FunctionScheduler {
   public:
    FunctionScheduler();

    ~FunctionScheduler();

    void addFunction(std::function<void()>&& cb, std::chrono::microseconds interval, const std::string& nameID,
        std::chrono::microseconds startDelay = std::chrono::microseconds(0), bool runOnce = false);

    bool cancelFunction(const std::string& nameID);
    // void cancelAllFunctions();

    // bool resetFunctionTimer(const string& nameId);
    void run();
    bool start();
    bool tearDown();

    using IntervalDistributionFunc = std::function<std::chrono::microseconds()>;
    using NextRunTimeFunc = std::function<std::chrono::steady_clock::time_point(
        std::chrono::steady_clock::time_point, std::chrono::steady_clock::time_point)>;

   private:
    struct RepeatFunc {
        std::function<void()> cb;
        NextRunTimeFunc nextRunTimeFunc;
        std::chrono::steady_clock::time_point nextRunTime;
        std::string name;
        std::string intervalDescr;
        std::chrono::microseconds startDelay;
        bool runOnce;

        RepeatFunc(std::function<void()>&& callback, IntervalDistributionFunc&& nextRunTimeFn,
            const std::string& nameID, const std::string& interval2String, std::chrono::microseconds delay, bool once)
            : cb(std::move(callback)),
              nextRunTime(),
              name(nameID),
              intervalDescr(interval2String),
              startDelay(delay),
              runOnce(once) {
            nextRunTimeFunc = std::move(getNextRunTimeFunc(move(nextRunTimeFn)));
        }

        static NextRunTimeFunc getNextRunTimeFunc(IntervalDistributionFunc&& intervalFn) {
            return [intervalFn = std::move(intervalFn)](std::chrono::steady_clock::time_point /* curNextRunTime */,
                       std::chrono::steady_clock::time_point curTime) mutable { return curTime + intervalFn(); };
        }

        void resetNextRunTime(std::chrono::steady_clock::time_point currTime) {
            nextRunTime = currTime + startDelay;
        }

        std::chrono::steady_clock::time_point getNextRunTime() const {
            return nextRunTime;
        }

        void setNextRunTime(std::chrono::steady_clock::time_point currTime) {
            // set nextRunTime based on the current time where we started the function call.
            nextRunTime = nextRunTimeFunc(nextRunTime, currTime);
#if 0
            {
                // Convert the time_point to a time_point with milliseconds precision
                auto nextRunTimeMs = std::chrono::time_point_cast<std::chrono::milliseconds>(nextRunTime);

                // Get the time in milliseconds since the epoch
                auto timeSinceEpoch = nextRunTimeMs.time_since_epoch();

                // Convert the time since epoch to a long long data type
                auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch).count();

                // Print the nextRunTime in milliseconds
                LOG_I("RepeatFunc::%s(): Next run time in ms %ld", __func__, timeMs);
            }
#endif
        }

        void cancel() {
            // Simply reset cb to an empty function.
            cb = {};
        }
        bool isValid() const {
            return static_cast<bool>(cb);
        }
    };

    static bool compareRepeatFunc(const std::unique_ptr<RepeatFunc>& f1, const std::unique_ptr<RepeatFunc>& f2) {
        return f1->getNextRunTime() > f2->getNextRunTime();
    }
    using FunctionHeap = std::vector<std::unique_ptr<RepeatFunc>>;
    using FunctionMap = std::map<std::string, RepeatFunc*>;

    void addFunctionInternal(std::function<void()>&& cb, IntervalDistributionFunc&& fn, const std::string& nameID,
        const std::string& intervalDescr, std::chrono::microseconds startDelay, bool runOnce);

    void addFunctionToHeapChecked(std::function<void()>&& cb, IntervalDistributionFunc&& fn, const std::string& nameID,
        const std::string& intervalDescr, std::chrono::microseconds startDelay, bool runOnce);

    void addFunctionToHeap(
        /*const unique_lock<std::mutex>& lock,*/  // TODO
        std::unique_ptr<RepeatFunc> func);

    void runOneFunction(std::unique_lock<std::mutex>& lock, std::chrono::steady_clock::time_point now);

    std::thread _thread;
    std::mutex _mutex;
    bool _running{false};
    std::condition_variable _runningCondvar;

    // the functions will be run
    // heap, order by run time
    FunctionHeap _functions;
    FunctionMap _functionMap;

    RepeatFunc* _currentFunction{nullptr};
};

#endif  //_FUNCTION_SCHEDULER_H_