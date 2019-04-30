#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define P 10 // number of posts
#define L 15 // capacity of lazaret

// structures
typedef struct Node {
  int rank;
  int clock;
  int action;
  struct Node *next;
} Node;

typedef struct List {
  Node *head;
  Node *tail;
} List;

// function declarations
List* createList();
void addNode(List*, int, int, int);
void removeNode(List*);
void printList(List*);

int drawLocation();

// pthread functionality
void *helperThread(void *vargp) {
  while(1) {
    sleep(10);
    printf("Helper thread - 10 seconds passed!\n");
  }
}

int main(int argc, char **argv) {
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int rank, size, namelen; // Jedi id, Jedi nbr, machine name length

  int location, clock = 0;
  List* queues[P+1]; // index coresponds to post id; lazaret's last

  pthread_t thread_id;

  // initialize
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Get_processor_name(processor_name, &namelen); // machine name in used cluster

  srand(time(NULL) + rank);
  location = drawLocation();
  for(int i = 0; i < (P+1); i++)
    queues[i] = createList();

  pthread_create(&thread_id, NULL, helperThread, NULL);



  // test initialization functionality
  printf( "Jestem %d z %d na maszynie %s na posterunku %d \n", rank, size, processor_name, location);

  addNode(queues[0], 1, 0, -1);
  addNode(queues[0], 2, 5, +1);
  printList(queues[0]);
  removeNode(queues[0]);
  printList(queues[0]);
  removeNode(queues[0]);
  printList(queues[0]);

  sleep(30);

  MPI_Finalize();
}

// function definitions
List* createList() {
  List *ptr = malloc(sizeof(*ptr));

  if(ptr == NULL)
    fprintf(stderr, "LINE: %d, malloc() failed\n", __LINE__);

  ptr->head = ptr->tail = NULL;
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
    list->head = list->tail = ptr;
  else {
    list->tail->next = ptr;
    list->tail = ptr;
  }
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

int drawLocation() { return (rand() % (P + 1)); }
