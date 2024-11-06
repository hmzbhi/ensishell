/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

struct infos {
	char* name;
	int pid;
	struct infos* next;
};

struct infos* Allprocess = NULL;

void addProcess(char* name, int pid) {
    struct infos* newProcess = (struct infos*)malloc(sizeof(struct infos));
    
    if (newProcess == NULL) { /* Obligé pour enlever le Warning du non-initialisé */
        fprintf(stderr, "Erreur d'allocation de mémoire\n");
        exit(EXIT_FAILURE);
    }
	newProcess->name = strdup(name);
    newProcess->pid = pid;
    newProcess->next = NULL;

    if (Allprocess == NULL) {
        Allprocess = newProcess;
    } else {
        struct infos* current = Allprocess;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newProcess;
    }
}

void removeProcess(int pid) { /* Car le pid est unique */
    struct infos* current = Allprocess;
    struct infos* previous = NULL;

    while (current != NULL && current->pid != pid) {
        previous = current;
        current = current->next;
    }

    if (current == NULL) {
        return;
    }

    if (previous == NULL) {
        Allprocess = current->next;
    } else {
        previous->next = current->next;
    }

    free(current->name);
    free(current);
}

void executepipe(struct cmdline* cmd){
	int res;
	//char *arg1[]={"ls" ,"-R" , 0};
	//char *arg2[]={"egrep" ,"\.c$" , 0};
	int tuyau[2];
	pipe(tuyau);
	res = fork();
	if(res == 0) { // si on est dans le fils
		dup2(tuyau[0], 0); // lecture de stdin dans le tuyau
		close(tuyau[1]); close(tuyau[0]);
		char **commandline = cmd->seq[1];
		execvp(commandline[0],commandline); // egrep. Ne retourne jamais
	}
	dup2(tuyau[1], 1); // ecriture de stdout dans le tuyau
	close(tuyau[0]); close(tuyau[1]); // ls. Ne retourne jamais
}

void execute(struct cmdline* cmd){
	char **commandline = cmd->seq[0];
	if (!strncmp(commandline[0],"jobs", 4)) {
			struct infos* current = Allprocess;

			while (current != NULL){
				if (!waitpid(current->pid,NULL,WNOHANG)){
					printf("[%i]+  Running %s \n",current->pid,current->name);
					current = current->next;
				} else {
					printf("[%i]+  Done %s \n",current->pid,current->name);
					removeProcess(current->pid);
					current = current->next;
				}
			}
		} else {
			int pid = fork();
			if (pid == 0) {
				if (cmd->in != NULL){
					int fd = open(cmd->in,O_RDONLY);
					if (fd == -1) { perror("open: " ); exit(EXIT_FAILURE);}
					dup2(fd, STDIN_FILENO);
					close(fd);
				}
				/* Redirection  > */
				if (cmd->out != NULL){
					int fd = open(cmd->out,O_WRONLY | O_CREAT,S_IRWXU);
					if (fd == -1) { perror("open: " ); exit(EXIT_FAILURE);}
					dup2(fd, 1);
					close(fd);
				}
				
				/* Try multiple pipes
				int i=0;
				while (cmd->seq[i] != NULL){
					i++;
				}
				if (i>=2){
					for(int j=i-1; j>0; j--){
						struct cmdline* temp = cmd; 
						temp->seq[1] = cmd->seq[j];
						executepipe(temp);
					}
				} */
				if (cmd->seq[1] != NULL){
					executepipe(cmd);
				}
				
				execvp(commandline[0], commandline);
			} else {
				if (cmd->bg == 0){
					waitpid(pid,NULL,0);
				} else {
					addProcess(*(cmd->seq)[0],pid);
				}
			}
		}
}

int question6_executer(char *line)
{
	struct cmdline* cmd = parsecmd(&line);
	execute(cmd);
	
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif

int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}
		execute(l);
	}

}
