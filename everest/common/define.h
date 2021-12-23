#ifndef DEFINE_H_
#define DEFINE_H_

#define NAMESPACE_BEGIN namespace everest{
#define NAMESPACE_END }

#define EVEREST_LOG_INFO(...) NAMED_LOG_INFO("everest",__VA_ARGS__)
#define EVEREST_LOG_ERROR(...) NAMED_LOG_ERROR("everest",__VA_ARGS__)

// �߳�Update����ʱ����,100ms
#define THREAD_UPDATE_INTERVAL (1000*1000*100)
#define THREAD_POOL_MIN_SIZE (2)
#define THREAD_POOL_MAX_SIZE (10)

// try�궨��
#define TRY_MACRO try{

// catch�궨��
#define CATCH_MACRO } \
catch(const std::exception& e) \
{ \
} \
catch (...) \
{ \
}

#endif
