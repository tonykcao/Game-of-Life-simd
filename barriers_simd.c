// Tony Cao
// GOL, C speed optimized attempt using pthread barriers and SIMD instructions

/*
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
/****************** Definitions **********************/
/* Two possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   (0)   // with no animation
#define OUTPUT_ASCII  (1)   // with ascii animation
#define SLEEP_USECS    (50000)

/* A global variable to keep track of the number of live cells in the
 * world (this is the ONLY global variable you may use in your program)*/
static int total_live = 0;
typedef char cellType;
typedef unsigned __int128 indexType; // overflow protection for turnUpdate

// SIMD support libs and macros
#define SIMDMODE 32 // 32 is 5% faster than 16, 64 is 2% faster than 32
#include <emmintrin.h> //SSE2: _mm_load_si128
#include <smmintrin.h> //SSE 4.1: _mm_testz_si128
#include <immintrin.h> //AVX: _mm256_load_si256 _mm256_testz_si256

struct gol_data {
    // NOTE: DO NOT CHANGE the names of these 4 fields (but USE them)
    unsigned int rows;  // the row dimension
    unsigned int cols;  // the column dimension
    unsigned int iters; // number of iterations to run the gol simulation
    int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI

    cellType* cellBoard;
    cellType* tempBoard;

    // for threading support
    unsigned int threadCount;
    pthread_mutex_t boardMutex;
};

typedef struct threadArgs {
    struct gol_data* data;
    int threadID;
    pthread_barrier_t* barrier;
} threadArgs;


/****************** Function Prototypes **********************/

/* init gol data from the input file and run mode cmdline args */
// data: gol game specific data, argv: commandline argv
// return: 0 if completed successfully, negative numbers if fails
int init_game_data_from_args(struct gol_data* data, char **argv);


/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   data: pointer to a struct gol_data initialized with
 *         all GOL game playing state
 */
void play_gol(struct gol_data* data);

// Waits for threads to process data for the turn, update game board
// and release threads for the next turn
//  data: gol game specific data
//  barrier: barrier for threads
void thread_turnUpdate(struct gol_data* data, pthread_barrier_t* barrier);

// Processess n round of game of life and updates tempBoard
//  args: thread specific data, expects data under threadArgs format
void* thread_boardUpdate(void* args);
void thread_slowCheck(struct gol_data* data, indexType index, indexType count);

// Toggle a cell to be live
// update their neighbors' neighbor count and total live w/ mutex protection
//  data: gol game specific data
//  index: coordinates of cell that needs to be changed
void cellOn(struct gol_data* data, indexType index);

// Toggle a cell to be dead
// update their neighbors' neighbor count and total live w/ mutex protection
//  data: gol game specific data
//  index: coordinates of cell that needs to be changed
void cellOff(struct gol_data* data, indexType index);

/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number*/
void print_board(struct gol_data *data, int round);

int main(int argc, char **argv) {
    int ret;
    struct gol_data data;
    double secs;

    struct timeval start_time, stop_time;
    /* check number of command line arguments */
    if (argc < 4) {
        printf("usage: %s <infile.txt> <output_mode>[0|1|2] <thread count>\n", argv[0]);
        printf("(0: no visualization, 1: ASCII)\n");
        exit(1);
    }
    if (atoi(argv[2])) {data.output_mode=OUTPUT_ASCII;}
    else               {data.output_mode=OUTPUT_NONE;}

    /* Initialize game state from input file */
    ret = init_game_data_from_args(&data, argv);
    if (ret != 0) {
        printf("Initialization error: file %s, mode %s\n", argv[1], argv[2]);
        exit(1);    
    }

    /* Invoke play_gol in different ways based on the run mode */
    if (data.output_mode == OUTPUT_NONE) {  // run with no animation
        ret = gettimeofday(&start_time, NULL);
        play_gol(&data);
        ret = gettimeofday(&stop_time, NULL);
    }
    else if (data.output_mode == OUTPUT_ASCII) { // run with ascii animation
        /* ASCII output: clear screen & print the initial board */
        if (system("clear")) {
            perror("clear");
            exit(1);
        }
        print_board(&data, 0);
        ret = gettimeofday(&start_time, NULL);
        play_gol(&data);
        ret = gettimeofday(&stop_time, NULL);

        // clear the previous print_board output from the terminal:
        if (system("clear")) {perror("clear"); exit(1); }

        print_board(&data, data.iters);
    }
    /* Print the total runtime, in seconds. */
    secs = (stop_time.tv_sec-start_time.tv_sec) + (stop_time.tv_usec-start_time.tv_usec)*0.000001;
    fprintf(stdout, "Total time: %0.3f seconds\n", secs);
    fprintf(stdout, "Number of live cells after %d rounds: %d\n\n", data.iters, total_live);
    // clean up
    free(data.cellBoard);
    free(data.tempBoard);
    pthread_mutex_destroy(&(data.boardMutex));
    return 0;
}

int init_game_data_from_args(struct gol_data *data, char **argv) {
    if (atoi(argv[3])>0) {data->threadCount = atoi(argv[3]);}
    else {
        printf("Invalid thread count arguments, terminating\n");
        exit(-1);
    }

    //init mutex for threading
    if (pthread_mutex_init(&(data->boardMutex), NULL) != 0) {
        printf("Mutex init failed, terminating\n");
        exit(-1);
    }
    // START READING
    FILE* infile=0;
    infile = fopen(argv[1], "r");
    if (!infile) {return -1;}
    if (fscanf(infile, "%i", &(data->rows))  != 1) {return -1;}
    if (fscanf(infile, "%i", &(data->cols))  != 1) {return -1;}
    if (fscanf(infile, "%i", &(data->iters)) != 1) {return -1;}
    if (fscanf(infile, "%i", &(total_live))  != 1) {return -1;}
    // PAUSE, MAKE LIST OF ALIVE CELLS
    indexType* liveCells = (indexType*)malloc(sizeof(indexType) * total_live*2);
    for (int i =0; i < total_live*2; i++) {
        if (fscanf(infile, "%llu", &(liveCells[i]))!=1) {return -6;}
    }
    // DONE READING
    indexType boardSize = sizeof(cellType) * data->rows * data->cols;
    if (boardSize%32) {boardSize += 32-boardSize%32;}

    data->cellBoard = (cellType*)aligned_alloc(32, boardSize);
    data->tempBoard = (cellType*)aligned_alloc(32, boardSize);
    if (data->cellBoard == NULL || data->tempBoard == NULL) {
        printf("Allocation error during board initialization, exiting...");
        exit(-1);
    }
    memset(data->cellBoard, 0, data->rows * data->cols);
    indexType row;
    indexType col;
    for (indexType i = 0; i<total_live*2; i+=2) {
        row = liveCells[i];
        col = liveCells[i+1];
        cellOn(data, (indexType)data->rows*row + col);
        total_live--;//adjusts for toggling in cellOn
    }
    //push changes
    memcpy(data->tempBoard, data->cellBoard, sizeof(cellType) * data->rows * data->cols);
    free(liveCells);
    fclose(infile);
    return 0;
}

void play_gol(struct gol_data* data) {
    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, data->threadCount+1)) {
        printf("Barrier init failed, terminating\n");
        exit(-1);
    }
    // making and activating threads
    pthread_t* threadPool = (pthread_t*)malloc(sizeof(pthread_t)*(data->threadCount));
    threadArgs* argsPool= (threadArgs*)malloc(sizeof(threadArgs)*(data->threadCount));
    // activate threads, turn 1 is now processing
    for (int i = 0; i < data->threadCount; i++) {
        argsPool[i].data = data;
        argsPool[i].threadID = i;
        argsPool[i].barrier = &barrier;
        pthread_create(&(threadPool[i]),NULL,thread_boardUpdate,(void*)&argsPool[i]);
    }
    if (data->output_mode==OUTPUT_NONE) {
        for (int round = 1; round <= data->iters; ++round) {
            thread_turnUpdate(data, &barrier);
        }
    }
    else if (data->output_mode==OUTPUT_ASCII) {
        for (int round = 1; round <= data->iters; ++round) {
            print_board(data, round);
            thread_turnUpdate(data, &barrier);
            system("clear");
            usleep(SLEEP_USECS);
        }
    }
    // stop all threads
    for (int i = 0; i < data->threadCount; i++) {
        pthread_join(threadPool[i],NULL);
    }
    free(threadPool);
    free(argsPool);
    pthread_barrier_destroy(&barrier);
}

void thread_turnUpdate(struct gol_data* data, pthread_barrier_t* barrier) {
    pthread_barrier_wait(barrier); // ensure threads from last turn finish
    memcpy(data->tempBoard, data->cellBoard, sizeof(cellType) * data->rows * data->cols);
    pthread_barrier_wait(barrier); // wake threads for next turn
}

void* thread_boardUpdate(void* t_args) {
    //parse args
    pthread_barrier_t* barrier = ((threadArgs*)t_args)->barrier;
    int threadID               = ((threadArgs*)t_args)->threadID;
    struct gol_data* data      = ((threadArgs*)t_args)->data;
    int threadCount            = data->threadCount;

    // calculating start and end point
    indexType start = (data->rows*data->cols)/(threadCount) * threadID;
    indexType end = (data->rows*data->cols)/(threadCount) * (threadID+1);
    if (threadID == (threadCount-1)) {
        end = data->rows*data->cols;
    } // in case threadcount is not a factor of no. of cells

    indexType index;
    cellType* const tempBoard = data->tempBoard; //minimize deref
    if (end-start<SIMDMODE || (SIMDMODE!=32 && SIMDMODE!=16)) {
        fprintf(stderr, "threadID: %d, shortcutting\n", threadID);
        for (int round = 1; round <= data->iters; ++round) {
            thread_slowCheck(data, start, end-start);
            pthread_barrier_wait(barrier);// block, wait for memory sync
            pthread_barrier_wait(barrier);// released, start next round
        }
    } else {
        __m256i vec32;
        __m128i vec16;
        for (int round = 1; round <= data->iters; ++round) {
            end-=end%SIMDMODE;
            start-=start%SIMDMODE;
            // last thread will have leftovers
            if (threadID == (threadCount-1)) {
                thread_slowCheck(data, end, (data->rows)*(data->cols)-end);
            }

            #if SIMDMODE==32 //32 cell per cycle, 5% faster than 16
                for (index = start;index < end;index+=32) {
                    vec32 = _mm256_load_si256((__m256i*)(tempBoard+index));
                    if (!_mm256_testz_si256(vec32,vec32)) { // next 32 is not empty
                        //if 1st 16 is not empty
                        vec16 = _mm_load_si128((__m128i*)(tempBoard+index));
                        if (!_mm_testz_si128(vec16,vec16)) {
                            thread_slowCheck(data, index, 16);
                        }
                        // if 2nd 16 is not empty
                        vec16 = _mm_load_si128((__m128i*)(tempBoard+index+16));
                        if (!_mm_testz_si128(vec16, vec16)) {
                            thread_slowCheck(data, index+16, 16);
                        }
                    }
                }
            #elif SIMDMODE==16// default simd, 3x faster than regular
                for (index=start;index < end;index+=16) {
                    // printf("%lld \n", index);
                    vec16 = _mm_load_si128((__m128i*)(tempBoard+index));
                    if (!_mm_testz_si128(vec16,vec16)) {
                        thread_slowCheck(data, index, 16);
                    }
                }
            #endif
            pthread_barrier_wait(barrier);// block, wait for memory sync
            pthread_barrier_wait(barrier);// released, start next round
        }
    }
    return NULL;
}

void thread_slowCheck(struct gol_data* data, indexType index, indexType count) {
    cellType cell;
    for (indexType i = index; i < index+count; i++) {
        cell = (data->tempBoard)[i];
        if (cell) {
            if (cell & 1 && !(cell == 5 || cell == 7)) {cellOff(data, i);}
                // ^alive, doesnt have 2/3 neighbors 
            else if (cell == 6) {cellOn(data, i);}// 3 neighbors, dead
        }
    }
}

void cellOn(struct gol_data* data, indexType index) {
    int row = (int) index/(data->rows);
    int col = (int) index%(data->rows);
    int rowAbove, rowBelow, colLeft, colRight;
    if (row == 0) {rowAbove = data->rows-1;} // loop to top
    else {rowAbove = row-1;}
    
    if (row == data->rows-1) {rowBelow = 0;} // loop to bottom
    else {rowBelow = row+1;}
    
    if (col == 0) {colLeft = data->cols-1;} // loop to right
    else {colLeft = col-1;}
    
    if (col == data->cols-1) {colRight = 0;} // loop to left
    else {colRight = col+1;}

    pthread_mutex_lock(&(data->boardMutex));
    data->cellBoard[index] |= 0x1;

    data->cellBoard[data->rows * rowAbove + colLeft] += 0x2;
    data->cellBoard[data->rows * rowAbove + colRight] += 0x2;
    data->cellBoard[data->rows * rowBelow + colLeft] += 0x2;
    data->cellBoard[data->rows * rowBelow + colRight] += 0x2;

    data->cellBoard[data->rows * row + colLeft] += 0x2;
    data->cellBoard[data->rows * row + colRight] += 0x2;
    data->cellBoard[data->rows * rowAbove + col] += 0x2;
    data->cellBoard[data->rows * rowBelow + col] += 0x2;
    total_live+=1;
    pthread_mutex_unlock(&(data->boardMutex));
}

void cellOff(struct gol_data* data, indexType index) {
    int row = (int) index/(data->rows);
    int col = (int) index%(data->rows);
    int rowAbove, rowBelow, colLeft, colRight;
    if (row == 0) {rowAbove = data->rows-1;} // loop to top
    else {rowAbove = row-1;}
    
    if (row == data->rows-1) {rowBelow = 0;} // loop to bottom
    else {rowBelow = row+1;}
    

    if (col == 0) {colLeft = data->cols-1;} // loop to right
    else {colLeft = col-1;}
    
    if (col == data->cols-1) {colRight = 0;} // loop to left
    else {colRight = col+1;}
    
    pthread_mutex_lock(&(data->boardMutex));
    data->cellBoard[index] &= ~0x1;

    data->cellBoard[data->rows * rowAbove + colLeft] -= 0x2;
    data->cellBoard[data->rows * rowAbove + colRight] -= 0x2;
    data->cellBoard[data->rows * rowBelow + colLeft] -= 0x2;
    data->cellBoard[data->rows * rowBelow + colRight] -= 0x2;

    data->cellBoard[data->rows * row + colLeft] -= 0x2;
    data->cellBoard[data->rows * row + colRight] -= 0x2;
    data->cellBoard[data->rows * rowAbove + col] -= 0x2;
    data->cellBoard[data->rows * rowBelow + col] -= 0x2;
    total_live-=1;
    pthread_mutex_unlock(&(data->boardMutex));
}

void print_board(struct gol_data* data, int round) {
    cellType cell;
    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round);
    // print out whole board
    for (int row = 0; row < data->rows; ++row) {
        for (int col = 0; col < data->cols; ++col) {
            cell = data->cellBoard[data->rows * row + col];
            if (cell & 1) {
                fprintf(stderr, " @");
            }
            else {
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}
