# Software Transactional Memory (STM) Library

**Note:** This project was created as my final project for CS-453 concurrent computing at EPFL during the Fall 2024 term.

## Performance:

My implementation of transactional memory achieves a performance improvement of approximately 6x compared to a coarse-grained locking solution.

![image](https://github.com/user-attachments/assets/306dde4b-53ac-400f-815c-2766b23eb113)


## Specification:
The **software transactional memory (STM) library** sees and controls every access to the shared memory. The library can delay execution, return two different values when reading the same address from two different concurrent transactions, etc. Essentially, the goal of the library is to make concurrent transactions appear as if they were executed serially, without concurrency.

### Core Notions of STM
To achieve the informal goal described above, STM relies on three core notions:
1. **Snapshot Isolation**
2. **Atomicity**
3. **Consistency**

#### 1. Snapshot Isolation
Ensures:
1. No **write/alloc/free** in a non-committed transaction can be read, become available, or unavailable in any other transaction.
2. All reads/writes/allocs/frees in a transaction appear to originate from the same atomic snapshot of the shared memory.
3. A transaction can only observe its own modifications (i.e., writes/allocs/frees).
   
#### 2. Atomicity
Ensures that all the memory writes/allocs/frees of any committed transaction seem to have all happened at one indivisible point in time.

#### 3. Consistency
For a committing transaction `T`, ensures:
1. None of the transactions that committed since `T`'s snapshot:
   - Freed a segment of memory read or written by `T`.
   - Allocated a segment overlapping with a segment allocated by `T`.
2. Each read made by `T` in its snapshot would yield the same value if taken after the last committed transaction.

By adhering to these principles, an STM library can ensure correct behavior in concurrent transactional systems.

## Implementation:

This transactional memory library implements the [TL2](https://dcl.epfl.ch/site/_media/education/4.pdf) algorithm.

The **TL2 (Transactional Locking 2)** algorithm is a widely used approach for **software transactional memory (STM)** that ensures **opacity**—a property where all transactions appear to execute sequentially, even when running concurrently.

### Key Concepts of TL2

1. **Read Validation:**
   - Every transaction maintains a local read set and validates it against the GVC to ensure it operates on a consistent snapshot.
   - If a conflict (e.g., a write from another transaction) is detected, the transaction must **retry**.
2. **Write Buffering:**
   - Writes are staged in a local buffer until the transaction successfully commits.
   - This prevents uncommitted changes from being visible to other transactions.
3. **Global Version Clock (GVC):**
   - A global counter that tracks the logical "version" of the shared memory.
   - Each transaction checks the GVC to determine the validity of its read set.
4. **Commit Phase:**
   - During commit, the transaction acquires locks for all its write locations.
   - The GVC is incremented, ensuring all subsequent transactions observe the committed changes.

### Why TL2 Works
- **Consistency:** Transactions always validate their read set before committing, ensuring they operate on a consistent snapshot of memory.
- **Atomicity:** The use of locks during the commit phase ensures that no partial changes are visible.
- **Performance:** TL2 minimizes contention by using a global clock and local buffering, making it efficient for many real-world STM use cases.

By combining read validation, write buffering, and careful use of locks, TL2 provides a practical and scalable solution for managing concurrent transactions in shared memory.

## Challenges:

This project was my first experience building code from the ground up to run concurrently, and it quickly taught me just how challenging writing correct concurrent code can be. The complexity lies in reasoning about the enormous number of possible states the program can occupy simultaneously. 

Debugging proved to be the most frustrating aspect. Traditional methods like print debugging are almost useless in a concurrent environment, and using tools like GDB for multithreaded programs is both complicated and requires very meticulous attention.

I learned that the most effective way to debug this type of code is by shifting to a theoretical examination (unless there is some better tool I am missing). Rather than relying on reacting to the output of my program, I focused on systematically analyzing the code to identify how it could break under specific states or conditions of the system. This approach not only improved my debugging process but also deepened my understanding of concurrent programming.

## Reflection:

Overall, this project was challenging, frustrating, and time-consuming, but it was also incredibly rewarding. The moment I got my code working and saw a 6x speedup over the single-lock implementation made all the effort worthwhile. Despite the difficulties, I genuinely enjoyed the process and am excited to continue exploring concurrent programming, improving my skills, and tackling even more complex problems in the future.

## Repository:

This repository provides:
* examples of how to use synchronization primitives (in `sync-examples/`)
* a reference implementation (in `reference/`) (course grain locking)
* The actual implementation (in `394984/`) (TL2)
* the program that tests the implementation (in `grading/`)
* a tool to submit the implementation (in `submit.py`)
