#include <pthread.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "sut.h"
#include "queue.h"

int C_EXEC_num = 1;
ucontext_t m, m1, n, cur;
struct queue readyQueue;
struct queue waitQueue;
struct queue requestQueue;
ucontext_t *cur_task;
ucontext_t *cur_task_in_2;
ucontext_t *cur_io_task;


pthread_t *C_EXEC0;
pthread_t *C_EXEC1;
pthread_t *I_EXEC;
pthread_mutex_t lock;
pthread_mutex_t lock1;

pthread_t C0;
pthread_t C1;

bool C0_close = false;
bool C1_close = false;

bool shutdown = false;

int task_num = 0;


void *I_EXEC_loop()
{
    
    while (true)
    {


        pthread_mutex_lock(&lock1);
        struct queue_entry *head = queue_pop_head(&waitQueue);
        pthread_mutex_unlock(&lock1);

        if(head) {
  
            cur_io_task = (ucontext_t *)head->data;
         
            swapcontext(&n, cur_io_task);
           
        }

        else {
            usleep(100);
            if (shutdown && task_num == 0) {
              
                pthread_exit(NULL);
            }
        }

       
    }
}

void *C_EXEC_0_loop() {
    C0 = pthread_self();

    while (true)
    {
        
        
        pthread_mutex_lock(&lock);
        struct queue_entry *head = queue_pop_head(&readyQueue);
        pthread_mutex_unlock(&lock);
        if(head) {
     
            //  printf("in C_EXEC\n");
            cur_task = (ucontext_t *)head->data;
           
            swapcontext(&m, cur_task);
        }

        else {
            
            usleep(1000);
            if (shutdown && task_num == 0) {
             
                pthread_exit(NULL);
            }
            // if (shutdown && !queue_peek_front(&waitQueue) && !queue_peek_front(&readyQueue)) {
            //     printf("break c1\n");
            //     C0_close = true;
            //     pthread_exit(NULL);
            // }
            
        }
        // usleep(100);
    }
}

void *C_EXEC_1_loop() {
    C1 = pthread_self();
    while (true)
    {

        pthread_mutex_lock(&lock);
        struct queue_entry *head = queue_pop_head(&readyQueue);
        pthread_mutex_unlock(&lock);
        if(head) {
            //  printf("in C_EXEC\n");
     
            cur_task_in_2 = (ucontext_t *)head->data;
           
            swapcontext(&m1, cur_task_in_2);
        }

        else {
            
            usleep(1000);
            if (shutdown && task_num == 0) {
          
                pthread_exit(NULL);
            }
            // if (shutdown && !queue_peek_front(&waitQueue) && !queue_peek_front(&readyQueue)) {
            //     break;
            // }
            // if (shutdown && !queue_peek_front(&waitQueue) && !queue_peek_front(&readyQueue)) {
            //     printf("break c2\n");
            //     C1_close = true;
            //     pthread_exit(NULL);
            // }
            // if (queue_peek_front(&readyQueue)==NULL) {
            //     break;
            // }
        }
        // usleep(100);
    }
}

void sut_init() {
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&lock1, NULL);
    
    readyQueue = queue_create();
    waitQueue = queue_create();
    queue_init(&readyQueue);
    queue_init(&waitQueue);
    C_EXEC0 = (pthread_t *)malloc(sizeof(pthread_t));
    I_EXEC = (pthread_t *)malloc(sizeof(pthread_t));
    pthread_create(C_EXEC0, NULL, C_EXEC_0_loop, NULL);
    pthread_create(I_EXEC, NULL, I_EXEC_loop, NULL);
    if (C_EXEC_num == 2) {
        C_EXEC1 = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(C_EXEC1, NULL, C_EXEC_1_loop, NULL);
    }
    
}

bool sut_create(sut_task_f fn) {
    
    ucontext_t *t1 = (ucontext_t *)malloc(sizeof(ucontext_t));
    if (getcontext(t1) == -1)
    {
        return false;
    }

    t1->uc_stack.ss_sp = (char *)malloc(16*1024);
    t1->uc_stack.ss_size = 16*1024;
    t1->uc_link = &m;
   
    
    makecontext(t1, fn, 0);
    struct queue_entry *node = queue_new_node(t1);
    
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&readyQueue, node);
    //     pthread_mutex_unlock(&lock);
    // }

    // else {
    //     queue_insert_tail(&readyQueue, node);
    // }
    pthread_mutex_lock(&lock);
    queue_insert_tail(&readyQueue, node);
    task_num++;
    pthread_mutex_unlock(&lock);

    return true;
}



void sut_yield() {
    
    pthread_t self = pthread_self();
    ucontext_t context;
    getcontext(&context);
    struct queue_entry *node = queue_new_node(&context);
    if (self == C0)
    {
        
        pthread_mutex_lock(&lock);
        queue_insert_tail(&readyQueue, node);
        
        pthread_mutex_unlock(&lock);

        swapcontext(&context, &m);
    }
    else {
    
    
        pthread_mutex_lock(&lock);
        queue_insert_tail(&readyQueue, node);
      
        pthread_mutex_unlock(&lock);
     
        swapcontext(&context, &m1);
    }
    
}

void sut_exit() {
   
    ucontext_t context;
    getcontext(&context);
    if (pthread_equal(pthread_self(), C0)) {
        pthread_mutex_lock(&lock);
        task_num--;
        pthread_mutex_unlock(&lock);
        swapcontext(&context, &m);
    }
    else {
        pthread_mutex_lock(&lock);
        task_num--;
        pthread_mutex_unlock(&lock);
        swapcontext(&context, &m1);
    }
}

int sut_open(char *dest) {

    //Put the task in wait queue
    ucontext_t context;
    getcontext(&context);
    struct queue_entry *node = queue_new_node(&context);
   

    pthread_mutex_lock(&lock1);
    queue_insert_tail(&waitQueue, node);
    
    pthread_mutex_unlock(&lock1);

    if (pthread_equal(pthread_self(), C0)) {
        swapcontext(&context, &m);
    }
    else {
        swapcontext(&context, &m1);
    }
    //Comes in from I_EXEC and do the fopen() and put it in ready queue
    int fd = fileno(fopen(dest, "r+"));
    
    struct queue_entry *node1 = queue_new_node(&context);
    
    pthread_mutex_lock(&lock);
    queue_insert_tail(&readyQueue, node1);
   
    pthread_mutex_unlock(&lock);

    swapcontext(&context, &n);
    //Comes in from C_EXEC again
    return fd;
}
void sut_write(int fd, char *buf, int size) {
  
    ucontext_t context;
    getcontext(&context);
    struct queue_entry *node = queue_new_node(&context);
   
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&waitQueue, node);
    //     pthread_mutex_unlock(&lock);
    // }

    // else {
    //     queue_insert_tail(&waitQueue, node);
    // }

    pthread_mutex_lock(&lock1);
    queue_insert_tail(&waitQueue, node);
    
    pthread_mutex_unlock(&lock1);

    if (pthread_equal(pthread_self(), C0)) {
        swapcontext(&context, &m);
    }
    else {
        swapcontext(&context, &m1);
    }

    write(fd, buf, size);

    struct queue_entry *node1 = queue_new_node(&context);
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&readyQueue, node1);
    //     pthread_mutex_unlock(&lock);
    // }
    // else {
    //     queue_insert_tail(&readyQueue, node1);
    // }
    pthread_mutex_lock(&lock);
    queue_insert_tail(&readyQueue, node1);
 
    pthread_mutex_unlock(&lock);

    swapcontext(&context, &n);
}

void sut_close(int fd) {

    ucontext_t context;
    getcontext(&context);
    struct queue_entry *node = queue_new_node(&context);
   
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&waitQueue, node);
    //     pthread_mutex_unlock(&lock);
    // }

    // else {
    //     queue_insert_tail(&waitQueue, node);
    // }

    pthread_mutex_lock(&lock1);
    queue_insert_tail(&waitQueue, node);
 
    pthread_mutex_unlock(&lock1);

    if (pthread_equal(pthread_self(), C0)) {
        swapcontext(&context, &m);
    }
    else {
        swapcontext(&context, &m1);
    }

    close(fd);
    struct queue_entry *node1 = queue_new_node(&context);
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&readyQueue, node1);
    //     pthread_mutex_unlock(&lock);
    // }
    // else {
    //     queue_insert_tail(&readyQueue, node1);
    // }
    pthread_mutex_lock(&lock);
    queue_insert_tail(&readyQueue, node1);

    pthread_mutex_unlock(&lock);

    swapcontext(&context, &n);
}

char *sut_read(int fd, char *buf, int size) {

    // struct queue_entry *node = queue_new_node(cur_task);
    // queue_insert_tail(&waitQueue, node);
    // swapcontext(cur_task, &m);
    ucontext_t context;
    getcontext(&context);
    struct queue_entry *node = queue_new_node(&context);
   
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&waitQueue, node);
    //     pthread_mutex_unlock(&lock);
    // }

    // else {
    //     queue_insert_tail(&waitQueue, node);
    // }

    pthread_mutex_lock(&lock1);
    queue_insert_tail(&waitQueue, node);
 
    pthread_mutex_unlock(&lock1);

    if (pthread_equal(pthread_self(), C0)) {
        swapcontext(&context, &m);
    }
    else {
        swapcontext(&context, &m1);
    }

    read(fd, buf, size);
    
    
    struct queue_entry *node1 = queue_new_node(&context);
    // if (C_EXEC_num == 2) {
    //     pthread_mutex_lock(&lock);
    //     queue_insert_tail(&readyQueue, node1);
    //     pthread_mutex_unlock(&lock);
    // }
    // else {
    //     queue_insert_tail(&readyQueue, node1);
    // }
    pthread_mutex_lock(&lock);
    queue_insert_tail(&readyQueue, node1);
   
    pthread_mutex_unlock(&lock);

    swapcontext(&context, &n);
    return buf;
}



void sut_shutdown() {
    shutdown = true;
    if (C_EXEC_num == 2)
    {
        pthread_join(*C_EXEC1, NULL);
        pthread_join(*C_EXEC0, NULL);
        pthread_join(*I_EXEC, NULL);
     
        free(C_EXEC0);
        free(I_EXEC);
        free(C_EXEC1);
        // free(t1->uc_stack.ss_sp);
    }
    else {
  
        pthread_join(*C_EXEC0, NULL);
        pthread_join(*I_EXEC, NULL);
  
        free(C_EXEC0);
        free(I_EXEC);
        // free(t1->uc_stack.ss_sp);
        
    }
}