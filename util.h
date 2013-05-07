// Binary maze stuff. Author: Iustina

#ifndef _utils_h
#define _utils_h


#define ERR_FILE "/root/mazeerrs"
#define BIT_PATH_LEN (8+8+1)
#define NUM_PATHS 7
#define FIXED_MSG_LEN 20
#define MIN_CHAR 33
#define MAX_CHAR 126


void write_to_file(char* fname,char* mode,char msg[]){
    FILE *fp;
    if((fp=fopen(fname,mode))==NULL){
        //write in current dir
        printf("err");
    }
    //append in mazeerrs
    if(fwrite(msg,1,strlen(msg),fp)!=strlen(msg)){
        //write in current dir
        printf("err1");
    }
    fclose(fp);
}


#endif

