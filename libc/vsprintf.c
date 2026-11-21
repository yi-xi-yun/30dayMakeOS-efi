/*
 * vsprintf.c
 *
 * Adapted from U-Boot lib/vsprintf.c (Copyright 1991, 1992 Linus Torvalds；
 * 2000-2009 Wolfgang Denk)
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed   int        s32;
typedef signed   long long  s64;
typedef unsigned long       ulong;
typedef unsigned int        uint;


#define likely(x)       __builtin_expect(!!(x), 1)
#define noinline        __attribute__((noinline))


static size_t local_strnlen(const char *s, size_t maxlen)
{
    size_t i;
    for (i = 0; i < maxlen && s[i]; i++)
        ;
    return i;
}


static inline uint32_t do_div(uint64_t *n, uint32_t base)
{
    uint32_t rem = (uint32_t)(*n % base);
    *n = *n / base;
    return rem;
}


static char *put_dec_trunc(char *buf, unsigned q)
{
    unsigned d3, d2, d1, d0;
    d1 = (q >> 4) & 0xf;
    d2 = (q >> 8) & 0xf;
    d3 = (q >> 12);

    d0 = 6 * (d3 + d2 + d1) + (q & 0xf);
    q = (d0 * 0xcd) >> 11;
    d0 = d0 - 10 * q;
    *buf++ = d0 + '0';
    d1 = q + 9 * d3 + 5 * d2 + d1;
    if (d1 != 0) {
        q = (d1 * 0xcd) >> 11;
        d1 = d1 - 10 * q;
        *buf++ = d1 + '0';

        d2 = q + 2 * d2;
        if ((d2 != 0) || (d3 != 0)) {
            q = (d2 * 0xd) >> 7;
            d2 = d2 - 10 * q;
            *buf++ = d2 + '0';

            d3 = q + 4 * d3;
            if (d3 != 0) {
                q = (d3 * 0xcd) >> 11;
                d3 = d3 - 10 * q;
                *buf++ = d3 + '0';
                if (q != 0)
                    *buf++ = q + '0';
            }
        }
    }
    return buf;
}

static char *put_dec_full(char *buf, unsigned q)
{
    unsigned d3, d2, d1, d0;
    d1 = (q >> 4) & 0xf;
    d2 = (q >> 8) & 0xf;
    d3 = (q >> 12);

    d0 = 6 * (d3 + d2 + d1) + (q & 0xf);
    q = (d0 * 0xcd) >> 11;
    d0 = d0 - 10 * q;
    *buf++ = d0 + '0';
    d1 = q + 9 * d3 + 5 * d2 + d1;
    q = (d1 * 0xcd) >> 11;
    d1 = d1 - 10 * q;
    *buf++ = d1 + '0';

    d2 = q + 2 * d2;
    q = (d2 * 0xd) >> 7;
    d2 = d2 - 10 * q;
    *buf++ = d2 + '0';

    d3 = q + 4 * d3;
    q = (d3 * 0xcd) >> 11;
    d3 = d3 - 10 * q;
    *buf++ = d3 + '0';
    *buf++ = q + '0';
    return buf;
}


static noinline char *put_dec(char *buf, uint64_t num)
{
    while (1) {
        unsigned rem;
        if (num < 100000)
            return put_dec_trunc(buf, num);
        rem = do_div(&num, 100000);
        buf = put_dec_full(buf, rem);
    }
}


#define ZEROPAD 1
#define SIGN    2
#define PLUS    4
#define SPACE   8
#define LEFT    16
#define SMALL   32
#define SPECIAL 64


#define ADDCH(str, ch) do { \
    if ((str) < end) \
        *(str) = (ch); \
    ++str; \
    } while (0)


static char *number(char *buf, char *end, u64 num,
                    int base, int size, int precision, int type)
{
    static const char digits[16] = "0123456789ABCDEF";

    char tmp[66];
    char sign;
    char locase;
    int need_pfx = ((type & SPECIAL) && base != 10);
    int i;

    locase = (type & SMALL);
    if (type & LEFT)
        type &= ~ZEROPAD;
    sign = 0;
    if (type & SIGN) {
        if ((s64)num < 0) {
            sign = '-';
            num = -(s64)num;
            size--;
        } else if (type & PLUS) {
            sign = '+';
            size--;
        } else if (type & SPACE) {
            sign = ' ';
            size--;
        }
    }
    if (need_pfx) {
        size--;
        if (base == 16)
            size--;
    }

    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else if (base != 10) {
        int mask = base - 1;
        int shift = 3;
        if (base == 16)
            shift = 4;
        do {
            tmp[i++] = (digits[((unsigned char)num) & mask] | locase);
            num >>= shift;
        } while (num);
    } else {
        i = put_dec(tmp, num) - tmp;
    }

    if (i > precision)
        precision = i;
    size -= precision;
    if (!(type & (ZEROPAD + LEFT))) {
        while (--size >= 0)
            ADDCH(buf, ' ');
    }
    if (sign)
        ADDCH(buf, sign);
    if (need_pfx) {
        ADDCH(buf, '0');
        if (base == 16)
            ADDCH(buf, 'X' | locase);
    }
    if (!(type & LEFT)) {
        char c = (type & ZEROPAD) ? '0' : ' ';
        while (--size >= 0)
            ADDCH(buf, c);
    }
    while (i <= --precision)
        ADDCH(buf, '0');
    while (--i >= 0)
        ADDCH(buf, tmp[i]);
    while (--size >= 0)
        ADDCH(buf, ' ');
    return buf;
}


static char *string(char *buf, char *end, const char *s, int field_width,
                    int precision, int flags)
{
    int len, i;

    if (s == NULL)
        s = "<NULL>";

    len = (int)local_strnlen(s, (size_t)precision);

    if (!(flags & LEFT))
        while (len < field_width--)
            ADDCH(buf, ' ');
    for (i = 0; i < len; ++i)
        ADDCH(buf, *s++);
    while (len < field_width--)
        ADDCH(buf, ' ');
    return buf;
}


static char *pointer(const char *fmt, char *buf, char *end, void *ptr,
                     int field_width, int precision, int flags)
{
    u64 num = (u64)(uintptr_t)ptr;

    (void)fmt;
    flags |= SMALL;
    if (field_width == -1) {
        field_width = 2 * (int)sizeof(void *);
        flags |= ZEROPAD;
    }
    return number(buf, end, num, 16, field_width, precision, flags);
}


static int vsnprintf_internal(char *buf, size_t size, const char *fmt, va_list args)
{
    u64 num;
    int base;
    char *str;
    int flags;
    int field_width;
    int precision;
    int qualifier;
    char *end = buf + size;

    if (end < buf) {
        end = (char *)(intptr_t)-1;
        size = (size_t)(end - buf);
    }
    str = buf;

    for (; *fmt; ++fmt) {
        if (*fmt != '%') {
            ADDCH(str, *fmt);
            continue;
        }

        flags = 0;
repeat:
            ++fmt;
            switch (*fmt) {
            case '-': flags |= LEFT;    goto repeat;
            case '+': flags |= PLUS;    goto repeat;
            case ' ': flags |= SPACE;   goto repeat;
            case '#': flags |= SPECIAL; goto repeat;
            case '0': flags |= ZEROPAD; goto repeat;
            }

        field_width = -1;
        if (*fmt >= '0' && *fmt <= '9') {
            field_width = 0;
            while (*fmt >= '0' && *fmt <= '9')
                field_width = field_width * 10 + (*fmt++ - '0');
        } else if (*fmt == '*') {
            ++fmt;
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (*fmt >= '0' && *fmt <= '9') {
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9')
                    precision = precision * 10 + (*fmt++ - '0');
            } else if (*fmt == '*') {
                ++fmt;
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }

        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' ||
            *fmt == 'Z' || *fmt == 'z' || *fmt == 't') {
            qualifier = *fmt;
            ++fmt;
            if (qualifier == 'l' && *fmt == 'l') {
                qualifier = 'L';
                ++fmt;
            }
        }

        base = 10;

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT)) {
                while (--field_width > 0)
                    ADDCH(str, ' ');
            }
            ADDCH(str, (unsigned char)va_arg(args, int));
            while (--field_width > 0)
                ADDCH(str, ' ');
            continue;

        case 's':
            str = string(str, end, va_arg(args, char *),
                         field_width, precision, flags);
            continue;

        case 'p':
            str = pointer(fmt + 1, str, end,
                          va_arg(args, void *),
                          field_width, precision, flags);
            /* 跳过后缀修饰符（%pM / %pI4 等，本实现不支持，跳过不输出） */
            while ((fmt[1] >= 'a' && fmt[1] <= 'z') ||
                   (fmt[1] >= 'A' && fmt[1] <= 'Z') ||
                   (fmt[1] >= '0' && fmt[1] <= '9'))
                fmt++;
            continue;

        case 'n':
            if (qualifier == 'l') {
                long *ip = va_arg(args, long *);
                *ip = (long)(str - buf);
            } else {
                int *ip = va_arg(args, int *);
                *ip = (int)(str - buf);
            }
            continue;

        case '%':
            ADDCH(str, '%');
            continue;

        case 'o':
            base = 8;
            break;

        case 'x':
            flags |= SMALL;
            /* fallthrough */
        case 'X':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
            /* fallthrough */
        case 'u':
            break;

        default:
            ADDCH(str, '%');
            if (*fmt)
                ADDCH(str, *fmt);
            else
                --fmt;
            continue;
        }

        if (qualifier == 'L')
            num = va_arg(args, unsigned long long);
        else if (qualifier == 'l') {
            num = va_arg(args, unsigned long);
            if (flags & SIGN)
                num = (signed long)num;
        } else if (qualifier == 'Z' || qualifier == 'z') {
            num = va_arg(args, size_t);
        } else if (qualifier == 't') {
            num = va_arg(args, ptrdiff_t);
        } else if (qualifier == 'h') {
            num = (unsigned short)va_arg(args, int);
            if (flags & SIGN)
                num = (signed short)num;
        } else {
            num = va_arg(args, unsigned int);
            if (flags & SIGN)
                num = (signed int)num;
        }
        str = number(str, end, num, base, field_width, precision, flags);
    }

    if (size > 0) {
        ADDCH(str, '\0');
        if (str > end)
            end[-1] = '\0';
        --str;
    }

    return (int)(str - buf);
}


int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    return vsnprintf_internal(buf, size, fmt, args);
}

int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int i = vsnprintf(buf, size, fmt, args);
    if (likely(i < (int)size))
        return i;
    if (size != 0)
        return (int)size - 1;
    return 0;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;
    va_start(args, fmt);
    i = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return i;
}

int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;
    va_start(args, fmt);
    i = vscnprintf(buf, size, fmt, args);
    va_end(args);
    return i;
}

int vsprintf(char *buf, const char *fmt, va_list args)
{
    return vsnprintf_internal(buf, (size_t)INT_MAX, fmt, args);
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    int i;
    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}
