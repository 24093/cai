#pragma once

#include <queue>
#include <functional>
#include <thread>
#include <future>
#include "threading_defs.h"

namespace chai::threading
{
    /* consumer */
    template<typename TProduct>
    class consumer
    {
    public:
        typedef std::function<void(const std::exception&)> errorFn;
                
        consumer(errorFn onError = [](const std::exception&) {}, size_t queueLimit = std::numeric_limits<int>::max())
            : _terminated(true), _queueLimit(queueLimit), _errorCallback(onError), _workerThread() 
        {             
        }

        virtual ~consumer()
        {
            stop();
            if (_workerThread != nullptr)
                _workerThread->join();

            const auto workerThread = _workerThread;
            _workerThread = nullptr;
            delete workerThread;
        }

        consumer(const consumer& other) noexcept = delete;

        consumer(const consumer&& other) noexcept = delete;

        consumer& operator=(const consumer& other) noexcept = delete;

        consumer& operator=(consumer&& other) noexcept = delete;

        void enqueue(const std::shared_ptr<TProduct> product)
        {
            std::unique_lock<std::mutex> productsLock(_productsMutex);

            if (_products.size() < _queueLimit)
            {
                _products.emplace(product);
                productsLock.unlock();
                _waitEmpty.notify_all();
            }
        }

        bool running() const
        {
            return !_terminated;
        }

        void run()
        {
            _terminated = false;

            auto task = std::packaged_task<void()>(
                [&, this]()
                {
                    while (!_terminated)
                    {
                        std::shared_ptr<TProduct> product;

                        {
                            std::unique_lock<std::mutex> productsLock(_productsMutex);

                            _waitEmpty.wait(productsLock, 
                                [&, this]()
                                {
                                    return !_products.empty() || _terminated;
                                });

                            if (_terminated)
                                break;

                            product = _products.back();
                            _products.pop();
                        }

                        try
                        {
                            consume(product);
                        }
                        catch (std::exception& ex)
                        {
                            _errorCallback(ex);
                        }
                        catch (...)
                        {
                            _errorCallback(std::exception());
                        }
                    }
                });

            _workerThread = new std::thread(std::move(task));
        }

        void stop()
        {
            _terminated = true;
            _waitEmpty.notify_all();
        }

        virtual void consume(std::shared_ptr<TProduct> product) = 0;

    private:
        std::atomic_bool _terminated;
        size_t _queueLimit;
        std::mutex _productsMutex;
        std::queue<std::shared_ptr<TProduct>> _products;
        std::condition_variable _waitEmpty;
        const std::function<void(const std::exception&)> _errorCallback;
        std::thread* _workerThread;
    };

    /* producer */
    template<typename TProduct>
    class producer
    {
    public:
        typedef std::function<void(const std::exception&)> errorFn;
        
        producer(errorFn onError = [](const std::exception&) { })
            : _terminated(true), _errorCallback(onError)
        {
        }

        virtual ~producer()
        {
            stop();

            if (_workerThread != nullptr)
            {
                _workerThread->join();
                const auto workerThread = _workerThread;
                _workerThread = nullptr;
                delete workerThread;
            }
        }

        producer(const producer& other) noexcept = delete;

        producer(const producer&& other) noexcept = delete;

        producer& operator=(const producer& other) noexcept = delete;

        producer& operator=(producer&& other) noexcept = delete;

        bool running() const
        {
            return !_terminated;
        }

        void run() noexcept
        {
            if (running())
                return;

            _terminated = false;

            _workerThread = new std::thread(
                [=]()
                {
                    while (!_terminated)
                    {
                        try
                        {
                            std::shared_ptr<TProduct> product;

                            if (produce(product))
                            {
                                for (auto& consumer : _consumers)
                                {
                                    if (consumer->running())
                                    {
                                        consumer->enqueue(product);
                                    }
                                }
                            }
                        }
                        catch (const std::exception& ex)
                        {
                            _errorCallback(ex);
                        }
                        catch (...)
                        {
                            _errorCallback(std::exception());
                        }
                    }
                });
        }

        void stop()
        {
            _terminated = true;
        }

        void registerConsumer(std::shared_ptr<Consumer<TProduct>> consumer)
        {
            _consumers.emplace_back(consumer);
        }

    protected:
        virtual bool produce(std::shared_ptr<TProduct>& product) = 0;

    private:
        std::atomic_bool _terminated;
        std::vector<std::shared_ptr<Consumer<TProduct>>> _consumers;
        const std::function<void(const std::exception&)> _errorCallback;
        std::thread* _workerThread;
    };
}
