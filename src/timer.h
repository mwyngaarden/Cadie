#ifndef TIMER_H
#define TIMER_H

#ifndef __GNUG__
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <chrono>
#include <locale>
#include <sstream>
#include <stack>
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

class Timer {
public:
    using Clock     = std::chrono::steady_clock;
    using Point     = std::chrono::steady_clock::time_point;
    using Seconds   = std::chrono::seconds;
    using Milli     = std::chrono::milliseconds;
    using Micro     = std::chrono::microseconds;
    using Nano      = std::chrono::nanoseconds;

    enum Status { None, Started, Stopped };

    Timer(bool b = false)
    {
        start(b);
    }
    
    static std::string to_string(int64_t n, std::vector<std::string> v)
    {
        std::ostringstream oss;

        for (auto s : v) {
            oss = std::ostringstream();
            oss.imbue(std::locale(""));
            oss << n << ' ' << s;
            if ((n /= 1000) < 10) break;
        }

        return oss.str();
    }

    template <class R = int64_t, class T = Milli>
    static R now()
    {
        return std::chrono::duration_cast<T>(Clock::now().time_since_epoch()).count();
    }

    template <class R = int64_t>
    static R now_cycles()
    {
        uint32_t i;
        uint64_t c = __rdtscp(&i);

        return c;
    }
    
    static Point now_tp() { return Clock::now(); }

    void start(bool b = true)
    {
        if (!b) return;

        tp1_ = now_tp();
        cycles1_ = now_cycles<uint64_t>();

        status_ = Status::Started;
    }

    bool stop(bool b = true)
    {
        if (!b || status_ != Status::Started)
            return false;

        tp2_ = now_tp();

        cycles2_ = now_cycles<uint64_t>();

        status_ = Status::Stopped;

        return true;
    }

    template <class T = Milli>
    int64_t elapsed_time(int64_t min = 0) const
    {
        Point p = status_ == Stopped ? tp2_ : now_tp();

        return std::max(std::chrono::duration_cast<T>(p - tp1_).count(), min);
    }
    
    template <class T = Nano>
    T dur() const
    {
        Point p = status_ == Stopped ? tp2_ : now_tp();

        return std::chrono::duration_cast<T>(p - tp1_);
    }
   
    template <class R = int64_t>
    R elapsed_cycles() const
    {
        uint64_t cycles = status_ == Status::Stopped ? cycles2_ : now_cycles<uint64_t>();

        return cycles - cycles1_;
    }

    void accrue(bool b = true)
    {
        if (!b) return;

        ttime_   += dur();
        tcycles_ += elapsed_cycles<uint64_t>();
    }

    template <class T = Nano>
    T accrued_time() const
    {
        return std::chrono::duration_cast<T>(ttime_);
    }
    
    template <class R = int64_t>
    R accrued_cycles() const
    {
        return tcycles_;
    }

private:
    Status status_ = None;

    Point tp1_;
    Point tp2_;
    Nano ttime_{};
    
    uint64_t cycles1_;
    uint64_t cycles2_;
    uint64_t tcycles_{};
};

#endif
