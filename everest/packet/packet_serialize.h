#ifndef PACKET_SERIALIZE_H_
#define PACKET_SERIALIZE_H_

// pb
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/repeated_field.h>

// message_pack

NAMESPACE_EVEREST_BEGIN

void operator << (BufferBase& buffer, const ::google::protobuf::Message& msg);

void operator >>(BufferBase& buffer, ::google::protobuf::Message& msg);

void operator << (std::string& to, const ::google::protobuf::Message& msg);

void operator >> (std::string& from, ::google::protobuf::Message& msg);

NAMESPACE_EVEREST_END

#endif // !PACKET_SERIALIZE_H_

