#ifndef DEFINE_H_
#define DEFINE_H_

#define NAMESPACE_BEGIN namespace everest{
#define NAMESPACE_END }

// 线程Update更新时间间隔,100ms
#define THREAD_UPDATE_INTERVAL (1000*1000*100)

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

#endif
