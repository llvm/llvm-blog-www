#include <stdio.h>
#include <stdlib.h>

int hot_work(int *a, int n) {
  int s = 0;
  for (int i = 0; i < n; ++i)
    s += a[i] * 2;
  return s;
}

int cold_work(int *a, int n) {
  int s = 0;
  for (int i = 0; i < n; ++i)
    s += a[i] * 3;
  return s;
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 100;
  int *a = (int *)malloc(n * sizeof(int));
  for (int i = 0; i < n; ++i)
    a[i] = i & 0xff;

  int total = 0;
  for (int run = 0; run < 1000000; ++run)
    total += hot_work(a, n);
  for (int run = 0; run < 100; ++run)
    total += cold_work(a, n);

  printf("%d\n", total);
  free(a);
  return 0;
}
