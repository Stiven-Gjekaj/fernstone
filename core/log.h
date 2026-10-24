// log.h: write a line to stderr and to the log file of the data directory.
#ifndef FERN_LOG_H
#define FERN_LOG_H

// Open "log.txt" in dir. Each later line goes to the file too.
void fern_log_open(const char *dir);
void fern_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
