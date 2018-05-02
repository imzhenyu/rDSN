# pragma once

# ifdef _WIN32
# include <Windows.h>
# endif

# include <dsn/cpp/auto_codes.h>
# include <functional>

#define FD_READABLE 1
#define FD_WRITABLE 2

namespace dsn { namespace tools {

    class event_loop;

    //
    // this structure is per io handle, and registered when bind_io_handle to completion queue
    // the callback will be executed per io completion or ready
    //
    // void handle_event(event_loop* loop, int fd, void* ctx, int events) on non-windows
    // void handle_event(event_loop* loop, int error, DWORD io_size, LPOVERLAPPED lolp) on windows
# ifdef _WIN32
    typedef void (*io_loop_callback)(event_loop*, int, DWORD, LPOVERLAPPED);
    //typedef std::function<void(int, DWORD, LPOVERLAPPED)> io_loop_callback;
# else
    typedef void (*io_loop_callback)(event_loop*, int, void*, int);
    //typedef std::function<void(int)> io_loop_callback; // with events mask from AE_xxx above
# endif    

    // (client-data, client-data2)
    typedef void (*lpc_callback) (event_loop*, void*, void*);

    // when per-thread == true, may use the shared per-thread loop directly no matter
    // how many event_loop_create is called
    // this is useful for shared loop with other modules
    event_loop* event_loop_create(bool per_thread = false);
    
    void event_loop_destroy(event_loop* loop);

    void event_loop_run(event_loop* loop);
    bool event_loop_running(event_loop* loop);

    void event_loop_stop(event_loop* loop);
    
    error_code event_loop_bind_fd(
        event_loop* loop,
        int fd,
        int mask, // FD_READABLE | FD_WRITABLE
        io_loop_callback cb,
        void* ctx,
        bool edge_trigger = false
        );

    // cb is not necessary on windows
    void event_loop_unbind_fd(
        event_loop* loop,
        int fd, 
        int mask // FD_READABLE | FD_WRITABLE
        );

    void event_loop_add_oneshot_timer(
        event_loop* loop,
        const char* name, 
        lpc_callback cb, 
        void* data, 
        void* data2, 
        int milliseconds
        );

    int event_loop_events(event_loop* l, int fd);
}
}
