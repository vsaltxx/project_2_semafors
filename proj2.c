#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <time.h>


// Define some numbers of errors
#define ARGS_PARSE_ERROR 2
#define FILE_OPEN_ERROR 3

typedef struct param {
    int NZ;             // number of customers
    int NU;             // number of postmen
    int TZ;             // maximum waiting time for customer
    int TU;             // maximum break time for postmen
    int F;              // total running time of post office
} args_t;

// Define shared memory struct
typedef struct {
    int num_customers;                      // Number of customers
    int num_postmen;                        // Number of postmen
    int action_counter;                     // Counter for actions
    bool post_office_open;                  // Flag to indicate if the post office is open
    int queue_size[3];                      // Size of each queue
} shared_mem_t;

// Struct to hold semaphores
typedef struct semaphores {
    sem_t postman_sem;              // if postman is available
    sem_t customer_sem;             // if customer is arrived
    sem_t postman_done;             // to signal that a postman process has finished its work.
    sem_t queue_sem[3];             // to protect each type of queue
    sem_t counter_sem_mutex;        // to protect the action counter
} semaphores_t;



// Define function to initialize semaphores
void initialize_semaphores(shared_mem_t *shared_mem,semaphores_t *semaphores, args_t args) {

    if (sem_init(&(semaphores->postman_sem), args.NU, 0) == -1)
    {
        perror("ERROR: Couldn't initialize semaphore\n");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&(semaphores->customer_sem), args.NZ, 0) == -1)
    {
        perror("ERROR: Couldn't initialize semaphore\n");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&(semaphores->queue_sem[0]), 1, 0) == -1)
    {
        perror("ERROR: Couldn't initialize semaphore\n");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&(semaphores->queue_sem[1]), 1, 0) == -1)
    {
        perror("ERROR: Couldn't initialize semaphore\n");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&(semaphores->queue_sem[2]), 1, 0) == -1)
    {
        perror("ERROR: Couldn't initialize semaphore\n");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&(semaphores->counter_sem_mutex), 1, 1) == -1)
    {
        perror("ERROR: Couldn't initialize semaphore\n");
        exit(EXIT_FAILURE);
    }

    // Initialize number of customers and postmen
    shared_mem->num_customers = args.NZ;
    shared_mem->num_postmen = args.NU;
    // Initialize shared counter for recording the actions
    shared_mem->action_counter = 0;
    // Initialize flag to indicate if the post office is open
    shared_mem->post_office_open = true;

}

// Define function to destroy semaphores
void destroy_semaphores(semaphores_t *semaphores) {
    sem_destroy(&(semaphores->postman_sem));
    sem_destroy(&(semaphores->customer_sem));
    sem_destroy(&(semaphores->postman_done));
    sem_destroy(&(semaphores->queue_sem[0]));
    sem_destroy(&(semaphores->queue_sem[1]));
    sem_destroy(&(semaphores->queue_sem[2]));
    sem_destroy(&(semaphores->counter_sem_mutex));
}



// Define function to handle postman process
void postman_process(int postman_id, shared_mem_t *shared_mem, semaphores_t *semaphores, FILE *outputFile) {
    //post_office_open = true; -- есть изначально
    sleep(rand() % 15);
    bool has_clients;
    bool is_leaving = false;

 while (true) {

     sem_wait(&(semaphores->counter_sem_mutex));
     printf("%d: U %d: start\n", shared_mem->action_counter, postman_id);
     shared_mem->action_counter++;
     sem_post(&(semaphores->counter_sem_mutex));

     sem_wait(&(semaphores->counter_sem_mutex));
     if (shared_mem->queue_size[0] > 0 || shared_mem->queue_size[1] > 0 || shared_mem->queue_size[2] > 0) {
         has_clients = true;
     } else {
         has_clients = false;
     }
     sem_post(&(semaphores->counter_sem_mutex));

     if (has_clients && shared_mem->post_office_open) {
         int service = rand() % 3;

         // Postman took a customer
         sem_wait(&(semaphores->queue_sem[service]));
         shared_mem->queue_size[service]--;
         sem_post(&(semaphores->counter_sem_mutex));

         sem_post(&(semaphores->postman_sem));

         sem_wait(&(semaphores->counter_sem_mutex));
         printf("%d: U: %d: Serving a service of type %d\n", shared_mem->action_counter, postman_id, service + 1);
         shared_mem->action_counter++;
         sem_post(&(semaphores->counter_sem_mutex));

         // Postman is serving a customer
         sleep(rand() % 5);

         // Postman finished serving a customer
         sem_post(&(semaphores->customer_sem));

         sem_wait(&(semaphores->counter_sem_mutex));
         printf("%d: U: %d: Service finished\n", shared_mem->action_counter, postman_id);
         shared_mem->action_counter++;
         sem_post(&(semaphores->counter_sem_mutex));

         continue;
     }

     if (!has_clients && shared_mem->post_office_open) {

         sem_wait(&(semaphores->counter_sem_mutex));
         printf("%d: U: %d: Break started\n", shared_mem->action_counter, postman_id);
         shared_mem->action_counter++;
         sem_post(&(semaphores->counter_sem_mutex));

         // Postman is taking a break
         sleep(rand() % 10);

         sem_wait(&(semaphores->counter_sem_mutex));
         printf("%d: U: %d: Break ended\n", shared_mem->action_counter, postman_id);
         shared_mem->action_counter++;
         sem_post(&(semaphores->counter_sem_mutex));

         if (!shared_mem->post_office_open) {
             sem_post(&(semaphores->postman_done));
         }
             continue;
     }

     if (has_clients && !shared_mem->post_office_open) {
         continue;
     }

     if (!has_clients && !shared_mem->post_office_open) {

         if (!is_leaving) {
             sem_wait(&(semaphores->postman_done));
         }

         sem_wait(&(semaphores->counter_sem_mutex));
         printf("%d: U: %d: Leaving\n", shared_mem->action_counter, postman_id);
         shared_mem->action_counter++;
         sem_post(&(semaphores->counter_sem_mutex));

         exit(0);
     }

 }
}

// Define function to handle customer process
void customer_process(int customer_id, shared_mem_t *shared_mem, semaphores_t *semaphores, FILE *outputFile) {
    sleep(rand() % 5);

    int service = rand() % 3;

    if (shared_mem->post_office_open) {

        // Add customer to the queue
        shared_mem->queue_size[service]++;

        // Wait for the postman to be available
        sem_wait(&(semaphores->postman_sem));

        sem_wait(&(semaphores->counter_sem_mutex));
        shared_mem->action_counter++;
        printf( "%d: Z: %d: Called by worker\n", shared_mem->action_counter, customer_id);
        sem_post(&(semaphores->counter_sem_mutex));

        // Wait for the postman to finish its work
        sem_wait(&(semaphores->customer_sem));

        sem_wait(&(semaphores->counter_sem_mutex));
        shared_mem->action_counter++;
        printf( "%d: Z: %d: Customer %d leaves\n", shared_mem->action_counter, customer_id);
        sem_post(&(semaphores->counter_sem_mutex));

        exit(0);

    } else {
        sem_wait(&(semaphores->counter_sem_mutex));
        shared_mem->action_counter++;
        printf( "%d: Z: %d: Post is closed. Customer %d leaves\n", shared_mem->action_counter, customer_id);
        sem_post(&(semaphores->counter_sem_mutex));
    }

}

// Define function to close the post office
// Function to wait for all the postman processes to finish their work
void close_post_office(shared_mem_t *shared_mem, semaphores_t *semaphores, args_t args, FILE *outputFile) {
    // Wait for all postman to finish their work
    for (int i = 0; i < args.NU; i++) {
        sem_post(&(semaphores->postman_done));
    }

    // Close the post office
    shared_mem->post_office_open = false;

    // Increment the action counter
    sem_wait(&(semaphores->counter_sem_mutex));
    shared_mem->action_counter++;
    fprintf(outputFile, "%d: closing\n", shared_mem->action_counter);
    sem_post(&(semaphores->counter_sem_mutex));


}



// Control of wrong input format
void args_parsing(int argc, char **argv, args_t *params) {
    if (argc != 6) {
        fprintf(stderr, "Wrong number of arguments\n");
        exit(EXIT_FAILURE);
    }

    params->NZ = atoi(argv[1]);
    params->NU = atoi(argv[2]);
    params->TZ = atoi(argv[3]);
    params->TU = atoi(argv[4]);
    params->F = atoi(argv[5]);

    if (params->NZ <= 0 || params->NU <= 0 ||
        params->TZ < 0 || params->TZ > 10000 ||
        params->TU < 0 || params->TU > 100 ||
        params->F < 0 || params->F > 10000)
    {
        fprintf(stderr, "Wrong arguments\n");
        exit(ARGS_PARSE_ERROR);
    }
}

FILE* openFile()
{
    FILE *outputFile = fopen("proj2.out", "w");
    if(outputFile == NULL)
    {
        fprintf(stderr, "ERROR: Couldn't create file\n");
        exit(FILE_OPEN_ERROR);
    }

    return outputFile;
}



int main(int argc, char **argv) {

    srand(time(NULL));

    args_t params;
    args_parsing(argc, argv, &params);

    FILE *outputFile = openFile();
    /* To keep correct printing */
    setbuf(outputFile, 0);

    shared_mem_t *shared_mem = mmap(NULL, sizeof(shared_mem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    semaphores_t *semaphores = mmap(NULL, sizeof(semaphores_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

    if (shared_mem == MAP_FAILED) {    // check if mmap failed to allocate shared memory
        fprintf(stderr, "Unable to create shared memory.");
        return 1;
    }
    initialize_semaphores(shared_mem, semaphores, params);

    pid_t postman_pids[params.NU+1];
    pid_t customer_pids[params.NZ+1];
    // Fork postman processes
    for (int i = 1; i <= params.NU; i++) {
        postman_pids[i] = fork();
        if (postman_pids[i] == -1) {
            perror("Failed to fork clerk process");
            exit(EXIT_FAILURE);
        } else if (postman_pids[i] == 0) {
            postman_process(i, shared_mem, semaphores, outputFile);
            exit(EXIT_SUCCESS);
        }
    }

    // Fork customer processes
    for (int i = 1; i <= params.NZ; i++) {
        customer_pids[i] = fork();
        if (customer_pids[i] == -1) {
            perror("Failed to fork customer process");
            exit(EXIT_FAILURE);
        } else if (customer_pids[i] == 0) {
            customer_process(i, shared_mem, semaphores, outputFile);
            exit(EXIT_SUCCESS);
        }
    }

    // Wait for F/2 to F milliseconds and then send signal to close post office
    sleep(params.F/2 + rand() % (params.F/2));
    close_post_office(shared_mem, semaphores, params, outputFile);

    // Wait for all processes to complete
    for (int i = 1; i <= params.NU; i++) {
        waitpid(postman_pids[i], NULL, 0);
    }
    for (int i = 1; i <= params.NZ; i++) {
        waitpid(customer_pids[i], NULL, 0);
    }

    destroy_semaphores(semaphores);
    fclose(outputFile);


    if (munmap(shared_mem, sizeof(shared_mem_t)) == -1) {       // unmap shared memory and check if it was unmapped correctly
        fprintf(stderr, "Unable to unmap shared memory.");
        return 1;
    }
    if (munmap(semaphores, sizeof(semaphores_t)) == -1) {       // unmap semaphores and check if it was unmapped correctly
        fprintf(stderr, "Unable to unmap semaphores.");
        return 1;
    }


    return 0;
}

