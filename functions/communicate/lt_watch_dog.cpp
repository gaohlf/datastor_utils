#include "lt_watch_dog.h"

#define DEFALUT_TIMER_SECS 5

lt_watch_dog::lt_watch_dog(boost::asio::io_service *_io_service) : timer(*_io_service), seconds(DEFALUT_TIMER_SECS)
{
    on_monitor = false;
    if_handled = false;
}

void lt_watch_dog::start_monitor()
{
    std::unique_lock<std::mutex> lck(m);
    is_monitoring = true;
    start_timer();
}

void lt_watch_dog::stop_monitor()
{
    std::unique_lock<std::mutex> lock(m);
    is_monitoring = false;
    timer.cancel();
    if(!if_handled)
    {
        if_handled = true;
        handle_event();
    }
    lock.unlock();
}

void lt_watch_dog::timer_handler(const boost::system::error_code &error)
{
    if ( error == boost::asio::error::operation_aborted)
    {
        std::unique_lock<std::mutex> lock(pending_m);
        on_monitor = false;
        pending_cond.notify_one();
        return;
    }

    bool to_feed = is_to_feed();
    std::unique_lock<std::mutex> lck(m);
    if ( to_feed )
    {
        start_timer();
    }
    else
    {
        std::unique_lock<std::mutex> lock(pending_m);
        on_monitor = false;
        pending_cond.notify_one();
        lock.unlock();

        is_monitoring = false;
        if(!if_handled)
        {
            if_handled = true;
            handle_event();
        }
        lck.unlock();
    }
}

void lt_watch_dog::set_seconds(int sec)
{
    seconds = sec;
}

void lt_watch_dog::start_timer()
{
    std::unique_lock<std::mutex> lock(pending_m);
    on_monitor = true;
    lock.unlock();
    timer.expires_from_now(boost::posix_time::seconds(seconds));
    timer.async_wait(boost::bind(&lt_watch_dog::timer_handler, this, boost::asio::placeholders::error));
}

lt_watch_dog::~lt_watch_dog()
{
    std::unique_lock<std::mutex> lock(pending_m);
    while ( on_monitor )
    {
        pending_cond.wait(lock);
    }
}
