#!/bin/bash
# tmp2.oにテスト用の関数を用意しておく
cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
EOF

assert() {
	expected="$1"
	input="$2"

	./chibicc "$input" > tmp.s
	gcc -static -o tmp tmp.s tmp2.o
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

# while statement
assert 10 "i=0; while(i<10) i=i+1; return i;"

# for statement
assert 55 "i=0; j=0; for(i=0; i<=10; i=i+1) j=i+j; return j;"
assert 3 "for(;;) return 3; return 5;"

# block statement
assert 3 "{1; {2;} return 3;}"
assert 55 "i=0; j=0; while(i<=10) { j=i+j; i=i+1; } return j;"

# function call
assert 3 "return ret3();"
assert 5 "return ret5();"
assert 8 "return add(3, 5);"
assert 2 "return sub(5, 3);"
assert 21 "return add6(1, 2, 3, 4, 5, 6);"
assert 6 "return add(1, add(2, 3));"

echo OK
