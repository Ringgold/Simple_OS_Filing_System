#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int cmdcounter=0;
struct pidjobs
{
    int pid;
    char *name;
}pidjobs[100];				// jobs array

int getcmd(char *prompt, char *args[], int *background)
{
    int length, i = 0;
    char *token, *loc;
    char *line;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0) {
        exit(-1);
    }
    
    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL){
        *background = 1;
        *loc = ' ';
    } else
        *background = 0;

    while ((token = strsep(&line, " \t\n")) != NULL){
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (strlen(token) > 0)
            args[i++] = token;
    }
    args[i++]=NULL;
    return i;
}

int redirection(char *args[]){				// function to check if input contains redirection symbol
	for(int i=0; args[i]!=NULL;i++){
		if(strcmp(">",args[i])==0){
			return i;}
	}
	return 0;
}

void commandexec(char *args[],int bg){			// function to execute all the command
    int cd=strcmp("cd",args[0]);
    int pwd=strcmp("pwd",args[0]);
    int fg=strcmp("fg",args[0]);
    int jobs=strcmp("jobs",args[0]);
    int exits=strcmp("exit",args[0]);
    int err=0;					// error flag for history
    if(cd==0){
    	chdir(args[1]);
    }
    else if(pwd==0){
        char *buf=malloc(sizeof(char)*1000);
        getcwd(buf,100);
        printf("%s\n",buf);
        free(buf);
    }
    else if(fg==0){
    	if(args[1]==NULL){
    		printf("error: an index needed\n");
    		err=1;
    	}
    	else{
    		int check=atoi(args[1]);
    		if(check==0){
    			printf("error: index should be integer\n");
    			err=1;
    		}
    		else{
    			int fground=waitpid(pidjobs[check-1].pid,NULL,WCONTINUED);
    			if(fground==-1){
    				printf("fg fail\n");
    			}else
    				printf("fg success\n");
    		}
    	}
    }
    else if(jobs==0){				//list jobs in background
     	for(int i=0;i<100;i++){
     		if(pidjobs[i].pid!=0){
     			if(waitpid(pidjobs[i].pid,NULL,WNOHANG)==-1){
				printf("[%d]: Done	command:%s\n",i+1,pidjobs[i].name);
     				pidjobs[i].pid=0;
     			}else{
				printf("[%d]: Running	command:%s\n",i+1,pidjobs[i].name);
			}
     		}
     	}
    }
    else if(exits==0){
        exit(0);
    }
    else{
    	int redir=redirection(args);
        int id=fork();
        if(id==0){             //child process
        	if(redir!=0){      // redirection when detects ">"
        		args[redir]=NULL;
            	FILE *fp=freopen(args[redir+1],"w+",stdout);
            	if(execvp(args[0],args)==-1){
            		printf("bash: %s: command not found\n", args[0]);
                    err=1;
            		// exit(0);
            	}
           		fclose(fp);
        	}
            else if(execvp(args[0],args)==-1){
                printf("bash: %s: command not found\n", args[0]);
                err=1;			// if command is illegal, set error flag to be 1
            	// exit(0);
            }
        }else {                 //parent process
            if(bg){             //add process into jobs array when bg enable
                for(int i=0;i<100;i++){
                    if(pidjobs[i].pid==0){
                        pidjobs[i].pid=id;
                        pidjobs[i].name=strdup(args[0]);
                        break;
                    }
                }
            }else{				// background not enable
				pid_t child=waitpid(id,NULL,WCONTINUED);
            }
        }
    }
    cmdcounter++;
}

int main()
{
    char *args[20];
    int bg;
    
	while(1){
        
		int cnt = getcmd("\nsh>  ", args, &bg);
		
		/*for (int i = 0; i < cnt; i++) //Current Status
		    printf("\nArg[%d] = %s", i, args[i]);
		if (bg)
		    printf("\nBackground enabled..\n");
		else
		    printf("\nBackground not enabled \n");*/ 
		
		if(args[0]==NULL){
	    		// do nothing if enter nothing
	    	}else{
			commandexec(args,bg);	
	    	}
    	}
}
