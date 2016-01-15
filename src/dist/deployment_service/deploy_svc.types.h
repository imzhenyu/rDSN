# pragma once
# include <dsn/service_api_cpp.h>
# include <dsn/dist/cluster_scheduler.h>

# include <rapidjson/document.h> 
# include <rapidjson/prettywriter.h>
# include <rapidjson/writer.h>
# include <rapidjson/stringbuffer.h>

//
// uncomment the following line if you want to use 
// data encoding/decoding from the original tool instead of rDSN
// in this case, you need to use these tools to generate
// type files with --gen=cpp etc. options
//
// !!! WARNING: not feasible for replicated service yet!!! 
//
// # define DSN_NOT_USE_DEFAULT_SERIALIZATION

# ifdef DSN_NOT_USE_DEFAULT_SERIALIZATION

# include <dsn/thrift_helper.h>
# include "deploy_svc_types.h" 

namespace dsn { namespace dist { 
    // ---------- deploy_request -------------
    inline void marshall(::dsn::binary_writer& writer, const deploy_request& val)
    {
        boost::shared_ptr< ::dsn::binary_writer_transport> transport(new ::dsn::binary_writer_transport(writer));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::marshall_rpc_args<deploy_request>(&proto, val, &deploy_request::write);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ deploy_request& val)
    {
        boost::shared_ptr< ::dsn::binary_reader_transport> transport(new ::dsn::binary_reader_transport(reader));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::unmarshall_rpc_args<deploy_request>(&proto, val, &deploy_request::read);
    };

    // ---------- deploy_info -------------
    inline void marshall(::dsn::binary_writer& writer, const deploy_info& val)
    {
        boost::shared_ptr< ::dsn::binary_writer_transport> transport(new ::dsn::binary_writer_transport(writer));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::marshall_rpc_args<deploy_info>(&proto, val, &deploy_info::write);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ deploy_info& val)
    {
        boost::shared_ptr< ::dsn::binary_reader_transport> transport(new ::dsn::binary_reader_transport(reader));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::unmarshall_rpc_args<deploy_info>(&proto, val, &deploy_info::read);
    };

    // ---------- deploy_info_list -------------
    inline void marshall(::dsn::binary_writer& writer, const deploy_info_list& val)
    {
        boost::shared_ptr< ::dsn::binary_writer_transport> transport(new ::dsn::binary_writer_transport(writer));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::marshall_rpc_args<deploy_info_list>(&proto, val, &deploy_info_list::write);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ deploy_info_list& val)
    {
        boost::shared_ptr< ::dsn::binary_reader_transport> transport(new ::dsn::binary_reader_transport(reader));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::unmarshall_rpc_args<deploy_info_list>(&proto, val, &deploy_info_list::read);
    };

    // ---------- cluster_info -------------
    inline void marshall(::dsn::binary_writer& writer, const cluster_info& val)
    {
        boost::shared_ptr< ::dsn::binary_writer_transport> transport(new ::dsn::binary_writer_transport(writer));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::marshall_rpc_args<cluster_info>(&proto, val, &cluster_info::write);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ cluster_info& val)
    {
        boost::shared_ptr< ::dsn::binary_reader_transport> transport(new ::dsn::binary_reader_transport(reader));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::unmarshall_rpc_args<cluster_info>(&proto, val, &cluster_info::read);
    };

    // ---------- cluster_list -------------
    inline void marshall(::dsn::binary_writer& writer, const cluster_list& val)
    {
        boost::shared_ptr< ::dsn::binary_writer_transport> transport(new ::dsn::binary_writer_transport(writer));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::marshall_rpc_args<cluster_list>(&proto, val, &cluster_list::write);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ cluster_list& val)
    {
        boost::shared_ptr< ::dsn::binary_reader_transport> transport(new ::dsn::binary_reader_transport(reader));
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        ::dsn::unmarshall_rpc_args<cluster_list>(&proto, val, &cluster_list::read);
    };

} } 


# else // use rDSN's data encoding/decoding

namespace dsn { namespace dist { 
    // ---------- deploy_request -------------
    struct deploy_request
    {
        std::string package_id;
        std::string package_full_path;
        ::dsn::rpc_address package_server;
        std::string cluster_name;
        std::string name;
    };

    inline void marshall(::dsn::binary_writer& writer, const deploy_request& val)
    {
        marshall(writer, val.package_id);
        marshall(writer, val.package_full_path);
        marshall(writer, val.package_server);
        marshall(writer, val.cluster_name);
        marshall(writer, val.name);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ deploy_request& val)
    {
        unmarshall(reader, val.package_id);
        unmarshall(reader, val.package_full_path);
        unmarshall(reader, val.package_server);
        unmarshall(reader, val.cluster_name);
        unmarshall(reader, val.name);
    };

    // ---------- deploy_info -------------
    struct deploy_info
    {
        std::string package_id;
        std::string name;
        std::string service_url;
        ::dsn::error_code error;
        std::string cluster;
        service_status status;
    };

    inline void marshall(::dsn::binary_writer& writer, const deploy_info& val)
    {
        marshall(writer, val.package_id);
        marshall(writer, val.name);
        marshall(writer, val.service_url);
        marshall(writer, val.error);
        marshall(writer, val.cluster);
        marshall(writer, val.status);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ deploy_info& val)
    {
        unmarshall(reader, val.package_id);
        unmarshall(reader, val.name);
        unmarshall(reader, val.service_url);
        unmarshall(reader, val.error);
        unmarshall(reader, val.cluster);
        unmarshall(reader, val.status);
    };

    // ---------- deploy_info_list -------------
    struct deploy_info_list
    {
        std::vector< deploy_info> services;
    };

    inline void marshall(::dsn::binary_writer& writer, const deploy_info_list& val)
    {
        marshall(writer, val.services);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ deploy_info_list& val)
    {
        unmarshall(reader, val.services);
    };

    // ---------- cluster_info -------------
    struct cluster_info
    {
        std::string name;
        cluster_type type;
    };

    inline void marshall(::dsn::binary_writer& writer, const cluster_info& val)
    {
        marshall(writer, val.name);
        marshall(writer, val.type);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ cluster_info& val)
    {
        unmarshall(reader, val.name);
        unmarshall(reader, val.type);
    };

    // ---------- cluster_list -------------
    struct cluster_list
    {
        std::vector< cluster_info> clusters;
    };

    inline void marshall(::dsn::binary_writer& writer, const cluster_list& val)
    {
        marshall(writer, val.clusters);
    };

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ cluster_list& val)
    {
        unmarshall(reader, val.clusters);
    };

#define TEST_PARAM(x) {if(!(x)){return ERR_INVALID_PARAMETERS;}}

    inline void marshall_json(rapidjson::Writer<rapidjson::StringBuffer>& writer, const error_code& err)
    {
       if(err == ERR_OK)
       {
            writer.String("ok");
       }
       else if (err == ERR_INVALID_PARAMETERS)
       {
           writer.String("invalid parameters");
       }
       else
       {
           writer.String("internal error");
       }
    };

    inline void marshall_json(rapidjson::Writer<rapidjson::StringBuffer>& writer, const service_status& status)
    {
        const char* status_name[] = { "prepare resource", "deploying", "running", "failover", "failed", "count", "invalid" };
        int status_val = (int)status;
        writer.String(status_name[status_val]);
    };

    inline void marshall_json(rapidjson::Writer<rapidjson::StringBuffer>& writer, const cluster_type& type)
    {
        int status_val = (int)type;
        writer.Int(status_val);
    };

    inline void marshall_json(rapidjson::Writer<rapidjson::StringBuffer>& writer, const std::string& str)
    {
        writer.String(str.c_str());
    };

    inline void marshall_json(rapidjson::Writer<rapidjson::StringBuffer>& writer, const deploy_info& info)
    {
        writer.StartObject();
        writer.String("cluster"); marshall_json(writer, info.cluster);
        writer.String("error"); marshall_json(writer, info.error);
        writer.String("name"); marshall_json(writer, info.name);
        writer.String("package_id"); marshall_json(writer, info.package_id);
        writer.String("service_url"); marshall_json(writer, info.service_url);
        writer.String("status"); marshall_json(writer, info.status);
        writer.EndObject();
    };

    inline void marshall_json(rapidjson::Writer<rapidjson::StringBuffer>& writer, const cluster_info& info)
    {
        writer.StartObject();
        writer.String("name"); marshall_json(writer, info.name);
        writer.String("type"); marshall_json(writer, info.type);
        writer.EndObject();
    };

    inline error_code unmarshall_json(const rapidjson::Value &doc, std::string& val)
    {
        TEST_PARAM(doc.IsString());
        val = doc.GetString();
        return ERR_OK;
    }

    inline error_code unmarshall_json(const rapidjson::Value &doc, rpc_address& val)
    {
        //TODO: unmarshall rpc address
        return ERR_OK;
    }

    /*
     * example format:
     * {"error":0}
     */
    inline std::string marshall_json(const error_code& err)
    {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sbuf);

        writer.StartObject();
        writer.String("error"); marshall_json(writer, err);
        writer.EndObject();

        return sbuf.GetString();
    };

    /*
     * example format:
     * {
          "cluster":"mycluster",
          "error":0,
          "name":"name",
          "package_id":"123",
          "service_url":"node1:8080",
          "status":0
       }
     */
    inline std::string marshall_json(const deploy_info& info)
    {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sbuf);

        marshall_json(writer, info);

        return sbuf.GetString();
    };

    /*
     * example format:
       {
         "services":
         [
           {
             "cluster":"mycluster",
             "error":0,
             "name":"name",
             "package_id":"123",
             "service_url":"node1:8080",
             "status":0
           },
           {
             "cluster":"mycluster",
             "error":0,
             "name":"name2",
             "package_id":"123",
             "service_url":"node1:8080",
             "status":0
           }
         ]
       }
     */
    inline std::string marshall_json(const deploy_info_list& dlist)
    {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sbuf);

        writer.StartObject();
        writer.String("services");
        writer.StartArray();
        for (std::vector<deploy_info>::const_iterator i = dlist.services.begin(); i != dlist.services.end(); i++)
        {
            marshall_json(writer, *i);
        }
        writer.EndArray();
        writer.EndObject();

        return sbuf.GetString();
    };

    /*
     * example format:
       {
         "clusters":
         [
           {
             "name":"cname",
             "type":1
           },
           {
             "name":"cname2",
             "type":2
           }
         ]
       }
     */
    inline std::string marshall_json(const cluster_list& clist)
    {
        rapidjson::StringBuffer sbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sbuf);

        writer.StartObject();
        writer.String("clusters");
        writer.StartArray();
        for (std::vector<cluster_info>::const_iterator i = clist.clusters.begin(); i != clist.clusters.end(); i++)
        {
            marshall_json(writer, *i);
        }
        writer.EndArray();
        writer.EndObject();

        return sbuf.GetString();
    };

    inline error_code unmarshall_json(const char* json_str, const char* key, std::string& val)
    {
        rapidjson::Document doc;

        TEST_PARAM(!doc.Parse<0>(json_str).HasParseError())
        TEST_PARAM(doc.IsObject())
        TEST_PARAM(doc.HasMember(key))
        TEST_PARAM(!unmarshall_json(doc[key], val))
        
        return ERR_OK;
    };

    inline error_code unmarshall_json(const char* json_str, const char* key, deploy_request& val)
    {
        rapidjson::Document doc;

        TEST_PARAM(!doc.Parse<0>(json_str).HasParseError())
        TEST_PARAM(doc.IsObject())
        TEST_PARAM(doc.HasMember("cluster_name"))
        TEST_PARAM(!unmarshall_json(doc[key]["cluster_name"], val.cluster_name))
        TEST_PARAM(doc.HasMember("name"))
        TEST_PARAM(!unmarshall_json(doc[key]["name"], val.name))
        TEST_PARAM(doc.HasMember("package_full_path"))
        TEST_PARAM(!unmarshall_json(doc[key]["package_full_path"], val.package_full_path))
        TEST_PARAM(doc.HasMember("package_id"))
        TEST_PARAM(!unmarshall_json(doc[key]["package_id"], val.package_id))
        TEST_PARAM(doc.HasMember("package_server"))
        TEST_PARAM(!unmarshall_json(doc[key]["package_server"], val.package_server))
            
        return ERR_OK;
    };
} } 

#endif 
