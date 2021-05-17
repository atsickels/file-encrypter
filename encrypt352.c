//
//  encrypt352.c
//  cs352proj2
//
//  Created by Austin Sickels on 4/11/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "encrypt-module.h"
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

size_t N; // size of the input buffer
size_t M; // size of the output buffer
char * inputBuffer; // input buffer
char * outputBuffer; // output buffer
int inhead = 0; //head of the input buffer list
int incnt = 0; // count for the input counter thread (keeps track of location in buffer)
int inenc = 0; // count to keep track of where in the buffer the encrypter is
int intail = 0; // tail of the input buffer list
// The next four variables are just like the last four but for the output buffer
int outhead = 0;
int outcnt = 0;
int outenc = 0;
int outtail = 0;
sem_t *read_mutex; // Sem to lock the reader thread
sem_t *count_in_sem; // sem to allow counter thread to run per input char read
sem_t *count_out_sem; // sem to allow counter thread to run per output char written
sem_t *encrypt_sem; // sem for each encryption cycle to run
sem_t *writer_sem; // sem for allowing a writer cycle
sem_t *reset_lock; // locks the reset thread to prevent reading more chars (not used)
sem_t *reset_read_done; // lcoks the reset thread from continuing until we finished reading the char we were reading when the reset was requested
int reset_req = 0; // bool to let all methods know a reset has been requested


/* You must implement this function.
 * When the function returns the encryption module is allowed to reset.
 */
void reset_requested() {
    reset_req = 1; // reset has been requested
    printf("\nReset Requested\n");
    sem_wait(reset_read_done); // make sure all input that has been read in gets a chance to be processed
    while (1) {  // wait for the buffers to clear and the counts to finalize
        if (inhead == intail && outhead == outtail) {
            break;
        }
    }
    // What a print statement
    printf("Total input count with the current key is %d\n", get_input_total_count());
    printf("A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d S:%d T:%d U:%d V:%d W:%d X:%d Y:%d Z:%d\n", get_input_count('a'), get_input_count('b'), get_input_count('c'), get_input_count('d'), get_input_count('e'), get_input_count('f'), get_input_count('g'), get_input_count('h'), get_input_count('i'), get_input_count('j'), get_input_count('k'), get_input_count('l'), get_input_count('m'), get_input_count('n'), get_input_count('o'), get_input_count('p'), get_input_count('q'), get_input_count('r'), get_input_count('s'), get_input_count('t'), get_input_count('u'), get_input_count('v'), get_input_count('w'), get_input_count('x'), get_input_count('y'), get_input_count('z'));
    printf("Total output count with the current key is %d\n", get_output_total_count());
    printf("A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d S:%d T:%d U:%d V:%d W:%d X:%d Y:%d Z:%d\n", get_output_count('a'), get_output_count('b'), get_output_count('c'), get_output_count('d'), get_output_count('e'), get_output_count('f'), get_output_count('g'), get_output_count('h'), get_output_count('i'), get_output_count('j'), get_output_count('k'), get_output_count('l'), get_output_count('m'), get_output_count('n'), get_output_count('o'), get_output_count('p'), get_output_count('q'), get_output_count('r'), get_output_count('s'), get_output_count('t'), get_output_count('u'), get_output_count('v'), get_output_count('w'), get_output_count('x'), get_output_count('y'), get_output_count('z'));
}
/* You must implement this function.
 * The function is called after the encryption module has finished a reset.
 */
void reset_finished() {
    printf("Reset finished\n");
    reset_req = 0; // We've finished the reset so no more request
    sem_post(read_mutex); // free the reader to read again once we've printed everything and finished reset
}


void parse_file() {
    while (1) {
        inputBuffer[inhead] = read_input(); // The head of the buffer gets the input char
        inhead++; // move the head
        if (inhead == N) { // if the head is at the end of the buffer size, loop to 0
            inhead = 0;
        }
        sem_post(count_in_sem); // let the input counter know it can read another char
        if (inputBuffer[inhead - 1] == EOF) { // if we just sent EOF theres no need to continue trying to read
            break;
        }
        if (reset_req == 1) { // if a reset has been requested
            sem_post(reset_read_done); // tell the reset that we've input all of the chars
            sem_wait(read_mutex); // wait for permission to continue reading
            reset_req = 0;
        }
    }
}

void write_file() {
    while (1) {
        sem_wait(writer_sem); // wait for permission to write a char
        if (outputBuffer[outtail] == EOF) {
            break; //we've written the entire file so exit
        }
        write_output(outputBuffer[outtail]); // write the char
        outtail++; // advance the head as before
        if (outtail == M) {
            outtail = 0;
        }
    }
}

void input_counter() {
    while (1) {
        sem_wait(count_in_sem); // wait for permission to read the input counter
        if (inputBuffer[incnt] == EOF) { // if we read eof send it down the line and stop counting
            sem_post(encrypt_sem);
            break;
        }
        count_input(inputBuffer[incnt]); // count the appropriate char
        incnt++;
        if (incnt == N) {
            incnt = 0;
        }
        sem_post(encrypt_sem); // allow encrypter to read a char once it's been counted
    }
}

void output_counter() { // Same as input counter but with output buffer instead
    while (1) {
        sem_wait(count_out_sem);
        if (outputBuffer[outcnt] == EOF) {
            sem_post(writer_sem);
            break;
        }
        count_output(outputBuffer[outcnt]);
        outcnt++;
        if (outcnt == M) {
            outcnt = 0;
        }
        sem_post(writer_sem);
    }
}

void encrypter() {
    while (1) {
        sem_wait(encrypt_sem); // wait for permission to run a cycle
        if (inputBuffer[inenc] == EOF) { // if EOF write EOF for the output file and let it count
            outputBuffer[outenc] = EOF;
            sem_post(count_out_sem);
            break;
        }
        outputBuffer[outenc] = caesar_encrypt(inputBuffer[inenc]); // encrypt the char available to us
        intail++; // remove an element from the input queue
        if (intail == N) {
            intail = 0;
        }
        outhead++; //remove an element from the output queue
        if (outhead == M) {
            outhead = 0;
        }
        inenc++; // move our input buffer head
        if (inenc == N) {
            inenc = 0;
        }
        outenc++; // move our output buffer head
        if (outenc == M) {
            outenc = 0;
        }
        sem_post(count_out_sem); // let counter count what we just put in the buffer
    }
}

// cleanup for semaphores and malloc'd objects
void cleanup() {
    free(inputBuffer);
    free(outputBuffer);
}

// Helper function for creating and joining threads because main was becoming a monster
void threads() {
    pthread_t reader, in_counter, out_counter, encrypt, writer;
    pthread_create(&reader, NULL, (void *)parse_file, NULL);
    pthread_create(&in_counter, NULL, (void *)input_counter, NULL);
    pthread_create(&out_counter, NULL, (void *)output_counter, NULL);
    pthread_create(&encrypt, NULL, (void *)encrypter, NULL);
    pthread_create(&writer, NULL, (void *)write_file, NULL);
    pthread_join(reader, NULL);
    pthread_join(in_counter, NULL);
    pthread_join(encrypt, NULL);
    pthread_join(out_counter, NULL);
    pthread_join(writer, NULL);
}

// didn't want this in main lol
void print_eof() {
    printf("\nEnd of file reached\n");
    printf("Total input count with the current key is %d\n", get_input_total_count());
    printf("A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d S:%d T:%d U:%d V:%d W:%d X:%d Y:%d Z:%d\n", get_input_count('a'), get_input_count('b'), get_input_count('c'), get_input_count('d'), get_input_count('e'), get_input_count('f'), get_input_count('g'), get_input_count('h'), get_input_count('i'), get_input_count('j'), get_input_count('k'), get_input_count('l'), get_input_count('m'), get_input_count('n'), get_input_count('o'), get_input_count('p'), get_input_count('q'), get_input_count('r'), get_input_count('s'), get_input_count('t'), get_input_count('u'), get_input_count('v'), get_input_count('w'), get_input_count('x'), get_input_count('y'), get_input_count('z'));
    printf("Total output count with the current key is %d\n", get_output_total_count());
    printf("A:%d B:%d C:%d D:%d E:%d F:%d G:%d H:%d I:%d J:%d K:%d L:%d M:%d N:%d O:%d P:%d Q:%d R:%d S:%d T:%d U:%d V:%d W:%d X:%d Y:%d Z:%d\n", get_output_count('a'), get_output_count('b'), get_output_count('c'), get_output_count('d'), get_output_count('e'), get_output_count('f'), get_output_count('g'), get_output_count('h'), get_output_count('i'), get_output_count('j'), get_output_count('k'), get_output_count('l'), get_output_count('m'), get_output_count('n'), get_output_count('o'), get_output_count('p'), get_output_count('q'), get_output_count('r'), get_output_count('s'), get_output_count('t'), get_output_count('u'), get_output_count('v'), get_output_count('w'), get_output_count('x'), get_output_count('y'), get_output_count('z'));
}

// create all the sems and immediately unlink them
void create_sems() {
    read_mutex = sem_open("/read_mutex", O_CREAT, 0644, 0);
    count_in_sem = sem_open("/count_in_sem", O_CREAT, 0644, 0);
    count_out_sem = sem_open("/count_out_sem", O_CREAT, 0644, 0);
    encrypt_sem = sem_open("/encrypt_sem", O_CREAT, 0644, 0);
    writer_sem = sem_open("/writer_sem", O_CREAT, 0644, 0);
    reset_lock = sem_open("/reset_lock", O_CREAT, 0644, 0);
    reset_read_done = sem_open("/reset_read_done", O_CREAT, 0644, 0);
    sem_unlink("/read_mutex");
    sem_unlink("/count_in_sem");
    sem_unlink("/count_out_sem");
    sem_unlink("/encrypt_sem");
    sem_unlink("/writer_sem");
    sem_unlink("/reset_lock");
    sem_unlink("/reset_read_done");
}

int main(int argc, char * argv[]) {
    create_sems();
    if (argc != 3) { // gotta have the right number of arguments
        printf("Incorrect number of arguments.\nExample: ./encrypt352 inputFile outputFile\n");
        return 1;
    }
    init(argv[1], argv[2]); // call init with the file names
    printf("Enter the input buffer size: ");
    scanf("%zu", &N); // make the input buffer as defined
    printf("Enter the output buffer size: ");
    scanf("%zu", &M); // make the output buffer as defined
    if (N < 2 || M < 2) { // must be larger than 1
        printf("Buffer sizes must be greater than 1\n");
        return 1;
    }
    inputBuffer = (char *) malloc(sizeof(char)*N); // malloc the buffers
    outputBuffer = (char *) malloc(sizeof(char)*M);
    threads(); // create call and wait for threads
    cleanup(); // free the buffers
    print_eof(); // print the amounts when we've written all chars
    return 0;
}
