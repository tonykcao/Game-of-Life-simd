# GOL

Inspired by a speed challege posted for a Game of Life lab from Intro to Computer Systems course at Swarthmore College.

### Notes
This implementation applies threading, AVX2 SIMD instructions, and bit masks to achieve 100 to 120 times faster performance than initial implementation, depending on world size.

For progress notes, please take a look at ChallengeNotes.adoc.
Times provided there were tested on a Linux system with Intel Xeon E5-1650 (6 cores, 12 threads).

Code rewritten from a coursework private repo to remove dependency of a visualization library. Program structure is retained from coursework implementation. Other intermediate versions available by request!

## Compilation & Usage

Compile with GNU Make

```bash
make all
```
To run provided challenge files: 
``` bash
#./barrsimd <file name> <0|1, enables or disables ASCII> <thread count>
# Example:
./barrsimd ./challenges/challenge_500.txt 0 12
# runs challenge_500.txt with no ASCII animation and 12 threads.
```
To create a new challenge file, provide a text file with game board height, width, turn count, no. of live cells at start, and coordinates of each cell.

## Contact
Feel free to open an issue or reach out to kcao1@swarthmore.edu or tonykcao@gmail.com for any questions!


## Challenge notes:

BEFORE:
    base code, -o3 flag
    
    6.6s for 1k x 1k
    
    27.1s for 2k x 2k

NOTE:

    OPTIMIZATION 1: 8x faster than base code
        > Bit operations + store neighbor count in every cell
            -added typedef cellType to quickly change type stored
                + char performed quite a bit better than int
            - removed malloc on every turn, simply keep around a tempBoard that is
                malloc-ed in init, free when program exits (reduced memory use significantly)
                + this was found when testing 4k x 4k took significantly longer than expected
                with massive system time according to fish shell time command 
        - results sample:
            + challenge_500.txt 0  ->  0.21   secs
            + challenge_1000.txt 0 ->  0.85   secs
            + challenge_2000.txt 0 ->  3.34   secs
            + challenge_4000.txt 0 -> 13.42   secs
            
    OPTIMIZATION 2: ~4x faster than OPT 1, ~16x faster than original on 4k
        > changed makefile to -O1 flag, runs about 15% faster than -O3
        > removed getIndex function dependency
            - this was a simple math ops that was called massive amounts
              of times (soaked up ~40% of runtime according to gprof)
        - results sample:
            + challenge_500.txt 0   ->  0.060   secs
            + challenge_1000.txt 0  ->  0.231   secs
            + challenge_2000.txt 0  ->  0.850   secs
            + challenge_4000.txt 0  ->  3.438   secs
            + challenge_8000.txt 0  -> 13.651   secs
            + challenge_16000.txt 0 -> 55.174   secs

    OPTIMIZATION 3: 15%-25% faster than OPT 2, ~35x faster than original on 4k
        > changed cellOn and cellOff to take raw index of cell instead of coords
        > theory: we only need cell coords if it needs modifying, this can be
            calculated in cellOn/cellOff
        > speed up with more empty cells
        > results sample: (single thread)
            + challenge_500.txt 0   ->  0.050   secs
            + challenge_1000.txt 0  ->  0.181   secs
            + challenge_2000.txt 0  ->  0.681   secs
            + challenge_4000.txt 0  ->  2.045   secs
            + challenge_8000.txt 0  ->  8.427   secs
            + challenge_16000.txt 0 -> 45.926   secs

    (5 threads)
    OPTIMIZATION 4/4.5: 15%-2x faster than OPT 3, estimate ~45-50x faster than original on 4k
        > IMPLEMENTED THREADING (sample time taken at 5 threads)
            + IMPLEMENTED BARRIER FOR THREADS
                - 5-10% faster than OPT 4, ~50x faster than original on 4k
                    estimate to be about 70x faster than original at 16k x 16k
        > significantly faster compared to OPT 3 on 16k, minor on small boards
        > results sample:               create/join     |     barrier
            + challenge_1000.txt 0  ->  0.244   secs    |   0.164   secs
            + challenge_2000.txt 0  ->  0.650   secs    |   0.569   secs
            + challenge_4000.txt 0  ->  2.092   secs    |   1.917   secs
            + challenge_8000.txt 0  ->  9.687   secs    |   7.634   secs
            + challenge_16000.txt 0 -> 25.380   secs    |  22.275   secs
            + challenge_25600.txt 0 -> 55.445   secs    |  50.940   secs

    (5 threads)
    OPTIMIZATION 5: 2x - 30~40% faster than OPT 4/4.5, estimate ~90-100x faster than original on 4k
        estimate to be about 100-120x faster than original at 16k x 16k
        > IMPLEMENTED SIMD INSTRUCTIONS (AVX2+SSE)
        > single thread impl achieved massive 3x speed up, could match threaded impls at small boards, 
                doesnt scale as well with massive boards
        > SAMPLE RUN AT 12 THREADS: 46.832s for challenge_32000.txt with barrier
        > results sample:               single thread   |     create/join   |     barrier
            + challenge_1000.txt 0  ->  0.108   secs    |   0.135   secs    |   0.096   secs
            + challenge_2000.txt 0  ->  0.316   secs    |   0.378   secs    |   0.319   secs
            + challenge_4000.txt 0  ->  1.247   secs    |   1.201   secs    |   1.144   secs
            + challenge_8000.txt 0  ->  4.724   secs    |   4.063   secs    |   3.828   secs
            + challenge_16000.txt 0 -> 18.631   secs    |  16.341   secs    |  15.680   secs
            + challenge_25600.txt 0 -> 47.777   secs    |  36.836   secs    |  34.507   secs
            + challenge_32000.txt 0 -> 74.390   secs    |  55.495   secs    |  53.121   secs
    

Thoughts: 
    
    NOT APPLIED:
    0. MEMOIZE
    test challenges are simply 1 oscilator, this could be exploited

    (kinda cheating ?)
    from chess engine transposition table:
    - keep list of live cells coords, compute next generation and put both
        into a hash table
    - if current generation found in hash table, retrieve future gen state
        - short circuit: if turn%2 = iters%2,
            check if future gen is in hash table and will result in current gen
            if yes: end the game immediately, we know its a loop and game will
            end exactly as it is now

    1. bit operations & memory (APPLIED)
    - every cell is a char for minimum memory
        - bit 0 (right most) keeps track if cell is alive, 1 if alive
        - bit 3-1 keeps track of neighbor count
            - >>1 on a cell would yield neighbor count immediately,
            allowing fast neighbor count check
        - if a cell is toggled to be alive, add 2 to surrounding cells
            - sub 2 if live cell dies
        - can directly compare cell to 0 (no neighbor and dead) to skip computations
    
    2. only consider living cells and neighbors
    - keep hash table of coords to living cells and their neighbors
    - only update these cells, we only consider 15 cells every round for
    these test files
    - might consume lots of memory on real simulations? (nvm i realized max
    mem consumtion is 2 times keeping 1 single board)
    - i didnt use this because i didnt want to use a lib or implement a table

    3. threading (APPLIED)
    - make a function that only update nth row on a new table
    - use a thread pool to handle row checks
    - each thread must be working a row thats is at least 2 rows away from another
    thread to avoid race conditions (each cell update may adjust row above or below)
    - ALT: all threads only queue cellOn/cellOff tasks
        when finished tasks are excecuted by the main thread
        no mutex lock needed or avoiding race con


## License
[MIT](https://choosealicense.com/licenses/mit/)
