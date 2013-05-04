// Binary maze stuff. Author: Iustina

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
extern int errno; 


#define STS_OK '0'
#define STS_FAIL '1'
#define STS_EXIT '2'
#define STS_MORE '3'
#define STS_WIN '4'

#define SHELL 0
#define MAZER 1
#define DUNNO 2

#define PERMS 0700
#define MAZE_DEPTH 8
#define MAX_ARG_LEN 13
#define BIT_PATH_LEN (8+8+1)
#define NUM_PATHS 7
#define FIXED_MSG_LEN 20
#define MIN_CHAR 33
#define MAX_CHAR 126

int debug=1;

char entities[3][6]={"sh","mazer",""};
int entity=2;
char* piece_file="piece"; //only the filename..no path
char our_path[26];

int i,j,k;
char msg[30];
char key[20]="none";
char key_paths[NUM_PATHS][BIT_PATH_LEN];
char mazedir[100]="none";
char args[3][MAX_ARG_LEN+1];
char pieces[NUM_PATHS][9]; //received pieces eg 01000101,..
char pass[NUM_PATHS+1]="none"; // ..the 7 pieces
int maze_set=0; //if the mazedir was set

char* help_msg="Available commands:\n login <name>\n key <13 chars key>\n mazedir <root dir for maze>\n setup\n run\n pass <found password>\n quit";
char* steps_msg="Steps to play:\n 1.set key\n 2.setup\n 3.run\n (4.try to understand what happen_ed/s)\n 5.write found password\n 6.glory";


char alfanum_chars[]= "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
char special_chars[]="|/[]><.,!?#$^&*()+_-=";



//==================================================================
//========================= Helpers  ===============================

void dbg(char *msg){
    if(!debug)
        return;
    printf("[%s]: %s\n",entities[entity],msg);fflush(0);
}
void dbg_fmt(char* fmt,char* msg1){
    if(!debug)
        return;
    char msg[1024];
    sprintf(msg,fmt,msg1);
    printf("[%s]: %s\n",entities[entity],msg);fflush(0);
}

int die(char* msg){
    char err[40];
    sprintf(err,"[%s error] %s",entities[entity],msg);
    perror(err);
    exit(errno); 
}

int die_fmt(char* fmt,char* msg){
    char err[40],m[40];
    sprintf(m,fmt,msg);
    sprintf(err,"[%s error] %s",entities[entity],m);
    perror(err);
    exit(errno);
}

//==================================================================
//========================= Validations ============================

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

int get_n_validate_port(char* port_arg){
    int port;
    
    char* endptr;
    port=strtol(port_arg,&endptr,10);
    if(strlen(endptr)!=0){
        die("Bad port argument");
    }
    if(port<1024 ||port > 65535){
        die("Bad port argument");
    }
    return port;
}

int validate_string(char* str,char* good1,char* good2){
    char good_chars[strlen(good1)+strlen(good2)+1];
    strcpy(good_chars, good1);
    strcat(good_chars, good2);
    dbg_fmt("validating %s",good_chars);
    if((i=strspn(str, good_chars))<strlen(str)){
        printf("found invalid char at %d\n",i);
        return 1;
    }
    return 0;
}

int get_n_validate_piece(char* piece){
    //eg "piece n 01010101";
    //n is a int betw 0 and 6
    char n[2]="\0";
    strncpy(n,piece+6,1);
    
    char *endptr;
    int nr=strtol(n,&endptr,10);
    if(strlen(endptr)!=0){
        return 1;
    }
    if(nr<0 || nr>6){
        return 1;
    }
    
    //binary len is 8 and should be convertible to a valid (allowed) ascii char
    char char_bin[9]="\0";
    strncpy(char_bin,piece+8,8);
    //should be 0 and 1s
    if(validate_string(char_bin,"01","")){
        dbg("char_bin not 01..");
        return 1;
    }
    int piece_int=strtol(char_bin,NULL,2);
    
    if(debug)
        printf("piece nr %d piece_int %d",nr,piece_int);
    if(piece_int<MIN_CHAR || piece_int>MAX_CHAR){
        dbg("piece_int not in range");
        return 1;
    }
    if(debug)
        printf("piece char %c ",(char)piece_int); 
    
    pass[nr]=(char)piece_int;
    
    return 0;
}

int validate_pass(char* str){
    for(i=0;i<strlen(str);i++){
        if((int)str[i]<MIN_CHAR || (int)str[i]>MAX_CHAR){
            return 1;
        }
    }
    return 0;
}

//==================================================================
//========================= Mazer functs ===========================

void print_banner(){
    printf("********************* Welcome to Mazer! *********************\n");
    printf("%s\n\n%s\n",help_msg,steps_msg);
    printf("*************************************************************\n");
    fflush(stdout);
}


void sendSts(char sts,int fd)
{
	if(debug) printf("sending sts %c",sts); fflush(stdout);
	if(write(fd,&sts,1)!=1){
		die("send sts");
	}
	dbg("sts sent");
}

void read_variable_msg(int sock){
    char err[30];
    int l,n;
    bzero(msg,sizeof msg);
    if((n=read(sock,&l,4))<=0){
        sprintf(err,"read msg len,read %d",n);
        die(err);
    }

    if( l<0 || l>=sizeof(msg) ){
        sprintf(err,"invalid msg len %d",l);
        die(err);
    }
    
    if(read(sock,msg,l)<=0){ 
        die("read msg");
    }
}
void send_msg(int sd,int l){
    //send length first
    char err[30];

    sprintf(err,"sending msg %s len %d",msg,l);
    dbg(err);

    if(write(sd,&l,4)!=4){
        sprintf(err,"send len for %s",msg);
        die(err);
    }
    sprintf(err,"sent msg len %d",l);
    dbg(err);

    //send message
    if(write(sd,&msg,l)!=l){
        dbg_fmt("err %s",msg);
        sprintf(err,"send msg %s",msg);
        die(err);
    }
    sprintf(err,"sent msg %s",msg);
    dbg(err);
}

void char_to_bitpath(int x,char* b){
    b[0]='\0';
    for (j = 128; j > 0; j >>= 1){
        strcat(b, ((x & j) == j) ? "1/" : "0/");
    }
}

void set_key_paths(){
    for(i=0,k=0;i<7;i++,k+=2){
        char_to_bitpath((int)key[k],key_paths[i]);
    }
    
    for(i=0;i<7;i++){
        printf("%s\n",key_paths[i]);
    }
}

void shuffle_paths(char src[][BIT_PATH_LEN],char dst[][BIT_PATH_LEN],int n){
    //seed rand
    srand(time(NULL));
    
    //copy in original order
    for(i=0;i<n;i++)
        strcpy(dst[i],src[i]);
        
    char tmp[BIT_PATH_LEN];

    for(i=0;i<n;i++){
        j=rand()%n;
        strcpy(tmp,dst[j]);
        strcpy(dst[j],dst[i]);
        strcpy(dst[i],tmp);
    }
}

void make_maze(int level,int max){
	if (level<=max) {
		if (mkdir("0",PERMS)<0) {
			die("mkdir");
		}
		if (mkdir("1",PERMS)<0) {
            die("mkdir");
		}
		if (level!=max) {
			if (chdir("0")<0) {
                die("chdir");
			}
			make_maze(level+1,max);
			if (chdir("..")<0) {
                die("chdir");
			}
			if (chdir("1")<0) {
                die("chdir");
			}
			make_maze(level+1,max);
			if (chdir("..")<0) {
                die("chdir");
			}
		}
	}
}

int cleanup_maze(){
    char cmd[200];
    
    sprintf(cmd,"/bin/rm -rf %s/maze",mazedir);
    dbg(cmd);
    return system(cmd);
}

//return 1 if arg length is more than 13 chars; ret 0 if ok (take only first 2 args)
int split_cmd_args(){
    //clean up 
    bzero(args,sizeof args);
    
    int arg_i=0; //which argument
    int arg_index=0; //position withing argument
    
    //ignore whitespaces from the beginning
    i=0;
    while(msg[i]==' ' || msg[i]=='\t'){
        i++;
    }
    int first_whitespace=1;
    for( ;i<strlen(msg);i++){
        if(msg[i]==' ' || msg[i]=='\t'){
            if(first_whitespace){
                arg_i++;
                arg_index=0;
            }
            first_whitespace=0;
            //just 2 args allowed
            if(arg_i>=2){
                return 0;
            }
        }else{
            first_whitespace=1;
            //copy into current arg
            //check if the argument length is greater than allowed
            if (arg_index>=MAX_ARG_LEN) {
                return 1;
            }
            args[arg_i][arg_index]=msg[i];
            arg_index++;
        }
    }
    return 0;
}

// frequently used :)
void send_STS_FAIL_msg(int sd,char * str){
    sendSts(STS_FAIL,sd);
    bzero(msg,sizeof msg);
    strcpy(msg,str);
    send_msg(sd,strlen(msg));
}

int copy_file(char* src_file,char* dst_file){
    if(debug)
        printf("copying %s to %s\n",src_file,dst_file);

    char ch;
    FILE *src,*dst;
    
    if((src=fopen(src_file,"rb"))==NULL){
        perror("UF");
        return 1;
    }
    if((dst=fopen(dst_file,"wb"))==NULL){
        perror("UF1");
        return 1;
    }
    
    int n, m;
    unsigned char buff[8192];
    do {
        n = fread(buff, 1, sizeof buff, src);
        if(n)
            m = fwrite(buff, 1, n, dst);
        else
            m = 0;
    } while ((n > 0) && (n == m));
    
    if(m){
        //wrote less than read
        return 1;
    }
    
    fclose(src);
    fclose(dst);
    return 0;
}

//in the run phase
void send_msg_to_parent(struct sockaddr_in srv,char * msg){
    char fixed_msg[FIXED_MSG_LEN];
    int sd1;

    if((sd1=socket(AF_INET,SOCK_STREAM,0))<0){
        die("socket"); 
    }
    
    if(connect(sd1,(struct sockaddr*)&srv,sizeof(struct sockaddr))<0){	die("connect");
    }
    
    bzero(fixed_msg,FIXED_MSG_LEN);
    strcpy(fixed_msg,msg);
    if(write(sd1,&fixed_msg,FIXED_MSG_LEN)!=FIXED_MSG_LEN){
        die("kid send msg");
    }

}

void run(int sd){
    
    char shuffled_key_paths[NUM_PATHS][BIT_PATH_LEN];
    shuffle_paths(key_paths,shuffled_key_paths,NUM_PATHS);
    
    if(debug){
        for(i=0;i<NUM_PATHS;i++) printf("%s ",key_paths[i]);
        printf("\n");
        for(i=0;i<NUM_PATHS;i++) printf("%s ",shuffled_key_paths[i]);
        printf("\n");
    }
    
    //copy all pieces in their places
    char dst_path[1000];
    
    for(i=0;i<NUM_PATHS;i++){
        
        bzero(dst_path,sizeof dst_path);
        sprintf(dst_path,"%s/maze/%s%s",mazedir,key_paths[i],piece_file);
        
        char piece_path[33];
        sprintf(piece_path,"%s/%s",our_path,piece_file);
        if(copy_file(piece_path,dst_path)){
            send_STS_FAIL_msg(sd,"copy_file");
            return;
        }
        if(chmod(dst_path,atoi("0700"))){
            send_STS_FAIL_msg(sd,"chmod");
            return;   
        }
    }
    
    //create server
    int st,l; //status,len
	struct sockaddr_in srv;
	struct sockaddr_in cli;
	int sd1,cd1;
    bzero(&srv,sizeof(&srv));
	bzero(&cli,sizeof(&cli));
	
    srv.sin_family=AF_INET;
    srv.sin_addr.s_addr=inet_addr("127.0.0.1");
    srv.sin_port=htons(1444); //todo random from os
    
    
    //fork to wait and run pieces
    char cmd[1000];
    char fixed_msg[FIXED_MSG_LEN];
    char msg1[25];
    char order[10];

    switch (fork()) {
        case -1:
            send_STS_FAIL_msg(sd,"..fork error");
            return;
        case 0:
            //kid
            //sleep a bit then run pieces
            sleep(2);
                        
            char dst_dir[1000];
            for(i=0;i<NUM_PATHS;i++){
                
                bzero(dst_dir,sizeof dst_dir);
                sprintf(dst_dir,"%s/maze/%s",mazedir,shuffled_key_paths[i]);
                bzero(dst_path,sizeof dst_path);
                sprintf(dst_path,"%s/maze/%s%s",mazedir,shuffled_key_paths[i],piece_file);
            
                switch (fork()) {
                    case -1:
                        
                        //let parent know
                        sprintf(msg1,"err fork %d",errno);
                        send_msg_to_parent(srv,msg1);

                        exit(errno); //to parent
                        
                    case 0:
                        //run
                        sprintf(order,"%d",i);
                        if(debug)
                            printf("running %s 1444 %s %s\n",dst_path,dst_dir,order);
                        execl(dst_path,"piece","1444",dst_dir,order,NULL);
                        
                        //if we get here, something went wrong
                        perror("execl err");
                        
                        //let parent know
                        
                        sprintf(msg1,"err exec %d",errno);
                        send_msg_to_parent(srv,msg1);
                        exit(0);
                        
                    default:
                        //wait exit sts
                        //no.. need to go on
                        wait(&st);
                        if(WEXITSTATUS(st)==0){
                            dbg("child exit ok");
                        }else{
                            //command failed
                            if(debug)
                                printf("command failed status %d",st);
                            
                            //let parent know
                            sprintf(msg1,"err exit_sts %d",st);
                            send_msg_to_parent(srv,msg1);
                            
                            exit(st); //to parent
                        }
                }
            
            }
            
            exit(0);
        default:
            //self
            
            if((sd1=socket(AF_INET,SOCK_STREAM,0))<0){
                send_STS_FAIL_msg(sd,"..socket error");
                return;
            }
            int val = 1;
            if (setsockopt(sd1, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
                send_STS_FAIL_msg(sd,"..setsockopt error");
                close(sd1);
                return;
            }
            
            if (bind(sd1,(struct sockaddr*)&srv,sizeof(struct sockaddr))<0){
                send_STS_FAIL_msg(sd,"..bind error");
                close(sd1);
                return;
            }
            if (listen(sd1,100)<0) {
                send_STS_FAIL_msg(sd,"..listen error");
                close(sd1);
                return;
            }
            
            //wait
            l=sizeof(cli);
            dbg("run listening ..");
            int done=0;
            int num_pieces=0;
            
            bzero(pass,sizeof pass);
            
            while(!done){
                if((cd1=accept(sd1,(struct sockaddr*)&cli,&l))<0){
                    send_STS_FAIL_msg(sd,"..accept error");
                    close(sd1);
                    return;
                }
                dbg("accepted");
                
                //recv fixed-len msg
                bzero(fixed_msg,FIXED_MSG_LEN);
                if(read(cd1,&fixed_msg,FIXED_MSG_LEN)<=0){
                    die("run read fixed_msg");
                }
                dbg_fmt("run recv %s",fixed_msg);
                
                //check if it is an error from the kid
                if(strncmp(fixed_msg,"err",3)==0){
                    send_STS_FAIL_msg(sd,fixed_msg);
                    close(sd1);
                    return;
                }
                //if it is from the pieces, save data and increment num_pieces
                else if(strncmp(fixed_msg,"piece",5)==0){
                    dbg_fmt("run recv: %s",fixed_msg);
                    if(get_n_validate_piece(fixed_msg)){
                        send_STS_FAIL_msg(sd,"bad piece.'run' again");
                        close(sd1);
                        return;
                    }
                    num_pieces++;
                }
                else{
                    send_STS_FAIL_msg(sd,"bogus msg.'run' again");
                    close(sd1);
                    return;
                }

                if(num_pieces==NUM_PATHS)
                    done=1;
            
            }
            dbg_fmt("pass %s",pass);
            
            sendSts(STS_OK,sd);
            close(sd1);
    }
}

//==================================================================
//=========================    main     ============================

int main(int argc, char** argv)
{
	int pid;
	int l=0;
	char sts;
	int st;
    
    if(argc<2){
        die_fmt("Usage: %s [port]",argv[0]);
    }
    int port=get_n_validate_port(argv[1]);

	struct sockaddr_in srv;
	struct sockaddr_in cli;
	int sd,cd;
    bzero(&srv,sizeof(&srv));
	bzero(&cli,sizeof(&cli));
	
    srv.sin_family=AF_INET;
    srv.sin_addr.s_addr=inet_addr("127.0.0.1");
    srv.sin_port=htons(port);

	if((pid=fork())<0){
		die("fork");
	}
	if(pid){
        entity=SHELL;
        
        if((sd=socket(AF_INET,SOCK_STREAM,0))<0){
            die("socket");
        }
        int val = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            die("setsockopt");
        }

		if (bind(sd,(struct sockaddr*)&srv,sizeof(struct sockaddr))<0){
			die("bind");
        }
		if (listen(sd,1)<0) {
			die("listen");
        }

        l=sizeof(cli);
        
        if((cd=accept(sd,(struct sockaddr*)&cli,&l))<0){
            die("accept");
        }
        
        //banner
        print_banner();
        
		while(1){
            read_variable_msg(cd);
            
            char prompt[35];
            sprintf(prompt,"%s >> ",&msg);
            
            l=0;
            while(l==0){
                printf(prompt); fflush(stdout);
                bzero(msg,sizeof msg);
        
                dbg("reading cmd");
                if(fgets(msg,sizeof(msg)-1,stdin)==NULL){
                    die("fgets");
                }

                msg[strlen(msg)-1]='\0';  //replace \n with \0
                l=strlen(msg);
            }
            send_msg(cd,l);
            
            bzero(msg,sizeof msg);
            sprintf(msg,"wait sts sizeof %d",sizeof sts);
			dbg(msg);
            
			while(read(cd,&sts,sizeof(sts))!=sizeof(sts)) {}
            
			bzero(msg,sizeof msg);
            sprintf(msg,"sts %c",sts);
			dbg(msg);

			switch(sts){
                    
				case STS_OK:
                    break;
                case STS_MORE:
                    read_variable_msg(cd);
                    printf("%s\n",msg);
                    break;
				case STS_FAIL:
                    read_variable_msg(cd);
                    printf("FAIL: %s\n",msg);
                    break;
				case STS_EXIT: {
                    wait(&st);
                    printf("Session terminated\n");
                    return 0;
                }
                case STS_WIN:
                    wait(&st);
                    printf("Congrats! You win.\n");
                    return 0;
			}
		}
	}
	else
	{
//        char dir[1024];

        entity=MAZER;
        if((sd=socket(AF_INET,SOCK_STREAM,0))<0){
            die("socket"); //todo
        }

		if(connect(sd,(struct sockaddr*)&srv,sizeof(struct sockaddr))<0){	die("connect");
        }
		dbg("connected");

        //save base dir
        if(getcwd(our_path,sizeof our_path)<0){
            die("getcwd");
        }
        dbg_fmt("our_path %s",our_path);
        
		while(1){
            
            bzero(msg,sizeof msg);
            if(getcwd(msg,sizeof msg)<0){
                die("getcwd");
            }
			dbg_fmt("cwd %s",msg);
			
			send_msg(sd,strlen(msg));
            
            //receive command
            read_variable_msg(sd);
			dbg_fmt("recv msg %s",msg);

			//split cmd args
            int ret=split_cmd_args();
            if(ret){
                send_STS_FAIL_msg(sd,"Bad command");
                continue;
            }
            dbg("tokenized");
            
			//send status, then response (if it is the case)
            
			if(strcmp(args[0],"quit")==0){
				sendSts(STS_EXIT,sd);
				return 0;
			}
            
            else if(strcmp(args[0],"key")==0){
                dbg("recv key");
                //if no key was specified
				if (args[1]==NULL || strcmp(args[1],"")==0){
                    send_STS_FAIL_msg(sd,"Usage: key <key_string>");
					continue;
				}
                //verify allowed characters
                if (validate_string(args[1],alfanum_chars,special_chars)) {
                    send_STS_FAIL_msg(sd,"Key contains invalid chars");
					continue;
                }
                if(strlen(args[1])<13){
                    send_STS_FAIL_msg(sd,"Key must be > 13 chars");
					continue;
                }
                
                bzero(key,sizeof key);
                strncpy(key,args[1],13);
                
                //set key_paths
                set_key_paths();
                
                sendSts(STS_MORE,sd);
                bzero(msg,sizeof msg);
                sprintf(msg,"Key set to %s",key);
                send_msg(sd,strlen(msg));
            }

            else if(strcmp(args[0],"mazedir")==0){
                //if no dir was specified
				if (args[1]==NULL || strcmp(args[1],"")==0){
                    send_STS_FAIL_msg(sd,"Usage: mazedir <dir>");
					continue;
				}
                //verify allowed characters
                if (validate_string(args[1],alfanum_chars,"_/")) {
                    send_STS_FAIL_msg(sd,"Dir contains invalid chars");
					continue;
                }
                if(args[1][0]!='/'){
                    send_STS_FAIL_msg(sd,"Need full path to mazedir");
					continue;
                }
                
                bzero(mazedir,sizeof mazedir);
                strncpy(mazedir,args[1],sizeof mazedir);
                mazedir[sizeof mazedir-1]='\0';

                //check if exists, if not, create it
                if(validate_directory(mazedir)){
                    //create it
                    char cmd[200];
                    sprintf(cmd,"/bin/mkdir -p %s",mazedir);
                    dbg(cmd);
                    if(system(cmd)){
                        send_STS_FAIL_msg(sd,"Cannot create mazedir");
                        continue;
                    }
                }
                
                //clean trailing slash
                if(mazedir[strlen(mazedir)-1]=='/'){
                    mazedir[strlen(mazedir)-1]='\0';
                }
                dbg(mazedir);
                
                sendSts(STS_MORE,sd);
                bzero(msg,sizeof msg);
                sprintf(msg,"Mazedir set to %s",mazedir);
                send_msg(sd,strlen(msg));
            }

            else if(strcmp(args[0],"setup")==0){
                char prev_dir[4096];
                getcwd(prev_dir,sizeof prev_dir);
                dbg("prev_dir");
                dbg(prev_dir);
                
                //check if mazedir and key were set
                if(strcmp(mazedir,"none")==0 || strcmp(key,"none")==0){
                    send_STS_FAIL_msg(sd,"Set mazedir and key first");
					continue;
                }
                
                if(chdir(mazedir)<0){
                    send_STS_FAIL_msg(sd,"Cannot access mazedir");
					continue;
                }
                if(mkdir("maze",PERMS)<0){
                    if(errno==EEXIST){
                        //if maze is already here from previous run, delete it
                        if(cleanup_maze()){
                            //error on rm dir
                            send_STS_FAIL_msg(sd,"Cannot remove old maze");
                            continue;
                        }
                        //try to create again
                        if(mkdir("maze",PERMS)<0){
                            send_STS_FAIL_msg(sd,"Cannot 'mkdir maze'");
                            continue;
                        }
                    }else{
                        send_STS_FAIL_msg(sd,"Cannot 'mkdir maze'");
                        continue;
                    }
                }
                if(chdir("maze")<0){
                    send_STS_FAIL_msg(sd,"Cannot chdir maze");
					continue;
                }
                
                make_maze(1,MAZE_DEPTH);
                if(chdir(prev_dir)<0){
                    send_STS_FAIL_msg(sd,"Cannot reset dir");
					continue;
                }
                dbg_fmt("curdir: %s",getcwd(0,0));

                maze_set=1; //mazedir was set
                sendSts(STS_MORE,sd);
                bzero(msg,sizeof msg);
                sprintf(msg,"Maze created.");
                send_msg(sd,strlen(msg));
            }
            
            else if(strcmp(args[0],"run")==0){
                //check if mazedir and key were set and if the setup was done
                if(strcmp(mazedir,"none")==0 || strcmp(key,"none")==0 || maze_set==0){
                    send_STS_FAIL_msg(sd,"Missing mazedir/key/setup.");
					continue;
                }
                
                run(sd);
			}
            
			else if(strcmp(args[0],"login")==0){
                //if no username was specified
				if (args[1]==NULL || strcmp(args[1],"")==0) { 
                    send_STS_FAIL_msg(sd,"Usage: login <user>");
					continue;
				}
                //verify allowed characters
                if (validate_string(args[1],alfanum_chars,"_")) {
                    send_STS_FAIL_msg(sd,"User contains invalid chars");
					continue;
                }

                //look for user in passwd
                char grep_arg[100];
				sprintf(grep_arg,"^%s:",args[1]);
				switch (fork()) {
					case -1: 
                        send_STS_FAIL_msg(sd,"..fork error");
						break;
					case 0: 
						execl("/bin/grep","grep",grep_arg,"/etc/passwd",NULL);
                        //if we get here, something went wrong
                        send_STS_FAIL_msg(sd,"..exec error");
						break;
					default:
						//wait for exec child status 
						wait(&st);	
						if (WEXITSTATUS(st)==0) {
							sendSts(STS_OK,sd);
						}
						else {						
							//command failed / No such user
                            send_STS_FAIL_msg(sd,"Username not found.");
						}
						break;
				}			
			}
            
			else if(strcmp(args[0],"cd")==0){
				if (chdir(args[1])) {
                    send_STS_FAIL_msg(sd,"Command failed");
				}
				else
					sendSts(STS_OK,sd);
			}
            
			else if(strcmp(args[0],"pass")==0){
                //if pass was not set, then we didn't run the game
                if(strcmp(pass,"none")==0){
                    send_STS_FAIL_msg(sd,"'run' first");
					continue;
                }

                //if no pass was specified
				if (args[1]==NULL || strcmp(args[1],"")==0) {
                    send_STS_FAIL_msg(sd,"Usage: pass <pass>");
					continue;
				}
                //validate pass
                if (validate_pass(args[1]) || strlen(args[1])!=(sizeof(pass)-1)) {
                    send_STS_FAIL_msg(sd,"Invalid pass len/chars");
					continue;
                }
                //verify correct pass
                if(strcmp(pass,args[1])){
                    send_STS_FAIL_msg(sd,"Incorrect pass");
					continue;
                }
                
                sendSts(STS_WIN,sd);
                return 0;
			}

			else{
                send_STS_FAIL_msg(sd,"No such command.");
                
			}
		}
	}
	return 0;
}				





