#ifndef LT_FUNCTION_LT_FUNCTION_ERROR_H
#define LT_FUNCTION_LT_FUNCTION_ERROR_H
//LOG_PRIORITY_ERROR 1000~1099
typedef enum
{
    RPC_ERROR_TYPE_OK,
    RPC_ERROR_TYPE_NET_BROKEN = 1000,
    RPC_ERROR_TYPE_CONNECT_FAIL,
    RPC_ERROR_TYPE_MEMORY,
    RPC_ERROR_TYPE_NETDOWN_ALREADY,
    RPC_ERROR_TYPE_NET_BOOST,
    RPC_ERROR_TYPE_SND_FAILD,
    RPC_ERROR_TYPE_RCV_FAILD,
    FUN_ERROR_TYPE_UNKNOWN = 1099
} LT_FUNERROR_T;

#define boost_err_translate(error) ((error)?-RPC_ERROR_TYPE_NET_BOOST:0)

#endif //LT_FUNCTION_LT_FUNCTION_ERROR_H

