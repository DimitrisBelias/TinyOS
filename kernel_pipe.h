#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H

#include "tinyos.h"
#include "kernel_streams.h"

#define PIPE_BUFFER_SIZE 20

/**
 * @brief Pipe Control Block
 */
typedef struct pipe_control_block {
    FCB *reader, *writer;
    CondVar has_space;
    CondVar has_data;
    int w_position, r_position;
    char BUFFER[PIPE_BUFFER_SIZE];
} pipe_cb;

#endif