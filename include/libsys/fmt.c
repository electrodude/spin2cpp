#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#ifdef TEST
#include <stdio.h>

#define _tx my_tx
#define _rx my_rx
#endif

#ifdef __FLEXC__
#define SMALL_INT
#endif

#define alloca(x) __builtin_alloca(x)

#include "sys/fmt.h"

#define DEFAULT_PREC 4
#define DEFAULT_FLOAT_FMT ((1<<ALTFMT_BIT)|(1<<UPCASE_BIT))

//
// reverse a string in-place
//
void _strrev(char *str)
{
    char *end;
    int c;

    if (!*str) return;
    for (end = str; *end; end++) ;
    --end;
    while (end > str) {
        c = *str;
        *str++ = *end;
        *end-- = c;
    }
}

//
// add padding to left or right
// fmt == flags as above
// width == width of field to output
// leftright == 2 for left, 1 for right
//
// returns: number of bytes output
//
#define PAD_ON_LEFT 2
#define PAD_ON_RIGHT 1

int _fmtpad(putfunc fn, unsigned fmt, int width, unsigned leftright)
{
    int minwidth = (fmt >> MINWIDTH_BIT) & 0xff;
    unsigned justify = (fmt >> JUSTIFY_BIT) & 3;
    int i, r;
    int n = 0;
    if (justify == 0) {
        justify = JUSTIFY_LEFT;
    }
    if ( (justify & leftright) == 0 ) {
        return 0;  // padding goes on other side
    }
    width = minwidth - width; // (amount to pad)
    if (width <= 0) {
        return 0;
    }
    if (justify == JUSTIFY_CENTER) { // centering, only half as much padding (if odd do on right)
        width = (width + (leftright==PAD_ON_RIGHT)) / 2;
    }
    for (i = 0; i < width; i++) {
        r = CALL(fn, ' ');
        if (r < 0) return r;
        n += r;
    }
    return n;
}

int _fmtstr(putfunc fn, unsigned fmt, const char *str)
{
    int maxwidth = (fmt >> MAXWIDTH_BIT) & 0xff;
    int width = strlen(str);
    int n;
    int r, i;
    if (maxwidth && width > maxwidth) {
        width = maxwidth;
    }
    n = _fmtpad(fn, fmt, width, PAD_ON_LEFT);
    if (n < 0) return n;
    for (i = 0; i < width; i++) {
        r = CALL(fn, *str++);
        if (r < 0) return r;
        n += r;
    }
    r = _fmtpad(fn, fmt, width, PAD_ON_RIGHT);
    if (r < 0) return r;
    n += r;
    return n;
}

int _fmtchar(putfunc fn, unsigned fmt, int c)
{
    char buf[2];
    buf[0] = c;
    buf[1] = 0;
    return _fmtstr(fn, fmt, buf);
}

int _uitoa(char *orig_str, UITYPE num, unsigned base, unsigned mindigits, int uppercase)
{
    char *str = orig_str;
    unsigned digit;
    unsigned width = 0;
    int letterdigit;

    if (uppercase) {
        letterdigit = 'A' - 10;
    } else {
        letterdigit = 'a' - 10;
    }
    do {
        digit = num % base;
        if (digit < 10) {
            digit += '0';
        } else {
            digit += letterdigit;
        }
        *str++ = digit;
        num = num / base;
        width++;
    } while (num > 0 || width < mindigits);
    *str++ = 0;
    _strrev(orig_str);
    return width;
}

#define MAX_NUM_DIGITS 64

int _fmtnum(putfunc fn, unsigned fmt, int x, int base)
{
    char buf[MAX_NUM_DIGITS+1];
    char *ptr = buf;
    int width = 0;
    int mindigits = (fmt >> PREC_BIT) & PREC_MASK;
    int maxdigits = (fmt >> MAXWIDTH_BIT) & 0xff;
    int signchar = (fmt >> SIGNCHAR_BIT) & 0x3;
    
    if (maxdigits > MAX_NUM_DIGITS || maxdigits == 0) {
        maxdigits = MAX_NUM_DIGITS;
    }
    if (signchar == SIGNCHAR_UNSIGNED) {
        signchar = SIGNCHAR_NONE;
    } else if (x < 0) {
        signchar = SIGNCHAR_MINUS;
        x = -x;
    }
    if (signchar != SIGNCHAR_NONE) {
        width++;
        if (mindigits == maxdigits) {
            mindigits--;
            if (!mindigits) {
                return _fmtchar(fn, fmt, '#');
            }
        }
        switch (signchar) {
        case SIGNCHAR_SPACE:
            *ptr++ = ' ';
            break;
        default:
        case SIGNCHAR_PLUS:
            *ptr++ = '+';
            break;
        case SIGNCHAR_MINUS:
            *ptr++ = '-';
            break;
        }
    }
    width += _uitoa(ptr, x, base, mindigits, 0 != (fmt & (1<<UPCASE_BIT)));
    if (width > maxdigits) {
        while (maxdigits-- > 0) {
            *ptr++ = '#';
        }
        *ptr++ = 0;
    }
    return _fmtstr(fn, fmt, buf);
}

/*******************************************************
 * floating point support
 *******************************************************/
typedef union DI {
    FTYPE d;
    UITYPE i;
} DI;

#ifdef __propeller__
# ifdef __GNUC__
extern long double _intpow(long double a, long double b, int n);
#define _float_pow_n(a, n) _intpow(1.0, a, n)
# endif
#else
// calculate b^n with as much precision as possible
double _float_pow_n(long double b, int n)
{
    long double r = powl((long double)b, (long double)n);
    return r;
}
#endif

/*
 * disassemble a positive floating point number x into
 * ai,n such that 
 * 1.0 <= a < base and x=a * base^n
 * (normally base will be 10, but we can accept 2 as well)
 * we output ai as an integer, but conceptually it is still
 * a float with one digit before the decimal point
 * if numdigits is negative, then make numdigits be n-(numdigits+1)
 * so as to get that many digits after the decimal point
 */
#ifdef SMALL_INT
#define DOUBLE_BITS 23
#define MAX_DEC_DIGITS 9
#define DOUBLE_ONE ((unsigned)(1<<DOUBLE_BITS))
#else
#define DOUBLE_BITS 52
#define MAX_DEC_DIGITS 17
#define DOUBLE_ONE (1ULL<<DOUBLE_BITS)
#endif
#define DOUBLE_MASK (DOUBLE_ONE-1)

static void disassemble(FTYPE x, UITYPE *aip, int *np, int numdigits, int base)
{
    FTYPE a;
    FTYPE p;
    int maxdigits;
    FTYPE based = (FTYPE)base;
    UITYPE ai;
    UITYPE u;
    UITYPE maxu;
    int n;
    int i;
    int trys = 0;
    DI un;


    if (x == 0.0) {
        *aip = 0;
        *np = 0;
        return;
    }

    // first, find (a,n) such that
    // 1.0 <= a < base and x = a * base^n
 
    n = ilogb(x);
    if (base == 10) {
        // initial estimate: 2^10 ~= 10^3, so 3/10 of ilogb is a good first guess
        n = (3 * n)/10 ;
        maxdigits = MAX_DEC_DIGITS;
    } else {
        maxdigits = DOUBLE_BITS+1;  // for base 2
    }
    
    while (trys++ < 8) {
        //
        //
        p = _float_pow_n(based, n);
        a = x / p;
        if (a < 1.0) {
            --n;
        } else if (a >= based) {
            ++n;
        } else {
            break;
        }
    }
#if defined(TEST) && !defined(__FLEXC__)
    if (trys == 8) {
        fprintf(stderr, "Warning hit retry count\n");
    }
#endif
    i = ilogb(a);
    un.d = a;
    ai = un.i & DOUBLE_MASK;
    ai |= DOUBLE_ONE;
    ai = ai<<i;

    // base 2 we will group digits into 4 to print as hex
    if (base == 2) {
        numdigits *= 4;
    }
    // now extract as many significant digits as we can
    // into u
    u = 0;
    if (numdigits< 0) {
        numdigits = n - numdigits;

        // "0" digits is a special case (we may need to round the
        // implicit 0 up to 1)
        // negative digits will always mean 0 though
        if (numdigits < 0) {
            goto done;
        }
    } else {
        numdigits = numdigits+1;
    }
    if (numdigits > maxdigits)
        numdigits = maxdigits;
    maxu = 1; // for overflow
    while ( u < DOUBLE_ONE && numdigits-- > 0) {
        FTYPE d;
        d = (ai >> DOUBLE_BITS); // next digit
        ai &= DOUBLE_MASK;
        u = u * base;
        maxu = maxu * base;
        ai = ai * base;
        u = u+d;
    }
    //
    // round
    //
    if (ai > (unsigned)(base*DOUBLE_ONE/2) || (ai == (unsigned)(base*DOUBLE_ONE/2) && (u & 1))) {
        u++;
        if (u == maxu) {
            ++n;
        }
    }
done:
    *aip = u;
    *np = n;
}

//
// output the sign and any hex digits required
// if buf is NULL print using pi
// otherwise put into buf
// returns number of bytes output, or -1 if an error happens
//
static int
emitsign(char *buf, int sign, int hex)
{
    int r = 0;
    if (sign) {
        *buf++ = sign;
        r++;
    }
    if (hex) {
        *buf++ = '0';
        *buf++ = hex;
        r += 2;
    }
    return r;
}

//
// find the digits for x 
// we will output "prec" digits after the decimal point
// spec is a C printf style specification ('g', 'f', 'a', etc.)
//

#define MAXWIDTH 64

int _fmtfloat(putfunc fn, unsigned fmt, FTYPE x, int spec)
{
    UITYPE ai;
    int i;
    int base = 10;
    int exp;  // exponent
    int isExpFmt;  // output should be printed in exponential notation
    int totalWidth;
    int sign = 0;
    int expchar;
    int stripTrailingZeros = 0;
    int expprec = 2;
    int needPrefix = 0;
    int hexSign = 0;
    int padkind = 0;
    
    // we print a series of 0's, then the digits, then more 0's if necessary
    // to force e.g. 4 leading 0's, set startdigit to -4
    int startdigit;
    int decpt;
    int enddigit;
    int numdigits;
    int expdigits;
    int expsign = 0;
    int signclass;
    int minwidth;
    char dig[64]; // digits for the number
    char expdig[8]; // digits for the exponent
    int prec;
    int needUpper;
    char *buf;
    char *origbuf;
    int justify;
    
    origbuf = buf = alloca(MAXWIDTH + 1);

    prec = (fmt >> PREC_BIT) & PREC_MASK;
    if (prec == 0) prec = DEFAULT_PREC;
    justify = (fmt >> JUSTIFY_BIT) & JUSTIFY_MASK;
    needUpper = (fmt >> UPCASE_BIT) & 1;
    minwidth = (fmt >> MINWIDTH_BIT) & WIDTH_MASK;
    isExpFmt = (spec == 'e');
    expchar = needUpper ? 'E' : 'e';
    if (spec == 'a') {
        isExpFmt = 1;
        expchar = needUpper ? 'P' : 'p';
        base = 2;
        // if precision is missing, %a needs enough digits
        // to precisely represent the number
        if (prec < 0) {
            prec = 13;
            stripTrailingZeros = 1;
        }
        expprec = 1;
        hexSign = needUpper ? 'X' : 'x';
    } else {
        // default precision
        if (prec < 0) {
            prec = 6;
        }
    }

    signclass = (fmt >> SIGNCHAR_BIT) & SIGNCHAR_MASK;
    
    if ( signbit(x) ) {
        // work on non-negative values only
        sign = '-';
        x = copysign(x, 1.0);
    } else {
        // sometimes we need positive sign
        if (signclass == SIGNCHAR_PLUS) {
            sign = '+';
        } else if (signclass == SIGNCHAR_SPACE) {
            sign = ' ';
        }
    }

    // if user requested 0 padding, emit the sign and adjust width
    // now
    padkind = (fmt >> PADCHAR_BIT) & PADCHAR_MASK;
    needPrefix = (sign || base != 10);
    if ( needPrefix && padkind == PADCHAR_ZERO && (justify != JUSTIFY_RIGHT)) {
        int r;
        r = emitsign(buf, sign, hexSign);
        if (r < 0) return r;
        buf += r;
        if (minwidth) {
            minwidth -= r;
            if (minwidth < 0) minwidth = 0;
            fmt = fmt & ~(WIDTH_MASK << MINWIDTH_BIT);
            fmt |= (minwidth << MINWIDTH_BIT);
        }
        needPrefix = 0;
    }

    // handle special cases
    if (isinf(x)) {
        origbuf = buf = alloca(6);
        // emit sign
        if (sign) *buf++ = sign;
        strcpy(buf, "inf");
        goto done;
    } else if (isnan(x)) {
        origbuf = buf = alloca(6);
        if (sign) *buf++ = sign;
        strcpy(buf, "nan");
        goto done;
    }

    if (spec == 'g' || spec == '#') {
        // find the exponent
        disassemble(x, &ai, &exp, prec ? prec - 1 : 0, base);
        if (spec == '#') {
            if (exp > prec) {
                isExpFmt = 1;
            } else if (exp < 0) {
                if (exp <= -prec) {
                    isExpFmt = 1;
                }
            } else if (exp > 0) {
                prec = prec - exp;
            }
        } else {
            // for g format, special handling
            stripTrailingZeros = (0 == ((fmt>>ALTFMT_BIT) & 1));
            if (exp >= prec || exp < -4) {
                isExpFmt = 1;
            } else {
                prec = prec - exp;
                disassemble(x, &ai, &exp, -prec, base);
            }
        }
    } else if (isExpFmt) {
        disassemble(x, &ai, &exp, prec, base);
    } else {
        disassemble(x, &ai, &exp, -(prec+1), base);
    }

    //
    // figure out how many digits we have to print
    //

    // put the digits in their buffers
    // now, a satisfies ai * 10^exp == x
    // if base 2, print digits in hex
    if (base == 2) {
        base = 16;
#ifdef SMALL_INT
        // we actually want hex output, but with small
        // integers there are 23 bits after the decimal point
        // rather than 24, so adjust here
        while (ai && ai < (DOUBLE_ONE << 1)) {
            ai = ai << 1;
        }
#endif        
    }

    numdigits = _uitoa(dig, ai, base, 1, needUpper);
    if (exp < 0) {
        expsign = '-';
        expdigits = _uitoa(expdig, -exp, 10, expprec, needUpper);
    } else {
        expsign = '+';
        expdigits = _uitoa(expdig, exp, 10, expprec, needUpper);
    }

    if (isExpFmt) {
        startdigit = decpt = 0;
        enddigit = prec + 1;
    } else {
        if (exp < 0) {
            // force leading 0's
            startdigit = decpt = exp;
            enddigit = exp + prec + 1;
        } else {
            startdigit = 0;
            decpt = exp;
            enddigit = decpt + prec + 1;
        }
    }

    // allocate a string buffer; leave extra room for '.' just in case
    totalWidth = (enddigit - startdigit) + 1;
    if (sign) {
        totalWidth++;  // we will be printing '+' or '-'
    }
    if (base == 16) {
        totalWidth += 2;  // for '0x'
    }
    if (isExpFmt) {
        // exponents can go up to +- 1024
        totalWidth += 2 + expdigits; // (for e+1024)
    }
    if (totalWidth > MAXWIDTH) {
        return -1;
    }
    // emit sign
    if (needPrefix) {
        int r = emitsign(buf, sign, hexSign);
        if (r < 0) return r;
        buf += r;
    }
    //
    //
    // emit the number
    //
    for (i = startdigit; i < enddigit; i++) {
        if (i >= 0 && i < numdigits) {
            *buf++ = dig[i];
        } else {
            *buf++ = '0';
        }
        if (i == decpt) {
            *buf++ = '.';
        }
    }

    if (stripTrailingZeros) {
        --buf;
        // remove any trailing 0's
        while (buf > origbuf && *buf == '0') {
            --buf;
        }
        // remove radix if appropriate
        if (*buf == '.') {
            --buf;
        }
        // *buf now points at the last valid character; advance so
        // that we can write starting after that
        buf++;
    }

    //
    // output the exponent
    // we want at least 2 digits
    // (up to 4 may be required)
    //
    if (isExpFmt)
    {
        *buf++ = expchar;  // 'p' for hex, 'e' for decimal
        *buf++ = expsign;
        for (i = 0; i < expdigits; i++) {
            *buf++ = expdig[i];
        }
    }

    *buf = 0;

done:
    // now output the string
    return _fmtstr(fn, fmt, origbuf);
}

// BASIC support routines
extern int _tx(int c);
extern int _rx(void);

typedef int (*TxFunc)(int c);
typedef int (*RxFunc)(void);
typedef int (*CloseFunc)(void);

TxFunc _bas_tx_handles[8] = {
    _tx, 0, 0, 0,
    0, 0, 0, 0
};
RxFunc _bas_rx_handles[8] = {
    _rx, 0, 0, 0,
    0, 0, 0, 0
};
CloseFunc _bas_close_handles[8] = { 0 };


//
// basic interfaces
//
int _basic_open(unsigned h, TxFunc sendf, RxFunc recvf, CloseFunc closef)
{
    if (h > 7) return -1;
    _bas_tx_handles[h] = sendf;
    _bas_rx_handles[h] = recvf;
    _bas_close_handles[h] = closef;
    return 0;
}

void _basic_close(unsigned h)
{
    CloseFunc cf;
    if (h > 7) return;
    cf = _bas_close_handles[h];
    _basic_open(h, 0, 0, 0);
    (*cf)();
}

int _basic_print_char(unsigned h, int c, unsigned fmt)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    (*tf)(c);
    return 1;
}

int _basic_print_string(unsigned h, const char *ptr, unsigned fmt)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    return _fmtstr(tf, fmt, ptr);
}

int _basic_print_unsigned(unsigned h, int x, unsigned fmt, int base)
{
    TxFunc tf = _bas_tx_handles[h];
    if (!tf) return 0;
    fmt |= 3<<SIGNCHAR_BIT;
    return _fmtnum(tf, fmt, x, base);
}

int _basic_print_integer(unsigned h, int x, unsigned fmt, int base)
{
    TxFunc tf;

    if (h > 7) return -1;
    tf = _bas_tx_handles[h];
    if (!tf) return 0;
    return _fmtnum(tf, fmt, x, base);
}

int _basic_get_char(unsigned h)
{
    RxFunc rf;
    if (h > 7) return -1;
    rf = _bas_rx_handles[h];
    if (!rf) return -1;
    return (*rf)();
}

int _basic_print_float(unsigned h, FTYPE x, unsigned fmt)
{
    TxFunc tf;
    if (fmt == 0) {
        fmt = DEFAULT_FLOAT_FMT;
    }
    if (h > 7) return -1;
    tf = _bas_tx_handles[h];
    if (!tf) return 0;
    return _fmtfloat(tf, fmt, x, '#');
}

#ifdef TEST
#include <stdio.h>

int _tx(int c)
{
    putchar(c);
    return 1;
}
int _rx(void)
{
    return -1;
}

void testfloat(float x)
{
#ifdef __FLEXC__
    printf("[");
#else    
    printf("%g -> [", x);
#endif    
    _basic_print_float(0, x, 0);
    printf("]\n");
}

int main()
{
    printf("[");
    _basic_print_integer(0, -99, 0x4830404, 10);
    printf("]\n");
    printf("[");
    _basic_print_integer(0, 98, 0, 10);
    printf("]\n");

    testfloat(0.0);
    testfloat(0.1);
    testfloat(0.01);
    testfloat(100.0);
    testfloat(-99.12345);
    testfloat(1e6);
    
    return 0;
}
#endif