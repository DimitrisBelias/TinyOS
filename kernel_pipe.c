/***************************

 This is where pipe driver lives 

 *****************************/

#include "tinyos.h"
#include "kernel_proc.h"
#include"kernel_streams.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_sched.h"
#include "kernel_dev.h"

/* FILE OPS FOR READER AND WRITER */

file_ops reader_file_ops = {
	.Open = no_action_pt,
	.Read = pipe_read,
	.Write = no_action,
	.Close = pipe_reader_close
}


file_ops writer_file_ops = {
	.Open = no_action_pt,
	.Read = no_action,
	.Write = pipe_write,
	.Close = pipe_reader_close
}


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
		return -1
	}

	Fid_t fid[2];
	FCB* fcb[2]

	// Reserve resources using FCB_reserve //
	if(!FCB_reserve(2, fid, fcb)){
		return -1;
	}

	pipe_cb* pipecb = initialize_pipe_cb();

	// Unreserve FCBs in case error occured in pipecb initialization
	if(pipe_cb == NULL){
		FCB_unreserve(2, fid, fcb);
		return -1
	}


	/* Connect reader and writer variables to the corresponding fcbs */
	pipe_cb->reader = fcb[0];
	pipe_cb->writer = fcb[1];

	fcb[0]->streamobj = pipe_cb;
	fcb[0]->streamfunc = &reader_file_ops;

	fcb[1]->streamobj = pipe_cb;
	fcb[1]->streamfunc = &writer_file_ops;

}

