#pragma once

namespace cai::threading
{
    typedef std::function<void(const TResult)> successFn;
    typedef std::function<void(const std::exception&)> errorFn;
    typedef std::function<TResult(std::function<void(TResult)> progress, Args&& ... args)> workFn;
}
