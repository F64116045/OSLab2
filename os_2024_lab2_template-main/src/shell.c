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
    // 處理輸入重定向 (<)
    if (p->in_file != NULL) {
        int in_fd = open(p->in_file, O_RDONLY);
        if (in_fd < 0) {
            perror("open input file");
            exit(EXIT_FAILURE);  // 無法打開文件時，退出程序
        }
        dup2(in_fd, STDIN_FILENO);  // 將標準輸入重定向到 in_file
        close(in_fd);               // 關閉文件描述符
    }

    // 處理輸出重定向 (>)
    if (p->out_file != NULL) {
        int out_fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            perror("open output file");
            exit(EXIT_FAILURE);  // 無法打開文件時，退出程序
        }
        dup2(out_fd, STDOUT_FILENO); // 將標準輸出重定向到 out_file
        close(out_fd);               // 關閉文件描述符
    }

    // 當前不處理管道重定向 (in, out)
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

    // 使用 fork 創建子進程
    pid = fork();
    if (pid == 0) { // 子進程部分
        // 使用 execvp 執行外部命令
		redirection(p);
        if (execvp(p->args[0], p->args) == -1) {
            // 如果 execvp 失敗，輸出錯誤訊息
            perror("exec");
            exit(EXIT_FAILURE); // 子進程退出，表示失敗
        }
    } else if (pid < 0) {
        // 如果 fork 失敗，輸出錯誤訊息
        perror("fork");
        return -1; // fork 失敗，返回錯誤
    } else {
        // 父進程等待子進程結束
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
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
int fork_cmd_node(struct cmd *cmd)
{
	int pipefd[2];
	pipe(pipefd);
	while(cmd->head->next != NULL){
		cmd->head->in = pipefd[0];
		cmd->head->out = pipefd[1];
		spawn_proc(cmd->head);

		cmd->head = cmd->head->next;
	}

	return 1;
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
