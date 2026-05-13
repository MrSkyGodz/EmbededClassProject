#ifndef CIRCULAR_BUFFER_HPP
#define CIRCULAR_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

class CircularBuffer
{
public:
    static constexpr size_t MaxCapacity = 256U;

    explicit CircularBuffer(size_t capacity = MaxCapacity)
        : head_(0U),
          tail_(0U),
          size_(0U),
          capacity_(capacity)
    {
        if ((capacity_ == 0U) || (capacity_ > MaxCapacity))
        {
            capacity_ = MaxCapacity;
        }
    }

    bool push(uint8_t value)
    {
        if (size_ >= capacity_)
        {
            return false;
        }

        data_[head_] = value;
        head_ = (head_ + 1U) % capacity_;
        size_++;
        return true;
    }

    bool pushOverwriteOldest(uint8_t value)
    {
        if (size_ >= capacity_)
        {
            tail_ = (tail_ + 1U) % capacity_;
            size_--;
        }

        return push(value);
    }

    bool pop(uint8_t &value)
    {
        if (size_ == 0U)
        {
            return false;
        }

        value = data_[tail_];
        tail_ = (tail_ + 1U) % capacity_;
        size_--;
        return true;
    }

    bool peek(size_t offset, uint8_t &value) const
    {
        if (offset >= size_)
        {
            return false;
        }

        value = data_[(tail_ + offset) % capacity_];
        return true;
    }

    bool discard(size_t count)
    {
        if (count > size_)
        {
            return false;
        }

        tail_ = (tail_ + count) % capacity_;
        size_ -= count;
        return true;
    }

    void clear()
    {
        head_ = 0U;
        tail_ = 0U;
        size_ = 0U;
    }

    size_t available() const
    {
        return size_;
    }

    size_t capacity() const
    {
        return capacity_;
    }

private:
    uint8_t data_[MaxCapacity];
    size_t head_;
    size_t tail_;
    size_t size_;
    size_t capacity_;
};

#endif
