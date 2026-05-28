#include <common.h>
// #include <timer/timer.h>

Timer::Timer(Time timeout, bool do_start)
    : _start_time()
    , _timeout(timeout)
    , _active(false)
    , _expired(false)
{
    if (do_start)
        start();
}

Timer::Time Timer::elapsed() const
{
    if (!_active)
        return 0;

    return now() - _start_time;
}

Timer::Time Timer::remain() const
{
    return timeout() - elapsed();
}

void Timer::start()
{
    _start_time = now();
    _expired = false;
    _active = true;
}

void Timer::update()
{
    if (INFINITE != _timeout && elapsed() > _timeout)
        expire();
}
