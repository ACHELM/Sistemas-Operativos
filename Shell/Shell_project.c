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

// La lista es una estructura global
job *lista;

void mysigchld(int s) {
	// Recorra la lista -> waitpid NO BLOQUEANTE y proceder

	// printf("Se recibió SIGCHLD\n"); // NO SE ACONSEJA usar printf en una señal
	// MANEJADOR DE SIGCHLD ->
	// recorrer todos los jobs en bg y suspendidor a ver
	// qué les ha pasado:
	// SI MUERTOS -> quitar de la lista
	// SI CAMBIAN DE ESTADO -> cambiar el job correspondiente

	int status, info, pid_wait;
	enum status status_res; /* status processed by analyze_status() */
	job *jb;

	for (int i = 1; i <= list_size(lista); ++i) {
		jb = get_item_bypos(lista, i);
		pid_wait = waitpid(jb->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);
		
		if (pid_wait == jb->pgid) { // A este jobs le ha pasado algo
			status_res = analyze_status(status, &info);
			// que puede ocurrir?
			// - EXITED
			// - SIGNALED
			// - SUSPENDED
			// - CONTINUED
			printf("[SIGCHLD] Wait realizado para trabajo en background: %s, pid=%i\n", jb->command, pid_wait);
			/* Actuar según los posibles casos reportados por status
				Al menos hay que considerar EXITED, SIGNALED y SUSPENDED
				En este ejemplo sólo se consideran los dos primeros */
			if ( (status_res == SIGNALED) || (status_res == EXITED) ) {
				delete_job(lista, jb);
				--i; // Ojo! El siguiente ha ocupado la posición de este en la lista
			}

			if ( status_res == CONTINUED) { jb->state = BACKGROUND; } // SOLO CAMBIAR DE ESTADO
			if ( status_res == SUSPENDED) { jb->state = STOPPED; } // SOLO CAMBIAR DE ESTADO
		}
	}

	return ;
}

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

	lista = new_list("unalista");

	job *nuevo; // Para almacenar un nuevo job

	terminal_signals(SIG_IGN); //^z ^c -> inmune, superpoderes de terminal

	signal(SIGCHLD, mysigchld); // Instalamos el SIGCHLD

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

		if (! strcmp(args[0], "jobs")) {
			block_SIGCHLD();
			print_job_list(lista);
			unblock_SIGCHLD();
			continue;
		}

		if (! strcmp(args[0], "fg")) {
			// Obtenemos la posición del job
			int pos = 1;
			if (args[1] != NULL) {
				pos = atoi(args[1]);
				if (pos <= 0) {
					fprintf(stderr, "Error en el argumento de fg\n");
					continue;
				}
			}

			// Acedemos a la lista
			block_SIGCHLD();
			job *job = get_item_bypos(lista, pos);
			unblock_SIGCHLD();

			if (job == NULL) {
				fprintf(stderr, "Job no encontrado\n");
				continue;
			}

			// Cedemos la terminal
			set_terminal(job->pgid);

			if (job->state == STOPPED) {
				// Mandamos una señal
				killpg(job->pgid, SIGCONT);
			}

			pid_wait = waitpid(job->pgid, &status, WUNTRACED);

			status_res = analyze_status(status, &info);

			if ((status_res == EXITED) || (status_res == SIGNALED)) {
				printf("El hijo en fg lo mataron o murio con la señal: %d\n", info);

				block_SIGCHLD();
				delete_job(lista, job);
				unblock_SIGCHLD();
			}

			if (status_res == SUSPENDED) {
				printf("El hijo en fg se suspendió\n");
				block_SIGCHLD();
				job->state = STOPPED;
				unblock_SIGCHLD();
			}

			set_terminal(getpid()); // shell recupera el terminal
			continue;
		}

		if (! strcmp(args[0], "bg")) {
			// Obtenemos la posición del job
			int pos = 1;
			if (args[1] != NULL) {
				pos = atoi(args[1]);
				if (pos <= 0) {
					fprintf(stderr, "Error en el argumento de bg\n");
					continue;
				}
			}

			// Acedemos a la lista
			block_SIGCHLD();
			job *job = get_item_bypos(lista, pos);
			unblock_SIGCHLD();

			if (job == NULL) {
				fprintf(stderr,"Job no encontrado\n");
				continue;
			}

			block_SIGCHLD();
			if (job->state == STOPPED) {
				job->state = BACKGROUND;
				// Mandamos una señal de grupo para reaunadar
				killpg(job->pgid, SIGCONT);
			}

			unblock_SIGCHLD();
			continue;
		}

		pid_fork = fork(); // Comando externo

		if (pid_fork > 0) {
			// PADRE -> Shell

			if (background) {
				// Poner job en la lista

				// Nuevo nodo job -> nuevo job BACKGROUD
				nuevo = new_job(pid_fork, args[0], BACKGROUND); // Nuevo nodo job
				block_SIGCHLD(); // enmascarar sigchld (sección libre de sigchld )
				add_job(lista, nuevo);
				unblock_SIGCHLD();
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

				if (status_res == SUSPENDED) {
					printf("El hijo en fg se suspendio\n");

					// Poner job (hijo) suspendido en la lista

					nuevo = new_job(pid_fork, inputBuffer, STOPPED);

					block_SIGCHLD();
					add_job(lista, nuevo);
					unblock_SIGCHLD();

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
