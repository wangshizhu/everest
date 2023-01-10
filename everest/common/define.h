#ifndef DEFINE_H_
#define DEFINE_H_

#define NAMESPACE_EVEREST_BEGIN namespace everest{
#define NAMESPACE_EVEREST_END }

#define EVEREST_LOG_INFO(...) NAMED_LOG_INFO("everest",__VA_ARGS__)
#define EVEREST_LOG_ERROR(...) NAMED_LOG_ERROR("everest",__VA_ARGS__)
#define EVEREST_LOG_DEBUG(...) NAMED_LOG_DEBUG("everest",__VA_ARGS__)

// �߳�Update����ʱ����,100ms
constexpr uint32_t kThreadUpdateInterval = 1000 * 1000 * 100;

constexpr uint16_t kOneByteBit = 8;

#define EVEREST_INLINE inline

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

NAMESPACE_EVEREST_BEGIN

using ThreadIdType = std::size_t;

using ServiceIdType = uint16_t;

NAMESPACE_EVEREST_END

#endif
