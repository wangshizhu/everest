#ifndef DEFINE_H_
#define DEFINE_H_

#define NAMESPACE_BEGIN namespace everest{
#define NAMESPACE_END }

// �߳�Update����ʱ����,100ms
#define THREAD_UPDATE_INTERVAL (1000*1000*100)

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
