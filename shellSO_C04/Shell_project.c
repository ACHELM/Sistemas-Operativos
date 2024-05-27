/**
UNIX Shell Project
 
Autor: [Angel Daniel Perez Madero, Amin Chaloukh El Mohammadi]

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA



Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc shell_project.c job_control.c -o Shell
   $ ./Shell
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

static void parse_redirections(char **args, char **file_in, char **file_out){
    *file_in = NULL;
    *file_out = NULL;
    char **args_start = args;
    while (*args) {
        int is_in = !strcmp(*args, "<");
        int is_out = !strcmp(*args, ">");
        if (is_in || is_out) {
            args++;
            if (*args) {
                if (is_in)  *file_in = *args;
                if (is_out) *file_out = *args;
                char **aux = args + 1;
                while (*aux) {
                    *(aux - 2) = *aux;
                    aux++;
                }
                *(aux - 2) = NULL;
                args--;
            } else {
                fprintf(stderr, "syntax error in redirection\n");
                args_start[0] = NULL; // Do nothing
                return;
            }
        } else {
            args++;
        }
    }
}

job *job_list; // La lista de trabajos es una estructura global

void mysigchld(int s){
    int status, info, pid_wait;
    enum status status_res;
    job* jb;

    for (int i = 1; i <= list_size(job_list); i++) {
        jb = get_item_bypos(job_list, i);
        pid_wait = waitpid(jb->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid_wait == jb->pgid) { // A este jobs le ha pasado algo
            status_res = analyze_status(status, &info);
            printf("[SIGCHLD] Wait realizado para trabajo en background: %s, pid=%i\n", jb->command, pid_wait);
            if (status_res == SIGNALED || status_res == EXITED) {
                delete_job(job_list, jb);
                i--; // Ojo! El siguiente ha ocupado la posición de este en la lista
            }
            if (status_res == CONTINUED) { jb->state = BACKGROUND; }
            if (status_res == SUSPENDED) { jb->state = STOPPED; }
        }
    }
}

int main(void)
{
    char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
    int background;             /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2];   /* command line (of 256) has max of 128 arguments */
    int pid_fork, pid_wait;     /* pid for created and waited process */
    int status;                 /* status returned by wait */
    enum status status_res;     /* status processed by analyze_status() */
    int info;                   /* info processed by analyze_status() */

    job_list = new_list("job_list");

    job *new_job_item; // Para almacenar un nuevo trabajo

    ignore_terminal_signals(); // Ignorar señales de terminal
    signal(SIGCHLD, mysigchld); // Instalar el manejador de SIGCHLD

    while (1) { /* Program terminates normally inside get_command() after ^D is typed */
        printf("COMMAND->");
        fflush(stdout);
        get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */

        char *file_in = NULL;
        char *file_out = NULL;
        parse_redirections(args, &file_in, &file_out);

        if (file_in) { printf("Hay redirección <: '%s'\n", file_in); }
        if (file_out) { printf("Hay redirección >: '%s'\n", file_out); }

        if (args[0] == NULL) continue;   // if empty command

        // Comandos internos
        if (!strcmp(args[0], "hola")) {
            printf("hola mundo\n");
            continue;
        }
        if (!strcmp(args[0], "jobs")) {
            block_SIGCHLD();
            print_job_list(job_list);
            unblock_SIGCHLD();
            continue;
        }
        if (!strcmp(args[0], "cd")) {
            if (args[1] == NULL || chdir(args[1]) == -1) {
                perror("cd error");
            }
            continue;
        }
        if (!strcmp(args[0], "fg")) {
            block_SIGCHLD();
            job *jb = get_item_bypos(job_list, 1); // Toma el primer trabajo
            if (jb != NULL) {
                set_terminal(jb->pgid);
                killpg(jb->pgid, SIGCONT);
                waitpid(jb->pgid, &status, WUNTRACED);
                status_res = analyze_status(status, &info);
                if (status_res == SUSPENDED) {
                    jb->state = STOPPED;
                } else {
                    delete_job(job_list, jb);
                }
                set_terminal(getpid());
            } else {
                printf("No hay trabajos para pasar a primer plano\n");
            }
            unblock_SIGCHLD();
            continue;
        }
        if (!strcmp(args[0], "bg")) {
            block_SIGCHLD();
            job *jb = get_item_bypos(job_list, 1); // Toma el primer trabajo
            if (jb != NULL) {
                jb->state = BACKGROUND;
                killpg(jb->pgid, SIGCONT);
            } else {
                printf("No hay trabajos para pasar a segundo plano\n");
            }
            unblock_SIGCHLD();
            continue;
        }
        if (!strcmp(args[0], "exit")) {
            exit(0);
        }

        // Comando externo
        pid_fork = fork();
        if (pid_fork > 0) {
            // PADRE -> Shell
            new_process_group(pid_fork); // hijo -> fuera

            if (background) {
                new_job_item = new_job(pid_fork, args[0], BACKGROUND); // Nuevo trabajo en background
                block_SIGCHLD();
                add_job(job_list, new_job_item);
                unblock_SIGCHLD();
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
                    printf("El hijo en fg se suspendió\n");
                    new_job_item = new_job(pid_fork, inputBuffer, STOPPED); // Trabajo suspendido
                    block_SIGCHLD();
                    add_job(job_list, new_job_item);
                    unblock_SIGCHLD();
                }
                set_terminal(getpid()); // shell recupera el terminal
            }
        } else if (pid_fork == 0) {
            // HIJO
            new_process_group(getpid()); // hijo -> me voy

            if (background) {
                // No hacer nada
            } else {
                set_terminal(getpid()); // ceder el terminal al hijo
            }

            restore_terminal_signals(); // Restablecer señales a su comportamiento por defecto

            if (file_in) {
                int fd_in = open(file_in, O_RDONLY);
                if (fd_in != -1) {
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                } else {
                    perror("Error abriendo archivo de entrada");
                    exit(EXIT_FAILURE);
                }
            }

            if (file_out) {
                int fd_out = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (fd_out != -1) {
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                } else {
                    perror("Error abriendo archivo de salida");
                    exit(EXIT_FAILURE);
                }
            }

            execvp(args[0], args);
            perror("Exec falló");
            exit(EXIT_FAILURE);
        } else {
            perror("Error en fork");
            continue;
        }
    }
}
