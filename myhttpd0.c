#include <stdio.h>

#include"wrap.h"

#define MAXSIZE 2048

int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}


void encode_str(char* to, int tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {    
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {      
            *to = *from;
            ++to;
            ++tolen;
        } else {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

void decode_str(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from  ) {     
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {       
            *to = hexit(from[1])*16 + hexit(from[2]);
            from += 2;                      
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}


void send_dir(int cfd, const char* dirname)
{
    int i, ret;

    char buf[4094] = {0};

    sprintf(buf, "<html><head><title>awesome!    %s</title></head>", dirname);
    sprintf(buf+strlen(buf), "<body><h1>directory%s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};

    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);

    
    for(i = 0; i < num; ++i) {

        char* name = ptr[i]->d_name;

        sprintf(path, "%s/%s", dirname, name);
        printf("path = %s ===================\n", path);
        struct stat st;
        stat(path, &st);

        encode_str(enstr, sizeof(enstr), name);

        if(S_ISREG(st.st_mode)) {       
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        } else if(S_ISDIR(st.st_mode)) {	
            sprintf(buf+strlen(buf), 
                    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error:");
                continue;
            } else if (errno == EINTR) {
                perror("send error:");
                continue;
            } else {
                perror("send error:");
                exit(1);
            }
        }
        memset(buf, 0, sizeof(buf));
         }

        sprintf(buf+strlen(buf), "</table></body></html>");
        send(cfd, buf, strlen(buf), 0);

        printf("dir message send OK!!!!\n");

}


void send_error(int cfd,int status,char *title,char *text){
    char buf[4096]={0};

    sprintf(buf,"%s %d %s\r\n","HTTP/1.1",status,title);
    sprintf(buf+strlen(buf),"Content-Type:%s\r\n","text/html");
    sprintf(buf+strlen(buf),"Content-Length:%d\r\n",-1);
    sprintf(buf+strlen(buf),"Connection:Close\r\n");
    send(cfd,buf,strlen(buf),0);
    send(cfd,"\r\n",2,0);

    memset(buf,0,sizeof(buf));

    sprintf(buf,"<html><head><title>%d %s</title></head>\n",status,title);
    sprintf(buf+strlen(buf),"<body bgcolor=\"#cc9cc\"><h4 align=\"center\">%d %s</h4>\n",status,title);
    sprintf(buf+strlen(buf),"%s\n",text);
    sprintf(buf+strlen(buf),"<hr>\n</body>\n</html>\n");
    send(cfd,buf,strlen(buf),0);

    return;
}

const char *get_file_type(const char *name)
{
    char* dot;

    dot = strrchr(name,'.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}


int get_line(int cfd,char *buf,int size){
    int i=0;
    char c='\0';
    int n;
    while((i<size-1)&&(c!='\n')){
        n=recv(cfd,&c,1,0);
        if(n>0){
            if(c=='\r'){
                n=recv(cfd,&c,1,MSG_PEEK);
                if((n>0)&&(c=='\n')){
                    recv(cfd,&c,1,0);
                }else{
                    c='\r';
                }
            }
            buf[i]=c;
            i++;
        }else{
            c='\n';
        }   
    }
    buf[i]='\0';

    if(-1==n){
        i=n;
    }
    return i;
}

int init_listen_fd(int port,int epfd){
    int lfd=Socket(AF_INET,SOCK_STREAM,0);
    //������������ַ�ṹ IP+port
    struct sockaddr_in srv_addr;

    bzero(&srv_addr,sizeof(srv_addr));
    srv_addr.sin_family=AF_INET;
    srv_addr.sin_port=htons(port);
    srv_addr.sin_addr.s_addr=htonl(INADDR_ANY);

    //�˿ڸ���
    int opt=1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //��lfd�󶨵�ַ�ṹ
    Bind(lfd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));

    //���ü�������
    Listen(lfd,128);

    //lfd��ӵ�epoll����
    struct epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=lfd;

    Epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);

    return lfd;
}

void do_accept(int lfd,int epfd){
    struct sockaddr_in clt_addr;
    socklen_t clt_addr_len=sizeof(clt_addr);

    int cfd=Accept(lfd,(struct sockaddr*)&clt_addr,&clt_addr_len);

    //��ӡ�ͻ���IP+port
    char client_ip[64]={0};
    printf("New Client IP:%s,Port:%d,cfd=%d\n",
           inet_ntop(AF_INET,&clt_addr.sin_addr.s_addr,client_ip,sizeof(client_ip)),
           ntohs(clt_addr.sin_port),cfd);

    //����cfd������
    int flag=fcntl(cfd,F_GETFL);
    flag|=O_NONBLOCK;
    fcntl(cfd,F_SETFL,flag);

    //���½ڵ�cfd�ҵ�epoll��������
    struct epoll_event ev;
    ev.data.fd=cfd;

    //���ط�����ģʽ
    ev.events=EPOLLIN|EPOLLET;
    Epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
}

//�Ͽ�����
void disconnect(int cfd,int epfd){
    Epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);
    close(cfd);
}

//�ͻ��˵�fd,����ţ������������ط��ļ����ͣ��ļ�����
void send_respond(int cfd,int no,char *disp,char *type,int len){
    char buf[1024]={0};
    sprintf(buf,"HTTP/1.1 %d %s\r\n",no,disp);
    sprintf(buf+strlen(buf),"%s\r\n",type);
    sprintf(buf+strlen(buf),"Content-Length:%d\r\n",len);
    Send(cfd,buf,strlen(buf),0);
    Send(cfd,"\r\n",2,0);
}

//���ͷ����������ļ��������
void send_file(int cfd,const char *file){

    int fd=Open(file,O_RDONLY);
    int n=0;
    char buf[1024];

    while((n=Read(fd,buf,sizeof(buf)))>0){
        printf("sendfile read n=%d----\n",n);
        //SeanSend(cfd,buf,n);
        int ret=send(cfd,buf,n,0);
        if(ret==-1){
            printf("errno=%d\n",errno);
            if(errno==EAGAIN){
                printf("------EAGAIN\n");
                continue;
            }else if(errno==EINTR){
                printf("-----EINTR\n");
                continue;
            }else{
                perror("send error");
                exit(1);
            }
        }
        if(ret<1024){
            printf("------send ret:%d\n",ret);
        }
        //usleep(1000);
        // Write(cfd,buf,n);
    }

    Close(fd);
}

//����http�����ж��ļ��Ƿ���ڣ��ط�
void http_request(int cfd,const char *file){
    struct stat sbuf;

    int ret=stat(file,&sbuf);

    //�ж��ļ��Ƿ���� 
    if(ret!=0){
        //�ط������404����ҳ��
        send_error(cfd,404,"Not Found","No such file or directory");
        perror("stat error\n");
    }
    //const char *type_suffix;
    //type_suffix=get_file_type(file);
    //char type[1024]={0};
    //sprintf(type,"Content-Type:%s",type_suffix);

    if(S_ISREG(sbuf.st_mode)){   //��һ����ͨ�ļ�

        printf("------it is a file\n");

        //��ȡ�ļ�����
        //�ط�httpЭ��Ӧ��
        //send_respond(cfd,200,"OK","Content-Type:text/plain;charset=iso-8859-1",sbuf.st_size);
        send_respond(cfd,200,"OK",get_file_type(file),sbuf.st_size);
        //send_respond(cfd,200,"OK","Content-Type:jpg:image/jpeg;charset=utf-8",-1);
        //send_respond(cfd,200,"OK","Content-Type:audio/mpeg;charset=utf-8",-1);


        //�ط����ͻ���������������
        send_file(cfd,file);
    }else if(S_ISDIR(sbuf.st_mode)){  //Ŀ¼
        //����ͷ��Ϣ
        send_respond(cfd,200,"OK",get_file_type("html"),-1);
        //����Ŀ¼��Ϣ
        send_dir(cfd,file);
    }
}

void do_read(int cfd,int epfd){
    //��ȡһ��httpЭ�飬��֣���ȡget���� �ļ��� Э���
    char line[1024]={0};
    int len=get_line(cfd,line,sizeof(line));    //��httpЭ������ GET /hello.c HTTP/1.1
    if(len==0){
        printf("��������⵽�ͻ��˹ر�....\n");
        disconnect(cfd,epfd);
    }
    else{
        char method[16],path[256],protocol[16];
        sscanf(line,"%[^ ] %[^ ] %[^ ]",method,path,protocol);
        printf("methmod=%s,path=%s,protocol=%s\n",method,path,protocol);
        char buf[1024]={0};
        while(1){
            len=get_line(cfd,buf,sizeof(buf)); 
            if(len==1){
                break;
            }
            //printf("----len=%d  ----%s\n",len,buf);
            //sleep(1);
        }
        if(strncasecmp(method,"GET",3)==0){
            char *file=path+1;  //ȡ���ͻ���Ҫ���ʵ��ļ���

            //���û��ָ�����ʵ���Դ��Ĭ����ʾ��ԴĿ¼�е�����
            if(strcmp(path,"/")==0){
                file="./";
            }

            http_request(cfd,file);
            disconnect(cfd,epfd);
        }
    }
}

void epoll_run(int port){
    int i=0;
    struct epoll_event all_events[MAXSIZE];

    //����һ��epoll��������
    int epfd=Epoll_create(MAXSIZE);

    //����lfd���������������
    int lfd=init_listen_fd(port,epfd);

    while(1){
        //�����ڵ��Ӧ�¼�
        int ret=Epoll_wait(epfd,all_events,MAXSIZE,-1);

        for(i=0;i<ret;i++){
            //ֻ������¼��������¼�Ĭ�ϲ�����
            struct epoll_event *pev=&all_events[i];

            //���Ƕ��¼�
            if(!(pev->events & EPOLLIN)){
                continue;
            }   
            if(pev->data.fd==lfd){  //������������

                do_accept(lfd,epfd);

            }else{  //������

                do_read(pev->data.fd,epfd);
            }
        }
    }
}

int main(int argc,char *argv[]){
    //�����в�����ȡ �˿� ��server�ṩ��Ŀ¼
    if(argc<3){
        printf("./a.out port path\n");
        exit(1);
    }

    //��ȡ�û�����Ķ˿�
    int port=atoi(argv[1]);

    //�ı���̹���Ŀ¼
    int ret=chdir(argv[2]);
    if(ret!=0){
        perror("chdir error");
        exit(1);
    }

    //����epoll����
    epoll_run(port);

    return 0;
}
