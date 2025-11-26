#include "tinyos.h"


pipe_cb* initialize_pipe_cb();

int sys_Pipe(pipe_t* pipe);

int pipe_read(*void pipecb_t, char* buf, unsigned int n);

int pipe_write(void* pipecb_t, const char *buf, unsigned int n);

int pipe_reader_close(void* pipe_cb);

int pipe_writer_clode(void* pipe_cb);