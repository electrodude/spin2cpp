# Fast Propeller Basic compiler

## Language Syntax

### Comments

Comments start with `rem` or a single quote character, and go to the end of line

### Integers

Decimal integers are a sequence of digits, 0-9.

Hexadecimal (base 16) integers start with the sequence "0h" followed by digits and/or the letters A-F or a-f.

Binary (base 2) integers start with the sequence "0b" followed by the digits 0 and 1.

Numbers may contain underscores anywhere to separate digits; those underscores are ignored.

For example, the following are all ways to represent the decimal number `10`:
```
   10
   1_0
   0hA
   0h_a
   0b1010
```

### Keywords

Keywords are always treated specially by the compiler, and no identifier may be
named the same as a keyword. The keyword `rem` is special, in that those 3 characters start a comment, so no identifier may start with `rem`.
```
and
as
asm
byte
class
continue
declare
dim
direction
do
else
end
enum
exit
for
function
goto
if
input
let
local
long
loop
mod
next
or
output
print
program
real
rem
return
step
sub
then
to
until
wend
while
with
word
```

### Variables

Variable names consist of a letter, followed by any sequence of letters or digits. Underscores may be inserted anywhere in this sequence, but are ignored for purposes of comparing variables. Similarly, case is ignored. So the names `avar`, `a_var`, `AVar`, `A_VAR`, etc. all refer to the same variable.

Variables may have a type specifier appended to them. `$` indicates a string variable, `%` an integer variable, and `#` a floating point variable. The type specifier is part of the name, so `a$` and `a#` are different variables: the first is a string variable, and the second is a floating point variable. If no type specifier is appended, the variable is assumed to be an integer.

#### Variable Scope

There are two kinds of variables: global variables and local variables. Global variables may be accessed from anywhere in the program, and exist for the duration of the program. Local variables are only available inside the function or subroutine where they are declared, and only exist for as long as that function or subroutine is running. When the routine returns, the variables lose any values they had at the time. They are re-created afresh the next time the function is called.

### Function declarations

Function names follow the same rules as variable names. Like variable names, function names may have a type specifier appended, and the type specifier gives the type that the function returns.

```
function Add(a, b)
  return a+b
end function
```