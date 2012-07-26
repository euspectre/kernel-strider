/*
 * Way for convert text strings in the stream using external program.
 * 
 * E.g., usage addr2line utility in streamed mode(addresses passed in
 * stdin, converted string is output to stdout) may be implemented in
 * that way.
 * 
 * Standard usage:
 * 
 * 1. Start converter program, using
 *       
 *          text_streamed_converter_start()
 *    
 *    Converter program is specified in execvp() style (path to the
 *    program, absolute or relative to PATH variable, plus NULL-
 *    terminated array of arguments).
 * 
 * 2. Text for convert is written using
 * 
 *          text_streamed_converter_put_text()
 *    
 *    Text shouldn't contain '\n' characters.
 * 
 * 3. Convertion of text chunk is performed using
 * 
 *          text_streamed_converter_convert()
 * 
 * 3. Converted text may be extracted using
 *    
 *         text_streamed_converter_get_text()
 * 
 *    Function should be called for each line of converted text.
 *    Note that '\n' is not extracted but ignored.
 * 
 * 4. When all strings are converted, converter program should be stopped
 *    using
 *    
 *          text_streamed_converter_stop()
 * 
 *    This function also wait until program exits.
 */

#ifndef TEXT_STREAMED_CONVERTER_H
#define TEXT_STREAMED_CONVERTER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <sys/wait.h>

/* Object incapsulated converter protram. */
struct text_streamed_converter
{
    /* All fields are private */
    int inputPipe[2];
    int outputPipe[2];
    pid_t converterPID;
};

/* 
 * Run converter program and fill 'converter' structure.
 * 
 * Return 0 on success.
 * Return -1 on fail and print error message.
 * 
 * Note, that errors in converter program are not detected here,
 * same for incorrect path to that program. These errors will be detected
 * at first attempt to put text for convert.
 */
static inline int text_streamed_converter_start(
    struct text_streamed_converter* converter,
    const char* program, char* argv[])
{
    int result = 0;
    
    result = pipe(converter->inputPipe);
    if(result)
    {
        perror("Failed to create input pipe for converter");
        goto err_input_pipe;
    }
    fcntl(converter->inputPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(converter->inputPipe[1], F_SETFD, FD_CLOEXEC);
    
    result = pipe(converter->outputPipe);
    if(result)
    {
        perror("Failed to create output pipe for converter");
        goto err_output_pipe;
    }
    fcntl(converter->outputPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(converter->outputPipe[1], F_SETFD, FD_CLOEXEC);

    converter->converterPID = fork();
    if(converter->converterPID == -1)
    {
        perror("Failed to fork converter program");
        goto err_fork;
    }
    else if(converter->converterPID == 0)
    {
        /* Child */
        (void) close(converter->inputPipe[1]);
        (void) close(converter->outputPipe[0]);
        
        result = dup2(converter->inputPipe[0], 0);
        if(result == -1)
        {
            perror("Failed to duplicate read end of input pipe.");
            exit(EXIT_FAILURE);
        }
        result = close(converter->inputPipe[0]);
        if(result == -1)
        {
            perror("Failed to close duplicated read end of input pipe");
            exit(EXIT_FAILURE);
        }

        result = dup2(converter->outputPipe[1], 1);
        if(result == -1)
        {
            perror("Failed to duplicate write end of output pipe");
            exit(EXIT_FAILURE);
        }
        result = close(converter->outputPipe[1]);
        if(result == -1)
        {
            perror("Failed to close duplicated read end of input pipe");
            exit(EXIT_FAILURE);
        }

        execvp(program, argv);
        perror("Failed to execute converter program");
        exit(EXIT_FAILURE);
    }
    else
    {
        /* Parent */
        (void)close(converter->inputPipe[0]);
        (void)close(converter->outputPipe[1]);
        
        return 0;
    }
err_fork:
    (void)close(converter->outputPipe[0]);
    (void)close(converter->outputPipe[1]);
err_output_pipe:
    (void)close(converter->inputPipe[0]);
    (void)close(converter->inputPipe[1]);
err_input_pipe:
    return -1;
}

/* 
 * Stop converter and release all resources.
 * 
 * Wait for converter program to terminate.
 */
static inline void text_streamed_converter_stop(
    struct text_streamed_converter* converter)
{
    (void)close(converter->inputPipe[1]);
    
    //fprintf(stderr, "Wait while converter is exited(pid is %d)...\n",
    //    (int)converter->converterPID);
    //fflush(stderr);
    
    (void)waitpid(converter->converterPID, NULL, 0);
    
    //fprintf(stderr, "Success(pid is %d).\n",
    //    (int)converter->converterPID);
    //fflush(stderr);
    
    (void)close(converter->outputPipe[0]);
}

/* 
 * Add text for translate.
 * 
 * Text shouldn't containt '\n' character.
 * 
 * Return 0 on success and negative error code otherwise.
 */
static inline int text_streamed_converter_put_text(
    struct text_streamed_converter* converter,
    const char* text, size_t text_len)
{
    ssize_t result = write(converter->inputPipe[1], text, text_len);
    if(result != (ssize_t)text_len)
    {
        /* 
         * Expect that write to the pipe is done in one step:
         * either all or nothing.
         */
        perror("Failed to write text for convert");
        return -1;
    }
    return 0;
}


/* 
 * Convert text collected.
 * 
 * Return 0 on success.
 * Return -1 if fail to initiate conversion.
 */
static inline int text_streamed_converter_convert(
    struct text_streamed_converter* converter)
{
    static char newline = '\n';
    ssize_t result = write(converter->inputPipe[1], &newline, 1);
    if(result != 1)
    {
        result = errno;
        perror("Failed to write newline for convert text");
        return result;
    }
    
    return 0;
}

/* 
 * Extract next line of convertion.
 *
 * For each chunk of converted text collect() callback will be called.
 * This callback should return 0 on success. Otherwise whole convertion
 * will be terminated with error.
 * 
 * '\n' character is not passed to collect() callback.
 * 
 * Return 0 on success.
 * Return -1 if fail to extract converted text. In that case error
 * message will be printed.
 * Return result of collect() call which fail(return non-zero).
 */
static inline int text_streamed_converter_get_text(
    struct text_streamed_converter* converter,
    int (*collect)(const char* text, size_t size, void* data),
    void* data)
{
    int result;
    /* Read until newline symbol appears. */
    char c;
    ssize_t len;
    
    while((len = read(converter->outputPipe[0], &c, 1)) == 1)
    {
        if(c == '\n')
        {
            /* Found end of converted line */
            return 0;
        }
        /* Converted line is not ended */
        result = collect(&c, 1, data);
        if(result) return result;
    }
    if(len == 0)
    {
        //EOF
        fprintf(stderr, "Converter program close write end of output pipe.\n");
        return -1;
    }
    else
    {
        perror("Failed to read converted line");
        return -1;
    }
}

#endif /* TEXT_STREAMED_CONVERTER_H */
