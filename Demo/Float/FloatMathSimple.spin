''***************************************
''*  Floating-Point Math                *
''*  Single-precision IEEE-754          *
''*  Author: Chip Gracey                *
''*  Copyright (c) 2006 Parallax, Inc.  *
''*  See end of file for terms of use.  *
''***************************************

#ifdef __FASTSPIN__
#define MULTI_UNPACK
#else
# ifdef __SPIN2CPP__
#define MULTI_UNPACK
# endif
#endif

#ifndef MULTI_UNPACK
VAR
  long gs, gx, gm
#endif

PUB start
  '' do nothing

PUB FFloat(integer) : single | s, x, m

''Convert integer to float    

  if m := ||integer             'absolutize mantissa, if 0, result 0
    s := integer >> 31          'get sign
    x := >|m - 1                'get exponent
    m <<= 31 - x                'msb-justify mantissa
    m >>= 2                     'bit29-justify mantissa

    return Pack(s, x, m)             'pack result
   

PUB FRound(single) : integer

''Convert float to rounded integer

  return FInteger(single, 1)    'use 1/2 to round


PUB FTrunc(single) : integer

''Convert float to truncated integer

  return FInteger(single, 0)    'use 0 to round


PUB FNeg(singleA) : single

''Negate singleA

  return singleA ^ $8000_0000   'toggle sign bit
  

PUB FAbs(singleA) : single

''Absolute singleA

  return singleA & $7FFF_FFFF   'clear sign bit
  

PUB FSqr(singleA) : single | s, x, m, root

''Compute square root of singleA

  if singleA > 0                'if a =< 0, result 0

#ifdef MULTI_UNPACK
    (s,x,m) := Unpack(singleA)
#else
    Unpack(singleA)         'unpack input
    s := gs
    x := gx
    m := gm
#endif
    m >>= !x & 1                'if exponent even, shift mantissa down
    x ~>= 1                     'get root exponent

    root := $4000_0000          'compute square root of mantissa
    repeat 31
      result |= root
      if result ** result > m
        result ^= root
      root >>= 1
    m := result >> 1
  
    return Pack(s, x, m)             'pack result


PUB FAdd(singleA, singleB) : single | sa, xa, ma, sb, xb, mb

''Add singleA and singleB

#ifdef MULTI_UNPACK
  (sa,xa,ma) := Unpack(singleA)          'unpack inputs
  (sb,xb,mb) := Unpack(singleB)
#else
  Unpack(singleA)          'unpack inputs
  sa := gs
  xa := gx
  ma := gm
  Unpack(singleB)
  sb := gs
  xb := gx
  mb := gm
#endif

  if sa                         'handle mantissa negation
    -ma
  if sb
    -mb

  result := ||(xa - xb) <# 31   'get exponent difference
  if xa > xb                    'shift lower-exponent mantissa down
    mb ~>= result
  else
    ma ~>= result
    xa := xb

  ma += mb                      'add mantissas
  sa := ma < 0                  'get sign
  ||ma                          'absolutize result

  return Pack(sa, xa, ma)              'pack result


PUB FSub(singleA, singleB) : single

''Subtract singleB from singleA

  return FAdd(singleA, FNeg(singleB))

             
PUB FMul(singleA, singleB) : single | sa, xa, ma, sb, xb, mb

''Multiply singleA by singleB

#ifdef MULTI_UNPACK
  (sa,xa,ma) := Unpack(singleA)          'unpack inputs
  (sb,xb,mb) := Unpack(singleB)
#else
  Unpack(singleA)          'unpack inputs
  sa := gs
  xa := gx
  ma := gm
  Unpack(singleB)
  sb := gs
  xb := gx
  mb := gm
#endif

  sa ^= sb                      'xor signs
  xa += xb                      'add exponents
  ma := (ma ** mb) << 3         'multiply mantissas and justify

  return Pack(sa, xa, ma)              'pack result


PUB FDiv(singleA, singleB) : single | sa, xa, ma, sb, xb, mb

''Divide singleA by singleB

#ifdef MULTI_UNPACK
  (sa,xa,ma) := Unpack(singleA)          'unpack inputs
  (sb,xb,mb) := Unpack(singleB)
#else
  Unpack(singleA)          'unpack inputs
  sa := gs
  xa := gx
  ma := gm
  Unpack(singleB)
  sb := gs
  xb := gx
  mb := gm
#endif

  sa ^= sb                      'xor signs
  xa -= xb                      'subtract exponents

  repeat 30                     'divide mantissas
    result <<= 1
    if ma => mb
      ma -= mb
      result++        
    ma <<= 1
  ma := result

  return Pack(sa, xa, ma)              'pack result


PRI FInteger(a, r) : integer | s, x, m

'Convert float to rounded/truncated integer

#ifdef MULTI_UNPACK
  (s,x,m) := Unpack(a)
#else
  Unpack(a)                 'unpack input
  s := gs
  x := gx
  m := gm
#endif
  if x => -1 and x =< 30        'if exponent not -1..30, result 0
    m <<= 2                     'msb-justify mantissa
    m >>= 30 - x                'shift down to 1/2-lsb
    m += r                      'round (1) or truncate (0)
    m >>= 1                     'shift down to lsb
    if s                        'handle negation
      -m
    return m                    'return integer


#ifdef MULTI_UNPACK
PRI Unpack(single) : s, x, m | tmp
#else
PRI Unpack(single) | s, x, m, tmp
#endif

'Unpack floating-point into (sign, exponent, mantissa) at pointer
  tmp := 0
  s := single >> 31             'unpack sign
  x := single << 1 >> 24        'unpack exponent
  m := single & $007F_FFFF      'unpack mantissa

  if x                          'if exponent > 0,
    m := m << 6 | $2000_0000    '..bit29-justify mantissa with leading 1
  else
    tmp := >|m - 23          'else, determine first 1 in mantissa
    x := tmp                 '..adjust exponent
    m <<= 7 - tmp            '..bit29-justify mantissa

  x -= 127                      'unbias exponent
#ifndef MULTI_UNPACK
  gs := s
  gx := x
  gm := m
#endif
  
PRI Pack(s, x, m) : single

'Pack floating-point from (sign, exponent, mantissa) at pointer

  if m                          'if mantissa 0, result 0
  
    result := 33 - >|m          'determine magnitude of mantissa
    m <<= result                'msb-justify mantissa without leading 1
    x += 3 - result             'adjust exponent

    m += $00000100              'round up mantissa by 1/2 lsb
    if not m & $FFFFFF00        'if rounding overflow,
      x++                       '..increment exponent
    
    x := x + 127 #> -23 <# 255  'bias and limit exponent

    if x < 1                    'if exponent < 1,
      m := $8000_0000 +  m >> 1 '..replace leading 1
      m >>= -x                  '..shift mantissa down by exponent
      x~                        '..exponent is now 0

    return s << 31 | x << 23 | m >> 9 'pack result

{{

┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                   TERMS OF USE: MIT License                                                  │                                                            
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    │ 
│files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    │
│modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software│
│is furnished to do so, subject to the following conditions:                                                                   │
│                                                                                                                              │
│The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.│
│                                                                                                                              │
│THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          │
│WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         │
│COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   │
│ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
}}    