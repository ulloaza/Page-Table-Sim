#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long) - __builtin_clzll((X)) - 1))

// Init the Frame Sim:
int nextFrame = 1;
int faults = 0;
int recentUse = 1;

// Function to process the virt -> phys address translation, and handles simulating the PageTable and Frames
unsigned long physicalTranslation(unsigned long virtAddr, int pSize, int totPTE, int totFrames, int **PTEvalid, int **PTEframes, int **PTElastref) {
    int pageNum = 0;
    int frameNum = 0;
    pageNum = virtAddr >> LOG2(pSize);
    unsigned long offset = virtAddr & (pSize-1);
    
    if(!(*PTEvalid)[pageNum]) {
        //Page Fault count
        faults++;
        // If there are no more frames...
        if(nextFrame > totFrames) {
            // LRU Here:

            // Find the smallest last reference count:
            int buff = recentUse;
            int LRU = 0;
            for(int j = 0; j < totPTE; j++) {
                if(((*PTElastref)[j] < buff) && ((*PTEvalid)[j] == 1)) {
                    buff = (*PTElastref)[j];
                    LRU = j;
                }
            }
            // Set that PTE as invalid so we can steal the frame...
            (*PTEframes)[pageNum] = (*PTEframes)[LRU];
            (*PTEvalid)[pageNum] = 1;
            frameNum = (*PTEframes)[pageNum];
            frameNum = frameNum << LOG2(pSize);
            // CLear old PTE:
            (*PTEvalid)[LRU] = 0;
            (*PTEframes)[LRU] = 0;
            (*PTElastref)[LRU] = 0;
            (*PTElastref)[pageNum] = recentUse;
        }
        // Get a frame like normal:
        else {
            (*PTEframes)[pageNum] = nextFrame;
            (*PTElastref)[pageNum] = recentUse;
            (*PTEvalid)[pageNum] = 1;
            frameNum = (*PTEframes)[pageNum];
            frameNum = frameNum << LOG2(pSize);
            nextFrame++;
        }
    }
    else {
        frameNum = (*PTEframes)[pageNum];
        frameNum = frameNum << LOG2(pSize);
        (*PTElastref)[pageNum] = recentUse;
    }
    recentUse++;
    //unsigned long physAddr = pageNum | offset;
    return (unsigned long) frameNum | offset;
}

int main(int argc, char ** argv) {
    //Prepared variables for later use.
	char *inFileName;
    char *outFileName;
	int fd_in;
    FILE * fd_out;
    int i;
	struct stat st;
	unsigned long filesize;
	unsigned long * memAccesses;

    // Basic usage info:
	if(argc != 6) {
		fprintf(stderr, "Usage: %s <BytesPerPage> <SizeOfVirtualMemory> <SizeOfPhysicalMemory> <SequenceFile> <outputfile>\n", argv[0]);
		exit(0);
	}
    // Parse and store CLI args in respective field:
    int pageSize = atoi(argv[1]);
    int virtMemSz = atoi(argv[2]);
    int physMemSz = atoi(argv[3]);
	inFileName = argv[4];
    outFileName = argv[5];

    // Calculate the amount of Page Table Entries and Frames required (Subtracting 1 for the OS Frame):
    int totalPTEs = virtMemSz / pageSize;
    int totalFrames = (physMemSz / pageSize) - 1;

    // Init the three arrays for the valid, frame, and recent use:
    int* PTEvalid = malloc(totalPTEs * sizeof(int));
    int* PTEframes = malloc(totalPTEs * sizeof(int));
    int* PTElastref = malloc(totalPTEs * sizeof(int));
    memset (PTEvalid, 0, sizeof (int) * totalPTEs);
    memset (PTEframes, 0, sizeof (int) * totalPTEs);
    memset (PTElastref, 0, sizeof (int) * totalPTEs);

	stat(inFileName, &st);
	filesize = st.st_size;
	// allocate space for all addresses
	memAccesses = (unsigned long*) malloc( (size_t) filesize );
	// use open and read the input file
	fd_in = open(inFileName, O_RDONLY);
	if(fd_in == -1) {
		fprintf(stderr, "fd in is invalid, with error: %s\n", strerror(errno));
		exit(-1);
	}
	read(fd_in, memAccesses, (int) filesize);
	close(fd_in);
	// use open and read the output file
    if ((fd_out = fopen(outFileName, "wb")) == NULL){
       printf("Error! opening file");
       exit(1);
    }
    // Traverse all address
	for(i = 0; i < filesize/(sizeof (unsigned long)); i++) {
      unsigned long virtAddr = 0;
      virtAddr = (unsigned long) memAccesses[i];
      unsigned long physAddress = physicalTranslation(virtAddr, pageSize, totalPTEs, totalFrames, &PTEvalid, &PTEframes, &PTElastref);
      printf("virtual address %d = %#010lx -> physical address %#010lx\n", i, memAccesses[i], physAddress);
      fwrite(&physAddress, sizeof(unsigned long), 1, fd_out);
	}
    fclose(fd_out);
    printf("Page Faults: %d\n", faults);
    // free dynamically allocated memory
    free(PTEvalid);
    free(PTEframes);
    free(PTElastref);
	free(memAccesses);
}