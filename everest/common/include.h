#ifndef INCLUDE_H_
#define INCLUDE_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <bitset>
#include <optional>
#include <utility>
#include <string>
#include <limits>
#include <type_traits>
#include <set>
#include <unordered_set>
#include <map>
#include <mutex>
#include <unordered_map>
#include <stdint.h>

// 通用定义
#include "common/define.h"

// 平台相关
#include "common/platform.h"

// 小工具函数
#include "common/helper.h"

// 拷贝时触发移动
#include "multi_producer_multi_consumer/move_on_copy.h"

// 整型类型数据安全加、减、乘、除
#include "calculator/safe_integral_calculator.h"

// 随机数
#include "effolkronium/random.hpp"

// buffer
#include "buffer/buffer.h"

// 命令行参数
#include "cmd_line/cmdline.h"
#include "cmd_line/command_line_parser.h"

// 禁止拷贝相关
#include "non_copyable/non_copyable.h"

// 单体
#include "singleton/singleton.h"

// 对象池
#include "object_pool/object_pool.h"

// log相关
#include "spdlog_wrapper/logger_create_info.h"
#include "spdlog_wrapper/everest_spdlog.h"

// 定时器和时钟相关
#include "timer/clock.h"
#include "timer/timer.h"

// 格式化相关
#include "fmt/format.h"
#include "fmt/chrono.h"

// 时间相关
#include "date/date.h"
#include "time_capsule/time_capsule.h"

// asio
#include <asio.hpp>

// 配置数据
#include "configuration/thread_allocate_configuration.h"
#include "configuration/service_config.h"
#include "configuration/configuration_manager.h"

// 监控器
#include "net/net_define.h"
#include "thread/thread_define.h"
#include "control_monitor/monitor_base.h"
#include "control_monitor/thread_monitor_data.h"
#include "control_monitor/thread_monitor.h"
#include "control_monitor/service_monitor.h"
#include "control_monitor/control_monitor.h"

// 线程相关
#include "thread/thread_base.h"
#include "thread/thread_pool.h"
#include "thread/monitor_thread.h"
#include "thread/session_thread.h"
#include "thread/listener_connector_thread.h"
#include "thread/session_thread_pool.h"
#include "thread/net_thread_allocate.h"

// 网络
#include "net/address.h"
#include "net/session_id.h"
#include "net/session_base.h"
#include "net/session_creator.h"
#include "net/listener.h"

// 消息包相关
#include "packet/packet_head.h"
#include "packet/packet.h"
#include "packet/packet_maker.h"
#include "packet/packet_handler.h"
#include "packet/packet_serialize.h"

// 服务
#include "service/service.h"

// 进程
#include "launcher/launcher_base.h"

#endif
