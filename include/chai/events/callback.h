#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>

#endif

namespace kruss::infrastructure
{
    template<typename... Args>
    class callback
    {
    public:
        callback()
            : _currentId(0) 
        { 
        }

    public:
        callback& operator=(callback const& other) { }

    public:
        int attach(std::function<void(Args...)> const& callback) const
        {
            std::lock_guard<std::mutex> lock(_mtx);

            _callbacks.insert(std::make_pair(++_currentId, callback));
            return _currentId;
        }

        template<typename T>
        int attach(T* instance, void (T::*function)(Args...))
        {
            return attach([=](Args... args)
            {
                (instance->*function)(args...);
            });
        }

        template<typename T>
        int attach(T* instance, void(T::*function)(Args...) const)
        {
            return attach([=](Args... args)
            {
                (instance->*function)(args...);
            });
        }

        template<typename T>
        int attach(std::shared_ptr<T> instance, void (T::*function)(Args...))
        {
            return attach([=](Args... args)
                          {
                              (instance.get()->*function)(args...);
                          });
        }

        template<typename T>
        int attach(std::shared_ptr<T> instance, void(T::*function)(Args...) const)
        {
            return attach([=](Args... args)
                          {
                              (instance.get()->*function)(args...);
                          });
        }

        void detach(int id) const
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _callbacks.erase(id);
        }

        void detachAll()
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _callbacks.clear();
        }

        void invoke(Args... p) const
        {
            std::lock_guard<std::mutex> lock(_mtx);

            for (auto it : _callbacks)
            {
                it.second(p...);
            }
        }

    private:
        mutable std::map<int, std::function<void(Args...)>> _callbacks;
        mutable int _currentId;
        mutable std::mutex _mtx;
    };
}
