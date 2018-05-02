/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rDSN) -=- 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# include "message_parser_manager.h"
# include <dsn/service_api_c.h>

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "message.parser"

namespace dsn {

    // ------------------- net headr format ------------------------------
    DEFINE_CUSTOMIZED_ID_TYPE(net_header_format_)

    net_header_format::net_header_format(const char* name)
    {
        _internal_code = net_header_format_(name);
    }

    const char* net_header_format::to_string() const
    {
        return net_header_format_::to_string(_internal_code);
    }

    int net_header_format::max_value()
    {
        return net_header_format_::max_value();
    }

    const char* net_header_format::to_string(int code)
    {
        return net_header_format_::to_string(code);
    }

    bool net_header_format::is_exist(const char* name)
    {
        return net_header_format_::is_exist(name);
    }

    net_header_format net_header_format::from_string(const char* name, net_header_format invalid_value)
    {
        net_header_format ch;
        ch._internal_code = net_header_format_::get_id(name);
        if (ch._internal_code == -1)
            return invalid_value;
        else
            return ch;
    }

    // ------------------- net channel ------------------------------
    DEFINE_CUSTOMIZED_ID_TYPE(net_channel_)

    net_channel::net_channel(const char* name)
    {
        _internal_code = net_channel_(name);
    }

    const char* net_channel::to_string() const
    {
        return net_channel_::to_string(_internal_code);
    }

    int net_channel::max_value()
    {
        return net_channel_::max_value();
    }
    
    const char* net_channel::to_string(int code)
    {
        return net_channel_::to_string(code);
    }

    bool net_channel::is_exist(const char* name)
    {
        return net_channel_::is_exist(name);
    }

    net_channel net_channel::from_string(const char* name, net_channel invalid_value)
    {
        net_channel ch;
        ch._internal_code = net_channel_::get_id(name);
        if (ch._internal_code == -1)
            return invalid_value;
        else
            return ch;
    }

    // ------------------- header type ------------------------------
    struct header_type
    {
    public:
        std::string type;

        header_type()
        {
            type = "";
        }

        header_type(const std::string& itype)
        {
            type = itype;
        }

        header_type(const char* str)
        {
            type = std::string(str);
        }
        
        header_type(const header_type& another)
        {
            type = another.type;
        }
        
        header_type& operator=(const header_type& another)
        {
            type = another.type;
            return *this;
        }
        
        bool operator==(const header_type& other) const
        {
            return type == other.type;
        }
        
        bool operator!=(const header_type& other) const
        {
            return type != other.type;
        }
        
        std::string debug_string() const { return type; }

    public:
        static net_header_format bytes_to_format(const char* bytes, int len);
        static net_header_format header_type_to_c_type(const header_type& hdr_type);
        static void register_header_signature(const char* sig, net_header_format type);

    private:
        static std::map<std::string, net_header_format> s_fmt_map;
    };

    std::map<std::string, net_header_format> header_type::s_fmt_map;

    /*static*/ net_header_format header_type::header_type_to_c_type(const header_type& hdr_type)
    {
        auto it = s_fmt_map.find(hdr_type.type);
        if (it != s_fmt_map.end())
        {
            return it->second;
        }
        else
            return NET_HDR_INVALID;
    }

    /*static*/ net_header_format header_type::bytes_to_format(const char* bytes, int len)
    {
        // longest prefix matching for 'bytes' and registered fmts
        net_header_format lmatch = NET_HDR_INVALID;

        for (auto& kv : s_fmt_map)
        {
            int clen = (int)kv.first.length();

            // not enough bytes received
            if (clen > len)
                continue;

            // longer header already matched
            if (lmatch != NET_HDR_INVALID && strlen(lmatch.to_string()) >= clen)
                continue;

            // current header matched
            if (memcmp((const void*)bytes, (const void*)kv.first.c_str(), clen) == 0)
            {
                ddebug("the current connection is now matched to '%s'", kv.second.to_string());
                lmatch = kv.second;
            }
        }

        return lmatch;
    }

    /*static*/ void header_type::register_header_signature(const char* sig, net_header_format type)
    {
        auto it = s_fmt_map.find(sig);
        if (it != s_fmt_map.end())
        {
            if (it->second != type)
            {
                dassert(false, "signature '%s' is already registerd for header type %s",
                    sig, type.to_string()
                    );
            }
        }
        else
        {
            s_fmt_map.emplace(sig, type);
        }
    }

    /*static*/ net_header_format message_parser::get_header_type(const char* bytes, int len)
    {
        return header_type::bytes_to_format(bytes, len);
    }

    /*static*/ safe_string message_parser::get_debug_string(const char* bytes, int len)
    {
        if (len > sizeof(uint32_t))
            len = sizeof(uint32_t);

        safe_string s;
        s.resize((size_t)len);
        memcpy((void*)s.c_str(), bytes, (size_t)len);
        return s;
    }

    /*static*/ message_parser* message_parser::new_message_parser(net_header_format hdr_format, bool is_client)
    {
        if (hdr_format != NET_HDR_INVALID)
        {
            message_parser* parser = message_parser_manager::instance().create_parser(hdr_format, is_client);
            dassert(parser, "message parser '%s' not registerd or invalid!", hdr_format.to_string());
            parser->_header_format = hdr_format;
            return parser;
        }
        else
        {
            return nullptr;
        }
    }

    //-------------------- msg reader --------------------
    char* message_reader::read_buffer_ptr(unsigned int read_next)
    {
        if (read_next + _buffer_occupied > _buffer.length())
        {
            // remember currently read content
            blob rb;
            if (_buffer_occupied > 0)
                rb = _buffer.range(0, _buffer_occupied);
            
            // switch to next
            unsigned int sz = (read_next + _buffer_occupied > _buffer_block_size ?
                        read_next + _buffer_occupied : _buffer_block_size);
            std::shared_ptr<char> holder(static_cast<char*>(dsn_transient_malloc(sz)), [](char* c) {dsn_transient_free(c);});
            _buffer.assign(std::move(holder), 0, sz);
            _buffer_occupied = 0;

            // copy
            if (rb.length() > 0)
            {
                memcpy((void*)_buffer.data(), (const void*)rb.data(), rb.length());
                _buffer_occupied = rb.length();
            }
            
            dassert (read_next + _buffer_occupied <= _buffer.length(), "");
        }

        return (char*)(_buffer.data() + _buffer_occupied);
    }

    //-------------------- msg parser manager --------------------
    message_parser_manager::message_parser_manager()
    {
    }

    void message_parser_manager::register_factory(net_header_format fmt, const std::vector<const char*>& signatures, message_parser::factory f, message_parser::factory2 f2, size_t sz)
    {
        if (static_cast<unsigned int>(fmt) >= _factory_vec.size())
        {
            _factory_vec.resize(fmt + 1);
        }

        parser_factory_info& info = _factory_vec[fmt];
        info.fmt = fmt;
        info.factory = f;
        info.factory2 = f2;
        info.parser_size = sz;

        for (auto& v : signatures)
        {
            header_type::register_header_signature(v, fmt);
        }
    }

    message_parser* message_parser_manager::create_parser(net_header_format fmt, bool is_client)
    {
        parser_factory_info& info = _factory_vec[fmt];
        if (info.factory)
            return info.factory(is_client);
        else
            return nullptr;
    }
}
