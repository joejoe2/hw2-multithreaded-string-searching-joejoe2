#ifndef SERVER_H
#define SERVER_H

struct job_queue
{
    struct job *head;
    struct job *tail;
};
struct job
{
    char** query;
    int cnt;
    struct job *next;
    int client;
};

#endif