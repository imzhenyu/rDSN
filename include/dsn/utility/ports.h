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
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

#if defined(_WIN32)

# include <Windows.h>

__pragma(warning(disable:4127))

# define __thread __declspec(thread)
# define __selectany __declspec(selectany) extern 

# elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)

# include <unistd.h>
# include <alloca.h>

# define __selectany __attribute__((weak)) extern 

# ifndef O_BINARY
# define O_BINARY 0
#endif

#else

#error "unsupported platform"
#endif

// stl headers
# include <string>
# include <memory>
# include <map>
# include <set>
# include <vector>
# include <list>
# include <algorithm>

# define __STDC_FORMAT_MACROS
// common c headers
# include <cassert>
# include <cstring>
# include <cstdlib>
# include <fcntl.h> // for file open flags
# include <cstdio>
# include <climits>
# include <cerrno>
# include <cstdint>
# include <inttypes.h>

// common utilities
# include <atomic>

# define DSN_MAX(x, y)  ((x) >= (y) ? (x) : (y))

# define DSN_MIN(x, y)  ((x) <= (y) ? (x) : (y))

inline uint16_t DSN_SWAP16(uint16_t x)
{
    uint8_t* ptr = (uint8_t*)&x;
    auto m = ptr[1];
    ptr[1] = ptr[0];
    ptr[0] = m;
    return x;
}

inline uint32_t DSN_SWAP32(uint32_t x)
{
    uint8_t* ptr = (uint8_t*)&x;
    auto m = ptr[3];
    ptr[3] = ptr[0];
    ptr[0] = m;
    m = ptr[2];
    ptr[2] = ptr[1];
    ptr[1] = m;
    return x;
}


inline uint32_t DSN_SWAP24(uint32_t x)
{
    uint8_t* ptr = (uint8_t*)&x;
    auto m = ptr[2];
    ptr[2] = ptr[0];
    ptr[0] = m;
    return x;
}

// common macros and data structures
# define TIME_MS_MAX                       0xffffffff

# ifndef FIELD_OFFSET
# define FIELD_OFFSET(s, field)  (((size_t)&((s *)(10))->field) - 10)
# endif

# ifndef CONTAINING_RECORD 
# define CONTAINING_RECORD(address, type, field) \
    ((type *)((char*)(address)-FIELD_OFFSET(type, field)))
# endif

# ifndef MAX_COMPUTERNAME_LENGTH
# define MAX_COMPUTERNAME_LENGTH 32
# endif

# ifndef ARRAYSIZE
# define ARRAYSIZE(a) \
    ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
# endif

# ifndef snprintf_p
# if defined(_WIN32)
# define snprintf_p sprintf_s
# else
# define snprintf_p std::snprintf
# endif
# endif

# if defined(_WIN32)

// make sure to include <Winsock2.h> before the usage

# ifndef be16toh
# define be16toh(x) ntohs(x)
# endif

# ifndef htobe16
# define htobe16(x) htons(x)
# endif

static_assert (sizeof(int32_t) == sizeof(long),
    "sizeof(int32_t) == sizeof(u_long) for use of ntohl");

# ifndef be32toh
# define be32toh(x) ntohl(x)
# endif

# ifndef htobe32
# define htobe32(x) htonl(x)
# endif

# ifndef be64toh
# define be64toh(x) ( (be32toh((x)>>32)&0xffffffff) | ( be32toh( (x)&0xffffffff ) << 32 ) )
# endif

# endif

# if defined(__APPLE__)


#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

# endif