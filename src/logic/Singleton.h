#ifndef _COMMON_FUNC_SINGLETON_H_
#define _COMMON_FUNC_SINGLETON_H_

template <typename T>
class SingletonClass
{
public:
    static T *GetInstance()
    {
        if (nullptr == s_inst)
        {
            s_inst = new (T)();
        }

        return s_inst;
    }

protected:
    static T* volatile s_inst;
};

template <typename T>
T* volatile SingletonClass<T>::s_inst = nullptr;

#endif

// Singleton���̰߳�ȫ���������������õ���ȷ�������߳���ִ�й�Singleton()
#define Singleton(t) SingletonClass<t>::GetInstance()
#define Ensuer_Only_One_Instance(t) static auto instance_##t = Singleton(t)
