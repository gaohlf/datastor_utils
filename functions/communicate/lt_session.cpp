#include "lt_session.h"
#include "../lt_function_error.h"
#include "../log/include/awe_log.h"

lt_session::lt_session(boost::asio::io_service *_io_service,
                       lt_session_callback *_cb) :
        lt_reference(),
        lt_session_dog(_io_service),
        max_wait_seconds(DEFAULT_WAIT_SECONDS),
        cb(_cb),
        io_service(_io_service),
        _socket(*_io_service),
        _connect(false, boost::bind(&lt_session::state_changed, this, _1))
{
    AWE_MODULE_INFO("comunicate", "struct lt_session %p", this);
//    std::cout << "session : this : " << __FUNCTION__ << this << std::endl;
}

void lt_session::rcv(lt_data_t *rcv_data)
{
    assert_legal();
    AWE_MODULE_DEBUG("comunicate", "lt_session::rcv %p data[%p]", this,
                      rcv_data);
    start_rcv(rcv_data);
    AWE_MODULE_DEBUG("comunicate", "lt_session::rcv %p data[%p]", this,
                     rcv_data);
}

void lt_session::snd(lt_data_t *data)
{
    AWE_MODULE_DEBUG("communicate", "enter lt_session::snd sess %p", this);
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "enter lt_session::snd sess %p data %p", this, data);
    if ( is_connected() )
    {
        AWE_MODULE_DEBUG("communicate", "start_snd_data lt_session::snd sess %p data %p", this, data);
        start_snd_data(data);
        AWE_MODULE_DEBUG("communicate", "start_snd_data lt_session::snd sess %p data %p", this, data);
    }
    else
    {
        AWE_MODULE_DEBUG("communicate", "not connected lt_session::snd sess %p data %p", this, data);
        clear();
        cb->snd_done(this, data, -RPC_ERROR_TYPE_CONNECT_FAIL);
    }
    AWE_MODULE_DEBUG("communicate", "leave lt_session::snd sess %p data %p", this, data);
}


void lt_session::start_rcv(lt_data_t *data)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "lt_session::start_rcv sess %p data [%p]",
                     this, data);

    rcv_queue.begin_to(
            boost::bind(&lt_session::start_rcv_head_unsafe, this, data),
            boost::bind(&lt_session::rcv_done, this, data,
                        boost::asio::error::network_down));
    AWE_MODULE_DEBUG("communicate", "lt_session::start_rcv sess %p data [%p]",
                     this, data);
  
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::start_rcv sess %p",
                     this);
    rcv_queue.begin_to(
            boost::bind(&lt_session::start_rcv_head_unsafe, this, data),
            boost::bind(&lt_session::rcv_done, this, data,
                        boost::asio::error::network_down));
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::start_rcv sess %p",
                     this);
}

void
lt_session::rcv_done(lt_data_t *data, const boost::system::error_code error)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::rcv_done sess %p data [%p]",
                     this, data);
    unsigned err = 0;
    if ( error )
    {
        AWE_MODULE_DEBUG("communicate", "lt_session::rcv_done sess %p data [%p]",
                         this, data);
        err = -RPC_ERROR_TYPE_RCV_FAILD;
        let_it_down();
    }
    else
    {
        mark_received();
    }
    AWE_MODULE_DEBUG("communicate", "lt_session::rcv_done sess %p data [%p]",
                     this, data);
    if ( err )
    {
        AWE_MODULE_DEBUG("communicate", "lt_session::rcv_done sess %p data [%p]",
                         this, data);
        clear();
    }
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::rcv_done sess %p data [%p]",
                     this, data);
    cb->rcv_done(this, data, err);
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::rcv_done sess %p data [%p]",
                     this, data);
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::rcv_done sess %p",
                     this);
    cb->rcv_done(this, data, RPC_ERROR_TYPE_OK);
    mark_received();
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::rcv_done sess %p",
                     this);
}

void lt_session::start_rcv_head_unsafe(lt_data_t *data)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate",
                     "--enter lt_session::start_rcv_head_unsafe sess %p", this);
    boost::asio::async_read(_socket,
                            boost::asio::buffer(&(data->_length), sizeof(data->_length)),
                            boost::bind(&lt_session::rcv_head_done_unsafe, this,
                                        data, boost::asio::placeholders::error));
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::start_rcv_head_unsafe sess %p", this);
}

void lt_session::rcv_head_done_unsafe(lt_data_t *data,
                                      const boost::system::error_code error)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate",
                     "--enter lt_session::rcv_head_done_unsafe sess %p data [%p]", this, data);
    int err = boost_err_translate(error);
    if ( err )
    {
        AWE_MODULE_ERROR("comunicate",
                         "lt_session::rcv_done %p err [%d]", this,
                         err);
        
        rcv_done(data, boost::asio::error::network_down);
        AWE_MODULE_DEBUG("communicate",
                         "--Err leave lt_session::start_rcv_head_unsafe sess %p",
                         this);
        AWE_MODULE_DEBUG("communicate", "rcv_head_done_unsafe sess %p data [%p]", this, data);
        return;
    }
    
    if ( data->_length == 0 || data->_length > (256 << 20) )
    {
        AWE_MODULE_ERROR("communicate", "too big len %ld sess %p localip[%s] remoteip[%s]",
                         data->_length, this,
                         this->_socket.local_endpoint().address().to_string()
                                 .c_str(),
                         this->_socket.remote_endpoint().address().to_string()
                                 .c_str());
        rcv_queue.continue_to();
        rcv_done(data, boost::asio::error::network_down);
        return;
    }
    
    data->realloc_buf();
    AWE_MODULE_DEBUG("communicate",
                     "--leave lt_session::rcv_head_done_unsafe sess %p", this);
    start_rcv_data_unsafe(data);
}

void lt_session::start_rcv_data_unsafe(lt_data_t *data)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "start_rcv_data_unsafe sess %p data [%p]", this, data);
    boost::asio::async_read(_socket,
                            boost::asio::buffer(data->get_buf(), data->_length),
                            boost::bind(&lt_session::rcv_data_done_unsafe, this,
                                        data,
                                        boost::asio::placeholders::error));
    AWE_MODULE_DEBUG("communicate", "start_rcv_data_unsafe sess %p data [%p]", this, data);
}

void lt_session::rcv_data_done_unsafe(lt_data_t *data,
                                      const boost::system::error_code error)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "rcv_data_done_unsafe sess %p data [%p]", this, data);
    boost::system::error_code err = error;
    
    if ( err )
    {
        AWE_MODULE_INFO("communicate", "rcv_done sess %p err [%d] data [%p]", this, err,data);
    }
    AWE_MODULE_DEBUG("communicate", "rcv_data_done_unsafe sess %p data [%p]", this, data);
    rcv_done(data, err);
    AWE_MODULE_DEBUG("communicate", "rcv_data_done_unsafe sess %p data [%p]", this, data);
    rcv_queue.continue_to();
    AWE_MODULE_DEBUG("communicate", "rcv_data_done_unsafe sess %p data [%p]", this, data);
}

void lt_session::let_it_up()
{
    assert_legal();
    _connect.set(true);
}

void lt_session::let_it_down()
{
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::let_it_down sess %p",
                     this);
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::let_it_down sess %p",
                     this);
    _connect.set(false);
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::let_it_down sess %p",
                     this);
}

void lt_session::connected()
{
    assert_legal();
    reset();
    start_monitor();
    cb->connected(this);
}

void lt_session::disconnected()
{
    assert_legal();
    AWE_MODULE_ERROR("communicate",
                     "--enter lt_session::disconnected sess [%p]",
                     this);
    stop_monitor();
    AWE_MODULE_ERROR("communicate",
                     "lt_session::disconnected sess [%p]",
                     this);
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::disconnected sess %p",
                     this);
    //FIXME:加入flag控制 使得在断开后不会出现新的 rcv
    _socket.close();
    AWE_MODULE_DEBUG("communicate", "after _socket.close(); sess %p", this);
    cb->disconnected(this);
    AWE_MODULE_DEBUG("communicate", "after cb->disconnected(this); sess %p",
                     this);
    queue.clear();
    AWE_MODULE_DEBUG("communicate", "after queue.clear(); sess %p", this);
    stop_monitor();
    AWE_MODULE_DEBUG("communicate", "--leave lt_session::disconnected sess %p",
                     this);
    AWE_MODULE_ERROR("communicate",
                     "lt_session::disconnected sess [%p]",
                     this);
    clear();
    AWE_MODULE_ERROR("communicate", "--leave lt_session::disconnected %p",this);
}

void lt_session::start_snd_data(lt_data_t *data)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "IN start_snd_data lt_session::start_snd_data sess %p data %p", this, data);
    
    queue.begin_to(
            boost::bind(&lt_session::start_snd_data_unsafe, this, data),
            boost::bind(&lt_session::snd_data_done, this, data,
                        boost::asio::error::network_down));
   
    AWE_MODULE_DEBUG("communicate", "OUT start_snd_data lt_session::start_snd_data sess %p data %p", this, data);
    queue.begin_to(boost::bind(&lt_session::start_snd_data_unsafe, this, data),
                   boost::bind(&lt_session::snd_data_done, this, data,
                               boost::asio::error::network_down));
}

void lt_session::snd_data_done(lt_data_t *data,
                               const boost::system::error_code &error)
{
    assert_legal();
    int err = 0;
    AWE_MODULE_DEBUG("communicate", "enter lt_session::snd_data_done sess %p data %p", this, data);
    if ( error )
    {
        AWE_MODULE_ERROR("communicate", "error lt_session::snd_data_done sess %p data %p", this, data);
        let_it_down();
        err = -RPC_ERROR_TYPE_SND_FAILD;
    }
    else
    {
        mark_sent();
    }
    
    if ( err )
    {
        AWE_MODULE_ERROR("comunicate",
                         "lt_session::rcv_done %p err [%d]", this,
                         err);
        clear();
    }
    AWE_MODULE_DEBUG("communicate", "lt_session::snd_data_done sess %p data %p", this, data);
    
    cb->snd_done(this, data, err);
    AWE_MODULE_DEBUG("communicate", "leave lt_session::snd_data_done sess %p data %p", this, data);
}

void lt_session::start_snd_data_unsafe(lt_data_t *data)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate", "enter lt_session::start_snd_data_unsafe sess %p data %p", this, data);
    boost::asio::async_write(_socket,
                             boost::asio::buffer(data->get_data(),
                                                 data->data_len()),
                             boost::bind(&lt_session::snd_data_done_unsafe,
                                         this,
                                         data,
                                         boost::asio::placeholders::error));
                            
    AWE_MODULE_DEBUG("communicate", "leave lt_session::start_snd_data_unsafe sess %p data %p", this, data);
}

void lt_session::snd_data_done_unsafe(lt_data_t *data,
                                      const boost::system::error_code &error)
{
    assert_legal();
    AWE_MODULE_DEBUG("communicate",
                     "--enter lt_session::snd_data_done_unsafe sess %p data %p", this, data);
    boost::system::error_code err = error;
    
    AWE_MODULE_DEBUG("communicate",
                     "lt_session::snd_data_done_unsafe sess %p data %p", this, data);
    
    snd_data_done(data, err);
    
    AWE_MODULE_DEBUG("communicate",
                     "lt_session::snd_data_done_unsafe sess %p data %p", this, data);
    AWE_MODULE_DEBUG("communicate",
                     "--enter lt_session::snd_data_done_unsafe sess %p", this);
    queue.continue_to();  //FIXME 不立即调done，有可能引发超时
    snd_data_done(data, error);
    AWE_MODULE_DEBUG("communicate",
                     "--leave lt_session::snd_data_done_unsafe sess %p", this);
    
    AWE_MODULE_DEBUG("communicate",
                     "--leave lt_session::snd_data_done_unsafe sess %p data %p", this, data);
}

void lt_session::state_changed(const bool &is_con)
{
    AWE_MODULE_DEBUG("communicate", "--enter lt_session::state_changed sess %p",
                     this);
    assert_legal();
    AWE_MODULE_ERROR("communicate", "--enter lt_session::state_changed sess %p",
                     this);
    if ( !is_con )
    {
        AWE_MODULE_ERROR("communicate", "--enter disconn sess %p",this);
        disconnected();
    }
    else
    {
        AWE_MODULE_ERROR("communicate", "--enter disconn sess %p",this);
        connected();
    }
}

bool lt_session::is_connected() const
{
    return _connect.get();
}

void lt_session::set_max_wait_seconds(int sec)
{
    assert_legal();
    max_wait_seconds = sec;
}

bool lt_session::is_to_feed() const
{
    unsigned long since_last_snd = get_time_frame_since_last_snd();
    unsigned long since_last_rcv = get_time_frame_since_last_rcv();
    return std::min(since_last_snd, since_last_rcv) < (unsigned long)max_wait_seconds;
}

void lt_session::handle_event()
{
    assert_legal();
    let_it_down();
}

lt_session::~lt_session()
{
    AWE_MODULE_INFO("comunicate", "~lt_session %p", this);
//    std::cout << "session : this : " << __FUNCTION__ << this << std::endl;
    clear();
    _socket.close();
}

void lt_session::from_json_obj(const json_obj &obj)
{

}

json_obj lt_session::to_json_obj() const
{
    return json_obj();
}

void lt_session::assert_queue_empy()
{
    assert(queue.is_empty());
    assert(rcv_queue.is_empty());
}

void lt_session::clear()
{
    queue.clear();
    rcv_queue.clear();
}

void *lt_session_description_imp::get_session_private() const
{
    return description_internal_pri;
}

void lt_session_description_imp::set_session_private(void *_pri)
{
    description_internal_pri = _pri;
}

