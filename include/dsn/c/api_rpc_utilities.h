/*
 * Description:
 *     help routines for rpc and network 
 *
 * Revision history:
 *     Feb., 2017, Zhenyu Guo, separated from api_service.h
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once 
# include <dsn/c/api_common.h>
# include <dsn/c/api_task.h>

/*!
@defgroup rpc-addr RPC Address Utilities

@ingroup rpc

RPC Address Utilities

@{
*/



/*! rpc address host type */
typedef enum dsn_host_type_t
{
    HOST_TYPE_INVALID = 0,
    HOST_TYPE_IPV4 = 1,  ///< 4 bytes for IPv4
    ///< TODO: more address types 
} dsn_host_type_t;

/*! rpc address, which is always encoded into a 64-bit integer */
typedef struct dsn_address_t
{
    union u_t {
        struct {
            unsigned long long type : 2;
            unsigned long long padding : 14;
            unsigned long long port : 16;
            unsigned long long ip : 32;
        } v4;    ///< \ref HOST_TYPE_IPV4
        uint64_t value;
    } u;
} dsn_address_t;

/*! translate from hostname to ipv4 in host machine order */
extern DSN_API uint32_t      dsn_ipv4_from_host(const char* name);

/*! get local ipv4 according to the given network interface name */
extern DSN_API uint32_t      dsn_ipv4_local(const char* network_interface);

/*! build a RPC address from given host name or IPV4 string, and port */
extern DSN_API dsn_address_t dsn_address_build(
                                const char* host, 
                                uint16_t port
                                );

/*! build a RPC address from a given ipv4 in host machine order and port */
extern DSN_API dsn_address_t dsn_address_build_ipv4(
                                uint32_t ipv4,
                                uint16_t port
                                );

/*! dump a RPC address to a meaningful string for logging purpose */
extern DSN_API const char*   dsn_address_to_string(dsn_address_t addr);

/*! get the primary address of the rpc engine attached to the current thread */
extern DSN_API dsn_address_t dsn_primary_address();

/*@}*/

/*!
@defgroup rpc-msg RPC Message Utilities

@ingroup rpc

RPC Message Utilities

rpc message and buffer management

Messages from dsn_msg_create_request/dsn_msg_create_response/dsn_rpc_call_wait must be paired
with dsn_rpc_xxx or dsn_msg_destroy. For messages seen in request and response
handlers (e.g., dsn_rpc_request_handler_t), see their comments about how the messages should be handled.
When timeout_milliseconds == 0, [task.%rpc_code%] rpc_timeout_milliseconds is used.

rpc message create/destroy:

@ref dsn_msg_create_request
@ref dsn_msg_create_response
@ref dsn_msg_destroy

rpc message read/write:

<PRE>
// apps write rpc message as follows:
       void* ptr;
       size_t size;
       dsn_msg_write_next(msg, &ptr, &size, min_size);
       write msg content to [ptr, ptr + size)
       dsn_msg_write_commit(msg, real_written_size);

// apps read rpc message as follows:
       void* ptr;
       size_t size;
       dsn_msg_read_next(msg, &ptr, &size);
       read msg content in [ptr, ptr + size)
       dsn_msg_read_commit(msg, real read size);
// if not committed, next dsn_msg_read_next returns the same read buffer
</PRE>

For C++ manipulation of the RPC messages, please refer to:

@ref rpc_read_stream
@ref rpc_write_stream

@{
*/

/*!
 create a rpc request message

 \param rpc_code              task code for this request
 \param timeout_milliseconds  timeout for the RPC call, 0 for default value as 
                              configued in config files for the task code 
 \param thread_hash           used for thread dispatching on server, 
                              if thread_hash == 0 && partition_hash != 0, thread_hash is computed from partition_hash
 \param partition_hash        used for finding which partition the request should be sent to
 \return RPC message handle
 */
extern DSN_API dsn_message_t dsn_msg_create_request(
                                dsn_task_code_t rpc_code, 
                                int timeout_milliseconds DEFAULT(0),
                                int thread_hash DEFAULT(0),
                                uint64_t partition_hash DEFAULT(0)
                                );

/*! create a RPC response message correspondent to the given request message */
extern DSN_API dsn_message_t dsn_msg_create_response(dsn_message_t request);

/*! destroy the message */
extern DSN_API void           dsn_msg_destroy(dsn_message_t msg);

/*! define various serialization format supported by rDSN, note any changes here must also be reflected in src/tools/.../dsn_transport.js */
typedef enum dsn_msg_serialize_format
{
    DSF_INVALID = 0,
    DSF_RDSN = 1,
    DSF_PROTOC_BINARY = 2,
    DSF_PROTOC_JSON = 3,
    DSF_JSON = 4,
    DSF_MAX = 15
} dsn_msg_serialize_format;

typedef union dsn_global_partition_id
{
    struct {
        int32_t app_id;          ///< 1-based app id (0 for invalid)
        int32_t partition_index; ///< zero-based partition index
    } u;
    uint64_t value;
} dsn_gpid;

inline int dsn_gpid_to_thread_hash(dsn_gpid gpid)
{
    return gpid.u.app_id * 7919 + gpid.u.partition_index;
}

# define DSN_MSGM_TIMEOUT        (0x1 << 0) ///< msg timeout is to be set/get
# define DSN_MSGM_THREAD_HASH    (0x1 << 1) ///< thread hash is to be set/get
# define DSN_MSGM_PARTITION_HASH (0x1 << 2) ///< partition hash is to be set/get
# define DSN_MSGM_VNID           (0x1 << 3) ///< virtual node id (gpid) is to be set/get

/*! options for RPC messages, used by \ref dsn_msg_set_options and \ref dsn_msg_get_options */
typedef struct dsn_msg_options_t
{
    int               timeout_ms;     ///< RPC timeout in milliseconds
    int               thread_hash;    ///< thread hash on RPC server
                                      ///< if thread_hash == 0 && partition_hash != 0, thread_hash is computed from partition_hash
    uint64_t          partition_hash; ///< partition hash for calculating partition index
    dsn_gpid         gpid;           ///< virtual node id, 0 for none
} dsn_msg_options_t;

/*!
 set options for the given message

 \param msg  the message handle
 \param opts options to be set in the message
 \param mask the mask composed using e.g., DSN_MSGM_TIMEOUT above to specify what to set
 */
extern DSN_API void          dsn_msg_set_options(
                                dsn_message_t msg,
                                dsn_msg_options_t *opts,
                                uint32_t mask
                                );

/*!
 get options for the given message

 \param msg  the message handle
 \param opts options to be get
 */
extern DSN_API void         dsn_msg_get_options(
                                dsn_message_t msg,
                                /*out*/ dsn_msg_options_t* opts
                                );

DSN_API void dsn_msg_set_serailize_format(dsn_message_t msg, dsn_msg_serialize_format fmt);

DSN_API dsn_msg_serialize_format dsn_msg_get_serialize_format(dsn_message_t msg);

/*! get message body size */
extern DSN_API size_t           dsn_msg_body_size(dsn_message_t msg);

/*! get read/write pointer with the given offset */
extern DSN_API void*            dsn_msg_rw_ptr(dsn_message_t msg, size_t offset_begin);

/*! get trace id of the message */
extern DSN_API uint64_t         dsn_msg_trace_id(dsn_message_t msg);

/*! get task code of the message */
extern DSN_API dsn_task_code_t dsn_msg_task_code(dsn_message_t msg);

/*! get session context of the message */
extern DSN_API void*           dsn_msg_session_context(dsn_message_t msg);

/*!
 get message write buffer

 \param msg      message handle
 \param ptr      *ptr returns the writable memory pointer
 \param size     *size returns the writable memory buffer size
 \param min_size *size must >= min_size
 */
extern DSN_API void          dsn_msg_write_next(
                                dsn_message_t msg, 
                                /*out*/ void** ptr, 
                                /*out*/ size_t* size, 
                                size_t min_size
                                );

/*! commit the write buffer after the message content is written with the real written size */
extern DSN_API void          dsn_msg_write_commit(dsn_message_t msg, size_t size);

/*! append an external buffer to the message, with a callback executed when the msg is sent or timed-out */
extern DSN_API void          dsn_msg_append(
                                dsn_message_t msg, 
                                void* buffer, 
                                size_t size, 
                                void* context, 
                                void (*sent_callback)(void*, void*) // (context, buffer)
                                );

/*!
 get message read buffer

 \param msg  message handle
 \param ptr  *ptr points to the next read buffer
 \param size *size points to the size of the next buffer filled with content

 \return true if it succeeds, false if it is already beyond the end of the message
 */
extern DSN_API bool          dsn_msg_read_next(
                                dsn_message_t msg, 
                                /*out*/ void** ptr, 
                                /*out*/ size_t* size
                                );

/*! commit the read buffer after the message content is read with the real read size,
    it is possible to use a different size to allow duplicated or skipped read in the message.
 */
extern DSN_API void          dsn_msg_read_commit(dsn_message_t msg, size_t size);

/*@}*/
