// emhash5::HashMap for C++11
// version 1.5.8
// https://github.com/ktprime/ktprime/blob/master/hash_table5.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

#pragma once

#include <cstring>
#include <string>
#include <cmath>
#include <cstdlib>
#include <type_traits>
#include <cassert>
#include <utility>
#include <cstdint>
#include <functional>
#include <iterator>
#include <algorithm>

#ifdef __has_include
    #if __has_include("wyhash.h")
    #include "wyhash.h"
    #endif
    #if __has_include("ahash.h")
    #include "ahash.h"
    #endif
#elif AHASH || AHASH_AHASH_H
    #include "ahash.h"
#elif EMH_WY_HASH
    #include "wyhash.h"
#endif

#ifdef EMH_KEY
    #undef  EMH_KEY
    #undef  EMH_VAL
    #undef  EMH_PKV
    #undef  EMH_BUCKET
    #undef  EMH_NEW
    #undef  EMH_EMPTY
    #undef  EMH_PREVET
#endif

// likely/unlikely
#if (__GNUC__ >= 4 || __clang__)
#    define EMH_LIKELY(condition)   __builtin_expect(condition, 1)
#    define EMH_UNLIKELY(condition) __builtin_expect(condition, 0)
#else
#    define EMH_LIKELY(condition)   condition
#    define EMH_UNLIKELY(condition) condition
#endif

#ifndef EMH_BUCKET_INDEX
    #define EMH_BUCKET_INDEX 1
#endif
#if EMH_CACHE_LINE_SIZE < 32
    #define EMH_CACHE_LINE_SIZE 64
#endif

#ifndef EMH_DEFAULT_LOAD_FACTOR
#define EMH_DEFAULT_LOAD_FACTOR 0.88f
#endif

#if EMH_BUCKET_INDEX == 0
    #define EMH_KEY(p,n)     p[n].second.first
    #define EMH_VAL(p,n)     p[n].second.second
    #define EMH_BUCKET(p,n)  p[n].first
    #define EMH_PKV(p,n)     p[n].second
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(bucket, value_type(key, value)); _num_filled ++
#elif EMH_BUCKET_INDEX == 2
    #define EMH_KEY(p,n)     p[n].first.first
    #define EMH_VAL(p,n)     p[n].first.second
    #define EMH_BUCKET(p,n)  p[n].second
    #define EMH_PREVET(p,n)  *(uint32_t*)(&p[n].first.first)
    #define EMH_PKV(p,n)     p[n].first
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(value_type(key, value), bucket); _num_filled ++
#else
    #define EMH_KEY(p,n)     p[n].first
    #define EMH_VAL(p,n)     p[n].second
    #define EMH_BUCKET(p,n)  p[n].bucket
    #define EMH_PREVET(p,n)  *(uint32_t*)(&p[n].first)
    #define EMH_PKV(p,n)     p[n]
    #define EMH_NEW(key, value, bucket) new(_pairs + bucket) PairT(key, value, bucket); _num_filled ++
#endif

#define EMH_EMPTY(p, b) (0 > (int)EMH_BUCKET(p, b))

namespace emhash6 {

const constexpr uint32_t INACTIVE = 0xFAAAAAAA;

template <typename First, typename Second>
struct entry {
    entry(const First& key, const Second& value, uint32_t ibucket)
        :second(value),first(key)
    {
        bucket = ibucket;
    }

    entry(First&& key, Second&& value, uint32_t ibucket)
        :second(std::move(value)), first(std::move(key))
    {
        bucket = ibucket;
    }

    template<typename K, typename V>
    entry(K&& key, V&& value, uint32_t ibucket)
        :second(std::forward<V>(value)), first(std::forward<K>(key))
    {
        bucket = ibucket;
    }

    entry(const std::pair<First,Second>& pairT)
        :second(pairT.second),first(pairT.first)
    {
        bucket = INACTIVE;
    }

    entry(std::pair<First, Second>&& pairT)
        :second(std::move(pairT.second)), first(std::move(pairT.first))
    {
        bucket = INACTIVE;
    }

    entry(const entry& pairT)
        :second(pairT.second),first(pairT.first)
    {
        bucket = pairT.bucket;
    }

    entry(entry&& pairT) noexcept
        :second(std::move(pairT.second)),first(std::move(pairT.first))
    {
        bucket = pairT.bucket;
    }

    entry& operator = (entry&& pairT)
    {
        second = std::move(pairT.second);
        bucket = pairT.bucket;
        first = std::move(pairT.first);
        return *this;
    }

    entry& operator = (const entry& o)
    {
        second = o.second;
        bucket = o.bucket;
        first  = o.first;
        return *this;
    }

    void swap(entry<First, Second>& o)
    {
        std::swap(second, o.second);
        std::swap(first, o.first);
    }

    Second second;//int
    uint32_t bucket;
    First first; //long
};// __attribute__ ((packed));

/// A cache-friendly hash table with open addressing, linear/qua probing and power-of-two capacity
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap
{
public:
    typedef HashMap<KeyT, ValueT, HashT, EqT> htype;
    typedef std::pair<KeyT,ValueT>            value_type;

#if EMH_BUCKET_INDEX == 0
    typedef value_type                        value_pair;
    typedef std::pair<uint32_t, value_type>   PairT;
#elif EMH_BUCKET_INDEX == 2
    typedef value_type                        value_pair;
    typedef std::pair<value_type, uint32_t>   PairT;
#else
    typedef entry<KeyT, ValueT>               value_pair;
    typedef entry<KeyT, ValueT>               PairT;
#endif

public:
    typedef KeyT   key_type;
    typedef ValueT val_type;
    typedef ValueT mapped_type;

    typedef uint32_t     size_type;
    typedef PairT&       reference;
    typedef const PairT& const_reference;

    class iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef value_pair*               pointer;
        typedef value_pair&               reference;

        //iterator() { }
        iterator(const htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { }

        iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const
        {
            return _map->EMH_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PKV(_pairs, _bucket));
        }

        bool operator==(const iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

        size_type bucket() const
        {
            return _bucket;
        }

    private:
        void goto_next_element()
        {
            while ((int)_map->EMH_BUCKET(_pairs, ++_bucket) < 0);
        }

    public:
        const htype* _map;
        uint32_t _bucket;
    };

    class const_iterator
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::ptrdiff_t            difference_type;
        typedef value_pair                value_type;

        typedef const value_pair*         pointer;
        typedef const value_pair&         reference;

        //const_iterator() { }
        const_iterator(const iterator& proto) : _map(proto._map), _bucket(proto._bucket) { }
        const_iterator(const htype* hash_map, uint32_t bucket) : _map(hash_map), _bucket(bucket) { }

        const_iterator& operator++()
        {
            goto_next_element();
            return *this;
        }

        const_iterator operator++(int)
        {
            auto old_index = _bucket;
            goto_next_element();
            return {_map, old_index};
        }

        reference operator*() const
        {
            return _map->EMH_PKV(_pairs, _bucket);
        }

        pointer operator->() const
        {
            return &(_map->EMH_PKV(_pairs, _bucket));
        }

        bool operator==(const const_iterator& rhs) const
        {
            return _bucket == rhs._bucket;
        }

        bool operator!=(const const_iterator& rhs) const
        {
            return _bucket != rhs._bucket;
        }

        size_type bucket() const
        {
            return _bucket;
        }

    private:
        void goto_next_element()
        {
            while ((int)_map->EMH_BUCKET(_pairs, ++_bucket) < 0);
        }

    public:
        const htype* _map;
        uint32_t _bucket;
    };

    void init(uint32_t bucket, float lf = EMH_DEFAULT_LOAD_FACTOR)
    {
        _pairs = nullptr;
        _mask  = _num_buckets = 0;
        _num_filled = 0;
        _ehead = 0;
        max_load_factor(lf);
        reserve(bucket);
    }

    HashMap(uint32_t bucket = 2, float lf = EMH_DEFAULT_LOAD_FACTOR)
    {
        init(bucket, lf);
    }

    HashMap(const HashMap& other)
    {
        _pairs = alloc_bucket(other._num_buckets);
        clone(other);
    }

    HashMap(HashMap&& other)
    {
        init(0);
        *this = std::move(other);
    }

    HashMap(std::initializer_list<value_type> ilist)
    {
        init((uint32_t)ilist.size());
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other)
            return *this;

        if (is_triviall_destructable())
            clearkv();

        if (_num_buckets < other._num_buckets || _num_buckets > 2 * other._num_buckets) {
            free(_pairs);
            _pairs = alloc_bucket(other._num_buckets);
        }

        clone(other);
        return *this;
    }

    HashMap& operator=(HashMap&& other)
    {
        if (this != &other) {
            swap(other);
            other.clear();
        }
        return *this;
    }

    ~HashMap()
    {
        if (is_triviall_destructable())
            clearkv();
        free(_pairs);
    }

    void clone(const HashMap& other)
    {
        _hasher      = other._hasher;
//        _eq          = other._eq;
        _num_buckets = other._num_buckets;
        _num_filled  = other._num_filled;
        _loadlf      = other._loadlf;
        _last        = other._last;
        _mask        = other._mask;
        _ehead       = other._ehead;

        auto opairs  = other._pairs;

        if (is_copy_trivially())
            memcpy(_pairs, opairs, (_num_buckets + 2) * sizeof(PairT));
        else {
            for (uint32_t bucket = 0; bucket < _num_buckets; bucket++) {
                auto next_bucket = EMH_BUCKET(_pairs, bucket) = EMH_BUCKET(opairs, bucket);
                if ((int)next_bucket >= 0)
                    new(_pairs + bucket) PairT(opairs[bucket]);
#if EMH_HIGH_LOAD
                else if (next_bucket != INACTIVE)
                    EMH_PREVET(_pairs, bucket) = EMH_PREVET(opairs, bucket);
#endif
            }
            memcpy(_pairs + _num_buckets, opairs + _num_buckets, sizeof(PairT) * 2);
        }
    }

    void swap(HashMap& other)
    {
        std::swap(_hasher, other._hasher);
        //      std::swap(_eq, other._eq);
        std::swap(_pairs, other._pairs);
        std::swap(_num_buckets, other._num_buckets);
        std::swap(_num_filled, other._num_filled);
        std::swap(_mask, other._mask);
        std::swap(_loadlf, other._loadlf);
        std::swap(_last, other._last);
        std::swap(_ehead, other._ehead);
    }

    // -------------------------------------------------------------

    iterator begin()
    {
        if (_num_filled == 0)
            return end();

        uint32_t bucket = 0;
        while (EMH_EMPTY(_pairs, bucket)) {
            ++bucket;
        }
        return {this, bucket};
    }

    iterator last()
    {
        if (_num_filled == 0)
            return end();

        uint32_t bucket = _num_buckets - 1;
        while (EMH_EMPTY(_pairs, bucket)) bucket--;
        return {this, bucket};
    }

    const_iterator cbegin() const
    {
        if (_num_filled == 0)
            return end();

        uint32_t bucket = 0;
        while (EMH_EMPTY(_pairs, bucket)) {
            ++bucket;
        }
        return {this, bucket};
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    iterator end()
    {
        return {this, _num_buckets};
    }

    const_iterator cend() const
    {
        return {this, _num_buckets};
    }

    const_iterator end() const
    {
        return cend();
    }

    size_type size() const
    {
        return _num_filled;
    }

    bool empty() const
    {
        return _num_filled == 0;
    }

    // Returns the number of buckets.
    size_type bucket_count() const
    {
        return _num_buckets;
    }

    /// Returns average number of elements per bucket.
    float load_factor() const
    {
        return static_cast<float>(_num_filled) / (_mask + 1);
    }

    HashT& hash_function() const
    {
        return _hasher;
    }

    EqT& key_eq() const
    {
        return _eq;
    }

    constexpr float max_load_factor() const
    {
        return (1 << 27) / (float)_loadlf;
    }

    void max_load_factor(float value)
    {
        if (value < 0.999f && value > 0.2f)
            _loadlf = (uint32_t)((1 << 27) / value);
    }

    constexpr size_type max_size() const
    {
        return (1ull << (sizeof(size_type) * 8 - 2));
    }

    constexpr size_type max_bucket_count() const
    {
        return max_size();
    }

#ifdef EMH_STATIS
    //Returns the bucket number where the element with key k is located.
    size_type bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return 0;
        else if (bucket == next_bucket)
            return bucket + 1;

        return hash_main(bucket) + 1;
    }

    //Returns the number of elements in bucket n.
    size_type bucket_size(const uint32_t bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return 0;

        next_bucket = hash_main(bucket);
        uint32_t ibucket_size = 1;

        //iterator each item in current main bucket
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket) {
                break;
            }
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    size_type get_main_bucket(const uint32_t bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return INACTIVE;

        return hash_main(bucket);
    }

    size_type get_diss(uint32_t bucket, uint32_t next_bucket, const uint32_t slots) const
    {
        auto pbucket = reinterpret_cast<uint64_t>(&_pairs[bucket]);
        auto pnext   = reinterpret_cast<uint64_t>(&_pairs[next_bucket]);
        if (pbucket / EMH_CACHE_LINE_SIZE == pnext / EMH_CACHE_LINE_SIZE)
            return 0;
        uint32_t diff = pbucket > pnext ? (pbucket - pnext) : (pnext - pbucket);
        if (diff / EMH_CACHE_LINE_SIZE < slots - 1)
            return diff / EMH_CACHE_LINE_SIZE + 1;
        return slots - 1;
    }

    int get_bucket_info(const uint32_t bucket, uint32_t steps[], const uint32_t slots) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0)
            return -1;

        const auto main_bucket = hash_main(bucket);
        if (next_bucket == main_bucket)
            return 1;
        else if (main_bucket != bucket)
            return 0;

        steps[get_diss(bucket, next_bucket, slots)] ++;
        uint32_t ibucket_size = 2;
        //find a new empty and linked it to tail
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;

            steps[get_diss(nbucket, next_bucket, slots)] ++;
            ibucket_size ++;
            next_bucket = nbucket;
        }
        return ibucket_size;
    }

    void dump_statics() const
    {
        const int slots = 128;
        uint32_t buckets[slots + 1] = {0};
        uint32_t steps[slots + 1]   = {0};
        for (uint32_t bucket = 0; bucket < _num_buckets; ++bucket) {
            auto bsize = get_bucket_info(bucket, steps, slots);
            if (bsize > 0)
                buckets[bsize] ++;
        }

        uint32_t sumb = 0, collision = 0, sumc = 0, finds = 0, sumn = 0;
        puts("============== buckets size ration =========");
        for (uint32_t i = 0; i < sizeof(buckets) / sizeof(buckets[0]); i++) {
            const auto bucketsi = buckets[i];
            if (bucketsi == 0)
                continue;
            sumb += bucketsi;
            sumn += bucketsi * i;
            collision += bucketsi * (i - 1);
            finds += bucketsi * i * (i + 1) / 2;
            printf("  %2u  %8u  %2.2lf|  %.2lf\n", i, bucketsi, bucketsi * 100.0 * i / _num_filled, sumn * 100.0 / _num_filled);
        }

        puts("========== collision miss ration ===========");
        for (uint32_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            sumc += steps[i];
            if (steps[i] <= 2)
                continue;
            printf("  %2u  %8u  %.2lf  %.2lf\n", i, steps[i], steps[i] * 100.0 / collision, sumc * 100.0 / collision);
        }

        if (sumb == 0)  return;
        printf("    _num_filled/bucket_size/packed collision/cache_miss/hit_find = %u/%.2lf/%d/ %.2lf%%/%.2lf%%/%.2lf\n",
                _num_filled, _num_filled * 1.0 / sumb, int(sizeof(PairT)), (collision * 100.0 / _num_filled), (collision - steps[0]) * 100.0 / _num_filled, finds * 1.0 / _num_filled);
        assert(sumc == collision);
        assert(sumn == _num_filled);
        puts("============== buckets size end =============");
    }
#endif

    // ------------------------------------------------------------

    iterator find(const KeyT& key) noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    const_iterator find(const KeyT& key) const noexcept
    {
        return {this, find_filled_bucket(key)};
    }

    ValueT& at(const KeyT& key)
    {
        const auto bucket = find_filled_bucket(key);
        //throw
        return EMH_VAL(_pairs, bucket);
    }

    const ValueT& at(const KeyT& key) const
    {
        const auto bucket = find_filled_bucket(key);
        //throw
        return EMH_VAL(_pairs, bucket);
    }

    bool contains(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) != _num_buckets;
    }

    size_type count(const KeyT& key) const noexcept
    {
        return find_filled_bucket(key) == _num_buckets ? 0 : 1;
    }

    std::pair<iterator, iterator> equal_range(const KeyT& key)
    {
        const auto found = find(key);
        if (found.second == _num_buckets)
            return { found, found };
        else
            return { found, std::next(found) };
    }

    template<typename V>
    V try_rget(const KeyT key, V val) const
    {
        const auto bucket = find_filled_bucket(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            return (EMH_VAL(_pairs, bucket)).get();
        }
        return nullptr;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    bool try_get(const KeyT& key, ValueT& val) const
    {
        const auto bucket = find_filled_bucket(key);
        const auto found = bucket != _num_buckets;
        if (found) {
            val = EMH_VAL(_pairs, bucket);
        }
        return found;
    }

    /// Returns the matching ValueT or nullptr if k isn't found.
    ValueT* try_get(const KeyT& key) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket != _num_buckets ? &EMH_VAL(_pairs, bucket) : nullptr;
    }

    /// Const version of the above
    ValueT* try_get(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket != _num_buckets ? &EMH_VAL(_pairs, bucket) : nullptr;
    }

    /// set value if key exist
    bool try_set(const KeyT& key, const ValueT& value) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return false;

        EMH_VAL(_pairs, bucket) = value;
        return true;
    }

    /// set value if key exist
    bool try_set(const KeyT& key, ValueT&& value) noexcept
    {
        const auto bucket = find_filled_bucket(key);
        if (bucket == _num_buckets)
            return false;

        EMH_VAL(_pairs, bucket) = std::move(value);
        return true;
    }

    /// Convenience function.
    ValueT get_or_return_default(const KeyT& key) const noexcept
    {
        const auto bucket = find_filled_bucket(key);
        return bucket == _num_buckets ? ValueT() : EMH_VAL(_pairs, bucket);
    }

    // -----------------------------------------------------

    /// Returns a pair consisting of an iterator to the inserted element
    /// (or to the element that prevented the insertion)
    /// and a bool denoting whether the insertion took place.
    std::pair<iterator, bool> insert(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert(key, value);
    }

    std::pair<iterator, bool> insert(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert(std::move(key), std::move(value));
    }

    std::pair<iterator, bool> insert(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        return do_insert(key, std::move(value));
    }

    std::pair<iterator, bool> insert(KeyT&& key, const ValueT& value)
    {
        check_expand_need();
        return do_insert(std::move(key), value);
    }

    template<typename K, typename V>
    inline std::pair<iterator, bool> do_insert(K&& key, V&& value)
    {
        const auto bucket = find_or_allocate(key);
        const auto empty = EMH_EMPTY(_pairs, bucket);
        if (empty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket);
        }
        return { {this, bucket}, empty };
    }

    template<typename K, typename V>
    inline std::pair<iterator, bool> do_assign(K&& key, V&& value)
    {
        const auto bucket = find_or_allocate(key);
        const auto empty = EMH_EMPTY(_pairs, bucket);
        if (empty) {
            EMH_NEW(std::forward<K>(key), std::forward<V>(value), bucket);
        } else {
            EMH_VAL(_pairs, bucket) = std::move(value);
        }
        return { {this, bucket}, empty };
    }

    std::pair<iterator, bool> insert(const value_type& p)
    {
        check_expand_need();
        return do_insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(iterator it, const value_type& p)
    {
        check_expand_need();
        return do_insert(p.first, p.second);
    }

    std::pair<iterator, bool> insert(value_type && p)
    {
        check_expand_need();
        return do_insert(std::move(p.first), std::move(p.second));
    }

    void insert(std::initializer_list<value_type> ilist)
    {
        reserve(ilist.size() + _num_filled);
        for (auto begin = ilist.begin(); begin != ilist.end(); ++begin)
            do_insert(begin->first, begin->second);
    }

#if 0
    template <typename Iter>
    void insert(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            emplace(*begin);
        }
    }

    template <typename Iter>
    void insert2(Iter begin, Iter end)
    {
        Iter citbeg = begin;
        Iter citend = begin;
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            if (try_insert_mainbucket(begin->first, begin->second) < 0) {
                std::swap(*begin, *citend++);
            }
        }

        for (; citbeg != citend; ++citbeg)
            insert(*citbeg);
    }
#endif

    template <typename Iter>
    void insert_unique(Iter begin, Iter end)
    {
        reserve(std::distance(begin, end) + _num_filled);
        for (; begin != end; ++begin) {
            insert_unique(*begin);
        }
    }

    /// Same as above, but contains(key) MUST be false
    uint32_t insert_unique(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(key, value, bucket);
        return bucket;
    }

    uint32_t insert_unique(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        auto bucket = find_unique_bucket(key);
        EMH_NEW(std::move(key), std::move(value), bucket);
        return bucket;
    }

    uint32_t insert_unique(entry<KeyT, ValueT>&& pairT)
    {
        auto bucket = find_unique_bucket(pairT.first);
        EMH_NEW(std::move(pairT.first), std::move(pairT.second), bucket);
        return bucket;
    }

    inline uint32_t insert_unique(value_type && p)
    {
        return insert_unique(std::move(p.first), std::move(p.second));
    }

    inline uint32_t insert_unique(value_type& p)
    {
        return insert_unique(p.first, p.second);
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace(Args&&... args)
    {
        return insert(std::forward<Args>(args)...);
    }

    //no any optimize for position
    template <class... Args>
    iterator emplace_hint(const_iterator position, Args&&... args)
    {
        check_expand_need();
        return do_insert(std::forward<Args>(args)...).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& k, Args&&... args)
    {
        check_expand_need();
        return do_insert(k, std::forward<Args>(args)...);
    }

    template <class... Args>
    inline std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert_or_assign(const KeyT& key, ValueT&& value)
    {
        check_expand_need();
        return do_assign(key, std::move(value));
    }

    std::pair<iterator, bool> insert_or_assign(KeyT&& key, ValueT&& value)
    {
        check_expand_need();
        return do_assign(std::move(key), std::move(value));
    }

    /// Return the old value or ValueT() if it didn't exist.
    ValueT set_get(const KeyT& key, const ValueT& value)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);

        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(key, value, bucket);
            return ValueT();
        } else {
            ValueT old_value(value);
            std::swap(EMH_VAL(_pairs, bucket), old_value);
            return old_value;
        }
    }

    /// Like std::map<KeyT,ValueT>::operator[].
    ValueT& operator[](const KeyT& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (EMH_EMPTY(_pairs, bucket)) {
            /* Check if inserting a new value rather than overwriting an old entry */
            EMH_NEW(key, std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    ValueT& operator[](KeyT&& key)
    {
        check_expand_need();
        const auto bucket = find_or_allocate(key);
        if (EMH_EMPTY(_pairs, bucket)) {
            EMH_NEW(std::move(key), std::move(ValueT()), bucket);
        }

        return EMH_VAL(_pairs, bucket);
    }

    // -------------------------------------------------------
    /// return 0 if not erase
    size_type erase_node(const KeyT& key, const size_type slot)
    {
        if (slot < _num_buckets && _pairs[slot].second != INACTIVE && _pairs[slot].first == key) {
            erase_bucket(slot);
            return 1;
        }

        return erase(key);
    }

    /// Erase an element from the hash table.
    /// return 0 if element was not found
    size_type erase(const KeyT& key)
    {
        const auto bucket = erase_key(key);
        if ((int)bucket < 0)
            return 0;

        clear_bucket(bucket);
        return 1;
    }

    //iterator erase(const_iterator begin_it, const_iterator end_it)
    iterator erase(const_iterator cit)
    {
        const auto bucket = erase_bucket(cit._bucket);
        clear_bucket(bucket);

        iterator it(this, cit._bucket);
        //erase from main bucket, return main bucket as next
        return (bucket == it._bucket) ? ++it : it;
    }

    void _erase(const_iterator it)
    {
        const auto bucket = erase_bucket(it._bucket);
        clear_bucket(bucket);
    }

    static constexpr bool is_triviall_destructable()
    {
#if __cplusplus >= 201402L || _MSC_VER > 1600
        return !(std::is_trivially_destructible<KeyT>::value && std::is_trivially_destructible<ValueT>::value);
#else
        return !(std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    static constexpr bool is_copy_trivially()
    {
#if __cplusplus >= 201103L || _MSC_VER > 1600
        return (std::is_trivially_copyable<KeyT>::value && std::is_trivially_copyable<ValueT>::value);
#else
        return (std::is_pod<KeyT>::value && std::is_pod<ValueT>::value);
#endif
    }

    void clearkv()
    {
        for (uint32_t bucket = 0; _num_filled > 0; ++bucket) {
            if (!EMH_EMPTY(_pairs, bucket))
                clear_bucket(bucket, false);
        }
    }

    /// Remove all elements, keeping full capacity.
    void clear()
    {
#if EMH_HIGH_LOAD
        if (_ehead > 0)
            clear_empty();
        clearkv();
#else
        if (is_triviall_destructable() || sizeof(PairT) > EMH_CACHE_LINE_SIZE / 2 || _num_filled < _num_buckets / 8)
            clearkv();
        else
            memset((char*)_pairs, INACTIVE, sizeof(_pairs[0]) * _num_buckets);
#endif

        _last = _num_filled = 0;
    }

    void shrink_to_fit(const float min_factor = EMH_DEFAULT_LOAD_FACTOR / 4)
    {
        if (load_factor() < min_factor) //safe guard
            rehash(_num_filled);
    }

    /// Make room for this many elements
    bool reserve(uint64_t num_elems)
    {
#if EMH_HIGH_LOAD < 1000
        const auto required_buckets = (uint32_t)(num_elems * _loadlf >> 27);
        if (EMH_LIKELY(required_buckets < _mask))
            return false;

#else
        const auto required_buckets = (uint32_t)(num_elems + num_elems * 1 / 9);
        if (EMH_LIKELY(required_buckets < _mask))
            return false;

        else if (_num_buckets < 16 && _num_filled < _num_buckets)
            return false;

        else if (_num_buckets > EMH_HIGH_LOAD) {
            if (_ehead == 0) {
                set_empty();
                return false;
            } else if (/*_num_filled + 100 < _num_buckets && */EMH_BUCKET(_pairs, _ehead) != 0-_ehead) {
                return false;
            }
        }
#endif

#if EMH_STATIS
        if (_num_filled > 1'000'000) dump_statics();
#endif

        //assert(required_buckets < max_size());
        rehash(required_buckets + 2);
        return true;
    }

private:

    static inline PairT* alloc_bucket(uint32_t num_buckets)
    {
        auto* new_pairs = (char*)malloc((2 + num_buckets) * sizeof(PairT));
        return (PairT *)(new_pairs);
    }

#if EMH_HIGH_LOAD
    void set_empty()
    {
        auto prev = 0;
        for (int32_t bucket = 1; bucket < _num_buckets; ++bucket) {
            if (EMH_EMPTY(_pairs, bucket)) {
                if (prev != 0) {
                    EMH_PREVET(_pairs, bucket) = prev;
                    EMH_BUCKET(_pairs, prev) = -bucket;
                }
                else
                    _ehead = bucket;
                prev = bucket;
            }
        }

        EMH_PREVET(_pairs, _ehead) = prev;
        EMH_BUCKET(_pairs, prev) = 0-_ehead;
        _ehead = 0-EMH_BUCKET(_pairs, _ehead);
    }

    void clear_empty()
    {
        auto prev = EMH_PREVET(_pairs, _ehead);
        while (prev != _ehead) {
            EMH_BUCKET(_pairs, prev) = INACTIVE;
            prev = EMH_PREVET(_pairs, prev);
        }
        EMH_BUCKET(_pairs, _ehead) = INACTIVE;
        _ehead = 0;
    }

    //prev-ehead->next
    uint32_t pop_empty(const uint32_t bucket)
    {
        const auto prev_bucket = EMH_PREVET(_pairs, bucket);
        int next_bucket = (int)(0-EMH_BUCKET(_pairs, bucket));
//        assert(next_bucket > 0 && _ehead > 0);
//        assert(next_bucket <= _mask && prev_bucket <= _mask);

        EMH_PREVET(_pairs, next_bucket) = prev_bucket;
        EMH_BUCKET(_pairs, prev_bucket) = -next_bucket;

        _ehead = next_bucket;
        return bucket;
    }

    //ehead->bucket->next
    void push_empty(const int32_t bucket)
    {
        const int next_bucket = 0-EMH_BUCKET(_pairs, _ehead);
        assert(next_bucket > 0);

        EMH_PREVET(_pairs, bucket) = _ehead;
        EMH_BUCKET(_pairs, bucket) = -next_bucket;

        EMH_PREVET(_pairs, next_bucket) = bucket;
        EMH_BUCKET(_pairs, _ehead) = -bucket;
        //        _ehead = bucket;
    }
#endif

    void rehash(uint32_t required_buckets)
    {
        if (required_buckets < _num_filled)
            return;

        uint32_t num_buckets = _num_filled > (1u << 16) ? (1u << 16) : 4u;
        while (num_buckets < required_buckets) { num_buckets *= 2; }

        auto new_pairs = (PairT*)alloc_bucket(num_buckets);
        auto old_num_filled  = _num_filled;
        auto old_pairs = _pairs;
#if EMH_REHASH_LOG
        auto last = _last;
        uint32_t collision = 0;
#endif

        _ehead = 0;
        _last = _num_filled  = 0;
        _num_buckets = num_buckets;
        _mask        = num_buckets - 1;

        for (uint32_t bucket = 0; bucket < num_buckets; bucket++)
            EMH_BUCKET(new_pairs, bucket) = INACTIVE;

        memset((char*)(new_pairs + num_buckets), 0, sizeof(PairT) * 2);
        _pairs       = new_pairs;

        for (uint32_t src_bucket = 0; _num_filled < old_num_filled; src_bucket++) {
            if ((int)EMH_BUCKET(old_pairs, src_bucket) < 0)
                continue;

            auto& key = EMH_KEY(old_pairs, src_bucket);
            const auto bucket = find_unique_bucket(key);
            new(_pairs + bucket) PairT(std::move(old_pairs[src_bucket])); _num_filled ++;
            EMH_BUCKET(_pairs, bucket) = bucket;

#if EMH_REHASH_LOG
            if (bucket != hash_main(bucket))
                collision ++;
#endif

            if (is_triviall_destructable())
                old_pairs[src_bucket].~PairT();
        }

#if EMH_REHASH_LOG
        if (_num_filled > EMH_REHASH_LOG) {
            auto mbucket = _num_filled - collision;
            char buff[255] = {0};
            sprintf(buff, "    _num_filled/aver_size/K.V/pack/collision|last = %u/%.2lf/%s.%s/%zd|%.2lf%%,%.2lf%%",
                    _num_filled, double (_num_filled) / mbucket, typeid(KeyT).name(), typeid(ValueT).name(), sizeof(_pairs[0]), collision * 100.0 / _num_filled, last * 100.0 / _num_buckets);
#ifdef EMH_LOG
            static uint32_t ihashs = 0; EMH_LOG() << "hash_nums = " << ihashs ++ << "|" <<__FUNCTION__ << "|" << buff << endl;
#else
            puts(buff);
#endif
        }
#endif

        free(old_pairs);
        assert(old_num_filled == _num_filled);
    }

    // Can we fit another element?
    inline bool check_expand_need()
    {
        return reserve(_num_filled);
    }

    void clear_bucket(uint32_t bucket, bool clear = true)
    {
        if (is_triviall_destructable()) {
            //EMH_BUCKET(_pairs, bucket) = INACTIVE; //loop call in destructor
            _pairs[bucket].~PairT();
        }
        EMH_BUCKET(_pairs, bucket) = INACTIVE; //some compiler the status is reset by destructor
        _num_filled--;

#if EMH_HIGH_LOAD
        if (_ehead && clear) {
            if (10 * _num_filled < 8 * _num_buckets)
                clear_empty();
            else if (bucket)
                push_empty(bucket);
        }
#endif
    }

    uint32_t erase_key(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if (next_bucket == bucket)
            return _eq(key, EMH_KEY(_pairs, bucket)) ? bucket : INACTIVE;
        else if ((int)next_bucket < 0)
            return INACTIVE;
        else if (_eq(key, EMH_KEY(_pairs, bucket))) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (is_copy_trivially())
                EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
            else
                EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));

            EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            return next_bucket;
        }/* else if (EMH_UNLIKELY(bucket != hash_main(bucket)))
            return INACTIVE;
        */

        auto prev_bucket = bucket;
        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (_eq(key, EMH_KEY(_pairs, next_bucket))) {
#if 1
                EMH_BUCKET(_pairs, prev_bucket) = (nbucket == next_bucket) ? prev_bucket : nbucket;
                return next_bucket;
#else
                if (nbucket == next_bucket) {
                    EMH_BUCKET(_pairs, prev_bucket) = prev_bucket;
                    return nbucket;
                }

                const auto last = EMH_BUCKET(_pairs, nbucket);
                if (is_copy_trivially())
                    EMH_PKV(_pairs, next_bucket) = EMH_PKV(_pairs, nbucket);
                else
                    EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, nbucket));
                EMH_BUCKET(_pairs, next_bucket) = (nbucket == last) ? next_bucket : last;
                return nbucket;
#endif
            }

            if (nbucket == next_bucket)
                break;
            prev_bucket = next_bucket;
            next_bucket = nbucket;
        }

        return INACTIVE;
    }

    uint32_t erase_bucket(const uint32_t bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto main_bucket = hash_main(bucket);
        if (bucket == main_bucket) {
            if (bucket != next_bucket) {
                const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
                if (is_copy_trivially())
                    EMH_PKV(_pairs, bucket) = EMH_PKV(_pairs, next_bucket);
                else
                    EMH_PKV(_pairs, bucket).swap(EMH_PKV(_pairs, next_bucket));
                EMH_BUCKET(_pairs, bucket) = (nbucket == next_bucket) ? bucket : nbucket;
            }
            return next_bucket;
        }

        const auto prev_bucket = find_prev_bucket(main_bucket, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = (bucket == next_bucket) ? prev_bucket : next_bucket;
        return bucket;
    }

    // Find the bucket with this key, or return bucket size
    uint32_t find_filled_bucket(const KeyT& key) const
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);

        if ((int)next_bucket < 0)
            return _num_buckets;
        else if (_eq(key, EMH_KEY(_pairs, bucket)))
            return bucket;
        else if (next_bucket == bucket)
            return _num_buckets;
//        else if (hash_bucket(EMH_KEY(_pairs, bucket)) != bucket)
//            return _num_buckets;

        while (true) {
            if (_eq(key, EMH_KEY(_pairs, next_bucket)))
                return next_bucket;

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return _num_buckets;
            next_bucket = nbucket;
        }

        return 0;
    }

    //kick out bucket and find empty to occpuy
    //it will break the orgin link and relnik again.
    //before: main_bucket-->prev_bucket --> bucket   --> next_bucket
    //atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket--> next_bucket
    uint32_t kickout_bucket(const uint32_t obmain, const uint32_t bucket)
    {
        const auto next_bucket = EMH_BUCKET(_pairs, bucket);
        const auto new_bucket  = find_empty_bucket(next_bucket);
        const auto prev_bucket = find_prev_bucket(obmain, bucket);
        EMH_BUCKET(_pairs, prev_bucket) = new_bucket;
        new(_pairs + new_bucket) PairT(std::move(_pairs[bucket]));
        if (next_bucket == bucket)
            EMH_BUCKET(_pairs, new_bucket) = new_bucket;

        _num_filled ++;
        clear_bucket(bucket, false);
        return bucket;
    }

/*
** inserts a new key into a hash table; first, check whether key's main
** bucket/position is free. If not, check whether colliding node/bucket is in its main
** position or not: if it is not, move colliding bucket to an empty place and
** put new key in its main position; otherwise (colliding bucket is in its main
** position), new key goes to an empty position.
*/
    uint32_t find_or_allocate(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        const auto& bucket_key = EMH_KEY(_pairs, bucket);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0) {
#if EMH_HIGH_LOAD
            if (next_bucket != INACTIVE)
                pop_empty(bucket);
#endif
            return bucket;
        } else if (_eq(key, bucket_key))
            return bucket;

        //check current bucket_key is in main bucket or not
        const auto obmain = hash_bucket(bucket_key);
        if (obmain != bucket)
            return kickout_bucket(obmain, bucket);
        else if (next_bucket == bucket)
            return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);

#if EMH_LRU_SET
        auto prev_bucket = bucket;
#endif
        //find next linked bucket and check key
        while (true) {
            if (EMH_UNLIKELY(_eq(key, EMH_KEY(_pairs, next_bucket)))) {
#if EMH_LRU_SET
                EMH_PKV(_pairs, next_bucket).swap(EMH_PKV(_pairs, prev_bucket));
                return prev_bucket;
#else
                return next_bucket;
#endif
            }

#if EMH_LRU_SET
            prev_bucket = next_bucket;
#endif

            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                break;
            next_bucket = nbucket;
        }

        //find a new empty and link it to tail
        const auto new_bucket = find_empty_bucket(next_bucket);
        return EMH_BUCKET(_pairs, next_bucket) = new_bucket;
    }

    uint32_t find_unique_bucket(const KeyT& key)
    {
        const auto bucket = hash_bucket(key);
        auto next_bucket = EMH_BUCKET(_pairs, bucket);
        if ((int)next_bucket < 0) {
#if EMH_HIGH_LOAD
            if (next_bucket != INACTIVE)
                pop_empty(bucket);
#endif
            return bucket;
        }

        //check current bucket_key is in main bucket or not
        const auto obmain = hash_main(bucket);
        if (EMH_UNLIKELY(obmain != bucket))
            return kickout_bucket(obmain, bucket);
        else if (EMH_UNLIKELY(next_bucket != bucket))
            next_bucket = find_last_bucket(next_bucket);

        //find a new empty and link it to tail
        return EMH_BUCKET(_pairs, next_bucket) = find_empty_bucket(next_bucket);
    }

/***
  Different probing techniques usually provide a trade-off between memory locality and avoidance of clustering.
Since Robin Hood hashing is relatively resilient to clustering (both primary and secondary), linear probing the most cache-friendly alternative is typically used.

    It's the core algorithm of this hash map with highly optimization/benchmark.
normaly linear probing is inefficient with high load factor, it use a new 3-way linear
probing strategy to search empty slot. from benchmark even the load factor > 0.9, it's more 2-3 timer fast than
one-way seach strategy.

1. linear or quadratic probing a few cache line for less cache miss from input slot "bucket_from".
2. the first  seach  slot from member variant "_last", init with 0
3. the second search slot from calculated pos "(_num_filled + _last) & _mask", it's like a rand value
*/
    // key is not in this map. Find a place to put it.
    uint32_t find_empty_bucket(const uint32_t bucket_from)
    {
#if EMH_HIGH_LOAD
        if (_ehead)
            return pop_empty(_ehead);
#endif
        auto bucket = bucket_from;
        if (EMH_EMPTY(_pairs, ++bucket) || EMH_EMPTY(_pairs, ++bucket))
            return bucket;

#ifdef EMH_LPL
        constexpr auto linear_probe_length = std::max((unsigned int)(192 / sizeof(PairT)) + 2, 4u);//cpu cache line 64 byte,2-3 cache line miss
        auto offset = 2u;

#ifdef EMH_QUADRATIC
        for (; offset < linear_probe_length; offset += 2) {
            auto bucket1 = (bucket + offset) & _mask;
            if (EMH_EMPTY(_pairs, bucket1) || EMH_EMPTY(_pairs, ++bucket1))
                return bucket1;
        }
#else
        for (auto next = offset; offset < linear_probe_length; next += ++offset) {
            auto bucket1 = (bucket + next) & _mask;
            if (EMH_EMPTY(_pairs, bucket1) || EMH_EMPTY(_pairs, ++bucket1))
                return bucket1;
        }
#endif

        while (true) {
            if (EMH_EMPTY(_pairs, _last) || EMH_EMPTY(_pairs, ++_last))
                return _last++;
            ++_last &= _mask;

#ifndef EMH_LINEAR3
            auto tail = _mask - _last;
            if (EMH_EMPTY(_pairs, tail) || EMH_EMPTY(_pairs, ++tail))
                return tail;
#else
            auto medium = (_mask / 2 + _last) & _mask;
            if (EMH_EMPTY(_pairs, medium) || EMH_EMPTY(_pairs, ++medium))
                return medium;
#endif
        }

#else
        constexpr auto linear_probe_length = sizeof(value_type) > EMH_CACHE_LINE_SIZE ? 3 : 5;
        for (uint32_t step = 2, slot = bucket + 1; ; slot += 2, step ++) {
            auto bucket1 = slot & _mask;
            if (EMH_EMPTY(_pairs, bucket1) || EMH_EMPTY(_pairs, ++bucket1))
                return bucket1;

            if (step > linear_probe_length) {
                if (EMH_EMPTY(_pairs, _last) || EMH_EMPTY(_pairs, ++_last))
                    return _last++;
                ++_last &= _mask;
#if 0
                auto tail = _mask - _last;
                if (EMH_EMPTY(_pairs, tail) || EMH_EMPTY(_pairs, ++tail))
                    return tail;
#endif
#if EMH_LINEAR3
                //auto medium = (_num_filled + _last) & _mask;
                auto medium = (_num_buckets / 2 + _last) & _mask;
                if (EMH_EMPTY(_pairs, medium) || EMH_EMPTY(_pairs, ++medium))
                    return _last = medium;
#endif
            }
        }
#endif
        return 0;
    }

    uint32_t find_last_bucket(uint32_t main_bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == main_bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == next_bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    uint32_t find_prev_bucket(const uint32_t main_bucket, const uint32_t bucket) const
    {
        auto next_bucket = EMH_BUCKET(_pairs, main_bucket);
        if (next_bucket == bucket)
            return main_bucket;

        while (true) {
            const auto nbucket = EMH_BUCKET(_pairs, next_bucket);
            if (nbucket == bucket)
                return next_bucket;
            next_bucket = nbucket;
        }
    }

    inline uint32_t hash_bucket(const KeyT& key) const
    {
        return (uint32_t)hash_key(key) & _mask;
    }

    inline uint32_t hash_main(const uint32_t bucket) const
    {
        return (uint32_t)hash_key(EMH_KEY(_pairs, bucket)) & _mask;
    }

    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    static inline uint64_t hash64(uint64_t key)
    {
#if __SIZEOF_INT128__ && EMH_INT_HASH == 1
        __uint128_t r = key; r *= KC;
        return (uint64_t)(r >> 64) + (uint64_t)r;
#elif EMH_INT_HASH == 2
        //MurmurHash3Mixer
        uint64_t h = key;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccd;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53;
        h ^= h >> 33;
        return h;
#elif _WIN64 && EMH_INT_HASH == 1
        uint64_t high;
        return _umul128(key, KC, &high) + high;
#elif EMH_INT_HASH == 3
        auto ror  = (key >> 32) | (key << 32);
        auto low  = key * 0xA24BAED4963EE407ull;
        auto high = ror * 0x9FB21C651E98DF25ull;
        auto mix  = low + high;
        return mix;
#elif EMH_INT_HASH == 1
        uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
        return (r >> 32) + r;
#else
        uint64_t x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }

    template<typename UType, typename std::enable_if<std::is_integral<UType>::value, uint32_t>::type = 0>
    inline uint64_t hash_key(const UType key) const
    {
#ifdef EMH_INT_HASH
        return hash64(key);
#elif EMH_IDENTITY_HASH
        return (key + (key >> (sizeof(UType) * 4)));
#elif EMH_WYHASH64
        return wyhash64(key, KC);
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint64_t hash_key(const UType& key) const
    {
#if defined(AHASH_AHASH_H)
        return ahash64(key.data(), key.size(), KC);
#elif WYHASH_LITTLE_ENDIAN
        return wyhash(key.data(), key.size(), key.size() + KC);
#else
        return _hasher(key);
#endif
    }

    template<typename UType, typename std::enable_if<!std::is_integral<UType>::value && !std::is_same<UType, std::string>::value, uint32_t>::type = 0>
    inline uint64_t hash_key(const UType& key) const
    {
#ifdef EMH_INT_HASH
        return _hasher(key) * KC;
#else
        return _hasher(key);
#endif
    }

private:
    PairT*    _pairs;
    HashT     _hasher;
    EqT       _eq;

    uint32_t  _num_buckets;
    uint32_t  _num_filled;
    uint32_t  _mask;
    uint32_t  _last;
    uint32_t  _ehead;
    uint32_t  _loadlf;
};
} // namespace emhash
#if __cplusplus > 199711
//template <class Key, class Val> using emhash5 = ehmap<Key, Val, std::hash<Key>>;
#endif