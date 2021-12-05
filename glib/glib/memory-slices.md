Title: Memory Slices

# Memory Slices

Memory slices provide a space-efficient and multi-processing scalable way to
allocate equal-sized pieces of memory, just like the original `GMemChunks`
(from GLib 2.8), while avoiding their excessive memory-waste, scalability
and performance problems.

To achieve these goals, the slice allocator uses a sophisticated, layered
design that has been inspired by Bonwick's slab allocator (_Jeff
Bonwick, The slab allocator: An object-caching kernel memory allocator.
USENIX 1994_, and _Bonwick and Jonathan Adams, Magazines and vmem:
Extending the slab allocator to many cpu's and arbitrary resources. USENIX
2001_)

It uses `posix_memalign()` to optimize allocations of many equally-sized
chunks, and has per-thread free lists (the so-called magazine layer) to
quickly satisfy allocation requests of already known structure sizes. This
is accompanied by extra caching logic to keep freed memory around for some
time before returning it to the system. Memory that is unused due to
alignment constraints is used for cache colorization (random distribution of
chunk addresses) to improve CPU cache utilization. The caching layer of the
slice allocator adapts itself to high lock contention to improve
scalability.

The slice allocator can allocate blocks as small as two pointers, and unlike
`malloc()`, it does not reserve extra space per block. For large block
sizes, `g_slice_new()` and `g_slice_alloc()` will automatically delegate to
the system `malloc()` implementation. For newly written code it is
recommended to use the new `g_slice` API instead of `g_malloc()` and
friends, as long as objects are not resized during their lifetime and the
object size used at allocation time is still available when freeing.

Here is an example for using the slice allocator:

```c
gchar *mem[10000];
gint i;

// Allocate 10000 blocks.
for (i = 0; i < 10000; i++)
  {
    mem[i] = g_slice_alloc (50);

    // Fill in the memory with some junk.
    for (j = 0; j < 50; j++)
      mem[i][j] = i * j;
  }

// Now free all of the blocks.
for (i = 0; i < 10000; i++)
  g_slice_free1 (50, mem[i]);
```

And here is an example for using the using the slice allocator with data
structures:

```c
GRealArray *array;

// Allocate one block, using the g_slice_new() macro.
array = g_slice_new (GRealArray);

// We can now use array just like a normal pointer to a structure.
array->data            = NULL;
array->len             = 0;
array->alloc           = 0;
array->zero_terminated = (zero_terminated ? 1 : 0);
array->clear           = (clear ? 1 : 0);
array->elt_size        = elt_size;

// We can free the block, so it can be reused.
g_slice_free (GRealArray, array);
```
