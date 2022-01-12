#ifndef DEFINE_H_
#define DEFINE_H_

#define NAMESPACE_EVEREST_BEGIN namespace everest{
#define NAMESPACE_EVEREST_END }

#define EVEREST_LOG_INFO(...) NAMED_LOG_INFO("everest",__VA_ARGS__)
#define EVEREST_LOG_ERROR(...) NAMED_LOG_ERROR("everest",__VA_ARGS__)

// 线程Update更新时间间隔,100ms
#define THREAD_UPDATE_INTERVAL (1000*1000*100)

#define ONE_BYTE_TO_BIT (8)

// try宏定义
#define TRY_MACRO try{

// catch宏定义
#define CATCH_MACRO } \
catch(const std::exception& e) \
{ \
} \
catch (...) \
{ \
}

NAMESPACE_EVEREST_BEGIN

using ThreadIdType = std::size_t;

NAMESPACE_EVEREST_END

#endif
