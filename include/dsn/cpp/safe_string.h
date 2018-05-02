/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rDSN) -=- 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     define safe string that can across 
 *
 * Revision history:
 *     July, 2016, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

# include <dsn/service_api_c.h>
# include <dsn/cpp/callocator.h>
# include <dsn/utility/misc.h>

# include <vector>
# include <list>
# include <cstring>
# include <string>
# include <sstream>
# include <map>
# include <unordered_map>

namespace dsn
{
    # ifdef _WIN32
    template<typename T>
    using safe_allocator = callocator<T, dsn_malloc, dsn_free>;
    
    template <class T, class U>
    bool operator==(const safe_allocator<T>&, const safe_allocator<U>&)
    {
        return true;
    }
    template <class T, class U>
    bool operator!=(const safe_allocator<T>&, const safe_allocator<U>&)
    {
        return false;
    }

    # else
    
    template<typename T>
    using safe_allocator = ::std::allocator<T>;
    
    # endif
    
    template<typename T>
    using safe_vector = ::std::vector<T, safe_allocator<T> >;

    template<typename T>
    using safe_list = ::std::list<T, safe_allocator<T> >;

    template<typename TKey, typename TValue>
    using safe_map = ::std::map<
            TKey, 
            TValue, 
            ::std::less<TKey>, 
            safe_allocator< ::std::pair< const TKey, TValue > >
            >;

    template<typename TKey, typename TValue>
    using safe_unordered_map = ::std::unordered_map<
            TKey, 
            TValue, 
            ::std::hash<TKey>,
            ::std::equal_to<TKey>,
            safe_allocator< ::std::pair< const TKey, TValue > >
            >;

    using safe_string = ::std::basic_string<char, ::std::char_traits<char>, safe_allocator<char> >;

    using safe_sstream = ::std::basic_stringstream<char, ::std::char_traits<char>,
        safe_allocator<char> >;

    namespace utils 
    {
        # ifdef _WIN32
        
        inline void split_args(const char* args, /*out*/ safe_vector<safe_string>& sargs, char splitter = ' ');
        inline void split_args(const char* args, /*out*/ safe_list<safe_string>& sargs, char splitter = ' ');


        // ------- inline implementation 

        inline void split_args(const char* args, safe_vector<safe_string>& sargs, char splitter)
        {
            sargs.clear();

            safe_string v(args);

            int lastPos = 0;
            while (true)
            {
                auto pos = v.find(splitter, lastPos);
                if (pos != safe_string::npos)
                {
                    safe_string s = v.substr(lastPos, pos - lastPos);
                    if (s.length() > 0)
                    {
                        safe_string s2 = trim_string((char*)s.c_str());
                        if (s2.length() > 0)
                            sargs.push_back(s2);
                    }
                    lastPos = static_cast<int>(pos + 1);
                }
                else
                {
                    safe_string s = v.substr(lastPos);
                    if (s.length() > 0)
                    {
                        safe_string s2 = trim_string((char*)s.c_str());
                        if (s2.length() > 0)
                            sargs.push_back(s2);
                    }
                    break;
                }
            }
        }

        inline void split_args(const char* args, safe_list<safe_string>& sargs, char splitter)
        {
            sargs.clear();

            safe_string v(args);

            int lastPos = 0;
            while (true)
            {
                auto pos = v.find(splitter, lastPos);
                if (pos != safe_string::npos)
                {
                    safe_string s = v.substr(lastPos, pos - lastPos);
                    if (s.length() > 0)
                    {
                        safe_string s2 = trim_string((char*)s.c_str());
                        if (s2.length() > 0)
                            sargs.push_back(s2);
                    }
                    lastPos = static_cast<int>(pos + 1);
                }
                else
                {
                    safe_string s = v.substr(lastPos);
                    if (s.length() > 0)
                    {
                        safe_string s2 = trim_string((char*)s.c_str());
                        if (s2.length() > 0)
                            sargs.push_back(s2);
                    }
                    break;
                }
            }
        }
        
        # endif 
    }
}

