#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))
/* #define usleep(smth) (usleep(0)) */

#define NUM_ASSISTANTS 3
#define PAINT_MAX 3 

#define BRUSH_CLEAN -1

#define TRUE 1
#define FALSE 0

#define STATE_PAINTING 101
#define STATE_THINKING 102
#define STATE_FINISHED 103

#define FULL_PAINTING 10

#define MIN_THINKING 2
#define MAX_THINKING 7

#define MIN_ASSISTANT_SLEEP 20
#define MAX_ASSISTANT_SLEEP 70

#define WINE_MAX 4

pthread_mutex_t workshop = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t* painters_mutex;
pthread_cond_t* painters_cond;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_t* painters;
int* painters_state;
int* paints, *brushes;
int* painting;
int wine = WINE_MAX;
int n = 10;
unsigned r_seed;

void print_state() {
	int i;
	printf("Workshop state:\nBrushes:\n");
	for(i = 0; i < n/2; i++) {
		printf("[#%d: %d] ", i, brushes[i]);
	}
	printf("\nPaints:\n");
	for(i = 0; i < n/2; i++) {
		printf("[#%d: %d] ", i, paints[i]);
	}
	printf("\nWine: %d\nPainters:\n", wine);
	for(i = 0; i < n; i++) {
		char* str = "painting";
		if( painters_state[i] == STATE_THINKING ) {
			str = "thinking";
		}
		if( painters_state[i] == STATE_FINISHED ) {
			str = "finished";
		}
		printf("[#%d: %s, %d] ", i, str, painting[i]);
	}
	printf("\n"); 
}

int painting_possible(int painter) {
	int brush = painter/2;
	int paint = (painter % 2) ? (painter/2 + 1) % n/2 : painter/2;
	return wine > 0 &&
		painters_state[ (painter + 1) % n ] != STATE_PAINTING && 
		painters_state[ (painter + n - 1) % n ] != STATE_PAINTING && 
		paints[paint] > 0 && 
		(brushes[brush] == painter || brushes[brush] == BRUSH_CLEAN);
}

int someone_is_painting() {
	int i;
	for(i = 0; i < n && painting[i] == FULL_PAINTING; i++);
	
	return i != n;
}

/*
* Paintbrush cleaner.
*/
void* assistant1(void* arg) {
	int i;
	
//	fprintf(stderr, "Assistant#1 started work!\n");
	while(someone_is_painting()) {
		usleep(rand_r(&r_seed) % (MAX_ASSISTANT_SLEEP - MIN_ASSISTANT_SLEEP) + MIN_ASSISTANT_SLEEP);
		
		pthread_mutex_lock(&workshop);
		for(i = 0; i < n/2; i++) {
			brushes[i] = BRUSH_CLEAN;
		}
//		print_state();

		if(wine > 0) {
			pthread_cond_broadcast(&cond);
		}

		pthread_mutex_unlock(&workshop);
//		printf("Assistant cleaned all brushes.\n");
	}

//	fprintf(stderr, "Assistant#1 ended work!\n");
	pthread_exit(NULL);
}

/*
* Assistant responsible for paint refill.
*/
void* assistant2(void* arg) {
	int i;

//	fprintf(stderr, "Assistant#2 started work!\n");

	while(someone_is_painting()) {
		usleep(rand_r(&r_seed) % (MAX_ASSISTANT_SLEEP - MIN_ASSISTANT_SLEEP) + MIN_ASSISTANT_SLEEP);
		
		pthread_mutex_lock(&workshop);
		for(i = 0; i < n/2; i++) {
			paints[i] = PAINT_MAX;
		}
		if(wine > 0) {
			pthread_cond_broadcast(&cond);
		}

		pthread_mutex_unlock(&workshop);

//		printf("Assistant refilled all paints.\n");
	}

//	fprintf(stderr, "Assistant#2 ended work!\n");
	pthread_exit(NULL);
}

/*
* Assistant responsible for wine delivery.
*/
void* assistant3(void* arg) {
//	fprintf(stderr, "Assistant#3 started work!\n");	
	while(someone_is_painting()) {
		usleep(rand_r(&r_seed) % (MAX_ASSISTANT_SLEEP - MIN_ASSISTANT_SLEEP) + MIN_ASSISTANT_SLEEP);
		
		pthread_mutex_lock(&workshop);
		if(wine != WINE_MAX) {
			wine = WINE_MAX;
		}
		pthread_cond_broadcast(&cond);
		pthread_mutex_unlock(&workshop);
//		printf("Assistant refilled wine bottle.\n");
	}
	
//	fprintf(stderr, "Assistant#3 ended work!\n");
	pthread_exit(NULL);
}

/*
* Painter thread.
*/
void* painter(void* arg) {
	int painterId = *((int*)arg);
	int brush = painterId / 2;
	int paint = (painterId % 2) ? (painterId/2 + 1) % n/2 : painterId/2;
/*	fprintf(stderr, "Painter#%d started!\n", painterId); */

	while(painting[painterId] != FULL_PAINTING) {
		
		print_state();

//		printf("Painter#%d is thinking...\n", painterId);
		usleep(rand_r(&r_seed) % (MAX_THINKING - MIN_THINKING) + MIN_THINKING);
		
//		fprintf(stderr, "[%d], Locking workshop.\n", painterId);
		pthread_mutex_lock(&workshop);

		/* wait for resources */
		while( !painting_possible(painterId) ) {

//			fprintf(stderr, "Painter#%d is waiting for resources...\n", painterId);
//			print_state();
/*			pthread_cond_wait(&painters_cond[painterId], &workshop); */
			pthread_cond_wait(&cond, &workshop);
		}

		/* Set own state to painting */
		painters_state[painterId] = STATE_PAINTING;

/*		printf("Paint#%d: %d, brush#%d: %d, wine: %d\n", (painterId % 2) ? painterId/2 + 1 : painterId/2, paints[ (painterId % 2) ? painterId/2 + 1 : painterId/2]-1,
			 painterId/2,
			 painterId, wine-1);*/

		pthread_mutex_lock(&painters_mutex[painterId]);
		/* Get resources */
		wine--;
		brushes[brush] = painterId;
		paints[paint]--;


		/* Unlock workshop */
		pthread_mutex_unlock(&workshop);
				
		painting[painterId]++;

		painters_state[painterId] = STATE_THINKING;
		pthread_cond_broadcast(&cond);

		pthread_mutex_unlock(&painters_mutex[painterId]);
//		printf("Painter's #%d painting: %d\n", painterId, painting[painterId]);

	}
	
	painters_state[painterId] = STATE_FINISHED;
//	printf("Painter#%d finished his painting!\n", painterId);
	pthread_exit(NULL);
}

int main(int argc, char** argv) {
	pthread_t assistants[NUM_ASSISTANTS];
	int i;
	int** paintersIds;

	if(argc==2) {
		n = atoi(argv[1]);
	}

	r_seed = getpid();

	if( (painters = calloc(n, sizeof(pthread_t))) == NULL ) {
		ERR("calloc");
	}

	if( (paintersIds = calloc(n, sizeof(int*))) == NULL) {
		ERR("calloc");
	}

	if( (painters_cond = calloc(n, sizeof(pthread_cond_t))) == NULL ) {
		ERR("calloc");
	}
	
	if( (paints = calloc(n/2, sizeof(int))) == NULL ) {
		ERR("calloc");
	}

	if( (brushes = calloc(n/2, sizeof(int))) == NULL ) {
		ERR("calloc");
	}

	if( (painters_state = calloc(n, sizeof(int))) == NULL ) {
		ERR("calloc");
	}

	if( (painting = calloc(n, sizeof(int))) == NULL) {
		ERR("calloc");
	}

	if( (painters_mutex = calloc(n, sizeof(pthread_mutex_t))) == NULL) {
		ERR("calloc");
	}

	/* Intializing brushes as clean and paints' vessels as full */
	for(i = 0; i < n/2; ++i) {
		brushes[i] = BRUSH_CLEAN;
		paints[i] = PAINT_MAX;
	}

//	printf("Initial state:\n");
//	printf("Brushes:\n");

//	for(i = 0; i < n/2; ++i) {
//		printf("Brush#%d: %d\n", i, brushes[i]);
//	}

//	for(i = 0; i < n/2; i++) {
//		printf("Paint#%d: %d\n", i, paints[i]);
//	}

	for(i = 0; i < n; i++) {
		/* Initializing conditional variables associated with painters */
		pthread_cond_init(&painters_cond[i], NULL);
		
		/* Initializing paitnes' mutexes */
		pthread_mutex_init(&painters_mutex[i], NULL);
	}

	pthread_create(&assistants[0], NULL, assistant1, NULL);
	pthread_create(&assistants[1], NULL, assistant2, NULL);
	pthread_create(&assistants[2], NULL, assistant3, NULL);
	for(i = 0; i < n; ++i) {
		

		/* Setting state to thinking */
		painters_state[i] = STATE_THINKING;
		/* Setting painters paiting */
		painting[i] = 0;

		/* Passing every painter his id */
		if( (paintersIds[i] = (int*)malloc(sizeof(int))) == NULL ) {
			ERR("malloc");
		}
		*paintersIds[i] = i;

		pthread_create(&painters[i], NULL, painter, (void*)paintersIds[i]);
	}

	for(i = 0; i < n; i++) {
		if(pthread_join(painters[i], NULL) == -1) {
			ERR("pthread_join");
		}
	}

	print_state();
	
	for(i = 0; i < n; i++) {
		pthread_cond_destroy(&painters_cond[i]);
	}

	free(paintersIds);
	free(painters);	

	return EXIT_SUCCESS;

}
