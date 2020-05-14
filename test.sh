#!/bin/bash
assert() {
	expected="$1"
	input="$2"

	./chibicc "$input" > tmp.s
	cc -o tmp tmp.s
	./tmp
	actual="$?"

	if [ "$actual" = "$expected" ]; then
		echo "$input => $actual"
	else
		echo "$input => $expected expected but got $actual"
		exit 1
	fi
}

assert 0 "return 0;"
assert 42 "return 42;"
assert 21 "5+20-4;"
assert 41 " 12 + 34 - 5 ;"
assert 47 "5+6*7;" 
assert 15 "5*(9-6);"
assert  4 "(3+5)/2;"
assert 12 "-4 + +16;"
assert 6 "-4 + 10;"

# <, <=, >, >= ==, != test
assert 0 '0==1;'
assert 1 "42==42;"
assert 1 "0!=1;"
assert 0 "42!=42;"

assert 1 "0<1;"
assert 0 "1<1;"
assert 0 "2<1;"
assert 1 "0<=1;"
assert 1 "1<=1;"
assert 0 "2<=1;"

assert 1 "1>0;"
assert 0 "1>1;"
assert 0 "1>2;"
assert 1 "1>=0;"
assert 1 "1>=1;"
assert 0 "1>=2;"

assert 1 "return 1; 2; 3;"
assert 2 "1; return 2; 3;"
assert 3 "1; 2; return 3;"

# variable
assert 3 "foo=3; return foo;"
assert 8 "foo123=3; bar=5; return foo123 + bar;"
assert 8 "a=4; a=8; return a;"

# if statement
assert 3 "if(0) return 2; return 3;"
assert 3 "if(1-1) return 2; return 3;"
assert 2 "if(1) return 2; return 3;"
assert 2 "if(2-1) return 2; return 3;"
assert 2 "if(1) return 2; else return 3;" 
assert 3 "if(0) return 2; else return 3;" 

echo OK

