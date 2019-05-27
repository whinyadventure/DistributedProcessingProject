#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

#define P 2  // number of posts
#define L 1  // capacity of lazaret
#define MAX_TIME 2  // sleep duration [seconds]
#define REQUEST 0  // msg type flags
#define CONFIRM 1
#define RELEASE 2
#define MSG_SIZE 3

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
int rank, size, namelen;  // Jedi id, Jedi nbr, machine name length

int request_clock_val, confirmation_counter;  // helper variables

int location, clock_val, action;  // action: 1 -> enter Lazaret, -1 -> leave Lazaret
List* queues[P+1];  // index coresponds to post id; Lazaret's last

pthread_mutex_t shared_mutex;
pthread_mutex_t crit_mutex;

// function declarations
List* createList();
void addNode(List*, int, int, int);
bool checkIfInsert(Node*, Node*);
void removeNodeLazaret(List*, int);
void removeNodeLocation(List*);
void printList(List*);
bool checkLazaret(List*, int);

int drawLocation();
int drawTime();

// pthread functionality
void *helperThread(void *vargp) {
  MPI_Status status;
  int msg[MSG_SIZE];

  while(1) {
    MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    if(status.MPI_TAG == REQUEST) {  // request received
      if(msg[1] == location) { // check if requested teleporter is in use
        pthread_mutex_lock(&crit_mutex);
	pthread_mutex_unlock(&crit_mutex); 
	}

      //printf("Rank %d, otrzymałem żądanie od %d\n", rank, status.MPI_SOURCE);

      pthread_mutex_lock(&shared_mutex);

      clock_val = max(msg[0], clock_val) + 1;  // clock synchronization
      addNode(queues[msg[1]], status.MPI_SOURCE, msg[0], msg[2]);  // add to teleporter request queue
      if(msg[2] == 1)
        addNode(queues[P], status.MPI_SOURCE, msg[0], msg[2]);  // if wants to enter Lazaret then add to Lazaret request queue

      msg[0] = clock_val; msg[1] = rank; msg[2] = 0;
      MPI_Send(msg, MSG_SIZE, MPI_INT, status.MPI_SOURCE, CONFIRM, MPI_COMM_WORLD);
      //printf("Rank %d, wysłałem potwierdzenie do %d\n", rank, status.MPI_SOURCE);

      pthread_mutex_unlock(&shared_mutex);
      //pthread_mutex_unlock(&crit_mutex);  // tutaj ??
    }
    else if(status.MPI_TAG == CONFIRM) {  // confirmation received
      //printf("Rank %d, otrzymałem potwierdzenie od %d\n", rank, status.MPI_SOURCE);
      if(msg[0] > request_clock_val)
        confirmation_counter = confirmation_counter + 1;
      else
        printf("Error! Wartość zegara z potwierdzenia mniejsza od tej z żądania!");
    }
    else if(status.MPI_TAG == RELEASE) {
      //printf("Rank %d, otrzymałem wiadomość RELEASE od %d\n", rank, status.MPI_SOURCE);
      pthread_mutex_lock(&shared_mutex);

      removeNodeLocation(queues[msg[1]]);
      if(msg[2] == -1)
        removeNodeLazaret(queues[P], status.MPI_SOURCE);

      pthread_mutex_unlock(&shared_mutex);
    }
  }
}

int main(int argc, char **argv) {
  pthread_t thread_id;
  pthread_mutex_init(&shared_mutex, NULL);
  pthread_mutex_init(&crit_mutex, NULL);

  int msg[MSG_SIZE];
  int new_location;
  int i;

  // initialization
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Get_processor_name(processor_name, &namelen); // machine name in used cluster

  srand(time(NULL) + rank);
  location = drawLocation(); clock_val = 0; action = 1;
  for(i = 0; i < (P+1); i++)
    queues[i] = createList();

  pthread_create(&thread_id, NULL, helperThread, NULL);

  while(1) {
    // local section at post
    printf("Rank %d, sekcja lokalna na posterunku %d\n", rank, location);
    sleep(drawTime());

    confirmation_counter = 1;

    pthread_mutex_lock(&shared_mutex);
    clock_val = clock_val + 1;
    request_clock_val = clock_val;
    action = 1;

    addNode(queues[location], rank, clock_val, action);
    addNode(queues[P], rank, clock_val, action);

    msg[0] = clock_val; msg[1] = location; msg[2] = action;
    pthread_mutex_unlock(&shared_mutex);

    for(i = 0; i < size; i++) {
  		if(i != rank) {
  			MPI_Send(msg, MSG_SIZE, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
        //printf("Rank %d, wysłałem żądanie do %d o teleporter %d\n", rank, i, location);
      }
    }
printList(queues[P]);
    // conditions to enter critical section; active waiting
    while(confirmation_counter != size || queues[location]->head->rank != rank || !checkLazaret(queues[P], rank)) {}

    //printf("Rank %d, warunki wejścia do sekcji krytycznej spełnione\n", rank);

    // critical section at post
    printf("Rank %d, sekcja krytyczna na posterunku\n", rank);
    //printf("Jedi %d teleportuje się do Lazaretu z posterunku %d", rank, location);
    pthread_mutex_lock(&crit_mutex);
    sleep(drawTime());
    pthread_mutex_lock(&shared_mutex);

    removeNodeLocation(queues[location]);

    msg[1] = location; msg[2] = 1;
    for(i = 0; i < size; i++) {
  		if(i != rank) {
  			MPI_Send(msg, MSG_SIZE, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
        //printf("Rank %d, wysłałem wiadomość RELEASE transportera %d do procesu %d\n", rank, location, i);
      }
    }
    location = P;
    pthread_mutex_unlock(&shared_mutex);
    pthread_mutex_unlock(&crit_mutex);


    // local section in Lazaret
    printf("Rank %d, sekcja lokalna w Lazarecie\n", rank);
    sleep(drawTime());

    confirmation_counter = 1;

    new_location = drawLocation();

    pthread_mutex_lock(&shared_mutex);
    clock_val = clock_val + 1;
    action = -1;

    addNode(queues[new_location], rank, clock_val, action);

    msg[0] = clock_val; msg[1] = new_location; msg[2] = action;

    pthread_mutex_unlock(&shared_mutex);

    for(i = 0; i < size; i++) {
      if(i != rank) {
        MPI_Send(msg, MSG_SIZE, MPI_INT, i, REQUEST, MPI_COMM_WORLD);
        //printf("Rank %d, wysłałem żądanie na wyjście z Lazaretu do procesu %d\n", rank, i);
      }
    }

    // conditions to enter critical section
    while(confirmation_counter != size || queues[new_location]->head->rank != rank) {}
 pthread_mutex_lock(&crit_mutex);
    // critical section in Lazaret
    printf("Rank %d, sekcja krytyczna w Lazarecie\n", rank);
	
    pthread_mutex_lock(&shared_mutex);
    location = new_location; // ATTENTION: changed before critical section -> blocking mechanism in helper thread checking wheather requested teleporter's in use
    removeNodeLazaret(queues[P], rank);
    pthread_mutex_unlock(&shared_mutex);
    //printf("Jedi %d teleportuje się z Lazaretu do posterunku %d", rank, location);

   
    sleep(drawTime());
    pthread_mutex_lock(&shared_mutex);
    removeNodeLocation(queues[location]);
    msg[1] = location; msg[2] = -1;
    for(i = 0; i < size; i++) {
      if(i != rank) {
        MPI_Send(msg, MSG_SIZE, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
        //printf("Rank %d, wysłałem wiadomość RELEASE transportera %d do procesu %d\n", rank, location, i);
      }
    }
    pthread_mutex_unlock(&shared_mutex);
    pthread_mutex_unlock(&crit_mutex);
  }

  printf("KONIEC\n");
  sleep(30);
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

void removeNodeLazaret(List *list, int rank){
    Node *tmp = list->head;

    if(tmp->rank == rank){
	list->head = list->head->next;
	free(tmp);
	
}else{
    while(tmp->next != NULL){
	if(tmp->next->rank == rank){
		Node *delete = tmp->next;
		tmp->next = delete->next;
		free(delete);
		break;
	}
    }
}
}

void removeNodeLocation(List *list) {
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

bool checkLazaret(List *list, int rank) {
  int counter = 0;
  Node *tmp = list->head;

  while(tmp->rank != rank) {
    counter = counter + 1;
    tmp = tmp->next;
  }

  if(counter < L)
    return true;
  else
    return false;
}

int drawLocation() { return (rand() % P); }

int drawTime() { return (rand() % (MAX_TIME+1)); }
