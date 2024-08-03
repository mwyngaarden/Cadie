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
#include <cassert>
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
    
    static std::string to_string(i64 n, std::vector<std::string> v)
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

#if PROFILE >= PROFILE_SOME
        if (status_ == Status::Started) return;
#endif
        
        assert(status_ != Status::Started);

        tp1_ = now_tp();
        cycles1_ = now_cycles<uint64_t>();

        status_ = Status::Started;
    }

    bool stop(bool b = true)
    {
        if (!b || status_ != Status::Started)
            return false;

        assert(status_ == Status::Started);

        tp2_ = now_tp();
        assert(tp2_ >= tp1_);

        cycles2_ = now_cycles<uint64_t>();
        assert(cycles2_ >= cycles1_);

        status_ = Status::Stopped;

        return true;
    }

    template <class T = Milli>
    i64 elapsed_time(i64 min = 0) const
    {
        assert(status_ != None);

        Point p = status_ == Stopped ? tp2_ : now_tp();
        assert(p >= tp1_);

        return std::max(std::chrono::duration_cast<T>(p - tp1_).count(), min);
    }
    
    template <class T = Nano>
    T dur() const
    {
        assert(status_ != None);

        Point p = status_ == Stopped ? tp2_ : now_tp();
        assert(p >= tp1_);

        return std::chrono::duration_cast<T>(p - tp1_);
    }
   
    template <class R = int64_t>
    R elapsed_cycles() const
    {
        assert(status_ != Status::None);

        uint64_t cycles = status_ == Status::Stopped ? cycles2_ : now_cycles<uint64_t>();
        assert(cycles >= cycles1_);

        return cycles - cycles1_;
    }

    void accrue(bool b = true)
    {
        if (!b) return;

        assert(status_ != Status::None);

        ttime_   += dur();
        tcycles_ += elapsed_cycles<uint64_t>();
    }

    template <class T = Nano>
    T accrued_time() const
    {
        assert(status_ != None);
        return std::chrono::duration_cast<T>(ttime_);
    }
    
    template <class R = int64_t>
    R accrued_cycles() const
    {
        assert(status_ != None);
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
