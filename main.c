#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))

#define NUM_ASSISTANTS 3
#define PAINT_MAX 3 

#define BRUSH_CLEAN -1

#define TRUE 1
#define FALSE 0

#define STATE_PAINTING 101
#define STATE_THINKING 102
#define STATE_FINISHED 103
#define STATE_WANTS_TO_PAINT 104

#define FULL_PAINTING 10

#define MIN_THINKING 250000
#define MAX_THINKING 1500000

#define MIN_ASSISTANT_SLEEP MIN_THINKING * 2
#define MAX_ASSISTANT_SLEEP MAX_THINKING * 2

#define PRINT_STATE_INTERVAL 2

#define WINE_MAX 4

pthread_mutex_t workshop = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t* painters_cond;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int* painters_state;
int* paints, *brushes;
int* painting;
int wine = WINE_MAX;
int n = 10;
unsigned r_seed;

void print_state() {
	int i;
	char* painter_state = "painting";
	printf("-------------------------\n");
	printf("Workshop state:\nBrushes:\n");
	for(i = 0; i < n/2; i++) {
		if(brushes[i] == BRUSH_CLEAN) {
			printf("[#%d: clean] ", i);
		} else {
			printf("[#%d: Painter#%d] ", i, brushes[i]);
		}
	}
	printf("\nPaints:\n");
	for(i = 0; i < n/2; i++) {
		printf("[#%d: %d] ", i, paints[i]);
	}
	printf("\nWine: %d\nPainters:\n", wine);
	for(i = 0; i < n; i++) {
		painter_state = "painting";
		if( painters_state[i] == STATE_THINKING ) {
			painter_state = "thinking";
		}
		if( painters_state[i] == STATE_FINISHED ) {
			painter_state = "finished";
		}
		if( painters_state[i] == STATE_WANTS_TO_PAINT ) {
			painter_state = "wants to paint";
		}
		printf("[#%d: %s, %d] ", i, painter_state, painting[i]);
	}
	printf("\n-------------------------\n");
	 
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
	int cleaned_something = 0;
	
	printf("Assistant#1 started work!\n");
	while(someone_is_painting()) {
		usleep(rand_r(&r_seed) % (MAX_ASSISTANT_SLEEP - MIN_ASSISTANT_SLEEP) + MIN_ASSISTANT_SLEEP);
		
		/* Lock workshop */
		pthread_mutex_lock(&workshop);
		for(i = 0; i < n/2; i++) {
			if( painters_state[(2*i) % n] != STATE_PAINTING && painters_state[(2*i + 1) % n] != STATE_PAINTING ) {
				brushes[i] = BRUSH_CLEAN;
				cleaned_something = 1;
			}
		}

		if(wine > 0 && cleaned_something) {
			/* Signal waiting threads (only if cleaned at least one brush and there's wine) */
			pthread_cond_broadcast(&cond);
		}

		/* Release workshop */
		pthread_mutex_unlock(&workshop);
		printf("Assistant#1 cleaned all brushes.\n");
	}

	printf("Assistant#1 ended work!\n");
	pthread_exit(NULL);
}

/*
* Assistant responsible for paint refill.
*/
void* assistant2(void* arg) {
	int i;

	printf("Assistant#2 started work!\n");

	while(someone_is_painting()) {
		usleep(rand_r(&r_seed) % (MAX_ASSISTANT_SLEEP - MIN_ASSISTANT_SLEEP) + MIN_ASSISTANT_SLEEP);
		/* Lock workshop */
		pthread_mutex_lock(&workshop);
		for(i = 0; i < n/2; i++) {
			paints[i] = PAINT_MAX;
		}
		if(wine > 0) {
			/* Signal waiting threads (only if there's wine) */
			pthread_cond_broadcast(&cond);
		}
		
		/* Release workshop */
		pthread_mutex_unlock(&workshop);

		printf("Assistant#2 refilled all paints.\n");
	}

	printf("Assistant#2 ended work!\n");
	pthread_exit(NULL);
}

/*
* Assistant responsible for wine delivery.
*/
void* assistant3(void* arg) {
	printf("Assistant#3 started work!\n");	
	while(someone_is_painting()) {
		usleep(rand_r(&r_seed) % (MAX_ASSISTANT_SLEEP - MIN_ASSISTANT_SLEEP) + MIN_ASSISTANT_SLEEP);
		
		/* Lock workshop */
		pthread_mutex_lock(&workshop);
		if(wine != WINE_MAX) {
			wine = WINE_MAX;
			/* Signal waiting threads. */
			pthread_cond_broadcast(&cond);
		}
		/* Release workshop */
		pthread_mutex_unlock(&workshop);
		printf("Assistant#3 refilled wine bottle.\n");
	}
	
	printf("Assistant#3 ended work!\n");
	pthread_exit(NULL);
}

/*
* Painter thread.
*/
void* painter(void* arg) {
	int painterId = *((int*)arg);
	int brush = painterId / 2;
	int paint = (painterId % 2) ? (painterId/2 + 1) % n/2 : painterId/2;
	printf("Painter#%d started!\n", painterId);

	while(painting[painterId] != FULL_PAINTING) {
		
		usleep(rand_r(&r_seed) % (MAX_THINKING - MIN_THINKING) + MIN_THINKING);		
		painters_state[painterId] = STATE_WANTS_TO_PAINT;
		printf("Painter#%d wants to paint...\n", painterId);

		/* Lock workshop in order to check resource availability. */
		pthread_mutex_lock(&workshop);

		/* wait for resources */
		while( !painting_possible(painterId) ) {
			pthread_cond_wait(&cond, &workshop);
		}

		/* Set own state to painting */
		painters_state[painterId] = STATE_PAINTING;
		printf("Paitner#%d is painting.\n", painterId);

		/* Get resources */
		wine--;
		brushes[brush] = painterId;
		paints[paint]--;


		/* Unlock workshop */
		pthread_mutex_unlock(&workshop);
				
		painting[painterId]++;

		/* Set own state to thinking. */
		painters_state[painterId] = STATE_THINKING;

		/* Signal others about painting end. Used in order to signal that paint vessel may be used now by your neightbour.  */
		pthread_cond_broadcast(&cond);

		/*printf("Painter's #%d painting: %d\n", painterId, painting[painterId]);*/

	}
	
	painters_state[painterId] = STATE_FINISHED;
	printf("Painter#%d finished his painting!\n", painterId);
	pthread_exit(NULL);
}

int main(int argc, char** argv) {
	pthread_t assistants[NUM_ASSISTANTS];
	pthread_t* painters;
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

	/* Intializing brushes as clean and paints' vessels as full */
	for(i = 0; i < n/2; ++i) {
		brushes[i] = BRUSH_CLEAN;
		paints[i] = PAINT_MAX;
	}

	for(i = 0; i < n; i++) {
		/* Initializing conditional variables associated with painters */
		pthread_cond_init(&painters_cond[i], NULL);
	}

	if(pthread_create(&assistants[0], NULL, assistant1, NULL)!= 0)
			ERR("pthread_create");
	if(pthread_create(&assistants[1], NULL, assistant2, NULL)!= 0)
			ERR("pthread_create");
	if(pthread_create(&assistants[2], NULL, assistant3, NULL)!= 0)
			ERR("pthread_create");
	
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

		/* Starting painters threads. */
		if(pthread_create(&painters[i], NULL, painter, (void*)paintersIds[i])!= 0)
			ERR("pthread_create");
	}

	/* This loop is used only for printing state (every 2 seconds table state is printed to stdout) */
	while(someone_is_painting()) {
		sleep(2);
		print_state();
	}
	
	/* Printing final state */
	print_state();


	for(i = 0; i < n; i++) {
		if(pthread_join(painters[i], NULL) == -1) {
			ERR("pthread_join");
		}
	}

	for(i = 0; i < 3; i++) {
		if(pthread_join(assistants[i], NULL) == -1) {
			ERR("pthread_join");
		}
	}

	for(i = 0; i < n; i++) {
		pthread_cond_destroy(&painters_cond[i]);
	}

	free(paintersIds);
	free(painters);	

	return EXIT_SUCCESS;

}
