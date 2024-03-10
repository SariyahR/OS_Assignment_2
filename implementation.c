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
#include <stdio.h>
#include <unistd.h>

#include <string.h>

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
static memory_block_header_t *free_block_list = NULL;


/* This function writes len bytes from the buffer buf
   to the file descriptor fd.

   It uses write() to accomplish this job.

   It returns 0 if everything went well.
   It returns a negative number (-1) otherwise.
*/
int my_write(int fd, const void *buf) {
  size_t bytes_to_be_written;
  size_t bytes_already_written;
  size_t bytes_written_this_time;
  size_t len = 0;
  size_t i = 0;
  ssize_t write_res;

  /* Count the length fo the string */
  if (buf != NULL) {
    const char *char_buf = (const char *)buf; 
    while (char_buf[i] != '\0') {
      len += 1;
      i += 1;
    }
  }

  /* Loop until all bytes have been written */
  for (bytes_already_written = (size_t) 0, bytes_to_be_written = len;
       bytes_to_be_written > (size_t) 0;
       bytes_already_written += bytes_written_this_time,
       bytes_to_be_written -= bytes_written_this_time) {
    
    /* Using write() function.

       Returns the number of bytes actually written if sucessful.
       Returns -1 if an error occurs.
    */
    write_res = write(fd, &(((char *)buf)[bytes_already_written]),
		      bytes_to_be_written);
    if (write_res < (ssize_t) 0) {
      /* Condition failure, write() returns a negative number*/
      return -1;
    }
    bytes_written_this_time = write_res;
  }
  return 0;
}

/* This function checks that there is space left over
   to at least create a header after the size of memory
   the user wants is allocated.
   - Returns 1 if there is enough space for a header
   - Returns 0 if there is not enought space for a header
*/
int check_enough_space_for_header_after_allocation(memory_block_header_t *ptr,
						   size_t desired_size) {
  return (ptr->size - desired_size - sizeof(memory_block_header_t) >= 0); 
}


/* This function makes a header and allocates the memory the user wants.
   We assume that there is enough space for the memory allocation
   and for the header.
   - Returns 1 if the making of the header and allocation was sucessful
   - Returns 0 if unsuccessful */
void make_header_and_allocate_memory(memory_block_header_t *start,
				     size_t desired_size) {
  memory_block_header_t *new_header;
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
  start->is_free = 0;
  start->next = new_header;
}


/* This function makes a new header and allocates spaces
   when we haven't done it before for that chunk of memory.*/
void make_header_and_allocate_memory_first_allocation(memory_block_header_t *start,
						       size_t desired_size) {
  size_t old_size = start->size;
  memory_block_header_t *new_header;
  
  start->size = desired_size;
  start->is_free = 0;

  new_header = start + sizeof(memory_block_header_t) + desired_size;
   /* Initialize its attributes */
  new_header->size = old_size - sizeof(memory_block_header_t) -sizeof(memory_block_header_t);
  new_header->is_free = 1;
  
  start->next = new_header;

  new_header->next = NULL; // Because we know it is first allocation
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
void *get_ptr_next_memory_fit(size_t size) {
  void *memory;
  memory_block_header_t *new_block;
  memory_block_header_t *cur = free_block_list;

  if (cur == NULL) {
    my_write(1, "free_block_list is null, so this is our first time allocating memory");
    /* First time allocating memory */
    memory = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(memory == MAP_FAILED) {
      return NULL;
    }

    new_block = (memory_block_header_t *)memory;
    new_block->size = getpagesize() - sizeof(memory_block_header_t);
    new_block->is_free = 1;
    new_block->next = NULL;
    free_block_list = new_block;

    if(check_enough_space_for_header_after_allocation(new_block, size)) {
      make_header_and_allocate_memory_first_allocation(new_block, size);
      return (void *)((char *)new_block + sizeof(memory_block_header_t)); 
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
      return (void *)((char *)cur + sizeof(memory_block_header_t)); // Do not include the header
    }
    // Go to next element in the linked list
    cur = cur->next;
  }

  /* After iterating through the entire linked list we see that the size
     of the memory we were asked for is more than the current available
     memory,so we need to allocate a fresh chunk of memory.
  */
  cur = free_block_list;
  memory = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(memory == MAP_FAILED) {
      return NULL;
  }
  new_block = (memory_block_header_t *)memory;

  while (cur != NULL) {
    if (cur->next == NULL) {
      /* We want to insert this new chunk after the last header in
	 our current linked list.*/
      make_header_and_allocate_memory_first_allocation(new_block, size);
      cur->next = new_block;
      return (void *)((char *)new_block + sizeof(memory_block_header_t)); 
    }
  }
  
  
  return NULL;
}

/* End of your helper functions */

/* Start of the actual malloc/calloc/realloc/free functions */

void __free_impl(void *);


void *__malloc_impl(size_t size) {
  my_write(1, "HELLOOOOOOOOOOOO\n");
  if (size == 0) {
    /* If the size we want to allocate is zero, do nothing*/
    return NULL;
  }
  
  return get_ptr_next_memory_fit(size);
}


void *__calloc_impl(size_t nmemb, size_t size) {
  void *allocated_block;
  size_t multiplication_result;
  
  /* Get total space we need by multiplying nmmeb and size */
  if (__try_size_t_multiply(&multiplication_result, nmemb, size) == 0) {
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
  void *new_ptr;
  memory_block_header_t *header;
  
  /* If ptr is NULL, behaves like malloc(size) */
  if (ptr == NULL) {
    return __malloc_impl(size);
  }
  
  /* If size is 0, behaves like free(ptr) and returns NULL */
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  
  /* Look at the header struct that controls the current
     memory block we want to free. */
  header = (memory_block_header_t *)((char *)ptr - sizeof(memory_block_header_t));

  if(header->size == size) {
    /* If the reallocation size is the same as before, return
       then original pointer */
    return ptr;
  }

  if (header->size > size) {
    /* Allocate a new memory block of the requested size */
    new_ptr = __malloc_impl(size);
    if (new_ptr == NULL) {
      return NULL;
    }
    
    /* Copy the contents from the old memory block to the new memory block
       The minimum size to copy is the minimum of the old and new sizes
    */
    __memcpy(new_ptr, ptr, size);

    /* Free the old memory block */
    free(ptr);

    return new_ptr;
  }
  return NULL;
}

void __free_impl(void *ptr) {
  memory_block_header_t *header, *prev_header, *next_header;

  if (ptr != NULL) {
    /* Nothing to free */
    return; 
  }
  
  /* Look at the header struct that controls the current
     memory block we want to free. */
  header = (memory_block_header_t *)((char *)ptr - sizeof(memory_block_header_t));

  /* Free chunk of memory by setting its header is_free
     attribute to 1 */
  header->is_free = 1;

  /* Handle the case where we need to coalesce either the now free
     memory block with the previous memory block if it is free
     or the consequent block if it is free */

  /* Check next header:
     - If it exists
     - If it is free
     - And if both headers are in a continuous chunk of memory
  */
  next_header = (memory_block_header_t *)((char *)header + sizeof(memory_block_header_t) + header->size);
  if (next_header != NULL && header->next->is_free &&
     ((char *)next_header == ((char *)header) + sizeof(memory_block_header_t) + header->size)) {
    /* We must merge the two blocks to create one big block */
    header->next = next_header->next;
    header->size += sizeof(memory_block_header_t) + next_header->size;
  }

  /* Check previous header:
     - If it exists
     - If it is free
     - And if both headers are in a continuous chunk of memory
  */
  prev_header = free_block_list;
  while (prev_header->next != header) {
    prev_header = prev_header->next;
  }
  if (prev_header != NULL && prev_header->is_free &&
      ((char *)header == ((char *)prev_header) + sizeof(memory_block_header_t) + prev_header->size)) {
      /* We must merge the two blocks to create one big block */
      prev_header->size += sizeof(memory_block_header_t) + header->size;
      prev_header->next = header->next;
  }
}

/* End of the actual malloc/calloc/realloc/free functions */
