/*
 * Swarthmore College, CS 31
 * Copyright (c) 2022 Swarthmore College Computer Science Department,
 * Swarthmore PA
 */

/*
* Siddhant Ranka
* Leia Donaway
* This program takes in files containing information for boards of 
* static or moving patterns of Conway's Game of Life and displays the results
* after multiple rounds in one of three ways specified by the user.
* Now, we use threads to execute the program on different parts of the board.
* The purpose of the threads is to speed up our program.
*/

/*
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation
 * ./gol file1.txt  2  # run with config file file1.txt, ParaVis animation
 *
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   (0)   // with no animation
#define OUTPUT_ASCII  (1)   // with ascii animation
#define OUTPUT_VISI   (2)   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 * Change this value to make the animation run faster or slower
 */
#define SLEEP_USECS    (1000000)

/* A global variable to keep track of the number of live cells in the
 * world (this is the ONLY global variable you may use in your program)
 */
static int total_live = 0;

static pthread_mutex_t my_mutex;

static pthread_barrier_t my_barrier;

/* This struct represents all the data you need to keep track of your GOL
 * simulation.  Rather than passing individual arguments into each function,
 * we'll pass in everything in just one of these structs.
 * this is passed to play_gol, the main gol playing loop
 */
struct gol_data {

    int rows;  // the row dimension
    int cols;  // the column dimension
    int iters; // number of iterations to run the gol simulation
    int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI
    int ntids;
    int partitionMode;
    int id;
    int printMode;
    int *arrayNew;
    //int *current;

    int *array;
    //int *next;
    int numberAlive;


    /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
    // NOTE: DO NOT CHANGE their definitions BUT USE these fields
    visi_handle handle;
    color3 *image_buff;
};

/****************** Function Prototypes **********************/

/* the main gol game playing loop (prototype must match this) */
void *play_gol(void *args);

/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char **argv);

/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);

int checkNeighbor(struct gol_data *data, int i, int j);

int neighborCheck(struct gol_data *data, int index);

void functionColor(struct gol_data *data, int index, int buff_i);
void partition(int thread, int threadNum, int rowNum, int colNum, int method, int *start, int *end);


/************ Definitions for using ParVisi library ***********/
/* initialization for the ParaVisi library (DO NOT MODIFY) */
int setup_animation(struct gol_data* data);
/* register animation with ParaVisi library (DO NOT MODIFY) */
int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";

int main(int argc, char **argv) {

    int ret;
    struct gol_data data;
    double secs;
    struct gol_data *data1;

    struct timeval start_time, stop_time;
    int ntids;
    int time_val;
    int threadHolder;
    

    pthread_t *tids;

    /* check number of command line arguments */
    if (argc < 6) {
        printf("usage: %s <infile.txt> <output_mode>[0|1|2]\n", argv[0]);
        printf("(0: no visualization, 1: ASCII, 2: ParaVisi)\n");
        exit(1);
    }

    if(atoi(argv[2]) != 0 && atoi(argv[2]) != 1 && atoi(argv[2]) != 2){
        printf("invalid mode type. Mode is either 0, 1, or 2.\n");
        exit(1);
    }
    //gets total number of threads
    ntids = atoi(argv[3]);
    if(pthread_barrier_init(&my_barrier, NULL, ntids)){
      printf("pthread_barrier_init error\n");
      exit(1);
    }
    data.partitionMode = atoi(argv[4]);
    
    

    /* Initialize game state (all fields in data) from information
     * read from input file */
    
    ret = init_game_data_from_args(&data, argv);
    if (ret != 0) {
        printf("Initialization error: file %s, mode %s\n", argv[1], argv[2]);
        exit(1);
    }

    /* initialize ParaVisi animation (if applicable) */
    if (data.output_mode == OUTPUT_VISI) {
        data.handle = init_pthread_animation(ntids, data.rows, data.cols, visi_name);
        data.image_buff = get_animation_buffer(data.handle);
        if(data.image_buff == NULL) {
            printf("ERROR get_animation_buffer returned NULL\n");
            exit(1);
        }
    }

    /* ASCII output: clear screen & print the initial board */
    if (data.output_mode == OUTPUT_ASCII) {
        if (system("clear")) { perror("clear"); exit(1); }
        print_board(&data, 0);
    }

    data.printMode = atoi(argv[5]);
    //allocates memory for our threads
    tids = malloc(sizeof(pthread_t) * ntids);
    data1 = malloc(sizeof(struct gol_data)*ntids);
    data.ntids = ntids;
   

    //This is how we calculate our time interval
    time_val = gettimeofday(&start_time, NULL);
    if(time_val == -1){
      printf("Timing is inaccurate. The user should end the program.\n");
    }

    if(pthread_mutex_init(&my_mutex, NULL)){
      printf("mutex error");
      exit(1);
    }

    
    /* Invoke play_gol in different ways based on the run mode */
    if (data.output_mode == OUTPUT_NONE) {  // run with no animation

      for(int i = 0; i < ntids; i++){

        data.id = i;
        data1[i] = data;
        threadHolder = pthread_create(&tids[i], 0, play_gol, &data1[i]);
       
      }

    }
    else if (data.output_mode == OUTPUT_ASCII) { // run with ascii animation

        for(int i = 0; i < ntids; i ++){

          data.id = i;
          data1[i] = data;
          threadHolder = pthread_create(&tids[i], 0, play_gol, &data1[i]);
          if(threadHolder){
            perror("Error pthread create");
            exit(1);
          }
          
        }
        

        // clear the previous print_board output from the terminal:
        // (NOTE: you can comment out this line while debugging)
        if (system("clear")) { perror("clear"); exit(1); }

        // NOTE: DO NOT modify this call to print_board at the end
        //       (it's to help us with grading your output)
        
    }
    else {  // OUTPUT_VISI: run with ParaVisi animation
            // tell ParaVisi that it should run play_gol
      //connect_animation(play_gol, &data);
      // start ParaVisi animation
      for(int i = 0; i < ntids; i++){

        data.id = i;
        data1[i] = data;
        threadHolder = pthread_create(&tids[i], 0, play_gol, &data1[i]);
        
      }
        run_animation(data.handle, data.iters);
      
    }
    //joins our threads
    for(int i = 0; i < ntids; i++){
      pthread_join(tids[i], 0);
    }

    if(data.output_mode == OUTPUT_ASCII){

      system("clear");
      print_board(&data, data.iters);
    }
   

    time_val = gettimeofday(&stop_time, NULL);

    if(time_val == -1){

      printf("Timing is inaccurate. The user should end the program.\n");

    }


    double totalsecondsStart = (start_time.tv_sec * 1000000) + start_time.tv_usec;

    double totalsecondsFinish = (stop_time.tv_sec * 1000000) +  stop_time.tv_usec;

    double secs1 = totalsecondsFinish - totalsecondsStart;
    
    if (data.output_mode != OUTPUT_VISI) {
        secs = 0.0;

        secs = secs1/1000000;

        /* Print the total runtime, in seconds. */
        fprintf(stdout, "Total time: %0.3f seconds\n", secs);
        fprintf(stdout, "Number of live cells after %d rounds: %d\n\n",
                data.iters, total_live);
    }

    //destroys barrier and mutex
    if (pthread_barrier_destroy(&my_barrier)){
          printf("pthread_barrier_destroy error\n");
          exit(1);
    }

    if(pthread_mutex_destroy(&my_mutex)){
      printf("destroying mutex error");
      exit(1);
    }

    

    free(data.array);
    free(data.arrayNew);
    free(tids);
    free(data1);

    return 0;
}

/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
//reads the data from the file to initialize our array
int init_game_data_from_args(struct gol_data *data, char **argv) {

    FILE *ret = NULL;

    ret = fopen(argv[1], "r");

    int num = -1;
    //will not compile unless we "use" num. This is cleaner than printing
    num = num + 1;

    int x = 0;
    int y = 0;

    data->output_mode = atoi(argv[2]);
    
    int numAlive = 0;

    for(int i = 0; i < 4; i++){

        num = fscanf(ret, "%d", &x);
        if(i == 0){
            data -> rows = x;
        }
         if(i == 1){
            data -> cols = x;
        }
         if(i == 2){
            data -> iters = x;
        }
         if(i == 3){
            numAlive = x;
        }
    }

    int rowNum = data -> rows;

    int colNum = data -> cols;

    data -> array = malloc(rowNum * colNum * sizeof(int));
    data->arrayNew = malloc(data->rows * data->cols*sizeof(int));

    for(int i = 0; i < rowNum*colNum; i++){
        data->array[i] = 0;
    }

    int index = 0;

    for(int c = 0; c < numAlive; c++){
        num = fscanf(ret, "%d%d", &x, &y);

        index = (x * colNum) + y;

        data->array[index] = 1;
    }

    data->numberAlive = numAlive;

    return 0;
}


/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 */
void *play_gol(void *args) {
    

    //create global variable
    struct gol_data *data;
    int i;
    int j;
    int index;

    int sum;
    int *temp;
    
    int *start = malloc(sizeof(int));
    int *end = malloc(sizeof(int));

    int local_alive = 0;

    data = (struct gol_data *)args;
    //partitions each thread
    partition(data->id, data->ntids, data->rows, data->cols, data->partitionMode, start, end);

    if(data->printMode !=0){
      
      if(data->partitionMode == 0){
        printf("We are doing row partition ");
      }
      else{
        printf("We are doing column partition\n");
      }
      if(data->partitionMode == 0){
      
    
        printf("tid = %d, 1st row = %d, last row = %d, total number = %d\n ", data->id, *start, *end, *end - *start + 1);
      }
      else{
        printf("tid = %d, 1st column = %d, last column = %d, total number = %d\n", data->id, *start, *end, *end - *start + 1);
      }
      if(data->partitionMode == 0){
        printf("number of columns starts from %d to %d, total number of columns = %d\n", 0, data->cols - 1, data->cols);
      }
      else{
        printf("number of rows starts from %d to %d, total number of rows = %d\n", 0, data->rows - 1, data->rows);
      }
    
      fflush(stdout);
    }
    
    for(int r = 1; r <= data->iters; r++){
      local_alive = 0;
      total_live = 0;
      pthread_barrier_wait(&my_barrier);
     
      if(data->partitionMode == 0){
        for(i = *start; i < *end + 1; i++){

            for(j = 0; j < data->cols; j++){
                index = (i * data->cols) + j;
                

                sum = checkNeighbor(data, i, j);

                if(data->array[index] == 1){

                    if(sum < 2 || sum > 3){
                        data->arrayNew[index] = 0;
                    }
                    else{
                        data->arrayNew[index] = 1;
                        
                        local_alive = local_alive + 1;
                        
                    }
                }

                if(data->array[index] == 0){

                    if(sum == 3){
                        data->arrayNew[index] = 1;
                        local_alive = local_alive + 1;
                    }
                    else{
                        data->arrayNew[index] = 0;
                    }
                }
            }
        }

      }
      else{
        for(i = *start; i < *end + 1; i++){

            for(j = 0; j < data->rows; j++){
                index = (j * data->cols) + i;

                sum = checkNeighbor(data, i, j);

                if(data->array[index] == 1){

                    if(sum < 2 || sum > 3){
                        data->arrayNew[index] = 0;
                    }
                    else{
                        data->arrayNew[index] = 1;
                        
                        local_alive = local_alive + 1;
                        
                    }
                }

                if(data->array[index] == 0){

                    if(sum == 3){
                        data->arrayNew[index] = 1;
                        
                        local_alive = local_alive + 1;
                        
                    }
                    else{
                        data->arrayNew[index] = 0;
                    }
                }
            }
        }
      }
        //creates mutex to update total alive. Then creates barrier to wait.
        pthread_mutex_lock(&my_mutex);
        total_live = total_live + local_alive;
        
        pthread_mutex_unlock(&my_mutex);
        
        pthread_barrier_wait(&my_barrier);
        temp = data->array;
        data->array = data->arrayNew;
        data->arrayNew = temp;
        pthread_barrier_wait(&my_barrier);

       

        if(data->output_mode == 1){
          
            if(data->id == 0){
              system("clear");
              
              print_board(data, r);
              
              usleep(SLEEP_USECS);

            }
            
        }

        if(data->output_mode == 2){
          if(data->partitionMode == 0){
            for(int a = *start; a < *end + 1; a++){

              for(int g = 0; g < data->cols; g++){
                int indice = (a * data->cols) + g;
                int buff_i = (data->rows - (a+1))*data->cols + g;
                functionColor(data, indice, buff_i);
              
              }
            }

          }
          else{
            for(int a = *start; a < *end + 1; a++){

              for(int g = 0; g < data->rows; g++){
                int indice = (g * data->cols) + a;
                int buff_i = (data->rows - (g+1))*data->cols + a;
                functionColor(data, indice, buff_i);
              
              }
            }

          }
          

          draw_ready(data->handle);
          usleep(SLEEP_USECS);
        }
        
      pthread_barrier_wait(&my_barrier);
      
    }

    free(start);
    
    free(end);
    return NULL;
  }

/*
* This function checks the total number of living neighbors for a given 
* cell in the Game of Life.
* Takes: data struct pointer, integers i and j for iteration
* Returns: int sum, total number of living neighbors
*/

void partition(int thread, int threadNum, int rowNum, int colNum, int method, int *start, int *end){
  int chunk;
  
  int extraNum;

  if(method == 0){

    chunk = rowNum/threadNum;
    extraNum = rowNum % threadNum;

    if(extraNum == 0){
    
      *start = thread*chunk;
      
      *end = (thread + 1)*chunk - 1;
      
    }

    else{
      if(thread < extraNum){

        if(thread == 0){
          *start = 0;
        }
        else{
          *start = thread*chunk + (thread);
        }

        *end = (thread + 1)*chunk + thread;
          
      }

      else{

        *start = thread*chunk + (extraNum);
        *end = (thread + 1)*chunk + (extraNum) - 1;
      }
      
    }
  }

  else{

    chunk = colNum/threadNum;
    extraNum = colNum % threadNum;

    if(extraNum == 0){
      int a = thread * chunk;
      *start = a;
      
      *end = (thread + 1)*chunk - 1;
   
    }

    else{

      if(thread < extraNum){
        if(thread == 0){
          *start = 0;
        }
        else{
          *start = thread*chunk + thread;
        }

        *end = (thread + 1)*chunk + thread;
          
      }
      else{
          
        *start = thread*chunk + extraNum;
        *end = (thread + 1)*chunk + extraNum - 1;
          
      }

    }
  }
    
}

//checks if our neighbors are alive or dead
int checkNeighbor(struct gol_data *data, int i, int j){

    int i_current = i;
    int j_current = j;
    int sum = 0;
    int i_temp;
    int j_temp;

    int index;
    int index_real;

    for(int a = -1; a < 2; a++){

        for(int b = -1; b < 2; b++){

            if((i + a) >= 0 && (i+a) < data->rows){
                i_temp = i + a;

            }
            if((j+b) >=0 && (j+b)<data->cols){
                j_temp = j + b;

            }
            if((i + a) < 0){
                i_temp = data->rows - 1;

            }
            if((i + a) >= data->rows){
                i_temp = 0;

            }
            if((j + b) < 0){
                j_temp = data->cols - 1;

            }
            if((j + b) > (data->cols - 1)){
                j_temp = 0;

            }

            if(data->partitionMode == 0){
              

              index = (i_temp * data->cols) + j_temp;
              index_real = (i_current*data->cols) + j_current;
            }

            else{

              index = (j_temp*data->cols) + i_temp;
              index_real = (j_current*data->cols) + i_current;
            }
            
            if(data->array[index] == 1 && index_real != index){
                sum = sum + 1; 

            }
            i = i_current;
            j = j_current;

        }
    }

    return sum;
}

/*
*This function sets living cells of thread 0 to blue, the living cells
 of thread 1 to orange, the living cells of thread 2 to green, the living 
 cells of all the other threads to yellow, and dead cells to red.
*Takes: data struct pointer, int index (the index of the cell)
*Returns: void
*/
void functionColor(struct gol_data *data, int index, int buff_i){
  

  if(data->id == 0){
    if(data->array[index] == 1){
      data->image_buff[buff_i]  = c3_blue;

    }
    else{
      data->image_buff[buff_i] = c3_red;

    }

  }

  else if(data->id == 1){
    if(data->array[index] == 1){
      data->image_buff[buff_i]  = c3_orange;

    }
    else{
      data->image_buff[buff_i] = c3_red;

    }

  }
  else if(data->id == 2){
    if(data->array[index] == 1){
      data->image_buff[buff_i]  = c3_green;

    }
    else{
      data->image_buff[buff_i] = c3_red;

    }

  }
  else{
    if(data->array[index] == 1){
      data->image_buff[buff_i]  = c3_yellow;

    }
    else{
      data->image_buff[buff_i] = c3_red;

    }
  }
  
  
}

/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 */
void print_board(struct gol_data *data, int round) {

    int i, j;

    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round);

    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) {
            int index = (i * data->cols) + j;
            if(data->array[index] == 1){
                fprintf(stderr, " @");
            }
            else{
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }
    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}


/**********************************************************/
/***** START: DO NOT MODIFY THIS CODE *****/
/* initialize ParaVisi animation */
int setup_animation(struct gol_data* data) {
    /* connect handle to the animation */
    int num_threads = 1;
    data->handle = init_pthread_animation(num_threads, data->rows,
            data->cols, visi_name);
    if (data->handle == NULL) {
        printf("ERROR init_pthread_animation\n");
        exit(1);
    }
    // get the animation buffer
    data->image_buff = get_animation_buffer(data->handle);
    if(data->image_buff == NULL) {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
    }
    return 0;
}

/* sequential wrapper functions around ParaVis library functions */
void (*mainloop)(struct gol_data *data);

void* seq_do_something(void * args){
    mainloop((struct gol_data *)args);
    return 0;
}

int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data)
{
    pthread_t pid;

    mainloop = applfunc;
    if( pthread_create(&pid, NULL, seq_do_something, (void *)data) ) {
        printf("pthread_created failed\n");
        return 1;
    }
    return 0;
}
/***** END: DO NOT MODIFY THIS CODE *****/
/******************************************************/
