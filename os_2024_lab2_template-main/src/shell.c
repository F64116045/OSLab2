#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p) {
    int in_fd = -1;
    int out_fd = -1;
    
    if (p->in_file != NULL) {
        in_fd = open(p->in_file, O_RDONLY);
        if (in_fd < 0) {
            perror("open input file");
            exit(EXIT_FAILURE);  
        }
    }

    if (p->out_file != NULL) {
        out_fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            if (in_fd != -1) close(in_fd);
            perror("open output file");
            exit(EXIT_FAILURE);  
        }
    }

    if (in_fd != -1) {
        if (dup2(in_fd, STDIN_FILENO) == -1) {
            perror("dup2 input");
            exit(EXIT_FAILURE);
        }
        close(in_fd);
    }

    if (out_fd != -1) {
        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("dup2 output");
            exit(EXIT_FAILURE);
        }
        close(out_fd);
    }
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) { 
        
        redirection(p);
        if (p->in != STDIN_FILENO) {
            if (dup2(p->in, STDIN_FILENO) == -1) {
                perror("dup2 input");
                exit(EXIT_FAILURE);
            }
            close(p->in);
        }
        
        if (p->out != STDOUT_FILENO) {
            if (dup2(p->out, STDOUT_FILENO) == -1) {
                perror("dup2 output");
                exit(EXIT_FAILURE);
            }
            close(p->out);
        }
        
        execvp(p->args[0], p->args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } 
    else if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    return 1;
}

// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd) {
    struct cmd_node *current = cmd->head;
    int num_pipes = cmd->pipe_num;
    int pipes[2 * num_pipes];
    int status = 0;

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipes + i * 2) == -1) {
            perror("pipe");
            return -1;
        }
    }

    int i = 0;
    while (current != NULL) {
        if (i > 0) {
            current->in = pipes[(i - 1) * 2];    // 讀取端
        } else {
            current->in = STDIN_FILENO;
        }

        if (current->next != NULL) {
            current->out = pipes[i * 2 + 1];     // 寫入端
        } else {
            current->out = STDOUT_FILENO;
        }

        status = spawn_proc(current);
        if (status == -1) {
            perror("spawn_proc failed");
            break;
        }

        if (i > 0) {
            close(pipes[(i - 1) * 2]);
        }
        if (current->next != NULL) {
            close(pipes[i * 2 + 1]);
        }

        current = current->next;
        i++;
    }

    for (int j = 0; j < 2 * num_pipes; j++) {
        if (fcntl(pipes[j], F_GETFD) != -1) {  
            close(pipes[j]);
        }
    }


    while (wait(&status) > 0);

    return status;
}

// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
