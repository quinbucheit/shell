/*
 * An implementation of a UNIX shell.
 *
 * Author: Quintin Bucheit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define SH_CL_BUFSIZE 512
#define SH_ARG_BUFSIZE 32

void shell_loop();
char *read_command_line();
char **parse_command_line(char *command_line);
int execute_command_line(char ** command_line_args);
int execute_builtin(char **command_line_args);
void execute_cd(char **args);
int block_check(char **command_line_args);
char *output_redirection(char **command_line_args);
int pipe_check(char **command_line_args);
int count_command_line_args(char **command_line_args);

void execute_external(char **command_line_args, 
  int block, char *output_file);

void execute_external_pipe(char **command_line_args, 
  int block, char *output_file, int pipe_index);

int main(void) {
  printf("$------------------------------------------------$\n");
  printf("$              Welcome to my shell!              $\n");
  printf("$                                                $\n");
  printf("$             Author: Quintin Bucheit            $\n");
  printf("$------------------------------------------------$\n");

  shell_loop();

  return 0;
}

/*
 * Loop that displays a user prompt, reads a command line,
 * parses the command line, and then takes the appropriate action.
 */
void shell_loop() {
  char *command_line;
  char **command_line_args;
  int exit_status = 0; 


  while (exit_status == 0) {
    printf("$ ");

    // Read command line.
    command_line = read_command_line();

    // Parse the previously read command line.
    command_line_args = parse_command_line(command_line);

    // Execute command line.
    exit_status = execute_command_line(command_line_args);

    free(command_line);
    free(command_line_args);
  }
}

/*
 * Reads input from the command line and returns it as a 
 * string literal.
 */
char *read_command_line() {
  int bufsize = SH_CL_BUFSIZE;
  int index = 0; // index of input from command line
  char c; // current char of the input from command line at index

  // Allocate memory for buffer.
  char *buffer = (char *)malloc(bufsize * sizeof(char));

  // If unable to allocate memory, print an error message 
  // and exit with the failed exit status.
  if (buffer == NULL) {
    printf("unable to allocate memory\n");
    exit(1);
  }

  while (1) {
    // Read next character.
    c = getchar();

    // If the end of the command line is reached, replace it 
    // with a NULL character and return buffer, else 
    // place c at current index in command line.
    if (c == EOF || c == '\n') {
      buffer[index] = '\0';
      return buffer;
    } else {
      buffer[index] = c;
    }
    index++;

    // If index >= bufsize, reallocate to allow for more
    // memory space for input.
    if (index >= bufsize) {
      bufsize += SH_CL_BUFSIZE;
      buffer = (char *)realloc(buffer, bufsize);

      // If unable to allocate memory, print an error message 
      // and exit with the failed exit status.
      if (buffer == NULL) {
        printf("unable to allocate memory\n");
        exit(1);
      }
    }
  }
}

/*
 * Parses the given string literal into arguments using " "
 * as the delimiter and then returns those arguments as 
 * an array of literal strings.
 */
char **parse_command_line(char *command_line) {
  int bufsize = SH_ARG_BUFSIZE;

  // Allocate memory for tokens.
  char **tokens = (char **)malloc(bufsize * sizeof(char*));

  // If unable to allocate memory, print an error message 
  // and exit with the failed exit status.
  if (tokens == NULL) {
    printf("unable to allocate memory\n");
    exit(1);
  }

  char *token;
  char *delim = " ";
  int index = 0; // index in tokens

  // Get the first token from command_line.
  token = strtok(command_line, delim);

  // Get the rest of the tokens from command_line and put
  // them in tokens.
  while (token != NULL) {
    tokens[index] = token;
    index++;

    // If index >= bufsize, reallocate to allow for more
    // memory space for input.
    if (index >= bufsize) {
      bufsize += SH_ARG_BUFSIZE;
      tokens = (char **)realloc(tokens, bufsize);

      // If unable to allocate memory, print an error message 
      // and exit with the failed exit status.
      if (tokens == NULL) {
        printf("unable to allocate memory\n");
        exit(1);
      }
    }

    token = strtok(NULL, delim);
  }

  // NULL terminate tokens and return.
  tokens[index] = NULL;
  return tokens;
}

/*
 * Determines what action to take based on the given input.
 * 
 * If the exit command is executed, 1 is returned else 0
 * is returned.
 */
int execute_command_line(char **command_line_args) {

  // List of built-in commands.
  char *builtins[] = { "cd", "exit" };
  int num_builtins = 2;

  // If no command was read, return 0.
  if (command_line_args[0] == NULL) {
    return 0;
  }

  // If command read is a built-in command, pass it to
  // execute_builtin.
  for (int i = 0; i < num_builtins; i++) {
    if (strcmp(command_line_args[0], builtins[i]) == 0) {
      return execute_builtin(command_line_args);
    }
  }

  // Check to see if & was read to run command in the 
  // background.
  int block = block_check(command_line_args);

  // Check for output redirection.
  char *output_file;
  output_file = output_redirection(command_line_args);

  // Check for pipe.
  int pipe_index = pipe_check(command_line_args);

  // Else external command was read. If a "|" was read,
  // call the pipe version, else call the version without
  // pipe.
  if (pipe_index != -1) {

    execute_external_pipe(command_line_args, block, 
      output_file, pipe_index);

  } else {
    execute_external(command_line_args, block, output_file);
  }

  return 0;
}

/*
 * Executes the given built-in command with the given
 * arguements. Returns 1 if the exit command was read,
 * else returns 0.
 */
int execute_builtin(char **command_line_args) {

  // If the exit command is read, return 1.
  if (strcmp(command_line_args[0], "exit") == 0) {
    return 1;
  }

  // sh_cd is called and 0 is returned since it is
  // currently the only other supported built-in
  // command.
  execute_cd(command_line_args);
  return 0;
}

/*
 * Changes the shell's current working directory.
 */
void execute_cd(char **command_line_args) {
  // If name is left blank or the name is "~", the shell's
  // current working directory becomes the home directory 
  // of the user, else the current working directory
  // becomes the directory with the given name if it exists.
  if (command_line_args[1] == NULL || 
      strcmp(command_line_args[1], "~") == 0) {

    chdir(getenv("HOME"));
  } else {

    // Change the directory to the directory with
    // the given name, else print an error message
    // and do not change the directory.
    if (chdir(command_line_args[1]) != 0) {
      printf("cd: %s: No such file or directory\n", 
        command_line_args[1]);
    }
  }

  return;
}

/*
 * Returns 0 if "&" was read, else returns 1.
 */
int block_check(char **command_line_args) {
  int i;
  for (i = 0; command_line_args[i] != NULL; i++);
  if (strcmp(command_line_args[i - 1], "&") == 0) {
    // Remove & from command_line_args.
    command_line_args[i - 1] = NULL;
    return 0; // shell does not block
  } else {
    return 1; // shell blocks
  }
}

/*
 * Returns output file name if there is output redirection 
 * else returns NULL.
 */
char *output_redirection(char **command_line_args) {
  char *output_file;

  int i, j;
  for (i = 0; command_line_args[i] != NULL; i++) {
    
    // Check for ">" or ">>".
    if (strcmp(command_line_args[i], ">") == 0 ||
        strcmp(command_line_args[i], ">>") == 0) {

      // If output file is directly after ">" or ">>",
      // assign it to output_file, else print an error 
      // message and exit.
      if (command_line_args[i + 1] != NULL) {
        output_file = command_line_args[i + 1];
      } else {
        printf("must specify output file\n");
        exit(1);
      }

      // Remove ">" or ">>" and output file name from 
      // command_line_args being sure command_line_args
      // is still NULL terminated.
      for (j = i; command_line_args[j - 1] != NULL; j++) {
        command_line_args[j] = command_line_args[j + 2];
      }

      return output_file;
    }
  }

  return NULL;
}

/*
 * Returns index of pipe if "|" is read, else returns -1.
 */
int pipe_check(char **command_line_args) {
  int i;
  for (i = 0; command_line_args[i] != NULL; i++) {
    if (strcmp(command_line_args[i], "|") == 0) {
      return i;
    }
  }
  
  return -1;
}

/*
 * Executes simple/external commands. The shell forks a
 * child which in turn calls execvp to replace itself with
 * the process image of the command.
 */
void execute_external(char **command_line_args, 
  int block, char *output_file) {

  // Fork a child.
  pid_t pid = fork();

  // If pid < 0, the fork failed.
  if (pid < 0) {
    printf("unable to fork\n");
    exit(1);
  }

  // Child calls execvp to replace itself with the process
  // image of the command.
  if (pid == 0) {

    // If output_file is not NULL, redirect output to given 
    // output_file.
    if (output_file != NULL) {
      int out = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
      close(1); // close stdout
      dup(out);
      close(out);
    }

    // Execute command.
    execvp(command_line_args[0], command_line_args);

    // If an error occurred, execvp will return a -1 else it
    // will not return at all.
    printf("unable to execute command\n");
    exit(1);
  }

  // Suspend the parent until the child is done if command
  // was not run in background.
  if (block == 1) { // shell blocks
    waitpid(pid, NULL, 0);
  }
  
  return;
}

/*
 * Executes piped simple/external commands. Splits command_line_args into the 
 * left and right side of "|". Two children are forked; one for pipe_left and 
 * the other for pipe_right. Each child then takes the appropriate action.
 */
void execute_external_pipe(char **command_line_args, 
  int block, char *output_file, int pipe_index) {

  // Find number of command line arguments to the right 
  // of the pipe.
  int num_command_line_args = count_command_line_args(command_line_args);
  int num_right = num_command_line_args - (pipe_index + 1);

  // Replace "|" with NULL in command_line_args in order to NULL terminate
  // left side.
  command_line_args[pipe_index] = NULL;
  
  // Allocate memory for pipe_left and pipe_right.
  char **pipe_left = (char **)malloc((pipe_index + 1) * sizeof(char *));
  char **pipe_right = (char **)malloc(num_right * sizeof(char *));

  // Split command_line_args left and right of pipe.
  
  // Copy args to the left of the pipe in command_line_args to 
  // pipe_left being sure to NULL terminate.
  int i = 0;
  for (; command_line_args[i - 1] != NULL; i++) {
    pipe_left[i] = command_line_args[i];
  }

  // Copy args to the right of the pipe in command_line_args to 
  // pipe_right.
  int j = 0;
  for (; command_line_args[i] != NULL; j++, i++) {
    pipe_right[j] = command_line_args[i];
  }

  // NULL terminate pipe_right
  pipe_right[j + 1] = NULL;

  int fd[2];
  pipe(fd);

  // Fork a child for pipe_left.
  pid_t pid1 = fork();

  // If pid < 0, the fork failed.
  if (pid1 < 0) {
    printf("unable to fork first child\n");
    exit(1);
  }

  // Child 1 reads input from stdin, writes output to the 
  // output side of the pipe and calls execvp to replace 
  // itself with the process image of the command.
  if (pid1 == 0) {

    // Child 1 closes input end.
    close(fd[0]);

    // Send output to output side of pipe instead stdout.
    dup2(fd[1], 1);

    // Execute command.
    execvp(pipe_left[0], pipe_left);

    // If an error occurred, execvp will return a -1 else it
    // will not return at all.
    printf("unable to execute command\n");
    exit(1);
  }

  // Fork second child for pipe_right.
  pid_t pid2 = fork();

  // If pid < 0, the fork failed.
  if (pid1 < 0) {
    printf("unable to fork second child\n");
    exit(1);
  }

  // Child 2 reads input from the input side of the pipe,
  // writes output to the stdout and calls execvp to replace 
  // itself with the process image of the command.
  if (pid2== 0) {

    // Child 2 closes output end.
    close(fd[1]);

    // If output_file is not NULL, redirect output to given 
    // output_file.
    if (output_file != NULL) {
      int out = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
      close(1); // close stdout
      dup(out);
      close(out);
    }

    // Read input from input side of pipe instead of stdin.
    dup2(fd[0], 0);

    // Execute command.
    execvp(pipe_right[0], pipe_right);

    // If an error occurred, execvp will return a -1 else it
    // will not return at all.
    printf("unable to execute command\n");
    exit(1);
  }

  // Parent closes its ends of the pipe.
  close(fd[0]);
  close(fd[1]);

  // Parent waits for both children to finish executing.
  waitpid(pid1, NULL, 0);
  waitpid(pid2, NULL, 0);

  free(pipe_left);
  free(pipe_right);
  
  return;
}

/*
 * Returns the number of args in command_line_args.
 */
int count_command_line_args(char **command_line_args) {
  int i = 0;
  for (; command_line_args[i] != NULL; i++);
  return i;
}
