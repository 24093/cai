#pragma once

#include <future>
#include <chrono>
#include <thread>
#include <utility>
#include "threading_defs.h"

namespace chai::threading
{    
    /* background worker */
    template<typename TResult, typename... Args>
    class bgwrk
    {
    public:
        typedef std::function<void(const TResult)> successFn;
        typedef std::function<void(const std::exception&)> errorFn;            

        bgwrk(successFn onSuccess,
            errorFn onError = [](std::exception&) { },
            successFn onProgress = [](TResult) { }) noexcept
            : _successCallback(onSuccess), _errorCallbackonError), _progressCallback(onProgress) { }

        virtual ~bgwrk() noexcept = default;

        bgwrk(const bgwrk& other) noexcept = delete;

        bgwrk(const bgwrk&& other) noexcept = delete;

        bgwrk& operator=(const bgwrk& other) noexcept = delete;

        bgwrk& operator=(bgwrk&& other) noexcept = delete;

        void run(Args&& ... args) noexcept
        {
			auto task = std::packaged_task<void()>(
				[&, this]()
				{
					try
					{
                        _successCallback(work(std::forward<Args&&>(args)...));
					}
					catch (std::exception& ex)
					{
                        _errorCallback(ex);
					}
					catch (...)
					{
                        _errorCallback(std::exception());
					}

				});

            std::thread(std::move(task)).detach();
        }

    protected:
        virtual TResult work(Args&& ... args) = 0;

        void progress(const TResult& intermediateResult) 
        { 
            _progressCallback(intermediateResult);
        }

    private:
        const std::function<void(const TResult)> _successCallback;
        const std::function<void(const std::exception&)> _errorCallback;
        const std::function<void(const TResult)> _progressCallback;
    };

    /* generic background worker */
    template<typename TResult, typename... Args>
    class gbgwrk final : public bgwrk<TResult, Args...>
    {
    public:    
        typedef std::function<void(const TResult)> successFn;
        typedef std::function<void(const std::exception&)> errorFn;
        typedef std::function<TResult(std::function<void(TResult)> progress, Args&& ... args)> workFn;
            
        gbgwrk(workFn workFunction,
            successFn onSuccess = [](TResult) { },
            errorFn onError = [](const std::exception&) { },
            successFn onProgress = [](TResult) { })
            : BackgroundWorker<TResult, Args...>(onSuccess, onError), _workFunction(workFunction), _progressFunction(onProgress) { }

    protected:
        TResult work(Args&& ... args) override
        {
            return _workFunction(_progressFunction, std::forward<Args&&>(args)...);
        }

    private:
        std::function<TResult(std::function<void(TResult)> progress, Args&& ... args)> _workFunction;
        std::function<void(const TResult)> _progressFunction;
    };
}


