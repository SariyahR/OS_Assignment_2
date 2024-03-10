#include <stddef.h>
#include <stdio.h>
 
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

void print_line() {
  printf("--------------------------------------------------\n");
}
 
int main(int argc, char *argv[]) {
  int *int_array1, *int_array2;
  
  /* Test 1: Simple allocation using malloc */
  print_line();
  printf("Test 1: Simple allocation of int array of size 10\n");
  print_line();

  int_array1 = (int *)malloc(10 * sizeof(int));
  if (int_array1 != NULL) {
    printf("Memory is allocated at: %p\n", int_array1);
    for (int i = 0; i < 10; i++) {
      int_array1[i] = i * i;
    }
    for (int i = 0; i < 10; i++) {
      printf("Element %d in the int array: %d\n", i, int_array1[i]);
    }
    free(int_array1);
  } else {
    printf("Failed to allocate memory\n");
  }

  /* Test 2: Simple allocation and initilization using calloc */
  print_line();
  printf("Test 2: Calloc to initialize memory\n");
  print_line();
  int_array2 = (int *)calloc(30, sizeof(int)); 
    if (int_array2 != NULL) {
        printf("Memory allocated and initialized at address: %p\n", int_array2);
	printf("Check that all elements are set to zero by adding all of them\n");
        int sum = 0;
        for (int i = 0; i < 25; ++i) {
            sum += int_array2[i];
        }
        printf("Sum of initialized values: %d (must be 0)\n", sum);
        free(int_array2);
    } else {
        printf("Calloc failed\n");
    }
 
  return 0;
}
