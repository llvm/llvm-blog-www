#include <stdlib.h>

__attribute__((noinline))
int work(int x) {
  return x * 3 + (x >> 2);
}

void process(int *a, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = work(a[i]);
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 10000000;
  int *a = (int *)malloc(n * sizeof(int));
  for (int i = 0; i < n; ++i)
    a[i] = i & 0xff;
  for (int run = 0; run < 10; ++run)
    process(a, n);
  int sum = 0;
  for (int i = 0; i < n; ++i)
    sum += a[i];
  free(a);
  return sum & 1;
}
