# Lamp

Lamp is a library of concurrent data structures and synchronization primitives implemented for learning purpose.

TODO:
- Add `try_lock()` method to the `Lock` interface
- `TOLock` still has memory leak
- `OptimisticList`, `LazyList`, `FreeList` only reclaims nodes's memory in the destructor. We can implement safe memory reclamation scheme to free memory sooner

## Data Structures
This project implements various concurrent data structures based on seminal research in the field for learning purpose

### List
- `CoarseList`: A concurrent list implementation that uses a single lock to guard the entire list. While simple, it offers limited concurrency as operations must acquire exclusive access to the entire structure.
- `FineList`: A fine-grained locking implementation that uses lock coupling (hand-over-hand locking) [[Bay77]](#Bay77) to allow multiple threads to access different parts of the list concurrently. Each node has its own lock, improving parallelism compared to coarse-grained locking.
- `LazyList`: An optimistic concurrency control [[Hel05]](#Hel05) implementation that separates logical deletion from physical removal. It uses a two-phase approach where nodes are first marked as deleted (logical removal) before being unlinked from the list (physical removal), allowing for greater concurrency.
- `LockFreeList`: a lock-free container that contains a sorted set of unique objects. This data structure is based on the solution proposed by Michael [[Mic02]](#Mic02) which builds upon the original proposal by Harris [[Har01]](#Har01).

## References
| Citation ID | Reference |
| ----------- | --------- |
| <a id="Bay77"></a> [Bay77] | R. Bayer, M. Schkolnick, Concurrency of operations on B-trees, Acta Informatica 9 (1977) 1–21. |
| <a id="Har01"></a> [Har01] | Tim Harris, [A pragmatic implementation of non-blocking linked-lists](https://timharris.uk/papers/2001-disc.pdf), in: Proceedings of 15th International Symposium on Distributed Computing, DISC 2001, Lisbon, Portugal, in: Lecture Notes in Computer Science, vol. 2180, Springer Verlag, October 2001, pp. 300–314. |
| <a id="Hel05"></a> [Hel05] | S. Heller, M. Herlihy, V. Luchangco, M. Moir, W.N. Scherer III, N. Shavit, [A lazy concurrent list-based set algorithm](https://people.csail.mit.edu/shanir/publications/Lazy_Concurrent.pdf), in: Proc. of the Ninth International Conference on Principles of Distributed Systems, OPODIS 2005, 2005, pp. 3–16. |
| <a id="Mic02"></a> [Mic02] | Maged M. Michael, [High performance dynamic lock-free hash tables and list-based sets](https://dl.acm.org/doi/pdf/10.1145/564870.564881), in: Proceedings of the Fourteenth Annual ACM Symposium on Parallel Algorithms and Architectures, ACM Press, 2002, pp. 73–82. |
