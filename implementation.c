/*  

    Copyright 2018-21 by

    University of Alaska Anchorage, College of Engineering.

    Copyright 2022-24 by

    University of Texas at El Paso, Department of Computer Science.

    All rights reserved.

    Contributors:  ...
                   ...
		   ...                 and
		   Christoph Lauter

    See file memory.c on how to compile this code.

    Implement the functions __malloc_impl, __calloc_impl,
    __realloc_impl and __free_impl below. The functions must behave
    like malloc, calloc, realloc and free but your implementation must
    of course not be based on malloc, calloc, realloc and free.

    Use the mmap and munmap system calls to create private anonymous
    memory mappings and hence to get basic access to memory, as the
    kernel provides it. Implement your memory management functions
    based on that "raw" access to user space memory.

    As the mmap and munmap system calls are slow, you have to find a
    way to reduce the number of system calls, by "grouping" them into
    larger blocks of memory accesses. As a matter of course, this needs
    to be done sensibly, i.e. without wasting too much memory.

    You must not use any functions provided by the system besides mmap
    and munmap. If you need memset and memcpy, use the naive
    implementations below. If they are too slow for your purpose,
    rewrite them in order to improve them!

    Catch all errors that may occur for mmap and munmap. In these cases
    make malloc/calloc/realloc/free just fail. Do not print out any 
    debug messages as this might get you into an infinite recursion!

    Your __calloc_impl will probably just call your __malloc_impl, check
    if that allocation worked and then set the fresh allocated memory
    to all zeros. Be aware that calloc comes with two size_t arguments
    and that malloc has only one. The classical multiplication of the two
    size_t arguments of calloc is wrong! Read this to convince yourself:

    https://bugzilla.redhat.com/show_bug.cgi?id=853906

    In order to allow you to properly refuse to perform the calloc instead
    of allocating too little memory, the __try_size_t_multiply function is
    provided below for your convenience.
    
*/

#include <stddef.h>
#include <sys/mman.h>
#include <stdlib.h>

/* Predefined helper functions */

static void *__memset(void *s, int c, size_t n) {
  unsigned char *p;
  size_t i;

  if (n == ((size_t) 0)) return s;
  for (i=(size_t) 0,p=(unsigned char *)s;
       i<=(n-((size_t) 1));
       i++,p++) {
    *p = (unsigned char) c;
  }
  return s;
}

static void *__memcpy(void *dest, const void *src, size_t n) {
  unsigned char *pd;
  const unsigned char *ps;
  size_t i;

  if (n == ((size_t) 0)) return dest;
  for (i=(size_t) 0,pd=(unsigned char *)dest,ps=(const unsigned char *)src;
       i<=(n-((size_t) 1));
       i++,pd++,ps++) {
    *pd = *ps;
  }
  return dest;
}

/* Tries to multiply the two size_t arguments a and b.

   If the product holds on a size_t variable, sets the 
   variable pointed to by c to that product and returns a 
   non-zero value.
   
   Otherwise, does not touch the variable pointed to by c and 
   returns zero.

   This implementation is kind of naive as it uses a division.
   If performance is an issue, try to speed it up by avoiding 
   the division while making sure that it still does the right 
   thing (which is hard to prove).

*/
static int __try_size_t_multiply(size_t *c, size_t a, size_t b) {
  size_t t, r, q;

  /* If any of the arguments a and b is zero, everthing works just fine. */
  if ((a == ((size_t) 0)) ||
      (b == ((size_t) 0))) {
    *c = a * b;
    return 1;
  }

  /* Here, neither a nor b is zero. 

     We perform the multiplication, which may overflow, i.e. present
     some modulo-behavior.

  */
  t = a * b;

  /* Perform Euclidian division on t by a:

     t = a * q + r

     As we are sure that a is non-zero, we are sure
     that we will not divide by zero.

  */
  q = t / a;
  r = t % a;

  /* If the rest r is non-zero, the multiplication overflowed. */
  if (r != ((size_t) 0)) return 0;

  /* Here the rest r is zero, so we are sure that t = a * q.

     If q is different from b, the multiplication overflowed.
     Otherwise we are sure that t = a * b.

  */
  if (q != b) return 0;
  *c = t;
  return 1;
}

/* End of predefined helper functions */

/* Your helper functions 

   You may also put some struct definitions, typedefs and global
   variables here. Typically, the instructor's solution starts with
   defining a certain struct, a typedef and a global variable holding
   the start of a linked list of currently free memory blocks. That 
   list probably needs to be kept ordered by ascending addresses.

*/



/* Struct that represents memory blocks implemented as
  a linked list.*/
typedef struct memory_block_header {
    size_t size;
    int is_free;
  struct memory_block_header *next;
} memory_block_header_t;


/* This global variable holds the start of the linked list
   of free memory blocks */
static free_block_list = NULL;

/* This function checks that there is space left over
   to at least create a header after the size of memory
   the user wants is allocated.
   - Returns 1 if there is enough space for a header
   - Returns 0 if there is not enought space for a header
*/
int check_enough_space_for_header_after_allocation(memory_block_header_t *ptr,
						   size_t desired_size) {
  return (*ptr->size - desired_size - sizeof(memory_block_header_t) >= 0); 
}


/* This function makes a header and allocates the memory the user wants.
   We assume that there is enough space for the memory allocation
   and for the header.
   - Returns 1 if the making of the header and allocation was sucessful
   - Returns 0 if unsuccessful */
void make_header_and_allocate_memory(memory_block_header_t *start,
				     size_t desired_size) {
  memory_block_header_t * new_header;
  /* Figure out where the new header goes */
  new_header = start + sizeof(memory_block_header_t) + desired_size;

  /* Initialize its attributes */
  new_header->size = start->size - sizeof(memory_block_header_t) - desired_size;
  new_header->is_free = 1;
  if (start != NULL) {
    new_header->next = start->next;
  } else {
    new_header->next = NULL;
  }
    
  start->size = desired_size; // Update size
  start->next = new_header;
}


/* This function makes */
void make_headers_and_allocate_memory_first_allocation(memory_block_header_t *start,
						       size_t desired_size) {
  start->size = desired_size;
  start->is_free = 0;

  new_header = start + sizeof(memory_block_header_t) + desired_size;
   /* Initialize its attributes */
  new_header->size = sizeof(memory_block_header_t) + desired_size;
  new_header->is_free = 1;
  
  start->next = new_header;

  new_header->next = NULL;
}

/*
  This function returns a ptr to a block of memory
  of size 'size'.
   - It allocates new memory using mmap if there is
     no more space in our linked list to store memory
     of that size.
   - If there is space in our linked list, it returns
     a ptr to the first block in our list that is the
     beginning of a chunk of at least the size requested.
*/
memory_block_header_t get_ptr_next_memory_fit(size_t size) {
  void  *new_block;
  memory_block_header_t *cur = free_block_list;

  if (cur == NULL) {
    /* First time allocating memory */
    new_block = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    free_block_list = (memory_block_header_t) new_block;
    
    cur = free_block_list;
    cur->size = getpagesize();
    cur->is_free = 1;
    if(check_enough_space_for_header_after_allocation(cur, size)) {
      make_headers_and_allocate_memory_first_allocation((memory_block_header_t)cur,
							size);
      return (cur + sizeof(memory_block_header_t));
    }
    return NULL;
  }
  
  /* Iterate through memory blocks linked list */
  while (cur != NULL) {
    if (cur->is_free &&
	check_enough_space_for_header_after_allocation(cur, size)) {
      /* If the current block of memory is free and the size of the
	 memory we were asked for is less than available memory, then
	 we have found continuous memory block of the size we need.
      */
      make_header_and_allocate_memory(cur, size);
      return (cur + sizeof(memory_block_header)); // Do not include the header
    }
    // Go to next element in the linked list
    cur = cur->next;
  }

  /* After iterating through the entire linked list we see that the size
     of the memory we were asked for is more than the current available
     memory,so we need to allocate a fresh chunk of memory.
  */
  cur = free_block_list;
  new_block = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  cur->size = getpagesize();
  cur->is_free = 1;
  
  while (cur != NULL) {
    if (cur->next == NULL) {
      /* We want to insert this new chunk after the last header in
	 our current linked list.*/
      cur->next = new_block;
      make_headers_and_allocate_memory_first_allocation((memory_block_header_t)cur,
							size);
    }
  }
  
  
  return NULL;
}

/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);


void *__malloc_impl(size_t size) {
  if (size == 0) {
    /* If the size we want to allocate is zero, do nothing*/
    return NULL;
  }
  
  return get_ptr_to_next_fit(size);
}


void *__calloc_impl(size_t nmemb, size_t size) {
  void *allocated_block;
  size_t *multiplication_result;
  
  /* Get total space we need by multiplying nmmeb and size */
  if (__try_size_t_multiply(multiplication_result, nmemb, size) == 0) {
    printf("Failed at multiplying numbers of that size.\n");
    return NULL;  
  }
  /* Multiplication was sucessful, now we know how much space to allocate*/
  allocated_block = __malloc_impl(multiplication_result);
  
  if (allocated_block == NULL) {
    printf("Unable to allocate memory using malloc");
    return NULL;
  }

  /* Initialize every byte to zero */
  __memset(allocated_block, 0, size);
  return allocated_block;
}


void *__realloc_impl(void *ptr, size_t size) {
  /* When the size we are asked to reallocate is less */
  return NULL;  
}

void __free_impl(void *ptr) {
  /* STUB */
}

/* End of the actual malloc/calloc/realloc/free functions */
