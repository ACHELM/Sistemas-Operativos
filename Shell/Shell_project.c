/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */
#include <string.h>

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */

	terminal_signals(SIG_IGN); //^z ^c -> inmune, superpoderes de terminal

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		if(args[0]==NULL) continue;   // if empty command

		if (! strcmp(args[0], "hola")) {
			printf("hola mundo\n");
			continue;
		}

		if (! strcmp(args[0], "cd")) {
			int err;
			if (args[1] == NULL){
				err = chdir (getenv("HOME"));
			} else {
				err = chdir (args[1]);
			}
			if (err) {fprintf(stderr, "Error en chdir\n");}
			continue;
		}


		pid_fork = fork(); // Comando externo

		if (pid_fork > 0) {
			// PADRE -> Shell

			if (background) {
				printf("Comando %s ejecutado en segundo plano con pid %d.\n", args[0], pid_fork);
			} else {

				set_terminal(pid_fork); // ceder el terminal al hijo
				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				status_res = analyze_status(status, &info);

				if (status_res == EXITED) {
					printf("El hijo en fg acabó con normalidad y devolvió %d\n", info);
				}

				if (status_res == SIGNALED) {
					printf("El hijo en fg lo mataron con la señal: %d\n", info);
				}

				set_terminal(getpid()); // shell recupera el terminal
			}

		} else if (pid_fork == 0) {
			// HIJO
			new_process_group(getpid()); // hijo -> me voy
			if (background) {
			} else {
				set_terminal(getpid()); // ceder el terminal al hijop
			}
			terminal_signals(SIG_DFL);
			execvp(args[0], args);
			// Si llegamos aquí es porque exec falló
			perror("Exec falló");
			exit(EXIT_FAILURE);
		} else {

			fprintf(stderr, "Error en fork\n");
			continue;
		}

		/* the steps are:
			 (1) fork a child process using fork()
			 (2) the child process will invoke execvp()
			 (3) if background == 0, the parent will wait, otherwise continue 
			 (4) Shell shows a status message for processed command 
			 (5) loop returns to get_commnad() function
		*/

	} // end while
}
