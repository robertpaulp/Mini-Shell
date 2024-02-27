// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>

#include <sys/stat.h>

#include <sys/wait.h>

#include <fcntl.h>

#include <unistd.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include "cmd.h"

#include "utils.h"

#define READ 0
#define WRITE 1

/**
 * Internal change-directory command.
 */
static bool shell_cd(word_t *dir)
{
	/* TODO: Execute cd. */
	if (dir == NULL)
		return false;

	if (chdir(dir->string) != 0)
		return false;

	return true;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	return SHELL_EXIT;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */
	if (s->verb == NULL)
		return shell_exit();

	char *command = get_word(s->verb);
	/* TODO: If builtin command, execute the command. */
	if (strcmp(command, "cd") == 0) {
		if (s->out != NULL) {
			int fd;

			if (s->io_flags & IO_OUT_APPEND || strstr(s->out->string, ">>") != NULL)
				fd = open(s->out->string, O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				fd = open(s->out->string, O_WRONLY | O_CREAT | O_TRUNC, 0666);

			if (fd == -1)
				return shell_exit();

			close(fd);
		}

		if (s->err != NULL) {
			int fd;

			if (s->io_flags & IO_ERR_APPEND || strstr(s->err->string, ">>") != NULL)
				fd = open(s->err->string, O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				fd = open(s->err->string, O_WRONLY | O_CREAT | O_TRUNC, 0666);

			if (fd == -1)
				return shell_exit();

			close(fd);
		}

		bool status = shell_cd(s->params);

		if (status == false) {
			printf("No such file or directory\n");
			fflush(stdout);
			return EXIT_FAILURE;
		}
		return 0;
	}

	if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0)
		return shell_exit();

	if (strchr(command, '=') != NULL) {
		char *var = strtok_r(command, "=", &command);
		char *value = strtok_r(NULL, "=", &command);

		if (setenv(var, value, 1) == -1)
			return shell_exit();

		return 0;
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */
	pid_t pid;
	int status;

	switch (pid = fork()) {
	case -1:
		return shell_exit();

	case 0:
		if (s->in != NULL) {
			int fd = open(get_word(s->in), O_RDONLY);

			if (fd == -1)
				return shell_exit();

			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		if (s->out != NULL && s->err != NULL) {
			if (strcmp(s->out->string, s->err->string) == 0) {
				int fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				if (fd == -1)
					return shell_exit();

				dup2(fd, STDOUT_FILENO);
				dup2(fd, STDERR_FILENO);
				close(fd);

			} else {
				int fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				if (fd == -1)
					return shell_exit();

				dup2(fd, STDOUT_FILENO);
				close(fd);

				int fd2 = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				if (fd2 == -1)
					return shell_exit();

				dup2(fd2, STDERR_FILENO);
				close(fd2);
			}
		} else {
			if (s->out != NULL) {
				int fd;

				if (s->io_flags & IO_OUT_APPEND || strstr(s->out->string, ">>") != NULL)
					fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
				else
					fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				if (fd == -1)
					return shell_exit();

				dup2(fd, STDOUT_FILENO);
				close(fd);
			}

			if (s->err != NULL) {
				int fd;

				if (s->io_flags & IO_ERR_APPEND || strstr(s->err->string, ">>") != NULL)
					fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
				else
					fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				if (fd == -1)
					return shell_exit();

				dup2(fd, STDERR_FILENO);
				close(fd);
			}
		}

		int size;

		if (execvp(command, get_argv(s, &size)) == -1) {
			fprintf(stdout, "Execution failed for '%s'\n", command);
			fflush(stdout);
			exit(EXIT_FAILURE);
		}

		return 0;

	default:
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}

	return EXIT_FAILURE;
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	pid_t pid, pid2;
	int status;

	switch (pid = fork()) {
	case -1:
		return shell_exit();

	case 0:
		status = parse_command(cmd1, level + 1, father);
		exit(status);
		break;

	default:
		switch (pid2 = fork()) {
		case -1:
			return shell_exit();

		case 0:
			status = parse_command(cmd2, level + 1, father);
			exit(status);
			break;

		default:
			waitpid(pid, &status, 0);
			if (WEXITSTATUS(status) != 0)
				return false;
			waitpid(pid2, &status, 0);
			if (WEXITSTATUS(status) != 0)
				return false;
			break;
		}
		break;
	}

	return true;
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level, command_t *father)
{
	int pipe_fd[2];
	int exit_status, fd;
	pid_t pid;

	if (pipe(pipe_fd) == -1)
		return false;

	switch (pid = fork()) {
	case -1:
		return shell_exit();

	case 0:
		fd = dup(STDOUT_FILENO);

		close(pipe_fd[READ]);
		dup2(pipe_fd[WRITE], STDOUT_FILENO);
		close(pipe_fd[WRITE]);

		exit_status = parse_command(cmd1, level + 1, father);
		dup2(fd, STDOUT_FILENO);
		close(fd);
		exit(exit_status);
		break;
	default:
		fd = dup(STDIN_FILENO);

		close(pipe_fd[WRITE]);
		dup2(pipe_fd[READ], STDIN_FILENO);
		close(pipe_fd[READ]);

		exit_status = parse_command(cmd2, level + 1, father);
		dup2(fd, STDIN_FILENO);
		close(fd);
		if (exit_status != 0)
			return false;
		else
			return true;
		break;
	}

	return true;
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */
	if (c == NULL)
		return SHELL_EXIT;

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		return parse_simple(c->scmd, level + 1, father);
	}

	switch (c->op) {
	case OP_SEQUENTIAL:

		/* TODO: Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);

		return parse_command(c->cmd2, level + 1, c);

		break;

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */

		if (run_in_parallel(c->cmd1, c->cmd2, level + 1, c))
			return 0;
		else
			return EXIT_FAILURE;
		break;

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */

		switch (parse_command(c->cmd1, level + 1, c)) {
		case 0:
			return EXIT_FAILURE;

		default:
			return parse_command(c->cmd2, level + 1, c);
		}

		break;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */

		switch (parse_command(c->cmd1, level + 1, c)) {
		case 0:
			return parse_command(c->cmd2, level + 1, c);

		default:
			return EXIT_FAILURE;
		}
		break;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		if (run_on_pipe(c->cmd1, c->cmd2, level + 1, c))
			return 0;
		else
			return EXIT_FAILURE;
		break;

	default:
		return SHELL_EXIT;
	}

	return 0; /* TODO: Replace with actual exit code of command. */
}
