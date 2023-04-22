#include <stdio.h>
#include <stdlib.h>
#include "eventbuf.h"
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

int num_producers;
int num_consumers;
int num_events;
int max_outstanding;
struct eventbuf *eb;

sem_t *mutex;
sem_t *items;
sem_t *spaces;

sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

void *run_producer(void *arg)
{
    int *id = arg;
    
    for(int i = 0; i < num_events; i++)
    {   
        int event_num = *id * 100 + i;
        sem_wait(spaces);
        sem_wait(mutex);
        printf("P%d: adding event %d\n", *id, event_num);
        eventbuf_add(eb, event_num);
        sem_post(mutex);
        sem_post(items);
    }

    printf("P%d: exiting\n", *id);
    return NULL;
}

void *run_consumer(void *arg)
{
    int *id = arg;

    while(1)
    {
        sem_wait(items);
        sem_wait(mutex);

        if(eventbuf_empty(eb))
        {
            sem_post(mutex);
            break;
        }

        int event_num = eventbuf_get(eb);
        printf("C%d: got event %d\n", *id, event_num);

        sem_post(mutex);
        sem_post(spaces);
    }
    printf("C%d: exiting\n", *id);
    return NULL;
}

int main(int argc, char *argv[]) 
{

    if (argc != 5) {
        fprintf(stderr, "usage: pcseml num_producers num_consumers num_events max_outstanding \n");
        exit(1);
    }

    num_producers = atoi(argv[1]);
    num_consumers = atoi(argv[2]);
    num_events = atoi(argv[3]);
    max_outstanding = atoi(argv[4]);

    eb = eventbuf_create();
    mutex = sem_open_temp("program-3-mutex", 1);
    items = sem_open_temp("program-3-items", 0);
    spaces = sem_open_temp("program-3-spaces", max_outstanding);

    pthread_t *prod_thread = calloc(num_producers, sizeof *prod_thread);
    int *prod_thread_id = calloc(num_producers, sizeof *prod_thread_id);

    for (int i = 0; i < num_producers; i++) {
        prod_thread_id[i] = i;
        pthread_create(prod_thread + i, NULL, run_producer, prod_thread_id + i);
    }

    pthread_t *cons_thread = calloc(num_consumers, sizeof *cons_thread);
    int *cons_thread_id = calloc(num_consumers, sizeof *cons_thread_id);
    for (int i = 0; i < num_consumers; i++)
    {
        cons_thread_id[i] = i;
        pthread_create(cons_thread + i, NULL, run_consumer, cons_thread_id + i);
    }

    for (int i = 0; i < num_producers; i++)
    {
        pthread_join(prod_thread[i], NULL);
    }
    
    for (int i = 0; i < num_consumers; i++)
    {
        sem_post(items);
    }
    
    for (int i = 0; i < num_consumers; i++)
    {
        pthread_join(cons_thread[i], NULL);
    }

    eventbuf_free(eb);
}