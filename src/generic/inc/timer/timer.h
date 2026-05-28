#ifndef TIMER_TIMER_H_
#define TIMER_TIMER_H_

class Timer
{
public:
    typedef uint32_t Time;

    static constexpr const Time INFINITE = ~0;

    constexpr Timer();
    Timer(Time timeout, bool do_start = true);

    bool is_expired() { update(); return _expired; }
    bool is_active() const { return _active; }

    bool has_fired() { return is_active() && is_expired(); }

    void start();

    void stop() { _active = false; }
    void expire() { _active = false; _expired = true; }

    void start(Time timeout) { set_timeout(timeout); start(); }
    void set_timeout(Time timeout) { _timeout = timeout; }
    void restart() { start(timeout()); }

    Time start_time() const { return _start_time; }
    Time timeout() const { return _timeout; }
    Time elapsed() const;
    Time remain() const;

    static Time now();
    
    static Time elapsed(Time since) { return Timer::now() - since; }
    static void delay(Time ms, uint us) { delay_us(1000u * ms + us); }

    static void delay(Time ms) { delay(ms, 0u); }
    static void delay_us(uint us);

protected:
    void update();

private:
    Time _start_time;
    Time _timeout;
    bool _active;
    bool _expired;
};

inline
constexpr Timer::Timer()
    : _start_time()
    , _timeout()
    , _active(false)
    , _expired(false)
{}

#endif /* TIMER_TIMER_H_ */