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

# pragma once

# include <string>
# include <sstream>
# include <dsn/cpp/rpc_stream.h>
# include <dsn/service_api_c.h>

namespace dsn
{
    template<typename T>
    inline void marshall(::dsn::binary_writer& writer, const T& val)
    {
        dassert (false, "marshall for %s is not implemented", typeid(T).name());
    }

    template<typename T>
    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ T& val)
    {
        dassert (false, "unmarshall for %s is not implemented", typeid(T).name());
    }

    template<typename T>
    inline void marshall(dsn_message_t msg, const T& val)
    {
        ::dsn::rpc_write_stream writer(msg);
        marshall(writer, val);
    }

    template<typename T>
    inline void unmarshall(dsn_message_t msg, /*out*/ T& val)
    {
        ::dsn::rpc_read_stream reader(msg);
        unmarshall(reader, val);
    }

    //------------------ DSF_RDSN implementation ------------------------
    # define DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(T) \
    inline void marshall(::dsn::binary_writer& writer, const T& val) \
    { \
        writer.write(val); \
    } \
    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ T& val) \
    { \
        reader.read(val); \
    }

    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(bool)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(int8_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(uint8_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(int16_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(uint16_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(int32_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(uint32_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(int64_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(uint64_t)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(float)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(double)
    DEFINE_NATIVE_TYPE_SERIALIZATION_FUNCTIONS(std::string)

    namespace serialization
    {
        template<typename T>
        std::string no_registered_function_error_notice(const T& t, dsn_msg_serialize_format fmt)
        {
            std::stringstream ss;
            ss << "This error occurs because someone is trying to ";
            ss << "serialize/deserialize an object of the type ";
            ss << typeid(t).name();
            ss << " but has not registered corresponding serialization/deserialization function for the format of ";
            //           ss << enum_to_string(fmt) << ".";
            ss << fmt << ".";
            return ss.str();
        }
    }
}

/*
#define PROTOBUF_MARSHALLER \
    case DSF_PROTOC_BINARY: marshall_protobuf_binary(writer, value); break; \
    case DSF_PROTOC_JSON: marshall_protobuf_json(writer, value); break;

#define PROTOBUF_UNMARSHALLER \
    case DSF_PROTOC_BINARY: unmarshall_protobuf_binary(reader, value); break; \
    case DSF_PROTOC_JSON: unmarshall_protobuf_json(reader, value); break;

#define GENERATED_TYPE_SERIALIZATION(GType, SerializationType) \
    inline void marshall(binary_writer& writer, const GType &value, dsn_msg_serialize_format fmt) \
    { \
        switch (fmt) \
        { \
            SerializationType##_MARSHALLER \
            default: dassert(false, ::dsn::serialization::no_registered_function_error_notice(value, fmt).c_str()); \
        } \
    } \
    inline void unmarshall(binary_reader& reader, GType &value, dsn_msg_serialize_format fmt) \
    { \
        switch (fmt) \
        { \
            SerializationType##_UNMARSHALLER \
            default: dassert(false, ::dsn::serialization::no_registered_function_error_notice(value, fmt).c_str()); \
        } \
    }
*/