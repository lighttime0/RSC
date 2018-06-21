#include <stdio.h>

int inc(int x)
{
	return x+1;
}

int dec(int x)
{
	return x-1;
}

int main()
{
	int a = 10;
	int b = inc(a);
	int c = dec(b);
	printf("%d\n", c);
}