/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
    int fds[2];
    if (pipe(fds) == -1){
        fprintf(stderr, "Failed to make the pipeline.\n");
    }
    //Pipe the two file descriptors
    int write_fd = fds[1];
    int read_fd = fds[0];
    pid_t pid = fork();
    if (pid == -1){
        printf("Failed to fork\n");
    }
    //Fork off the first child
    if (pid == 0){
        pid_t child_pid1 = getpid();
	//Get the pid of the first child
        pids[0] = child_pid1;
        close(read_fd);
        dup2(write_fd,1);
	//Connect write end of the pipe in the first child with the standard output
        execvp(argv1[0],argv1);
        close(write_fd);
    }

    pid_t pid2 = fork();
    if (pid2 == -1){
        printf("Failed to fork\n");
    }

    if (pid2 == 0){
        pid_t child_pid2 = getpid();
        pids[1] = child_pid2;
        close(write_fd);
        dup2(read_fd,0);
	//Connect read end of the pipe in the second child with the standard input
        execvp(argv2[0],argv2);
        close(read_fd);
    }
    //When all is done, close the read and write file descriptors
    close(read_fd);
    close(write_fd);

}
