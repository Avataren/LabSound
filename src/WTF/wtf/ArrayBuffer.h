/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ArrayBuffer_h
#define ArrayBuffer_h

#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/RefPtr.h>
#include <vector>
#include <stdlib.h>

namespace WTF {

class ArrayBuffer;
class ArrayBufferView;

#if defined(WTF_USE_V8)
// The current implementation assumes that the instance of this class is a
// singleton living for the entire process's lifetime.
class ArrayBufferDeallocationObserver {
public:
    virtual void ArrayBufferDeallocated(unsigned sizeInBytes) = 0;
};
#endif


class ArrayBufferContents {
    WTF_MAKE_NONCOPYABLE(ArrayBufferContents);
public:
    ArrayBufferContents() 
        : m_data(0)
        , m_sizeInBytes(0)
#if defined(WTF_USE_V8)
        , m_deallocationObserver(0)
#endif
    { }

    inline ~ArrayBufferContents();

    void* data() { return m_data; }
    unsigned sizeInBytes() { return m_sizeInBytes; }

private:
    ArrayBufferContents(void* data, unsigned sizeInBytes) 
        : m_data(data)
        , m_sizeInBytes(sizeInBytes)
#if defined(WTF_USE_V8)
        , m_deallocationObserver(0)
#endif
    { }

    friend class ArrayBuffer;

    enum InitializationPolicy {
        ZeroInitialize,
        DontInitialize
    };

    static inline void tryAllocate(unsigned numElements, unsigned elementByteSize, InitializationPolicy, ArrayBufferContents&);
    void transfer(ArrayBufferContents& other)
    {
        ASSERT(!other.m_data);
        other.m_data = m_data;
        other.m_sizeInBytes = m_sizeInBytes;
        m_data = 0;
        m_sizeInBytes = 0;
#if defined(WTF_USE_V8)
        // Notify the current V8 isolate that the buffer is gone.
        if (m_deallocationObserver)
            m_deallocationObserver->ArrayBufferDeallocated(other.m_sizeInBytes);
        ASSERT(!other.m_deallocationObserver);
        m_deallocationObserver = 0;
#endif
    }

    void* m_data;
    unsigned m_sizeInBytes;

#if defined(WTF_USE_V8)
    ArrayBufferDeallocationObserver* m_deallocationObserver;
#endif
};

class ArrayBuffer : public ThreadSafeRefCounted<ArrayBuffer> {
public:
    static inline PassRefPtr<ArrayBuffer> create(unsigned numElements, unsigned elementByteSize);
    static inline PassRefPtr<ArrayBuffer> create(ArrayBuffer*);
    static inline PassRefPtr<ArrayBuffer> create(const void* source, unsigned byteLength);
    static inline PassRefPtr<ArrayBuffer> create(ArrayBufferContents&);

    // Only for use by Uint8ClampedArray::createUninitialized.
    static inline PassRefPtr<ArrayBuffer> createUninitialized(unsigned numElements, unsigned elementByteSize);

    inline void* data();
    inline const void* data() const;
    inline unsigned byteLength() const;

    inline PassRefPtr<ArrayBuffer> slice(int begin, int end) const;
    inline PassRefPtr<ArrayBuffer> slice(int begin) const;

    void addView(ArrayBufferView*);
    void removeView(ArrayBufferView*);

    WTF_EXPORT_PRIVATE bool transfer(ArrayBufferContents&, std::vector<RefPtr<ArrayBufferView> >& neuteredViews);
    bool isNeutered() { return !m_contents.m_data; }

#if defined(WTF_USE_V8)
    void setDeallocationObserver(ArrayBufferDeallocationObserver* deallocationObserver)
    {
        m_contents.m_deallocationObserver = deallocationObserver;
    }
#endif

    ~ArrayBuffer() { }

private:
    static inline PassRefPtr<ArrayBuffer> create(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy);

    inline ArrayBuffer(ArrayBufferContents&);
    inline PassRefPtr<ArrayBuffer> sliceImpl(unsigned begin, unsigned end) const;
    inline unsigned clampIndex(int index) const;
    static inline int clampValue(int x, int left, int right);

    ArrayBufferContents m_contents;
    ArrayBufferView* m_firstView;
};

int ArrayBuffer::clampValue(int x, int left, int right)
{
    ASSERT(left <= right);
    if (x < left)
        x = left;
    if (right < x)
        x = right;
    return x;
}

PassRefPtr<ArrayBuffer> ArrayBuffer::create(unsigned numElements, unsigned elementByteSize)
{
    return create(numElements, elementByteSize, ArrayBufferContents::ZeroInitialize);
}

PassRefPtr<ArrayBuffer> ArrayBuffer::create(ArrayBuffer* other)
{
    return ArrayBuffer::create(other->data(), other->byteLength());
}

PassRefPtr<ArrayBuffer> ArrayBuffer::create(const void* source, unsigned byteLength)
{
    ArrayBufferContents contents;
    ArrayBufferContents::tryAllocate(byteLength, 1, ArrayBufferContents::ZeroInitialize, contents);
    if (!contents.m_data)
        return 0;
    RefPtr<ArrayBuffer> buffer = adoptRef(new ArrayBuffer(contents));
    memcpy(buffer->data(), source, byteLength);
    return buffer.release();
}

PassRefPtr<ArrayBuffer> ArrayBuffer::create(ArrayBufferContents& contents)
{
    return adoptRef(new ArrayBuffer(contents));
}

PassRefPtr<ArrayBuffer> ArrayBuffer::createUninitialized(unsigned numElements, unsigned elementByteSize)
{
    return create(numElements, elementByteSize, ArrayBufferContents::DontInitialize);
}

PassRefPtr<ArrayBuffer> ArrayBuffer::create(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy policy)
{
    ArrayBufferContents contents;
    ArrayBufferContents::tryAllocate(numElements, elementByteSize, policy, contents);
    if (!contents.m_data)
        return 0;
    return adoptRef(new ArrayBuffer(contents));
}

ArrayBuffer::ArrayBuffer(ArrayBufferContents& contents)
    : m_firstView(0)
{
    contents.transfer(m_contents);
}

void* ArrayBuffer::data()
{
    return m_contents.m_data;
}

const void* ArrayBuffer::data() const
{
    return m_contents.m_data;
}

unsigned ArrayBuffer::byteLength() const
{
    return m_contents.m_sizeInBytes;
}

PassRefPtr<ArrayBuffer> ArrayBuffer::slice(int begin, int end) const
{
    return sliceImpl(clampIndex(begin), clampIndex(end));
}

PassRefPtr<ArrayBuffer> ArrayBuffer::slice(int begin) const
{
    return sliceImpl(clampIndex(begin), byteLength());
}

PassRefPtr<ArrayBuffer> ArrayBuffer::sliceImpl(unsigned begin, unsigned end) const
{
    unsigned size = begin <= end ? end - begin : 0;
    return ArrayBuffer::create(static_cast<const char*>(data()) + begin, size);
}

unsigned ArrayBuffer::clampIndex(int index) const
{
    unsigned currentLength = byteLength();
    if (index < 0)
        index = currentLength + index;
    return clampValue(index, 0, currentLength);
}

void ArrayBufferContents::tryAllocate(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy policy, ArrayBufferContents& result)
{
    // Do not allow 32-bit overflow of the total size.
    // FIXME: Why not? The tryFastCalloc function already checks its arguments,
    // and will fail if there is any overflow, so why should we include a
    // redudant unnecessarily restrictive check here?
    if (numElements) {
        unsigned totalSize = numElements * elementByteSize;
        if (totalSize / numElements != elementByteSize) {
            result.m_data = 0;
            return;
        }
    }
    bool allocationSucceeded = false;
    if (policy == ZeroInitialize) {
        result.m_data = malloc(numElements * elementByteSize);
        allocationSucceeded = result.m_data != 0;
        if (allocationSucceeded)
            memset(result.m_data, 0, numElements * elementByteSize);
    }
    else {
        ASSERT(policy == DontInitialize);
        result.m_data = malloc(numElements * elementByteSize);
        allocationSucceeded = result.m_data != 0;
    }

    if (allocationSucceeded) {
        result.m_sizeInBytes = numElements * elementByteSize;
        return;
    }
    result.m_data = 0;
}

ArrayBufferContents::~ArrayBufferContents()
{
#if defined (WTF_USE_V8)
    if (m_deallocationObserver)
        m_deallocationObserver->ArrayBufferDeallocated(m_sizeInBytes);
#endif
    free(m_data);
}

} // namespace WTF

using WTF::ArrayBuffer;

#endif // ArrayBuffer_h
