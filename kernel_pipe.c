/***************************

 This is where pipe driver lives 

		|all new code|
 *****************************/

#include "tinyos.h"
#include "kernel_proc.h"
#include"kernel_streams.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_dev.h"
#define PIPE_BUFFER_SIZE 20

/* Calculates used slots */
static int pipe_used_slots(pipe_cb* pipe){

	int count = pipe->w_position - pipe->r_position;

	if(count<0){
		count += PIPE_BUFFER_SIZE;
	}

	return count;
}

/* Calculates empty slots */
static int pipe_empty_slots(pipe_cb* pipe){
	return PIPE_BUFFER_SIZE - 	pipe_used_slots(pipe);
}

/* void* pipecb_t is the pipe_cb* pipecb_t */

int pipe_write(void* pipecb_t, const char *buf, unsigned int n){
	
	/* Casting the first input parameter to the type it is, otherwise compilation error */
	pipe_cb* pipe = (pipe_cb*)pipecb_t;
	int bytes_written = 0;

	/* That means that reader end of pipe is closed so nobody listens. Returning -1 to indicate error. */
	if(pipe->reader == NULL){
		return -1;
	}

	while(bytes_written < n){ /* Write as much as possible */

		/* wait if buffer is full */
		while(pipe_empty_slots(pipe) == 0){
			kernel_broadcast(&pipe->has_data);
			kernel_wait(&pipe->has_space, SCHED_USER);

			/* check if reader closed while waiting */
			if(pipe->reader == NULL)
				return (bytes_written > 0) ? bytes_written : -1;
		}

		/* Write one byte */
		pipe->BUFFER[pipe->w_position] = buf[bytes_written];
		pipe->w_position = (pipe->w_position + 1) % PIPE_BUFFER_SIZE;
		bytes_written++;

	}

	/* Tell readers that data is available */
	kernel_broadcast(&pipe->has_data);
	return bytes_written;
}



int pipe_read(void* pipecb_t, char* buf, unsigned int n){
	pipe_cb* pipe = (pipe_cb*)pipecb_t;
	int bytes_read = 0;

	/* Check the coniditon: writer closed, buffer empty */
	if(pipe->writer == NULL && pipe_used_slots(pipe) == 0)
		return 0; // Handle EOF

	/* Read as much as possible */
	while(bytes_read < n){
		int available_data = pipe_used_slots(pipe);

		while(available_data == 0){

			// In case writer end is closed
			if(pipe->writer == NULL)
				return bytes_read;

			kernel_broadcast(&pipe->has_space);				// wake up writers
			kernel_wait(&pipe->has_data, SCHED_USER);		// sleep readers

			available_data = pipe_used_slots(pipe);
		}

		// Read a byte
		buf[bytes_read] = pipe->BUFFER[pipe->r_position];
		pipe->r_position = (pipe->r_position + 1) % PIPE_BUFFER_SIZE;
		bytes_read++;
	}

	// Signal writters that there is space available
	kernel_broadcast(&pipe->has_space);

	return bytes_read;
}


int pipe_writer_close(void* _pipecb){
	pipe_cb* pipe = (pipe_cb*)_pipecb;

	// Mark writer as closed
	pipe->writer = NULL;

	// Wake up readers waiting for data (they need to know writer is closed to retun EOF)
	kernel_broadcast(&pipe->has_data);

	// If reader end is closed, free the pipe
	if(pipe->reader == NULL){
		free(pipe);
	}

	return 0;
}

int pipe_reader_close(void* _pipecb){
	pipe_cb* pipe = (pipe_cb*)_pipecb;

	// Mark reader closed
	pipe->reader = NULL;

	// Wake up writer waiting for space (need to know reader is closed so as to return an error code)
	kernel_broadcast(&pipe->has_space);

	// If writer is also closed, free the pipe
	if(pipe->writer == NULL){
		free(pipe);
	}

	return 0;

}







/* FILE OPS FOR READER AND WRITER */

file_ops reader_file_ops = {
	.Open = NULL,
	.Read = pipe_read,
	.Write = NULL,
	.Close = pipe_reader_close
};


file_ops writer_file_ops = {
	.Open = NULL,
	.Read = NULL,
	.Write = pipe_write,
	.Close = pipe_writer_close
};






pipe_cb* initialize_pipe_cb(){
	/* THis function is responsible for creating a pipe and initializing the variables of it. */
	
	pipe_cb* pipecb = (pipe_cb*)xmalloc(sizeof(pipe_cb));

	pipecb->reader = NULL;
	pipecb->writer = NULL;

	pipecb->has_data = COND_INIT;
	pipecb->has_space = COND_INIT;

	pipecb->w_position = 0;
	pipecb->r_position = 0;

	return pipecb;

}


int sys_Pipe(pipe_t* pipe)
{
	if(pipe == NULL){
		return -1;
	}

	Fid_t fid[2];
	FCB* fcb[2];

	// Reserve resources using FCB_reserve //
	if(!FCB_reserve(2, fid, fcb)){
		return -1;
	}

	pipe_cb* pipecb = initialize_pipe_cb();

	// Unreserve FCBs in case error occured in pipecb initialization
	if(pipecb == NULL){
		FCB_unreserve(2, fid, fcb);
		return -1;
	}


	/* Connect reader and writer variables to the corresponding fcbs */
	pipecb->reader = fcb[0];
	pipecb->writer = fcb[1];

	fcb[0]->streamobj = pipecb;
	fcb[0]->streamfunc = &reader_file_ops;

	fcb[1]->streamobj = pipecb;
	fcb[1]->streamfunc = &writer_file_ops;

	pipe->read = fid[0];
	pipe->write = fid[1];

	return 0;
}

