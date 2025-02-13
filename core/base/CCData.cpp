/****************************************************************************
 Copyright (c) 2010-2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 https://adxeproject.github.io/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "base/CCData.h"
#include "base/CCConsole.h"

NS_CC_BEGIN

const Data Data::Null;

Data::Data() : _bytes(nullptr), _size(0)
{
    CCLOGINFO("In the empty constructor of Data.");
}

Data::Data(Data&& other) : _bytes(nullptr), _size(0)
{
    CCLOGINFO("In the move constructor of Data.");
    move(other);
}

Data::Data(const Data& other) : _bytes(nullptr), _size(0)
{
    CCLOGINFO("In the copy constructor of Data.");
    if (other._bytes && other._size)
    {
        copy(other._bytes, other._size);
    }
}

Data::~Data()
{
    CCLOGINFO("deallocing Data: %p", this);
    clear();
}

Data& Data::operator=(const Data& other)
{
    if (this != &other)
    {
        CCLOGINFO("In the copy assignment of Data.");
        copy(other._bytes, other._size);
    }
    return *this;
}

Data& Data::operator=(Data&& other)
{
    if (this != &other)
    {
        CCLOGINFO("In the move assignment of Data.");
        move(other);
    }
    return *this;
}

void Data::move(Data& other)
{
    if (_bytes != other._bytes)
        clear();

    _bytes = other._bytes;
    _size  = other._size;

    other._bytes = nullptr;
    other._size  = 0;
}

bool Data::isNull() const
{
    return (_bytes == nullptr || _size == 0);
}

uint8_t* Data::getBytes() const
{
    return _bytes;
}

ssize_t Data::getSize() const
{
    return _size;
}

ssize_t Data::copy(const unsigned char* bytes, const ssize_t size)
{
    CCASSERT(size >= 0, "copy size should be non-negative");
    CCASSERT(bytes, "bytes should not be nullptr");

    if (size <= 0)
        return 0;

    if (bytes != _bytes)
    {
        clear();
        _bytes = (unsigned char*)malloc(sizeof(unsigned char) * size);
        memcpy(_bytes, bytes, size);
    }

    _size = size;
    return _size;
}

uint8_t* Data::resize(ssize_t size)
{
    if (_size < size)
    {
        auto newmb = (uint8_t*)realloc(_bytes, size);
        if (!newmb)
            return _bytes;
        _bytes = newmb;
    }
    _size = size;
    return _bytes;
}

void Data::fastSet(uint8_t* bytes, const ssize_t size)
{
    CCASSERT(size >= 0, "fastSet size should be non-negative");
    // CCASSERT(bytes, "bytes should not be nullptr");
    _bytes = bytes;
    _size  = size;
}

void Data::clear()
{
    if (_bytes)
        free(_bytes);
    _bytes = nullptr;
    _size  = 0;
}

uint8_t* Data::takeBuffer(ssize_t* size)
{
    auto buffer = getBytes();
    if (size)
        *size = getSize();
    fastSet(nullptr, 0);
    return buffer;
}

NS_CC_END
