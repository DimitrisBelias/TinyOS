#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H


#include "tinyos.h"
#include "kernel_streams.h"

#define PIPE_BUFFER_SIZE 4000

typedef struct struct_pipe_control_block{

FCB *reader , *writer ;

CondVar has_space; /*For writer  */
CondVar has_data ; /*For reader */

int w_position , r_position ;

char BUFFER[PIPE_BUFFER_SIZE];

int empty_slots;


}pipe_cb;


pipe_cb* initialize_pipe_cb();
int sys_Pipe(pipe_t* pipe);
int pipe_reader_close(void* _pipecb);
int pipe_writer_close(void* _pipecb);
int pipe_read(void* pipecb_t, char* buf, unsigned int n);
int pipe_write(void* pipecb_t, const char *buf, unsigned int n);
int do_nothing();
void* do_nothing_pt();


#endif