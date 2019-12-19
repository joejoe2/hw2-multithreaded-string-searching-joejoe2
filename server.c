#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>

char* root;
int port;
int size;

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
struct job_queue queue= {NULL, NULL};

void push(struct job* task)
{
    if(queue.head==NULL&&queue.tail==NULL)
    {
        task->next=NULL;
        queue.head=task;
        queue.tail=queue.head;
    }
    else
    {
        task->next=queue.head;
        queue.head=task;
    }
}

struct job* pop()
{
    if(queue.head==NULL&&queue.tail==NULL)
    {
        return NULL;
    }
    else if(queue.tail==queue.head)
    {
        struct job* r=queue.tail;
        queue.head=NULL;
        queue.tail=queue.head;
        return r;
    }
    else
    {
        struct job* r=queue.head;
        while(r!=NULL)
        {
            if(r->next==queue.tail)
            {
                break;
            }
            r=r->next;
        }
        struct job* t=queue.tail;
        r->next=NULL;
        queue.tail=r;
        return t;
    }
}

int search(const char* filename,const char* query)
{

    FILE *fp;
    char ch;
    if((fp=fopen(filename,"r"))==NULL)
    {
        return 0;
    }
    long lSize;
    char *buffer;

    fseek( fp, 0L, SEEK_END);
    lSize = ftell( fp );
    rewind( fp );
    /* allocate memory for entire content */
    buffer = calloc( 1, lSize+1 );
    if( !buffer ) fclose(fp),fputs("memory alloc fails",stderr),exit(1);
    /* copy the file into the buffer */
    if( 1!=fread( buffer, lSize, 1, fp) )
        fclose(fp),free(buffer),fputs("entire read fails",stderr),exit(1);
    //
    fclose(fp);
    /* do your work here, buffer is a string contains the whole text */
    int total=0;
    int count=0;
    int len=0;
    while(1)
    {
        if(query[len]=='\0')
        {
            break;
        }
        len++;
    }
    long index=0;
    ch=buffer[index];
    while(1)
    {
        //printf("%d %c\n",index,ch);
        if(ch==EOF||ch=='\0')
        {
            break;
        }
        if(len==1)
        {
            if(ch==query[0])
            {
                total++;
            }
        }
        else if(len==0)
        {
            break;
        }
        else
        {
            if(ch==query[0])
            {
                int i;
                for(i=0; i<len; ++i)
                {
                    if(buffer[i+index]!=EOF&&buffer[i+index]!='\0'&&buffer[i+index]==query[i])
                    {
                        count++;
                    }
                    else
                    {
                        break;
                    }
                }
                if(count==len)
                {
                    total++;
                }
                count=0;
            }
        }

        ch=buffer[++index];
    }

    free(buffer);
    return total;
}

int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

void listFilesRecursively(char *basePath,char** result,int *cnt)
{
    char path[1024];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // Unable to open directory stream
    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            if(strlen(basePath)+strlen(dp->d_name)+1>=1023)
            {
                printf("path length >=1023 will be ignored\n");
                return;
            }
            char t[1024];
            memset(t,0,sizeof(t));
            strcpy(t,basePath);
            strcat(t, "/");
            strcat(t,dp->d_name);
            if(is_regular_file(t)&&*cnt<255)
            {
                result[*cnt]=malloc(sizeof(t));
                strcpy(result[*cnt],t);
                *cnt=(*cnt)+1;
            }
            else if(*cnt>=255)
            {
                printf(">= 255 files will be ignored\n");
                return;
            }
            // Construct new path from our base path
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            listFilesRecursively(path,result,cnt);
        }
    }

    closedir(dir);
}

void runnable(int id)
{
    while(1)
    {
        pthread_mutex_lock( &mutex1 ); // 上鎖
        struct job* task=pop();
        pthread_mutex_unlock( &mutex1 ); // 解鎖

        if(task!=NULL)
        {
            //sleep(5);
            pthread_mutex_lock( &mutex1 ); // 上鎖

            //printf("enter thread %d\n",id);
            int client=task->client;

            char message[4096];

            char** files=malloc(256*sizeof(char*));
            int count=0;
            listFilesRecursively(root,files,&count);

            pthread_mutex_unlock( &mutex1 ); // 解鎖

            int i,j;

            memset(message,0,sizeof(message));
            for(j=0; j<task->cnt; j++)
            {
                int total=0;
                strcat(message,"String: ");
                strcat(message,"\"");
                strcat(message,task->query[j]);
                strcat(message,"\"");
                strcat(message,"\n");
                for(i=0; i<count; i++)
                {
                    pthread_mutex_lock( &mutex1 ); // 上鎖
                    int someInt = search(files[i],task->query[j]);
                    pthread_mutex_unlock( &mutex1 ); // 解鎖
                    char t[128];
                    total+=someInt;
                    if(total!=0&&someInt!=0)
                    {
                        sprintf(t, "File: %s, Count: %d\n",files[i],someInt);
                        strcat(message,t);
                    }
                }
                if(total==0)
                {
                    strcat(message,"Not found\n");
                }
                free(task->query[j]);
            }
            send(client,message,strlen(message),0);

            free(files);
            free(task->query);
            free(task);

            close(client);


            sleep(0.2);
        }
        else
        {
            sleep(0.1);
            continue;
        }
    }
}


char** getqstr(char* in,int* c)
{
    int i;
    int cnt=0;
    int len=strlen(in);
    for(i=0; i<len; i++)
    {
        if(in[i]=='"')
        {
            cnt++;
        }
    }
    cnt=cnt/2;
    char** r=malloc(cnt*sizeof(char*));
    bool start=false;
    cnt=0;
    int s=0,f=0;
    for(i=0; i<len; i++)
    {
        if(in[i]=='"')
        {
            if(!start)
            {
                start=true;
                s=i+1;
                continue;
            }
            else if(start)
            {
                start=false;
                f=i;
                r[cnt]=malloc(sizeof(char[f-s]));
                strncpy(r[cnt],&in[s],f-s);
                r[cnt][f-s]='\0';
                cnt++;
            }

        }
    }
    *c=cnt;
    return r;
}

int serversock;
void intHandler()
{

    shutdown(serversock,2);
    printf("%d\n",close(serversock));

    exit(0);
}

int main(int argc, char *argv[])
{
    int i;
    for(i=0; i<argc; ++i)
    {
        if(strcmp(argv[i],"-r")==0)
        {
            if(argv[i+1][0]=='/')
            {
                root=argv[i+1];
            }
            else if(argv[i+1][0]=='.'&&argv[i+1][1]=='/')
            {
                root=argv[i+1];
            }
            else
            {
                root=malloc(strlen(argv[i+1])+2);
                memset(root,0,strlen(argv[i+1])+2);
                root[0]='.';
                root[1]='/';
                strncpy(root+2,argv[i+1],strlen(argv[i+1]));
            }

            if(root[strlen(root)-1]=='/')
            {
                root[strlen(root)-1]='\0';
            }

            continue;
        }
        if(strcmp(argv[i],"-p")==0)
        {
            port=atoi(argv[i+1]);
            continue;
        }
        if(strcmp(argv[i],"-n")==0)
        {
            size=atoi(argv[i+1]);
            continue;
        }
    }

    //socket的建立

    serversock = 0;

    serversock = socket(AF_INET, SOCK_STREAM, 0);

    if (serversock == -1)
    {
        printf("Fail to create a socket.");
    }

    //socket的連線
    struct sockaddr_in serverInfo;

    bzero(&serverInfo,sizeof(serverInfo));

    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);

    //set reuse port
    int reuseaddr=1;
    int reuseaddr_len=sizeof(serverInfo);
    setsockopt(serversock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, reuseaddr_len);

    if(bind(serversock,(struct sockaddr *)&serverInfo,sizeof(serverInfo))==-1)
    {
        printf("bind fail\n");
        exit(0);
    }
    listen(serversock,32);

    pthread_t t[size]; // 宣告 pthread 變數
    for(i=0; i<size; ++i)
    {
        pthread_create(&t[i], NULL, runnable,i); // 建立子執行緒
    }

    signal(SIGINT, intHandler);

    while(1)
    {

        int forClientSockfd = 0;
        struct sockaddr_in clientInfo;
        int addrlen = sizeof(clientInfo);

        char inputBuffer[4096];
        memset(inputBuffer,0,sizeof(char[4096]));
        forClientSockfd = accept(serversock,(struct sockaddr*) &clientInfo, &addrlen);

        recv(forClientSockfd,inputBuffer,sizeof(char[4096]),0);
        printf("%s\n",inputBuffer);
        int c=0;
        char **r=malloc(sizeof(char**));
        r=getqstr(inputBuffer,&c);

        struct job* j=malloc(sizeof(struct job));

        j->query=r;
        j->cnt=c;
        j->client=forClientSockfd;
        pthread_mutex_lock(&mutex1);
        push(j);
        pthread_mutex_unlock(&mutex1);
    }
    return 0;
}