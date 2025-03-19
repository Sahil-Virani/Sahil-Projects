// Name: Sahil Virani
// Author: I pledge my honor that I have abided by the Stevens Honor System.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"

volatile sig_atomic_t interrupted = 0;

// Handler for SIGINT signals (typically issued by pressing Ctrl+C). When SIGINT is received, this handler sets a flag indicating the shell has been interrupted.
void sigint_handler(int sig) {
    interrupted = 1;
}

// Prints the shell prompt. It fetches the current working directory and displays it in blue, followed by a '> ' symbol in the default terminal color.
void printPrompt() {
    char cwd[PATH_MAX]; 
    // Attempts to get the current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // Prints the directory in blue, followed by the shell prompt symbol '>'
        printf("%s[%s]> %s", BLUE, cwd, DEFAULT); 
    } else {
        // If getting the directory fails, prints an error message to stderr
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
    }
    // Flushes the stdout buffer to ensure immediate display of the prompt
    fflush(stdout);
}

// Changes the shell's current working directory. If the path is NULL or "~", it changes to the user's home directory. Otherwise, it changes to the specified path.
void cd(char *path) {
    // Handling the case where the 'cd' command is called without arguments or with '~'
    if (path == NULL || strcmp(path, "~") == 0) {
        // Fetches the user's home directory using their user ID
        struct passwd *pw = getpwuid(getuid());
        if (pw == NULL) {
            // If fetching the home directory fails, prints an error message to stderr
            fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
            return;
        }
        // Sets the path to the user's home directory
        path = pw->pw_dir;
    }

    // Attempts to change to the specified directory
    if (chdir(path) != 0) {
        // If changing the directory fails, prints an error message to stderr
        fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", path, strerror(errno));
    }
}

// Prints the current working directory to stdout.
void pwd() {
    char cwd[PATH_MAX]; 
    // Attempts to get the current working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // If successful, prints the directory to stdout
        printf("%s\n", cwd);
    } else {
        // If getting the directory fails, prints an error message to stderr
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
    }
}

// Lists all non-hidden files in the current directory to stdout.
void lf() {
    // Opens the current directory
    DIR *dir = opendir(".");
    // Checks if the directory could be opened
    if (dir != NULL) {
        struct dirent *entry;
        // Reads each directory entry
        while ((entry = readdir(dir)) != NULL) {
            // Skips hidden files (those starting with '.')
            if (entry->d_name[0] != '.') {
                // Prints the filename to stdout
                printf("%s\n", entry->d_name);
            }
        }
        // Closes the directory stream
        closedir(dir);
    } else {
        // If opening the directory fails, prints an error message to stderr
        fprintf(stderr, "Error: Cannot read directory. %s.\n", strerror(errno));
    }
}

// Lists processes information by reading from the /proc directory.
#include <limits.h> // Make sure to include this at the top of your file

// ...

void lp() {
    DIR *proc = opendir("/proc");
    struct dirent *entry;

    if (!proc) {
        fprintf(stderr, "Error: Cannot open /proc. %s.\n", strerror(errno));
        return;
    }

    while ((entry = readdir(proc)) != NULL) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            char path[PATH_MAX]; 
            int written = snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
            // Checks for truncation or any snprintf error
            if (written < 0 || written >= sizeof(path)) {
                fprintf(stderr, "Error: Path buffer for cmdline of /proc/%s may be truncated.\n", entry->d_name);
                continue;
            }

            FILE *cmdline = fopen(path, "r");
            if (!cmdline) continue;

            char command[256]; 
            if (fgets(command, sizeof(command), cmdline) != NULL) {
                struct stat stats;
                char userPath[PATH_MAX]; 
                // Constructs the path to the process's directory to obtain UID
                snprintf(userPath, sizeof(userPath), "/proc/%s", entry->d_name);

                // Gets the file status
                if (stat(userPath, &stats) == -1) {
                    fprintf(stderr, "Error: Cannot access %s. %s.\n", userPath, strerror(errno));
                    fclose(cmdline);
                    continue;
                }

                struct passwd *pw = getpwuid(stats.st_uid);
                if (!pw) {
                    fprintf(stderr, "Error: Cannot find user for UID %ld.\n", (long)stats.st_uid);
                    fclose(cmdline);
                    continue;
                }

                // Replaces null bytes with spaces to make the command readable
                for (char *p = command; *p != '\0'; p++) {
                    if (*p == '\0' && *(p + 1) != '\0' && p != command) {
                        *p = ' ';
                    }
                }

                printf("%s %s %s\n", entry->d_name, pw->pw_name, command);
            }
            fclose(cmdline);
        }
    }
    closedir(proc);
}


// Forks a new process to execute a given command. The child process will execute the command, while the parent waits for the command to complete.
void execute_command(char **args) {
    // Creates a new process
    pid_t pid = fork();
    if (pid == -1) {
        // If forking a new process fails, prints an error message to stderr
        fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
    } else if (pid == 0) {
        struct sigaction sa_child;
        // Ignores SIGINT signals
        sa_child.sa_handler = SIG_IGN;
        sigemptyset(&sa_child.sa_mask);
        sa_child.sa_flags = 0;
        sigaction(SIGINT, &sa_child, NULL);

        // Executes the command using execvp
        if (execvp(args[0], args) == -1) {
            // If execution fails, prints an error message to stderr
            fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
            // Terminates the child process
            exit(EXIT_FAILURE);
        }
    } else {
        // In the parent process waits for the child to finish
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            // If waiting fails, prints an error message to stderr
            fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
        }

        // Checks if the child was terminated by a signal (e.g., SIGINT)
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT) {
            // If so, prints a newline for clean output
            printf("\n");
        }
    }
}

// Processes user input, tokenizing the input string and determining whether to execute a built-in command or an external command.
void process_input(char *input) {
    // Tokenizes the input into command and arguments
    char *args[10]; 
    int i = 0;
    char *saveptr; // For strtok_r's internal bookkeeping

    char *token = strtok_r(input, " ", &saveptr);
    while (token != NULL && i < (sizeof(args) / sizeof(char *) - 1)) {
        args[i++] = token;
        token = strtok_r(NULL, " ", &saveptr);
    }
    args[i] = NULL; 

    // Handles built-in commands and external commands
    if (i == 0) { 
        return;
    }

    if (strcmp(args[0], "cd") == 0) {
        // Changes directory command
        cd(args[1]);
    } else if (strcmp(args[0], "exit") == 0) {
        // Exit command
        exit(EXIT_SUCCESS);
    } else if (strcmp(args[0], "pwd") == 0) {
        // Prints working directory command
        pwd();
    } else if (strcmp(args[0], "lf") == 0) {
        // Lists files command
        lf();
    } else if (strcmp(args[0], "lp") == 0) {
        // Lists processes command
        lp();
    } else {
        // Executes external command
        execute_command(args);
    }
}


// The main entry point of the shell program. Sets up signal handling and enters an interactive loop to process user input.
int main() {
    char input[1024];  
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction)); // Zero out the structure before using it
    sa.sa_handler = sigint_handler; 
    sa.sa_flags = SA_RESTART; 
    sigemptyset(&sa.sa_mask); 

    if (sigaction(SIGINT, &sa, NULL) == -1) {
    fprintf(stderr, "Error: Cannot register signal handler. %s.\n", strerror(errno));
    exit(EXIT_FAILURE);
}
    // Main loop to process commands
    while (1) {
        // Checks if the SIGINT signal was received
        if (interrupted) {
            // Resets the flag and print a newline
            printf("\n");
            interrupted = 0;
        }
        // Prints the prompt
        printPrompt(); 
    
        // Reads input from the user
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // If reading input fails due to an interrupt, clears the error and continue
            if (errno == EINTR) {
                clearerr(stdin); 
                continue; 
            } else {
                // If another error occurs, prints it and continue
                fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
                continue;
            }
        }
    
        // Removes the trailing newline character from the input
        input[strcspn(input, "\n")] = '\0';
    
        // If the input is empty, restarts the loop
        if (strlen(input) == 0) continue; 
        
        // Processes the input command
        process_input(input);
    }

    return 0; 
}

