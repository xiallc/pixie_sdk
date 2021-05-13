/*----------------------------------------------------------------------
* Copyright (c) 2021, XIA LLC
* All rights reserved.
*
* Redistribution and use in source and binary forms,
* with or without modification, are permitted provided
* that the following conditions are met:
*
*   * Redistributions of source code must retain the above
*     copyright notice, this list of conditions and the
*     following disclaimer.
*   * Redistributions in binary form must reproduce the
*     above copyright notice, this list of conditions and the
*     following disclaimer in the documentation and/or other
*     materials provided with the distribution.
*   * Neither the name of XIA LLC nor the names of its
*     contributors may be used to endorse or promote
*     products derived from this software without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*----------------------------------------------------------------------*/
/// @file pixie_buffer.cpp
/// @brief
/// @author Chris Johns
/// @date May 5, 2021

#include <cstring>
#include <iostream>
#include <iomanip>

#include <pixie_buffer.hpp>
#include <pixie_error.hpp>
#include <pixie_log.hpp>

namespace xia
{
namespace buffer
{
struct pool::releaser {
    pool& pool_;
    releaser(pool& pool_);
    void operator()(buffer* buf) const;
};

pool::releaser::releaser(pool& pool__)
    : pool_(pool__)
{
}

void pool::releaser::operator()(buffer* buf) const
{
    pool_.release(buf);
}

pool::pool()
    : number(0),
      size(0),
      count_(0)
{
}

pool::~pool()
{
    try {
        destroy();
    } catch (...) {
        /* any error will be logged */
    }
}

void
pool::create(const size_t number_, const size_t size_)
{
    log(log::info) << "pool create: num=" << number_
                   << " size=" << size_;
    lock_guard guard(lock);
    if (number != 0) {
        throw error(error::code::buffer_pool_not_empty,
                    "pool is already created");
    }
    number = number_;
    size = size_;
    for (size_t n = 0; n < number; ++n) {
        buffer_ptr buf = new buffer;
        buf->reserve(size);
        buffers.push_front(buf);
    }
    count_ = number;
}

void
pool::destroy()
{
    lock_guard guard(lock);
    if (number > 0) {
        log(log::info) << "pool destroy";
        if (count_.load() != number) {
            throw error(error::code::buffer_pool_busy,
                        "pool destroy made while busy");
        }
        while (!buffers.empty()) {
            buffer_ptr buf = buffers.front();
            delete buf;
            buffers.pop_front();
        }
        number = 0;
        size = 0;
        count_ = 0;
    }
}

handle
pool::request()
{
    lock_guard guard(lock);
    if (empty()) {
        throw error(error::code::buffer_pool_empty,
                    "no buffers avaliable");
    }
    count_--;
    buffer_ptr buf = buffers.front();
    buffers.pop_front();
    return handle(buf, releaser(*this));
}

void
pool::release(buffer_ptr buf)
{
    buf->clear();
    lock_guard guard(lock);
    buffers.push_front(buf);
    count_++;
}

void
pool::output(std::ostream& out)
{
    out << "count=" << count_.load()
        << " num=" << number
        << " size=" << size;
}

queue::queue()
    : size_(0),
      count_(0)
{
}

void
queue::push(handle buf)
{
    if (buf->size() > 0) {
        lock_guard guard(lock);
        buffers.push_back(buf);
        size_ += buf->size();
        ++count_;
    }
}

handle
queue::pop()
{
    lock_guard guard(lock);
    handle buf = buffers.front();
    buffers.pop_front();
    size_ -= buf->size();
    --count_;
    return buf;
}

void
queue::copy(buffer& to)
{
    lock_guard guard(lock);
    /*
     * If the `to` size is 0 copy all the available data
     */
    size_t count = to.size();
    if (count == 0) {
        count = size_.load();
        to.resize(count);
    }
    copy_unprotected(to.data(), count);
}

void
queue::copy(buffer_value_ptr to, const size_t count)
{
    lock_guard guard(lock);
    copy_unprotected(to, count);
}

void
queue::copy_unprotected(buffer_value_ptr to, const size_t count)
{
    if (count > size_.load()) {
        throw error(error::code::buffer_pool_not_enough,
                    "not enough data in queue");
    }
    auto to_move = count;
    auto from_bi = buffers.begin();
    while (to_move > 0 && from_bi != buffers.end()) {
        auto from = *from_bi;
        if (to_move >= from->size()) {
            std::memcpy(to, from->data(), from->size() * sizeof(*to));
            to += from->size();
            to_move -= from->size();
            size_ -= from->size();
            count_--;
            from->clear();
        } else {
            std::memcpy(to, from->data(), to_move);
            std::move(from->begin() + to_move, from->end(), from->begin());
            from->resize(from->size() - to_move);
            to += from->size();
            to_move = 0;
            size_ -= to_move;
        }
        from_bi++;
    }
    if (from_bi != buffers.begin()) {
        buffers.erase(buffers.begin(), from_bi);
    }
}

void
queue::compact()
{
    lock_guard guard(lock);

    /*
     * Erasing elements from the queue's container invalidates the
     * iterators. After moving one or more buffers into another buffer and
     * removing them we start again so we have valid iterators.
     */
    bool rerun = true;
    while (rerun) {
        rerun = false;
        auto to_bi = buffers.begin();
        while (to_bi != buffers.end()) {
            auto& to = *to_bi;
            if (to->capacity() - to->size() > 0) {
                auto erase_from = buffers.end();
                auto erase_to = buffers.end();
                auto to_move = to->capacity() - to->size();
                auto from_bi = to_bi + 1;
                while (to_move > 0 && from_bi != buffers.end()) {
                    auto from = *from_bi;
                    if (to_move >= from->size()) {
                        to->insert(to->end(), from->begin(), from->end());
                        to_move -= from->size();
                        if (erase_from == buffers.end()) {
                            erase_from = from_bi;
                        }
                        from_bi++;
                        erase_to = from_bi;
                        count_--;
                        from->clear();
                    } else {
                        to->insert(to->end(),
                                   from->begin(), from->begin() + to_move);
                        from->erase(from->begin(), from->begin() + to_move);
                        to_move = 0;
                    }
                }
                if (erase_from != buffers.end()) {
                    buffers.erase(erase_from, erase_to);
                    rerun = true;
                    break;
                }
            }
            to_bi++;
        }
    }
}

void
queue::flush()
{
    lock_guard guard(lock);
    buffers.clear();
}

void
queue::output(std::ostream& out)
{
    out << "count=" << count()
        << " size=" << size();
}
}
}

std::ostream&
operator<<(std::ostream& out, xia::buffer::pool& pool)
{
    pool.output(out);
    return out;
}

std::ostream&
operator<<(std::ostream& out, xia::buffer::queue& queue)
{
    queue.output(out);
    return out;
}
