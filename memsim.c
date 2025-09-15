#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>


typedef struct {
        int pageNo;
        int modified;
} page;
enum repl { REPL_RAND, REPL_FIFO, REPL_LRU, REPL_CLOCK };
int createMMU( int);
int checkInMemory( int ) ;
int allocateFrame( int ) ;
page selectVictim( int, enum repl) ;
const int pageoffset = 12; /* Page size is fixed to 4 KB */
int numFrames ;

/* Per-frame metadata used by policies */
typedef struct {
    int pageNo; /* -1 if free */
    int modified; /* dirty bit */
    int ref; /* CLOCK reference bit */
    unsigned long lastUsed; /* LRU timestamp */
    unsigned long loadSeq; /* FIFO sequence */
} Frame;

static Frame *framesTbl = NULL;
static unsigned long tickCounter = 0; /* advances every access/install */
static unsigned long loadCounter = 0; /* advances on installs only */
static int clockHandIdx = 0; /* CLOCK pointer */


/* Creates the page table structure to record memory allocation */
int createMMU (int frames)
{
    framesTbl = (Frame*)calloc(frames, sizeof(Frame));
    if (!framesTbl) return -1;
    for (int i = 0; i < frames; i++) {
        framesTbl[i].pageNo = -1;  /* mark free */
        framesTbl[i].modified = 0;
        framesTbl[i].ref = 0;
        framesTbl[i].lastUsed = 0;
        framesTbl[i].loadSeq = 0;
    }
    /* Deterministic by default; allow override for experiments */
	const char *seedEnv = getenv("MEMSIM_SEED");
	srand(seedEnv ? (unsigned)strtoul(seedEnv, NULL, 10) : 1u);
	return 0;
}

/* Checks for residency: returns frame no or -1 if not found */
int checkInMemory(int page_number)
{
    for (int i = 0; i < numFrames; i++) {
        if (framesTbl[i].pageNo == page_number) {
            framesTbl[i].ref = 1; /* used for CLOCK */
            framesTbl[i].lastUsed = ++tickCounter; /* used for LRU   */
            return i;
        }
    }
    return -1;
}

/* allocate page to the next free frame and record where it put it */
int allocateFrame(int page_number)
{
    for (int i = 0; i < numFrames; i++) {
        if (framesTbl[i].pageNo == -1) { /* free slot */
            framesTbl[i].pageNo = page_number;
            framesTbl[i].modified = 0;
            framesTbl[i].ref = 1;
            framesTbl[i].lastUsed = ++tickCounter;
            framesTbl[i].loadSeq = ++loadCounter;
            return i;
        }
    }
    return -1; /* none free (shouldn't happen when caller checks) */
}

/* Selects a victim for eviction/discard according to the replacement algorithm */
page selectVictim(int page_number, enum repl mode)
{
    int victimIdx = -1;

    switch (mode) {
        case REPL_RAND:
            victimIdx = rand() % numFrames;
            break;

        case REPL_FIFO: {
            unsigned long best = (unsigned long)-1;
            for (int i = 0; i < numFrames; i++) {
                if (framesTbl[i].pageNo != -1 && framesTbl[i].loadSeq < best) {
                    best = framesTbl[i].loadSeq;
                    victimIdx = i;
                }
            }
            break;
        }

        case REPL_LRU: {
            unsigned long best = (unsigned long)-1;
            for (int i = 0; i < numFrames; i++) {
                if (framesTbl[i].pageNo != -1 && framesTbl[i].lastUsed < best) {
                    best = framesTbl[i].lastUsed;
                    victimIdx = i;
                }
            }
            break;
        }

        case REPL_CLOCK: {
            while (1) {
                if (framesTbl[clockHandIdx].ref == 0) {
                    victimIdx = clockHandIdx;
                    clockHandIdx = (clockHandIdx + 1) % numFrames;
                    break;
                }
                framesTbl[clockHandIdx].ref = 0; /* give second chance */
                clockHandIdx = (clockHandIdx + 1) % numFrames;
            }
            break;
        }
        default:
            victimIdx = 0; /* fallback */
    }

    /* Package victim info for the caller’s accounting/prints */
    page victim;
    victim.pageNo   = framesTbl[victimIdx].pageNo;
    victim.modified = framesTbl[victimIdx].modified;

    /* Install the new page into the victim's frame */
    framesTbl[victimIdx].pageNo = page_number;
    framesTbl[victimIdx].modified = 0; /* caller will mark on 'W' */
    framesTbl[victimIdx].ref = 1;
    framesTbl[victimIdx].lastUsed = ++tickCounter;
    framesTbl[victimIdx].loadSeq = ++loadCounter;

    return victim;
}


		
int main(int argc, char *argv[])
{
  
	char *tracename;
	int	page_number,frame_no, done ;
	int	do_line;
	int	no_events, disk_writes, disk_reads;
	int debugmode;
 	enum repl  replace;
	int	allocated=0; 
	int	victim_page;
    unsigned address;
    char rw;
	page Pvictim;
	FILE *trace;


        if (argc < 5) {
             printf( "Usage: ./memsim inputfile numberframes replacementmode debugmode \n");
             exit ( -1);
	}
	else {
        tracename = argv[1];	
	trace = fopen( tracename, "r");
	if (trace == NULL ) {
             printf( "Cannot open trace file %s \n", tracename);
             exit ( -1);
	}
	numFrames = atoi(argv[2]);
        if (numFrames < 1) {
            printf( "Frame number must be at least 1\n");
            exit ( -1);
        }
        if (strcmp(argv[3], "lru\0") == 0)
			replace = REPL_LRU;
		else if (strcmp(argv[3], "rand\0") == 0)
			replace = REPL_RAND;
		else if (strcmp(argv[3], "clock\0") == 0)
			replace = REPL_CLOCK;
		else if (strcmp(argv[3], "fifo\0") == 0)
			replace = REPL_FIFO;
		 
        else 
	  {
             printf( "Replacement algorithm must be rand/fifo/lru/clock  \n");
             exit ( -1);
	  }

        if (strcmp(argv[4], "quiet\0") == 0)
            debugmode = 0;
	else if (strcmp(argv[4], "debug\0") == 0)
            debugmode = 1;
        else 
	  {
             printf( "Replacement algorithm must be quiet/debug  \n");
             exit ( -1);
	  }
	}
	
	done = createMMU (numFrames);
	if ( done == -1 ) {
		 printf( "Cannot create MMU" ) ;
		 exit(-1);
        }
	no_events = 0 ;
	disk_writes = 0 ;
	disk_reads = 0 ;

        do_line = fscanf(trace,"%x %c",&address,&rw);

	if (do_line != 2) {
    printf("total memory frames:  %d\n", numFrames);
    printf("events in trace:      0\n");
    printf("total disk reads:     0\n");
    printf("total disk writes:    0\n");
    printf("page fault rate:      0.0000\n");
    fclose(trace); free(framesTbl); return 0;
}

	while ( do_line == 2)
	{
		page_number =  address >> pageoffset;
		frame_no = checkInMemory( page_number) ;    /* ask for physical address */

		if ( frame_no == -1 )
		{
		  disk_reads++ ;			/* Page fault, need to load it into memory */
		  if (debugmode) 
		      printf( "Page fault %8d \n", page_number) ;
		  if (allocated < numFrames)  			/* allocate it to an empty frame */
		   {
                     frame_no = allocateFrame(page_number);
		     allocated++;
                   }
                   else{
		      Pvictim = selectVictim(page_number, replace) ;   /* returns page number of the victim  */
		      frame_no = checkInMemory( page_number) ;    /* find out the frame the new page is in */
		   if (Pvictim.modified)           /* need to know victim page and modified  */
	 	      {
                      disk_writes++;			    
                      if (debugmode) printf( "Disk write %8d \n", Pvictim.pageNo) ;
		      }
		   else
                      if (debugmode) printf( "Discard    %8d \n", Pvictim.pageNo) ;
		   }
		}
		if ( rw == 'R'){
		    if (debugmode) printf( "reading    %8d \n", page_number) ;
		}
		else if ( rw == 'W'){
			/* mark resident page dirty */
			if (frame_no >= 0 && frame_no < numFrames) {
				framesTbl[frame_no].modified = 1;
			}
			if (debugmode) printf( "writting   %8d \n", page_number) ;
		}

		 else {
		      printf( "Badly formatted file. Error on line %d\n", no_events+1); 
		      exit (-1);
		}

		no_events++;
        	do_line = fscanf(trace,"%x %c",&address,&rw);
	}

	fclose(trace);
	free(framesTbl);
	return 0;


	printf( "total memory frames:  %d\n", numFrames);
	printf( "events in trace:      %d\n", no_events);
	printf( "total disk reads:     %d\n", disk_reads);
	printf( "total disk writes:    %d\n", disk_writes);
	printf( "page fault rate:      %.4f\n", (float) disk_reads/no_events);
}
				
