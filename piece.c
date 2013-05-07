// Binary maze stuff. Author: Iustina

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "utils.h"

char my_path[BIT_PATH_LEN];
char my_rand_char[9];
char piece_filepath[2000];
char my_position[3];
char srv_port[7];
int i,j,k;
char msg[1024];


void log_error(char msg[]){
    char err[1000];
    sprintf(err,"[%s error] %s\n",my_path,msg);
    err[sizeof(err)-1]='\0';
    
    write_to_file(ERR_FILE,"a",err);
}

void char_to_bitstring(int x,char* b){
    b[0]='\0';
    for (j = 128; j > 0; j >>= 1){
        strcat(b, ((x & j) == j) ? "1" : "0");
    }
}

void generate_magic_char(){
    srand(time(NULL));
    
    //use my_path and my_position
    int pos=(rand()/(atoi(my_position)+1))%MAX_CHAR;
    while(pos<MIN_CHAR){
        pos+=MAX_CHAR-MIN_CHAR;
    }
    char_to_bitstring(pos,my_rand_char);

//    strcpy(my_rand_char,"00110001");
    my_rand_char[8]='\0';
}

void send_back(char *msg){
    char fixed_msg[FIXED_MSG_LEN];
	int sd;

	struct sockaddr_in srv;
    bzero(&srv,sizeof(&srv));
	
    srv.sin_family=AF_INET;
    srv.sin_addr.s_addr=inet_addr("127.0.0.1");
    srv.sin_port=htons(atoi(srv_port));
    

    if((sd=socket(AF_INET,SOCK_STREAM,0))<0){
        log_error("socket");
        exit(1);
    }
    if(connect(sd,(struct sockaddr*)&srv,sizeof(struct sockaddr))<0){
        log_error("connect");
        exit(1);
    }
    
    bzero(fixed_msg,FIXED_MSG_LEN);
    strcpy(fixed_msg,msg);
    if(write(sd,&fixed_msg,FIXED_MSG_LEN)!=FIXED_MSG_LEN){
        log_error("kid send msg");
        exit(1);
    }

}

int validate_directory(char *path){
    struct stat s;
    if(stat(path, &s)<0){
        return 1;
    }
    if(!S_ISDIR(s.st_mode)){
        return 1;
    }
    return 0;
}

int validate_args(char* argv[]){
    int ret=0;
    //1 is a port
    char* endptr;
    strtol(argv[1],&endptr,10);
    if(strlen(endptr)!=0){
        log_error("invalid arg 1 srv port");
        ret=1;
    }
    
    //2 is a path
    if(validate_directory(argv[2])){
        log_error("invalid arg 2 mypath");
        ret=1;
    }
    
    //3 is a no between 0 and 6
    int n=strtol(argv[3],&endptr,10);
    if(strlen(endptr)!=0 || n<0 || n>6){
        log_error("invalid arg 3 myposition");
        ret=1;
    }
    return ret;
}

int main(int argc,char* argv[]){
    char err[200];
    sprintf(err,"%s %s %s",argv[1],argv[2],argv[3]);
    err[sizeof(err)-1]='\0';
    log_error(err);
    if(validate_args(argv))
        return 0; //could send to srv the err..we need at least the port
    
    strcpy(srv_port,argv[1]);
    srv_port[sizeof(srv_port)-1]='\0';
    
    strcpy(my_path,argv[2]);
    my_path[BIT_PATH_LEN-1]='\0';
    //todo check if valid dir
    
    strcpy(my_position,argv[3]); 
    my_position[2]='\0'; //no need I guess
    
    char *fmt=my_path[strlen(my_path)-1]=='/' ? "%spickme" : "%s/pickme" ;
    sprintf(piece_filepath,fmt,my_path);
    
    write_to_file(piece_filepath,"w",my_position);
    
    generate_magic_char();
    
    bzero(msg,sizeof msg);
//    sprintf(msg,"piece %s %s","7",my_rand_char); //fuzz
//    sprintf(msg,"piece %s %s",my_position,"11111111"); //fuzz
//    sprintf(msg,"piece %s %s",my_position,"001100010001"); //should be fine (only first 8 are taken)
//    sprintf(msg," %s %s",my_position,my_rand_char); //fuzz
    
    sprintf(msg,"piece %s %s",my_position,my_rand_char);
    send_back(msg);

    return 0;
}


