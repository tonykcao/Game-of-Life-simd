# GOL

Inspired by a speed challege posted for a Game of Life lab from Data Struct & Algo class.

### Notes
This implementation applies threading, AVX2 SIMD instructions, and bit masks to achieve 85 to 100 times faster performance than initial implementation.

For progress notes, please take a look at ChallengeNotes.adoc.
Times provided there were tested on a Linux system with Intel Xeon E5-1650 (6 cores, 12 threads)

Code rewritten from
https://github.swarthmore.edu/CS31-F22/Lab6-kcao1-wrisse1 (coursework private repo)
to remove dependency of a visualization library. Program structure is retained from coursework implementation.

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
Feel free to open an issue or reach out to kcao1@swarthmore.edu for any questions!

## License
[MIT](https://choosealicense.com/licenses/mit/)