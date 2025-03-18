# Lamp

Lamp is a library of concurrent data structures and synchronization primitives developed while following the book "The Art of Multiprocessor Programming".

TODO:
- Add `try_lock()` method to the `Lock` interface
- `TOLock` still has memory leak
- `OptimisticList` and `LazyList` only reclaims nodes's memory in the destructor. We can implement safe memory reclamation scheme to free memory sooner