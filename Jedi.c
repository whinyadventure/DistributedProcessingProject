#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

#define P 10 // number of posts
#define L 15 // capacity of lazaret
#define MAX_TIME 10 // sleep duration [seconds]
#define REQUEST 0
#define CONFIRM 1
#define RELEASE 2
#define REQ_SIZE 3
#define CON_SIZE 2

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

// structures
typedef struct Node {
  int rank;
  int clock;
  int action;
  struct Node *next;
} Node;

typedef struct List {
  Node *head;
} List;

// shared variables & structures
char processor_name[MPI_MAX_PROCESSOR_NAME];
int rank, size, namelen; // Jedi id, Jedi nbr, machine name length

int confirmation_counter;
bool in_crit;

int location, clock_val, action;
List* queues[P+1]; // index coresponds to post id; Lazaret's last

pthread_mutex_t shared_mutex;
pthread_mutex_t crit_mutex;

// function declarations
List* createList();
void addNode(List*, int, int, int);
bool checkIfInsert(Node*, Node*);
void removeNode(List*);
void printList(List*);

int drawLocation();
int drawTime();

// pthread functionality
void *helperThread(void *vargp) {
  MPI_Status status;
  int msg_req[REQ_SIZE];
  int msg_con[CON_SIZE];
  while(1) {
    MPI_Recv(msg_req, REQ_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  	printf("Jestem %d i otrzymalem wiadomosc od %d\n", rank, status.MPI_SOURCE);

    if(status.MPI_TAG == REQUEST) { // request received
      if(msg_req[1] == location) { // check if requested teleporter is in use
        pthread_mutex_lock(&crit_mutex);
      }
      pthread_mutex_lock(&shared_mutex);
        clock_val = max(msg_req[0], clock_val) + 1; // clock synchronization
        addNode(queues[msg_req[1]], status.MPI_SOURCE, msg_req[0], msg_req[2]); // add to teleporter request queue
        if(msg_req[2] == -1)
          addNode(queues[P], status.MPI_SOURCE, msg_req[0], msg_req[2]); // if wants to enter Lazaret then add to Lazaret wait queue

        msg_con[0] = clock_val; msg_con[1] = rank;
        MPI_Send(msg_con, CON_SIZE, MPI_INT, status.MPI_SOURCE, CONFIRM, MPI_COMM_WORLD);
      pthread_mutex_unlock(&shared_mutex);
      pthread_mutex_unlock(&crit_mutex);
    }
    else if(status.MPI_TAG == CONFIRM) { // confirmation received
      // TODO
    }
  }
}

int main(int argc, char **argv) {
  pthread_t thread_id;
  pthread_mutex_init(&shared_mutex, NULL);
  pthread_mutex_init(&crit_mutex, NULL);

  int msg_req[REQ_SIZE];
  int msg_rel;
  int i;

  // initialization
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Get_processor_name(processor_name, &namelen); // machine name in used cluster

  srand(time(NULL) + rank);
  location = drawLocation(); clock_val = 0; action = -1;
  for(i = 0; i < (P+1); i++)
    queues[i] = createList();

  pthread_create(&thread_id, NULL, helperThread, NULL);

  // local section at post
  sleep(drawTime());

  pthread_mutex_lock(&shared_mutex);
  clock_val = clock_val + 1;

  addNode(queues[location], rank, clock_val, action);
  addNode(queues[P], rank, clock_val, action);

  msg_req[0] = clock_val; msg_req[1] = location; msg_req[2] = action;
  confirmation_counter = 0;
  pthread_mutex_unlock(&shared_mutex);

  for(i = 0; i < size; i++) {
		if(i != rank) {
			MPI_Send(msg_req, REQ_SIZE, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
      printf("Jestem %d i wyslalem wiadomosc do %d\n", rank, i);
    }
  }

  // conditions to enter critical section
  while() {
    // TODO
  }

  // critical section at post
  pthread_mutex_lock(&crit_mutex);
  sleep(drawTime());
  pthread_mutex_lock(&shared_mutex);
  removeNode(queues[location]);
  for(i = 0; i < size; i++) {
		if(i != rank) {
			MPI_Send(location, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
      printf("Jestem %d i wyslalem wiadomosc RELEASE dla transportera %d\n", rank, location);
    }
  }
  location = P;
  pthread_mutex_unlock(&shared_mutex);
  pthread_mutex_unlock(&crit_mutex);


  // local section in Lazaret
  sleep(drawTime);
  int new_location = drawLocation();

  pthread_mutex_lock(&shared_mutex);
  clock_val = clock_val + 1;
  action = 1;

  addNode(queues[new_location], rank, clock_val, action);

  msg_req[0] = clock_val; msg_req[1] = new_location; msg_req[2] = action;
  confirmation_counter = 0;
  pthread_mutex_unlock(&shared_mutex);

  for(i = 0; i < size; i++) {
    if(i != rank) {
      MPI_Send(msg_req, REQ_SIZE, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
      printf("Jestem %d i wyslalem wiadomosc do %d\n", rank, i);
    }
  }

  // conditions to enter critical section
  while() {
    // TODO
  }

  // critical section in Lazaret
  pthread_mutex_lock(&shared_mutex);
  location = new_location; // ATTENTION: changed before critical section -> blocking mechanism in helper thread checking wheather requested teleporter's in use
  pthread_mutex_unlock(&shared_mutex);

  pthread_mutex_lock(&crit_mutex);
  sleep(drawTime());
  pthread_mutex_lock(&shared_mutex);
  removeNode(queues[P]);
  removeNode(queues[location]);
  for(i = 0; i < size; i++) {
    if(i != rank) {
      MPI_Send(location, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
      printf("Jestem %d i wyslalem wiadomosc RELEASE dla transportera %d\n", rank, location);
    }
  }
  pthread_mutex_unlock(&shared_mutex);
  pthread_mutex_unlock(&crit_mutex);





  /*// test initialization functionality
  printf( "Jestem %d z %d na maszynie %s na posterunku %d \n", rank, size, processor_name, location);

  addNode(queues[0], 6, 6, -1);
  addNode(queues[0], 3, 5, 1);
  addNode(queues[0], 1, 5, -1);
  addNode(queues[0], 5, 4, 1);
  addNode(queues[0], 4, 4, 1);
  addNode(queues[0], 2, 5, -1);
  printList(queues[0]);*/

  sleep(10);
  pthread_cancel(thread_id);
  pthread_mutex_destroy(&shared_mutex);
  pthread_mutex_destroy(&crit_mutex);
  MPI_Finalize();
}

// function definitions
List* createList() {
  List *ptr = malloc(sizeof(*ptr));

  if(ptr == NULL)
    fprintf(stderr, "LINE: %d, malloc() failed\n", __LINE__);

  ptr->head = NULL;
  return ptr;
}

void addNode(List *list, int rank, int clock, int action) {
  Node *ptr = malloc(sizeof(*ptr));

  if(ptr == NULL) {
    fprintf(stderr, "IN %s, %s: malloc() failed\n", __FILE__, "addNode");
    return;
  }

  ptr->rank = rank;
  ptr->clock = clock;
  ptr->action = action;
  ptr->next = NULL;

  if(list->head == NULL)
    list->head = ptr;
  else {
    Node *tmp = list->head;
    if(checkIfInsert(ptr, tmp)) {
      ptr->next = list->head;
      list->head = ptr;
    }
    else {
      while(tmp->next != NULL) {
        if(checkIfInsert(ptr, tmp->next)) {
          ptr->next = tmp->next;
          tmp->next = ptr;
          return;
        }
        else {
          tmp = tmp->next;
        }
      }
      tmp->next = ptr;
    }
  }
}

bool checkIfInsert(Node *added, Node *tmp) {
  bool decision = false;
  if(added->action == tmp->action) {
    if(added->clock == tmp->clock) {
      if(added->rank < tmp->rank)
        decision = true;
    }
    else if(added->clock < tmp->clock)
      decision = true;
  }
  else if(added->action < tmp->action)
    decision = true;

  return decision;
}

void removeNode(List *list) {
  Node *tmp = list->head;
  list->head = list->head->next;
  free(tmp);
}

void printList(List *list) {
  if(!list->head)
    printf("Empty list!\n");
  else {
    Node *tmp = list->head;
    while(tmp != NULL) {
      printf("P%d: clock=%d, action=%d\n", tmp->rank, tmp->clock, tmp->action);
      tmp = tmp->next;
    }
  }
}

int drawLocation() { return (rand() % P); }

int drawTime() { return (rand() % (MAX_TIME+1)); }
