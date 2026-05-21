#include <ctype.h>
#include <common.h>
#include <log/log.h>
#include <debug/trace.h>
#include <cfloat>

static char pchar(uint ch)
{
    return isprint(ch) ? static_cast<char>(ch) : '.';
}

static char tohex(size_t value)
{
    return "0123456789ABCDEF"[value & 0xf];
}

static char * space(char * buf, size_t count)
{
    while (count--)
        *buf++ = ' ';

    return buf;
}

static char * eos(char * buf)
{
    *buf++ = '\n';
    *buf++ = '\0';
    return buf;
}

static char * putx(char * buf, size_t value, size_t width)
{
    char * const res = buf + width;

    while (width)
    {
        buf[--width] = tohex(value);
        value >>= 4;
    }

    return res;
}

static size_t address_size(size_t addr, size_t size)
{
    if ((addr + size) <= 0x10000)
        return 4;
    else
        return 8;
}

enum class OutFormat
{
    DEFAULT,
    HEX
};

template <class T, OutFormat F = OutFormat::DEFAULT>
struct PutVal {};

template <>
struct PutVal<double>
{
    static char * put(char * buf, double value, uint width)
    {
        size_t written = ::snprintf(buf, width + 1, "%-+*f", width, value);
        return buf + written;
    }
};

template <>
struct PutVal<float>
{
    static char * put(char * buf, float value, uint width)
    {
        return PutVal<double>::put(buf, value, width);
    }
};

template <>
struct PutVal<uint>
{
    static char * put(char * buf, uint value, uint width)
    {
        size_t written = ::snprintf(buf, width + 1, "%*u", width, value);
        return buf + written;
    }
};

template <>
struct PutVal<uint, OutFormat::HEX>
{
    static char * put(char * buf, uint value, uint width)
    {
        return putx(buf, value, width);
    }
};

template <>
struct PutVal<int>
{
    static char * put(char * buf, int value, uint width)
    {
        size_t written = ::snprintf(buf, width + 1, "%*d", width, value);
        return buf + written;
    }
};

template <>
struct PutVal<ushort>
{
    static char * put(char * buf, uint value, uint width)
    {
        return PutVal<uint>::put(buf, value, width);
    }
};

template <>
struct PutVal<ushort, OutFormat::HEX>
{
    static char * put(char * buf, uint value, uint width)
    {
        return putx(buf, value, width);
    }
};

template <>
struct PutVal<short>
{
    static char * put(char * buf, int value, uint width)
    {
        return PutVal<int>::put(buf, value, width);
    }
};

EXTERN_C void dump(const char * banner, size_t addr, const void * data, size_t size, size_t addr_step)
{
    char buf[sizeof("AAAAAAAA: XX XX XX XX  XX XX XX XX  XX XX XX XX  XX XX XX XX  CCCCCCCCCCCCCCCC\n")];

    if (banner)
        LOG("%s\n", banner);

    const uint n = 16;
    const size_t asize = address_size(addr, size);
    const uint8_t * p = (const uint8_t *)data;
    for (size_t i = 0; i < size; i += n, p += n, addr += addr_step)
    {
        char * b = putx(buf, addr, asize);
        *b++ = ':';

        for (uint j = 0; j < n; ++j)
        {
            b = space(b, 1);
            if (i + j < size)
                b = putx(b, p[j], 2);
            else
                b = space(b, 2);

            if (j % 4 == 3)
                b = space(b, 1);
        }

        b = space(b, 1);

        for (uint j = 0; j < n && i + j < size; ++j)
            *b++ = pchar(p[j]);

        b = eos(b);
        (void)b; // make clang-tidy happy

        LOG("%s", buf);
    }
}

template <class T, size_t COLUMN_COUNT, size_t COLUMN_WIDTH, OutFormat OF = OutFormat::DEFAULT>
static void dump(const char * banner, size_t addr, const T * data, size_t size)
{
    const size_t SSIZE = 1;    // spacing
    const size_t asize = address_size(addr, size); // length of address

    char buf[ sizeof("AAAAAAAA: ") + COLUMN_COUNT * (SSIZE + COLUMN_WIDTH) + sizeof("  \n") ];

    if (banner)
        LOG("%s\n", banner);

    for (size_t i = 0; i < size; i += COLUMN_COUNT, data += COLUMN_COUNT, addr += COLUMN_COUNT * sizeof(T))
    {
        char * b = putx(buf, addr, asize);
        *b++ = ':';

        for (uint j = 0; j < COLUMN_COUNT; ++j)
        {
            b = space(b, SSIZE);
            if (i + j < size)
                b = PutVal<T, OF>::put(b, data[j], COLUMN_WIDTH);
            else
                break;
        }

        b = eos(b);

        LOG("%s", buf);
    }
}

EXTERN_C void dumpf(const char * banner, size_t addr, const float * data, size_t size)
{
    dump<float, 4, 15>(banner, addr, data, size);
}
EXTERN_C void dumpd(const char * banner, size_t addr, const double * data, size_t size)
{
    dump<double, 4, 15>(banner, addr, data, size);
}
EXTERN_C void dumpl(const char * banner, size_t addr, const long * data, size_t size)
{
    STATIC_ASSERT(sizeof(long) == sizeof(int));
    dump<int, 4, 11>(banner, addr, reinterpret_cast<const int*>(data), size);
}
EXTERN_C void dumpul(const char * banner, size_t addr, const ulong * data, size_t size)
{
    STATIC_ASSERT(sizeof(ulong) == sizeof(uint));
    dump<uint, 4, 11>(banner, addr, reinterpret_cast<const uint*>(data), size);
}
EXTERN_C void dumpi(const char * banner, size_t addr, const int * data, size_t size)
{
    dump<int, 4, 11>(banner, addr, data, size);
}
EXTERN_C void dumpu(const char * banner, size_t addr, const uint * data, size_t size)
{
    dump<uint, 4, 11>(banner, addr, data, size);
}
EXTERN_C void dumph(const char * banner, size_t addr, const short * data, size_t size)
{
    dump<short, 8, 6>(banner, addr, data, size);
}
EXTERN_C void dumpuh(const char * banner, size_t addr, const ushort * data, size_t size)
{
    dump<ushort, 8, 5>(banner, addr, data, size);
}

EXTERN_C void dumpxul(const char * banner, size_t addr, const ulong * data, size_t size)
{
    STATIC_ASSERT(sizeof(ulong) == sizeof(uint));
    dumpxu(banner, addr, reinterpret_cast<const uint *>(data), size);
}

EXTERN_C void dumpxl(const char * banner, size_t addr, const long * data, size_t size)
{
    STATIC_ASSERT(sizeof(long) == sizeof(uint));
    dumpxu(banner, addr, reinterpret_cast<const uint *>(data), size);
}

EXTERN_C void dumpxi(const char * banner, size_t addr, const int * data, size_t size)
{
    dumpxu(banner, addr, reinterpret_cast<const uint *>(data), size);
}

EXTERN_C void dumpxu(const char * banner, size_t addr, const uint * data, size_t size)
{
    dump<uint, 4, 8, OutFormat::HEX>(banner, addr, data, size);
}

EXTERN_C void dumpxh(const char * banner, size_t addr, const short * data, size_t size)
{
    dumpxuh(banner, addr, reinterpret_cast<const ushort *>(data), size);
}

EXTERN_C void dumpxuh(const char * banner, size_t addr, const ushort * data, size_t size)
{
    dump<ushort, 8, 4, OutFormat::HEX>(banner, addr, data, size);
}