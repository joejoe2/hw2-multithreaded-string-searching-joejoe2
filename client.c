#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

char* host;
int port;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

void sendquery(char* cmd)
{
    //socket的建立
    int sockfd = 0;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        printf("Fail to create a socket.");
    }
    //socket的連線
    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;
    //localhost
    info.sin_addr.s_addr = inet_addr(host);
    info.sin_port = htons(port);

    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1)
    {
        printf("Connection error");
    }


    send(sockfd,cmd,strlen(cmd),0);
    int i;

    char receiveMessage[4096];
    recv(sockfd,receiveMessage,sizeof(receiveMessage),0);

    pthread_mutex_lock(&mutex1);
    printf("%s",receiveMessage);
    pthread_mutex_unlock(&mutex1);


    close(sockfd);
    free(cmd);
    pthread_exit(NULL); // 離開子執行緒
}

char * getline(void)
{
    char * line = malloc(100), * linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if(line == NULL)
        return NULL;

    for(;;)
    {
        c = fgetc(stdin);
        if(c == EOF)
            break;

        if(--len == 0)
        {
            len = lenmax;
            char * linen = realloc(linep, lenmax *= 2);

            if(linen == NULL)
            {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if((*line++ = c) == '\n')
            break;
    }
    *--line = '\0';
    return linep;
}

int main(int argc, char *argv[])
{
    int i;
    for(i=0; i<argc; ++i)
    {
        if(strcmp(argv[i],"-h")==0)
        {
            host=argv[i+1];
            continue;
        }
        if(strcmp(argv[i],"-p")==0)
        {
            port=atoi(argv[i+1]);
            continue;
        }
    }

    while(1)
    {
        char *cmd=getline();
        if(strlen(cmd)>=4096)
        {
            printf("too long(must <= 4095 bytes)\n");
            free(cmd);
            continue;
        }
        else
        {
            int i;
            int cnt=0;
            int len=strlen(cmd);
            for(i=0; i<len; i++)
            {
                if(cmd[i]=='"')
                {
                    cnt++;
                }
                else if(i>=5&&cnt%2==0&&cmd[i]!=' ')
                {
                    cnt=0;
                    break;
                }
            }
            if(cnt==0||cnt%2!=0||cmd[0]!='Q'||cmd[1]!='u'||cmd[2]!='e'||cmd[3]!='r'||cmd[4]!='y')
            {
                printf("The strings format is not correct\n");
                continue;
            }
        }
        pthread_t t; // 宣告 pthread 變數
        pthread_create(&t, NULL, sendquery, cmd); // 建立子執行緒
    }

    return 0;
}