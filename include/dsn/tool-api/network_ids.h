
# pragma once

# include <dsn/utility/ports.h>
# include <dsn/utility/dlib.h>

#define DEFINE_NET_PROTOCOL(x) __selectany const ::dsn::net_header_format x(#x);
#define DEFINE_NET_CHANNEL(x) __selectany const ::dsn::net_channel x(#x);

namespace dsn
{
    class net_header_format
    {
    public:
        DSN_API net_header_format(const char* name);

        DSN_API const char* to_string() const;

        DSN_API static int max_value();
        DSN_API static const char* to_string(int code);
        DSN_API static bool is_exist(const char* name);
        DSN_API static net_header_format from_string(const char* name, net_header_format invalid_value);

        net_header_format()
        {
            _internal_code = 0;
        }

        net_header_format(const net_header_format& r)
        {
            _internal_code = r._internal_code;
        }

        net_header_format& operator=(const net_header_format& source)
        {
            _internal_code = source._internal_code;
            return *this;
        }

        bool operator == (const net_header_format& r)
        {
            return _internal_code == r._internal_code;
        }

        bool operator != (const net_header_format& r)
        {
            return !(*this == r);
        }

        operator int () const { return _internal_code; }

    private:
        int _internal_code;
    };

    DEFINE_NET_PROTOCOL(NET_HDR_INVALID)
    DEFINE_NET_PROTOCOL(NET_HDR_DSN)


    class net_channel
    {
    public:
        DSN_API net_channel(const char* name);

        DSN_API const char* to_string() const;

        DSN_API static int max_value();
        DSN_API static const char* to_string(int code);
        DSN_API static bool is_exist(const char* name);
        DSN_API static net_channel from_string(const char* name, net_channel invalid_value);

        net_channel()
        {
            _internal_code = 0;
        }

        net_channel(const net_channel& r)
        {
            _internal_code = r._internal_code;
        }

        net_channel& operator=(const net_channel& source)
        {
            _internal_code = source._internal_code;
            return *this;
        }

        bool operator == (const net_channel& r)
        {
            return _internal_code == r._internal_code;
        }

        bool operator != (const net_channel& r)
        {
            return !(*this == r);
        }

        operator int() const { return _internal_code; }

    private:
        int _internal_code;
    };

    DEFINE_NET_CHANNEL(NET_CHANNEL_INVALID)
    DEFINE_NET_CHANNEL(NET_CHANNEL_TCP)
}