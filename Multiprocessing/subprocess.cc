/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
using namespace std;

subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {
    int supply_fds[2];
    int ingest_fds[2];

    if (pipe(supply_fds) == -1){
	fprintf(stderr, "Failed to make the supply pipeline.\n");
    }

    if (pipe(ingest_fds) == -1){
	fprintf(stderr, "Failed to make the ingest pipeline.\n");
    }

    struct subprocess_t process = {fork(), kNotInUse, kNotInUse};

    if (process.pid == 0){
	if (ingestChildOutput){
	    close(ingest_fds[0]);
	    dup2(ingest_fds[1],1);
	    close(ingest_fds[1]);
	    //This dup the write end of the ingest pipe to the STDOUT when the ingest flag is on
	}else{
	    close(ingest_fds[0]);
	    close(ingest_fds[1]);
	    //Otherwise, close both ends of the pipe
	}if (supplyChildInput){
	    close(supply_fds[1]);
	    dup2(supply_fds[0],0);
	    close(supply_fds[0]);
	    //This dup the read end of the supply pipe to the STDIN when the supply flag is on
	}
	else{
	    close(supply_fds[0]);
	    close(supply_fds[1]);
	}
	execvp(argv[0], argv);
    }
    close(ingest_fds[1]);
    close(supply_fds[0]);
    //when finished, close the write end of the ingest and read end of supply in the parent
    if(supplyChildInput) {
	process.supplyfd = supply_fds[1];
	//Parent write to the child through the write end of supply
    }
    if(ingestChildOutput) {
	process.ingestfd = ingest_fds[0];
	//Parent read from the child through the read end of ingest
    }
    return process;

}
