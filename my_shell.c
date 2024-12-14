#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"


/* 

    Read a line of characters from stdin.
    buf: stores user input
    nbuf: stores the number of characters buf can store and
    ensures it does not exceed tht character

*/
int getcmd(char *buf, int nbuf) {
    memset(buf, 0, nbuf); // Clear command buffer at the start
    // Display the shell prompt
    printf(">>> ");
    // Read from stdin
    int n = read(0, buf, nbuf); // fd=0 reserved for stdin

    // Check for errors during read
    if (n<0){
        printf("Error: failed to read input.\n");
        return -1;
    }

    // Handle end of input (Ctrl+D)
    if (n==0) {
        return -1;
    }

  // Check for buffer overflow and truncate if necessary
    if (n >= nbuf) {
        printf("Warning: Command too long and was truncated.\n");
        buf[nbuf - 1] = '\0';  // Null-terminate to prevent overflow
    } else {
        buf[n - 1] = '\0';  // Standard null-termination
    }

    // Check for empty or whitespace-only command
    int is_empty = 1;
    for (int i = 0; i < n - 1; i++) { 
        if (buf[i] != ' ' && buf[i] != '\t') { // Detect non-whitespace characters
            is_empty = 0;
            break;
        }
    }

    if (is_empty) {
        return 0; 
    }
  
    return n;

}


__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {
    char *commands[10]; // Array to store each command segment
    int num_cmds = 0; // store the no of commands in the pipeline
    int pipes[9][2]; // array of pipes supporting upto 10 commands
    
    char *arguments[10];
    int numargs = 0;
    int redirection_left = 0, redirection_right = 0;
    char *file_name_l = 0, *file_name_r = 0;
    int p[2];
    int pipe_cmd = 0;

    memset(commands, 0, sizeof(commands)); 
    memset(arguments, 0, sizeof(arguments)); 

    // Manual Parsing of buf to split commands by |
    int i = 0;
    int start = 0;
    int len = strlen(buf);
    while (i <= len && num_cmds < 10) {
        if (buf[i] == '|' || buf[i] == '\0') {
            buf[i] = '\0';  
            commands[num_cmds++] = &buf[start];  
            start = i + 1;  
        }
        i++;
    }

    for (int k = 0; k < num_cmds; k++) {

        // Reset redirection flags for each command
        redirection_left = 0;
        redirection_right = 0;
        file_name_l = 0;
        file_name_r = 0;

        // Check for redirections in the current command
        int j = 0;
        while (commands[k][j] != '\0') {
            if (commands[k][j] == '>') {
                redirection_right = 1;
                commands[k][j] = '\0';
                j++;
                while (commands[k][j] == ' ' || commands[k][j] == '\t') j++;
                file_name_r = &commands[k][j];
                while (commands[k][j] != ' ' && commands[k][j] != '\t' && commands[k][j] != '\0') j++;
                commands[k][j] = '\0';
            } else if (commands[k][j] == '<') {
                redirection_left = 1;
                commands[k][j] = '\0';
                j++;
                while (commands[k][j] == ' ' || commands[k][j] == '\t') j++;
                file_name_l = &commands[k][j];
                while (commands[k][j] != ' ' && commands[k][j] != '\t' && commands[k][j] != '\0') j++;
                commands[k][j] = '\0';
            }
            j++;
        }

        // Handle input redirection for the current command
        if (redirection_left) {
            int fd_in = open(file_name_l, O_RDONLY);
            if (fd_in < 0) {
                printf("Error: cannot open input file %s\n", file_name_l);
                exit(1);
            }
            close(0); // Close stdin
            dup(fd_in);
            close(fd_in);
        }

        // Handle output redirection for the current command
        if (redirection_right) {
            int fd_out = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
            if (fd_out < 0) {
                printf("Error: cannot open output file %s\n", file_name_r);
                exit(1);
            }
            close(1); // Close stdout
            dup(fd_out);
            close(fd_out);
        }
    }


    // If more than one command was found, we have a pipeline
    if (num_cmds > 1) {
        // Set up pipes for the multi-element pipeline
        for (int k = 0; k < num_cmds - 1; k++) {
            if (pipe(pipes[k]) < 0) {
                printf("Error: failed to create pipe.\n");
                exit(1);
            }
        }

        // Fork each command in the pipeline
        for (int k = 0; k < num_cmds; k++) {
            if (fork() == 0) {
                // In each child process
                // If not the first command, redirect stdin to the read end of the previous pipe
                if (k > 0) {
                    close(0);                   
                    dup(pipes[k - 1][0]);       
                }

                // If this is the last command and redirection is needed
                if (k == num_cmds - 1 && redirection_right) {
                    int fd_out = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
                    if (fd_out < 0) {
                        printf("Error: cannot open output file %s\n", file_name_r);
                        exit(1);
                    }
                    close(1);               // Close stdout
                    dup(fd_out);            // Duplicate output file descriptor to stdout
                    close(fd_out);          // Close the original output file descriptor
                } else if (k < num_cmds - 1) {
                    // If not the last command, redirect stdout to the write end of the current pipe
                    close(1);               // Close stdout
                    dup(pipes[k][1]);       // Duplicate current pipe's write end to stdout
                }

                // Close all pipe ends in this child that aren't used
                for (int l = 0; l < num_cmds - 1; l++) {
                    close(pipes[l][0]);
                    close(pipes[l][1]);
                }

                // Parse arguments for the current command segment
                numargs = 0;
                int m = 0;
                int cmd_len = strlen(commands[k]);
                int arg_start = 0;

                // Tokenize the command to get arguments manually
                while (m <= cmd_len) {
                    if (commands[k][m] == ' ' || commands[k][m] == '\t' || commands[k][m] == '\0') {
                        if (m > arg_start) {
                            commands[k][m] = '\0';  
                            arguments[numargs++] = &commands[k][arg_start];
                        }
                        arg_start = m + 1;  
                    }
                    m++;
                }
                arguments[numargs] = 0; 

                // Execute the current command
                if (exec(arguments[0], arguments) < 0) {
                    printf("Error: command not found: %s\n", arguments[0]);
                    exit(1);
                }
            }
        }

        // Parent process: Close all pipe ends and wait for each child process
        for (int k = 0; k < num_cmds - 1; k++) {
            close(pipes[k][0]);
            close(pipes[k][1]);
        }
        for (int k = 0; k < num_cmds; k++) {
            wait(0);
        }
        exit(0);
    }


    // Inline tokenization and redirection parsing
    i = 0;
    start = 0;
    len = strlen(buf);
    numargs = 0;

    while (i < len) {
        // Skip over any spaces
        while (i < len && (buf[i] == ' ' || buf[i] == '\t')) {
            i++;
        }

        // If we're at the end of the buffer, break
        if (i == len) break;

        // Check for redirection operators directly attached to the command
        if (buf[i] == '>') {
            redirection_right = 1;
            i++; 
            while (i < len && (buf[i] == ' ' || buf[i] == '\t')) i++; 
            file_name_r = &buf[i];
            while (i < len && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '<' && buf[i] != '>') i++; // Move to end of filename or next operator
            buf[i] = '\0'; // Null-terminate filename
            continue;
        } else if (buf[i] == '<') {
            redirection_left = 1;
            i++; 
            while (i < len && (buf[i] == ' ' || buf[i] == '\t')) i++; 
            file_name_l = &buf[i];
            while (i < len && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '<' && buf[i] != '>') i++; // Move to end of filename or next operator
            buf[i] = '\0'; // Null-terminate filename
            continue;
        }

        // Mark the start of a word
        start = i;

        // Find the end of the word
        while (i < len && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '<' && buf[i] != '>') {
            i++;
        }

        // Null-terminate the word
        buf[i] = '\0';

        // Check if it's a pipe operator
        if (strcmp(&buf[start], "|") == 0) {
            pipe_cmd = 1;
            buf[start] = '\0'; 
            break;
        } else {
            // Store the argument
            arguments[numargs++] = &buf[start];
        }

        // Move past the null terminator
        i++;
    }

    arguments[numargs] = 0;  // Null-terminate the array of arguments

  // If this is a pipe command, set up and execute both sides
  if (pipe_cmd) {
    if (pipe(p) < 0) {
      printf("Error: failed to create pipe.\n");
      exit(1);
    }

    // Fork the first child for the left-hand command (before the pipe)
    if (fork() == 0) {
      close(p[0]);            // Close read end of the pipe in the first child
      close(1);               // Close stdout
      dup(p[1]);              // Redirect stdout to the write end of the pipe
      close(p[1]);            // Close the original write end

      // Execute the left-hand command
      if (exec(arguments[0], arguments) < 0) {
        printf("Error: command not found: %s\n", arguments[0]);
        exit(1);
      }
    }

    // Fork the second child for the right-hand command (after the pipe)
    if (fork() == 0) {
      close(p[1]);            // Close write end of the pipe in the second child
      close(0);               // Close stdin
      dup(p[0]);              // Redirect stdin to the read end of the pipe
      close(p[0]);            // Close the original read end

      // Parse arguments for the right-hand command (after the pipe)
      numargs = 0;
      int j = i + 1;
      while (j < len) {
        // Skip leading spaces
        while (j < len && (buf[j] == ' ' || buf[j] == '\t' )) j++;
        if (j == len) break;

        // Set argument start
        arguments[numargs++] = &buf[j];

        // Move to the end of the argumente
        while (j < len && buf[j] != ' ' && buf[j] != '\t') j++;
        if (j < len) buf[j++] = '\0';  // Null-terminate the argument
      }
      arguments[numargs] = 0; // Null-terminate the arguments array

      // Execute the right-hand command
      if (exec(arguments[0], arguments) < 0) {
        printf("Error: command not found: %s\n", arguments[0]);
        exit(1);
      }
    }

    // Parent process: close both ends of the pipe and wait for children
    close(p[0]);
    close(p[1]);
    wait(0);  // Wait for the first child
    wait(0);  // Wait for the second child
    exit(0);  // Exit after completing the pipe command
  }

  // Handle input redirection
  if (redirection_left) {
    int fd_in = open(file_name_l, O_RDONLY);
    if (fd_in < 0) {
        printf("Error: cannot open input file %s\n", file_name_l);
        exit(1);
    }
    close(0); // close stdin 
    dup(fd_in); // duplicte fd_in to the lowest available fd
    close(fd_in); // close the original fd_in
  }

    // Handle output redirection
    if (redirection_right) {
        int fd_out = open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC);
        if (fd_out < 0) {
            printf("Error: cannot open output file %s\n", file_name_r);
            exit(1);
        }
        close(1);              // Close stdout
        dup(fd_out);           // Redirect stdout to the file
        close(fd_out);         // Close the original file descriptor
    }

    // Execute the command
    if (exec(arguments[0], arguments) < 0) {
        printf("Error: command not found: %s\n", arguments[0]);
        exit(1);
    }

    // Check if it's a cd command
    if (strcmp(arguments[0], "cd") == 0) {
        exit(2); // Exit with status 2 to tell the parent that this is a cd command
    } else {
        // Execute the command
        if (exec(arguments[0], arguments) < 0) {
            printf("Error: command not found: %s\n", arguments[0]);
        }
    }

    exit(0); // Exit child process after executing the command
    }

int main(void) {
    static char buf[100];
    int pcp[2];
    pipe(pcp);

    /* Read and run input commands. */
    while (getcmd(buf, sizeof(buf)) >= 0) {
        // Pointer to the start of the current command segment
        char *cmd = buf;
        char *next_cmd;

        // Loop over each command separated by ;
        while ((next_cmd = strchr(cmd, ';')) != 0) {
            *next_cmd = '\0';  // Null-terminate each command at ;
            
            // Remove leading spaces for each command segment
            while (*cmd == ' ' || *cmd == '\t') cmd++;

             // Remove trailing spaces for each command segment
            char *end = cmd + strlen(cmd) - 1;
            while (end > cmd && (*end == ' ' || *end == '\t')) {
                *end = '\0';
                end--;
            }

            // Check if there's an "exit" command
            if (strcmp(cmd, "exit") == 0) {
                printf("Exiting shell...\n");
                exit(0);
            }

            // Handle cd command
            if (cmd[0] == 'c' && cmd[1] == 'd' && (cmd[2] == ' ' || cmd[2] == '\t' || cmd[2] == '\0')) {
                // Move pointer after cd
                char *directory = cmd + 2;

                // Skip leading spaces after cd
                while (*directory == ' ' || *directory == '\t') directory++;

                // Check if no directory was specified
                if (*directory == '\0') {
                    printf("Error: no directory specified for 'cd' command.\n");
                    continue;
                }

                // Remove trailing spaces from directory path
                char *end = directory + strlen(directory) - 1;
                while (end > directory && (*end == ' ' || *end == '\t')) {
                    *end = '\0';
                    end--;
                }

                // Attempt to change directory
                if (chdir(directory) < 0) {
                    printf("Error: failed to change directory to %s\n", directory);
                }
                continue;  // Move to the next command after handling cd
            }

            // Fork a child process to execute each command segment
            if (fork() == 0) {
                run_command(cmd, 100, pcp);  // Pass the command segment to run_command
                exit(0);  // Child exits after executing the command
            }
            wait(0);  // Parent waits for each child to complete

            // Move to the next command after ;
            cmd = next_cmd + 1;
        }

        // Handle the last command (or single command if no ; found)
        // Remove leading spaces for the last command as well
        while (*cmd == ' ' || *cmd == '\t') cmd++;

        // Check if there's an "exit" command
        if (strcmp(cmd, "exit") == 0) {
            printf("Exiting shell...\n");
            exit(0);
        }

        // Handle "cd" command
        if (cmd[0] == 'c' && cmd[1] == 'd' && (cmd[2] == ' ' || cmd[2] == '\t' || cmd[2] == '\0')) {
            // Move pointer after cd
            char *directory = cmd + 2;

            // Skip leading spaces after cd
            while (*directory == ' ' || *directory == '\t') directory++;

            // Check if no directory was specified
            if (*directory == '\0') {
                printf("Error: no directory specified for 'cd' command.\n");
                continue;
            }

            // Remove trailing spaces from directory path
            char *end = directory + strlen(directory) - 1;
            while (end > directory && (*end == ' ' || *end == '\t')) {
                *end = '\0';
                end--;
            }

            // Attempt to change directory
            if (chdir(directory) < 0) {
                printf("Error: failed to change directory to %s\n", directory);
            }
            continue;  // Move to the next command after handling cd
        }

        // Execute the last or only command if it's not cd or exit
        if (*cmd) {  // Only proceed if theres a command to execute
            if (fork() == 0) {
                run_command(cmd, 100, pcp);  // Execute the last or only command
                exit(0);  
            }
            wait(0);  
        }
    }
    exit(0);  
}