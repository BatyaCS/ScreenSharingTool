#ifndef TYPE_DOUBLE_UINT_H_
#define TYPE_DOUBLE_UINT_H_

#include <traits/enable-if.h>
#include <traits/disable-if.h>
#include <traits/is-integer-type.h>
#include <traits/is-same-types.h>
#include <traits/type-bit-count.h>
#include <traits/unsigned-type.h>

//
// TODO: Find a time to make this class more beauty
// TODO: Add more unit tests
//

template <class T = uintmax_t>
class DoubleUint
{
    static constexpr const uint N_BITS = 2 * TypeBitCount<T>::bit_count;

public:
    typedef typename UnsignedType<T>::Type Type;


    constexpr DoubleUint(Type v0 = 0)
        : _lo(v0), _hi(0)
    {}

    constexpr DoubleUint(Type v0, Type v1)
        : _lo(v0), _hi(v1)
    {}

    constexpr DoubleUint(const DoubleUint& other) = default;
    constexpr DoubleUint(DoubleUint&& other) = default;


    constexpr Type low() const { return _lo; }
    constexpr Type hi() const { return _hi; }

    constexpr bool operator !() const
    {
        return _lo == 0 && _hi == 0;
    }

    constexpr explicit operator bool() const
    {
        return _lo != 0 || _hi != 0;
    }
    constexpr explicit operator uint8_t() const
    {
        return static_cast<uint8_t>(_lo);
    }
    constexpr explicit operator uint16_t() const
    {
        if constexpr (sizeof(Type) < sizeof(uint16_t))
            return static_cast<uint16_t>(_lo) | (static_cast<uint16_t>(_hi) << N_BITS / 2);
        else
            return static_cast<uint16_t>(_lo);
    }
    constexpr explicit operator uint32_t() const
    {
        if constexpr (sizeof(Type) < sizeof(uint32_t))
            return static_cast<uint32_t>(_lo) | (static_cast<uint32_t>(_hi) << N_BITS / 2);
        else
            return static_cast<uint32_t>(_lo);
    }
    constexpr explicit operator uint64_t() const
    {
        if constexpr (sizeof(Type) < sizeof(uint64_t))
            return static_cast<uint64_t>(_lo) | (static_cast<uint64_t>(_hi) << N_BITS / 2);
        else
            return static_cast<uint64_t>(_lo);
    }
    template <class U = uintmax_t, class = typename DisableIf<IsSameTypes<uint64_t, U>::answer || !IsIntegerType<U>::answer, U>::Type>
    constexpr explicit operator uintmax_t() const
    {
        if constexpr (sizeof(Type) < sizeof(uintmax_t))
            return static_cast<uintmax_t>(_lo) | (static_cast<uintmax_t>(_hi) << N_BITS / 2);
        else
            return static_cast<uintmax_t>(_lo);
    }


    constexpr DoubleUint& operator = (const DoubleUint& other) = default;
    constexpr DoubleUint& operator = (DoubleUint&& other) = default;

    template <typename U, typename = typename EnableIf<IsIntegerType<U>::answer, U>::Type>
    constexpr bool operator == (const U& other) const
    {
        if constexpr (sizeof(Type) < sizeof(U))
            return uintmax_t(*this) == uintmax_t(other);
        else
            return _hi == 0 && _lo == other;
    }

    template <typename U, typename = typename EnableIf<IsIntegerType<U>::answer, U>::Type>
    constexpr bool operator != (const U& other) const
    {
        return !(*this == other);
    }

    constexpr bool operator == (const DoubleUint& other) const
    {
        return _lo == other._lo && _hi == other._hi;
    }
    constexpr bool operator != (const DoubleUint& other) const
    {
        return _lo != other._lo || _hi != other._hi;
    }

    constexpr bool operator && (const DoubleUint& rhs) const
    {
        return bool(*this) && bool(rhs);
    }

    constexpr bool operator || (const DoubleUint& rhs) const
    {
        return bool(*this) || bool(rhs);
    }

    constexpr bool operator > (const DoubleUint& rhs) const
    {
        if (_hi == rhs._hi) 
            return _lo > rhs._lo;

        return _hi > rhs._hi;
    }

    constexpr bool operator < (const DoubleUint& rhs) const
    {
        if (_hi == rhs._hi) 
            return _lo < rhs._lo;

        return _hi < rhs._hi;
    }

    constexpr bool operator >= (const DoubleUint& rhs) const
    {
        return *this > rhs || *this == rhs;
    }

    constexpr bool operator <= (const DoubleUint& rhs) const
    {
        return *this < rhs || *this == rhs;
    }

    constexpr DoubleUint operator & (const DoubleUint& rhs) const
    {
        return DoubleUint(_hi & rhs._hi, _lo & rhs._lo);
    }

    constexpr DoubleUint& operator &= (const DoubleUint& rhs)
    {
        _hi &= rhs._hi;
        _lo &= rhs._lo;
        return *this;
    }

    constexpr DoubleUint operator | (const DoubleUint& rhs) const
    {
        return DoubleUint(_hi | rhs._hi, _lo | rhs._lo);
    }

    constexpr DoubleUint& operator |= (const DoubleUint& rhs)
    {
        _hi |= rhs._hi;
        _lo |= rhs._lo;
        return *this;
    }

    constexpr DoubleUint operator ^ (const DoubleUint& rhs) const
    {
        return DoubleUint(_hi ^ rhs._hi, _lo ^ rhs._lo);
    }

    constexpr DoubleUint& operator ^= (const DoubleUint& rhs)
    {
        _hi ^= rhs._hi;
        _lo ^= rhs._lo;
        return *this;
    }

    constexpr DoubleUint operator~() const
    {
        return DoubleUint(~_hi, ~_lo);
    }

    constexpr DoubleUint operator << (uint shift) const
    {
        if (0 == shift)
            return *this;

        if (shift > N_BITS)
            return DoubleUint();

        if (shift < N_BITS / 2)
            return DoubleUint(_lo << shift, (_hi << shift) | (_lo >> (N_BITS / 2 - shift)));

        // if (shift >= N_BITS / 2)
        return DoubleUint(0, _lo << (shift - N_BITS / 2));
    }

    constexpr DoubleUint operator >> (uint shift) const
    {
        if (0 == shift)
            return *this;

        if (shift > N_BITS)
            return DoubleUint();

        if (shift < N_BITS / 2)
            return DoubleUint((_hi << (N_BITS / 2 - shift)) | (_lo >> shift), _hi >> shift);

        // if (shift >= N_BITS / 2)
        return DoubleUint(_hi >> (shift - N_BITS / 2), 0);
    }

    constexpr DoubleUint operator + (const DoubleUint& other) const
    {
        const Type lo = other._lo + _lo;
        const Type hi = other._hi + _hi;
        return DoubleUint(lo, hi + (lo < _lo));
    }

    constexpr DoubleUint operator - (const DoubleUint& other) const
    {
        const Type lo = _lo - other._lo;
        const Type hi = _hi - other._hi;

        return DoubleUint(lo, hi - (lo > _lo));
    }

    constexpr DoubleUint operator * (const DoubleUint& other) const
    {
        const uint HALF_SHIFT = N_BITS / 4;
        const Type HALF_MASK = ::bit<Type>(HALF_SHIFT) - 1;

        // split values into 4 half-bit parts
        Type top[4] = { _hi >> HALF_SHIFT, _hi & HALF_MASK, _lo >> HALF_SHIFT, _lo & HALF_MASK };
        Type bottom[4] = { other._hi >> HALF_SHIFT, other._hi & HALF_MASK, other._lo >> HALF_SHIFT, other._lo & HALF_MASK };
        Type products[4][4] = {};

        // multiply each component of the values
        for (int y = 3; y > -1; y--)
            for (int x = 3; x > -1; x--)
                products[3 - x][y] = top[x] * bottom[y];

        // first row
        Type fourth32 = (products[0][3] & HALF_MASK);
        Type third32 = (products[0][2] & HALF_MASK) + (products[0][3] >> HALF_SHIFT);
        Type second32 = (products[0][1] & HALF_MASK) + (products[0][2] >> HALF_SHIFT);
        Type first32 = (products[0][0] & HALF_MASK) + (products[0][1] >> HALF_SHIFT);

        // second row
        third32 += (products[1][3] & HALF_MASK);
        second32 += (products[1][2] & HALF_MASK) + (products[1][3] >> HALF_SHIFT);
        first32 += (products[1][1] & HALF_MASK) + (products[1][2] >> HALF_SHIFT);

        // third row
        second32 += (products[2][3] & HALF_MASK);
        first32 += (products[2][2] & HALF_MASK) + (products[2][3] >> HALF_SHIFT);

        // fourth row
        first32 += (products[3][3] & HALF_MASK);

        // move carry to next digit
        third32 += fourth32 >> HALF_SHIFT;
        second32 += third32 >> HALF_SHIFT;
        first32 += second32 >> HALF_SHIFT;

        // remove carry from current digit
        fourth32 &= HALF_MASK;
        third32 &= HALF_MASK;
        second32 &= HALF_MASK;
        first32 &= HALF_MASK;

        // combine components
        return DoubleUint((third32 << HALF_SHIFT) | fourth32, (first32 << HALF_SHIFT) | second32);
    }

    constexpr uint bits() const
    {
        uint8_t out = 0;
        if (_hi)
        {
            out = 64;
            uint64_t up = _hi;
            while (up)
            {
                up >>= 1;
                out++;
            }
        }
        else
        {
            uint64_t low = _lo;
            while (low)
            {
                low >>= 1;
                out++;
            }
        }
        return out;
    }

    constexpr DoubleUint& operator <<= (uint shift)
    {
        return *this = *this << shift;
    }

    constexpr DoubleUint& operator >>= (uint shift)
    {
        return *this = *this >> shift;
    }

    constexpr DoubleUint& operator += (const DoubleUint& other)
    {
        return *this = *this + other;
    }

    constexpr DoubleUint& operator -= (const DoubleUint& other)
    {
        return *this = *this - other;
    }

    constexpr DoubleUint& operator *= (const DoubleUint& other)
    {
        return *this = *this * other;
    }

    constexpr DoubleUint operator / (const DoubleUint& rhs) const
    {
        return divmod(*this, rhs).q;
    }

    constexpr DoubleUint& operator /= (const DoubleUint& rhs)
    {
        return *this = *this / rhs;
    }

    constexpr DoubleUint operator % (const DoubleUint& rhs) const
    {
        return divmod(*this, rhs).r;
    }

    constexpr DoubleUint& operator %= (const DoubleUint& rhs)
    {
        return *this = *this % rhs;
    }

    struct QR 
    { 
        DoubleUint q; 
        DoubleUint r; 
    };

    static constexpr QR divmod(const DoubleUint& lhs, const DoubleUint& rhs)
    {
        // Save some calculations /////////////////////
        //static_assert(rhs == DoubleUint(0), "Invalid divider");
        if (rhs == DoubleUint(0))
            return QR();

        if (rhs == DoubleUint(1))
            return QR{ lhs, 0 };

        if (lhs == rhs)
            return QR{1, 0};

        if (lhs == DoubleUint(0) || lhs < rhs)
            return QR{ 0, lhs };

        QR qr{ 0, 0 };

        for (uint x = lhs.bits(); x > 0; x--)
        {
            qr.q <<= 1;
            qr.r <<= 1;

            const DoubleUint tt = lhs >> (x - 1);
            if (tt & DoubleUint(1))
            {
                qr.r += 1;
            }

            if (qr.r >= rhs)
            {
                qr.r -= rhs;
                qr.q += 1;
            }
        }

        return qr;
    }
    
private:
    Type _lo;
    Type _hi;
};


#endif /* TYPE_DOUBLE_UINT_H_ */