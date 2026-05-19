#ifndef NON_COPYABLE_H_
#define NON_COPYABLE_H_

class NonCopyable
{
    NonCopyable(const NonCopyable&) = delete;
    void operator = (const NonCopyable&) = delete;

protected:
    constexpr NonCopyable() = default;
    ~NonCopyable() = default;
};

#endif /* NON_COPYABLE_H_ */