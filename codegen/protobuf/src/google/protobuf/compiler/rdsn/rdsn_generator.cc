// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: imzhenyu@outlook.com (Zhenyu Guo)
//
// Generates rDSN php definition code for a given .proto file for 
// further coe generation in rDSN.
// See project Robust Distributed System Nucleus for more details.

#include <google/protobuf/compiler/rdsn/rdsn_generator.h>

#include <vector>
#include <memory>
#ifndef _SHARED_PTR_H
#include <google/protobuf/stubs/shared_ptr.h>
#endif
#include <utility>

#include <google/protobuf/compiler/cpp/cpp_file.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace rdsn {

using namespace ::google::protobuf::compiler::cpp;

RdsnGenerator::RdsnGenerator() {}
RdsnGenerator::~RdsnGenerator() {}

void RdsnGenerator::PrintEnum(io::Printer& pt, const EnumDescriptor* t) const
{
    pt.Print("$tmp = new t_enum($_PROG, \"@name@\");\n", "name", t->full_name());
    for (int j = 0; j < t->value_count(); j++)
    {
        auto c = t->value(j);
        char nv[32];
        sprintf(nv, "%d", c->number());
        pt.Print("$tmp->add_value(\"@key@\", @val@);\n",
            "key", c->name(),
            "val", nv
            );
    }
    pt.Print("\n");
}

void RdsnGenerator::PrintMessage(io::Printer& pt, const Descriptor* t) const
{
    if (IsMapEntryMessage(t)) return;

    for (int i = 0; i < t->enum_type_count(); i++) {
        PrintEnum(pt, t->enum_type(i));
    }

    for (int i = 0; i < t->nested_type_count(); i++) {
        PrintMessage(pt, t->nested_type(i));
    }

    pt.Print("$tmp = new t_struct($_PROG, \"@name@\");\n", "name", t->full_name());
    for (int j = 0; j < t->field_count(); j++)
    {
        auto f = t->field(j);

        std::string type = f->type_name();
        if (f->message_type())
        {
            type = f->message_type()->full_name();
        }
        else if (f->enum_type())
        {
            type = f->enum_type()->full_name();
        }

        pt.Print("$tmp->add_field(\"@name@\", \"@type@\");\n",
            "type", type,
            "name", f->name()
            );
    }
    pt.Print("\n");
}

bool RdsnGenerator::Generate(const FileDescriptor* file,
                            const string& parameter,
                            GeneratorContext* generator_context,
                            string* error) const {
  vector<pair<string, string> > options;
  ParseGeneratorParameter(parameter, &options);

  string basename = StripProto(file->name());
  basename.append(".pb");

  std::map<string, string> vars;
  vars["pname"] = StripProto(file->name());
  vars["nm"] = file->package();

  google::protobuf::scoped_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(basename + ".php"));
  io::Printer pt(output.get(), '@');  
  pt.Print("<?php\n");
  pt.Print(vars, "$_PROG = new t_program(\"@pname@\");\n");
  pt.Print("\n");

  // namespaces 
  // TODO: namespace definition for other languages
  pt.Print("$_PROG->namespaces[\"cpp\"] = \"@pkg@\";\n", "pkg", file->package());
  pt.Print("\n");

  // includes
  for (int i = 0; i < file->dependency_count(); i++) 
  {
      pt.Print("$tmp = new t_program(\"@inc@\");\n",
          "inc",
          file->dependency(i)->name()
          );
      pt.Print("$_PROG->includes[$tmp->name] = $tmp;\n");
      pt.Print("$tmp->namespaces[\"cpp\"] = \"@pkg@\";\n", "pkg", file->dependency(i)->package());
  }
  pt.Print("\n");

  for (int i = 0; i < file->enum_type_count(); i++) {
      auto t = file->enum_type(i);
      PrintEnum(pt, t);
  }

  for (int i = 0; i < file->message_type_count(); i++) {
      auto t = file->message_type(i);
      PrintMessage(pt, t);
  }
    
  for (int i = 0; i < file->service_count(); i++) {
      auto t = file->service(i);
      pt.Print("$tmp = new t_service($_PROG, \"@name@\");\n", "name", t->name());
      for (int j = 0; j < t->method_count(); j++)
      {
          auto m = t->method(j);
          pt.Print("$tmp2 = $tmp->add_function(\"@return_type@\", \"@name@\");\n",
              "return_type", m->output_type()->full_name(),
              "name", m->name()
              );

          pt.Print("$tmp2->add_param(\"@name@\", \"@type@\");\n",
              "name", "req",
              "type", m->input_type()->full_name()
              );
      }
      pt.Print("\n");
  }

  for (int i = 0; i < file->extension_count(); i++) {
      // TODO: anything to do here?
  }

  pt.Print("?>\n");
  return true;
}

}  // namespace rdsn
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
