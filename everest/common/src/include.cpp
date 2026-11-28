// 统一编译单元按依赖顺序包含各组件实现，顺序不可被 clang-format 重排。
// clang-format off

#include "common/include.h"

// helper
#include "common/src/helper.cpp"

// buffer
#include "buffer/buffer.cpp"

// 命令行
#include "cmd_line/src/command_line_parser.cpp"

// 对象池
#include "object_pool/src/object_pool.cpp"

// 环形队列
#include "multi_producer_multi_consumer/src/circular_queue.cpp"

// 多生产者多消费者队列
#include "multi_producer_multi_consumer/src/multi_producer_multi_consumer.cpp"

// 时间
#include "timer/src/clock.cpp"
#include "time_capsule/src/time_capsule.cpp"

// 监控数据
#include "control_monitor/src/thread_monitor_data.cpp"

// 线程
#include "thread/src/thread_base.cpp"
#include "thread/src/monitor_thread.cpp"
#include "thread/src/session_thread.cpp"
#include "thread/src/listener_connector_thread.cpp"
#include "thread/src/session_thread_pool.cpp"
#include "thread/src/net_thread_allocate.cpp"

// log
#include "spdlog_wrapper/src/logger_create_info.cpp"
#include "spdlog_wrapper/src/everest_spdlog.cpp"

// 监控器
#include "control_monitor/src/thread_monitor.cpp"
#include "control_monitor/src/service_monitor.cpp"
#include "control_monitor/src/control_monitor.cpp"

// 配置
#include "configuration/src/configuration_manager.cpp"

// 网络
#include "net/src/session_id.cpp"
#include "net/src/address.cpp"
#include "net/src/session_base.cpp"
#include "net/src/listener.cpp"

// 消息包相关
#include "packet/src/packet_serialize.cpp"

// 服务
#include "service/src/service.cpp"

// 进程
#include "launcher/src/launcher_base.cpp"

// clang-format on
