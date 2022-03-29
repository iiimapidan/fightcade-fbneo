#ifndef _COMMON_FUNC_MACRO_H_
#define _COMMON_FUNC_MACRO_H_

#include <string>
#include <functional>


#define NAMESPACE_BEGIN(name) namespace name {
#define NAMESPACE_END }


#ifndef ASSERT
#define ASSERT(f)	ATLASSERT(f)
#endif
#ifndef VERIFY
#define VERIFY(f)	ATLVERIFY(f)
#endif

// ÔÝÊ±Ö»Ð´3¸ö
#define CALLBACK_G0(__selector__, ...) std::bind(&__selector__, ##__VA_ARGS__)
#define CALLBACK_G1(__selector__, ...) std::bind(&__selector__, std::placeholders::_1, ##__VA_ARGS__)
#define CALLBACK_G2(__selector__, ...) std::bind(&__selector__, std::placeholders::_1, std::placeholders::_2, ##__VA_ARGS__)
#define CALLBACK_G3(__selector__, ...) std::bind(&__selector__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, ##__VA_ARGS__)
#define CALLBACK_G4(__selector__, ...) std::bind(&__selector__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, ##__VA_ARGS__)
#define CALLBACK_G5(__selector__, ...) std::bind(&__selector__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, ##__VA_ARGS__)
#define CALLBACK_G6(__selector__, ...) std::bind(&__selector__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, ##__VA_ARGS__)

#define CALLBACK_0(__selector__,__target__, ...) std::bind(&__selector__,__target__, ##__VA_ARGS__)
#define CALLBACK_1(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, ##__VA_ARGS__)
#define CALLBACK_2(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, ##__VA_ARGS__)
#define CALLBACK_3(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, ##__VA_ARGS__)
#define CALLBACK_4(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, ##__VA_ARGS__)
#define CALLBACK_5(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, ##__VA_ARGS__)
#define CALLBACK_6(__selector__,__target__, ...) std::bind(&__selector__,__target__, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, ##__VA_ARGS__)

#endif
