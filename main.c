#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL_log.h>
#include <systemd/sd-journal.h>

/* I chose to use a noncharacter code point to separate extra data from the log
 * message itself. I chose to use a noncharacter one because noncharacter code
 * points should never appear in actual log messages. I chose this noncharacter
 * code point in particular because the Unicode standard mentions that this one
 * in particular might be useful for this purpose. The Unicode Standard says
 * “This attribute renders these two noncharacter code points useful for
 * internal purposes as sentinels. For example, they might be used to indicate
 * the end of a list, to represent a value in an index guaranteed to be higher
 * than any valid character value, and so on.” [1].
 *
 * [1]: <https://www.unicode.org/versions/Unicode15.1.0/ch23.pdf#G12612>
 */
#define LOG_PARAM_SEPARATOR "\uFFFF"
// Thanks to Dan (<https://stackoverflow.com/users/27816/dan>) for this idea:
// <https://stackoverflow.com/a/240370/7593853>
#define TO_STRING_HELPER(value) #value
#define TO_STRING(value) TO_STRING_HELPER(value)

bool defaults_initialized = false;
SDL_LogOutputFunction default_output_function;
void *default_userdata;
FILE *log_file;

void log_with_location_implementation(SDL_LogPriority priority,
                                      const char *file, const char *line,
                                      const char *func, const char *message) {
  SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority,
                 "%s" LOG_PARAM_SEPARATOR "%s" LOG_PARAM_SEPARATOR
                 "%s" LOG_PARAM_SEPARATOR "%s",
                 file, line, func, message);
}

#define LogWithLocation(priority, message)                                     \
  log_with_location_implementation(priority, __FILE__, TO_STRING(__LINE__),    \
                                   __FUNCTION__, message)

int sdl_log_priority_to_syslog_priority(SDL_LogPriority priority) {
  switch (priority) {
  case SDL_LOG_PRIORITY_VERBOSE:
    return LOG_DEBUG;
  case SDL_LOG_PRIORITY_DEBUG:
    return LOG_INFO;
  case SDL_LOG_PRIORITY_WARN:
    return LOG_WARNING;
  case SDL_LOG_PRIORITY_ERROR:
    return LOG_ERR;
  case SDL_LOG_PRIORITY_CRITICAL:
    return LOG_CRIT;
  default:
    // We got passed an invalid SDL_LogPriority.
    return LOG_ALERT;
  }
}

void create_sd_journal_argument(char *buffer, const char *prefix,
                                const char *suffix) {
  int result = sprintf(buffer, "%s%s", prefix, suffix);
  if (result < 0) {
    fputs("Failed to create argument for sd_journal_send_with_location().\n",
          stderr);
    exit(1);
  }
}

void open_log(const char *mode) {
  log_file = fopen("log.txt", mode);
  if (log_file == NULL) {
    fputs("Failed to open log.txt\n", stderr);
    exit(1);
  }
}

void close_log() {
  if (fclose(log_file)) {
    fputs("Failed to close log.txt\n", stderr);
    exit(1);
  }
}

void CustomLogOutputFunction(void *userdata, int category,
                             SDL_LogPriority priority, const char *message) {
  const char sd_code_file[] = "CODE_FILE=";
  const char sd_code_line[] = "CODE_LINE=";
  size_t message_length = strlen(message);

  char *file = malloc(message_length + 1);
  char *line = malloc(message_length + 1);
  char *func = malloc(message_length + 1);
  size_t parameters_end;
  int result = sscanf(message,
                      "%[^" LOG_PARAM_SEPARATOR "]" LOG_PARAM_SEPARATOR
                      "%[^" LOG_PARAM_SEPARATOR "]" LOG_PARAM_SEPARATOR
                      "%[^" LOG_PARAM_SEPARATOR "]" LOG_PARAM_SEPARATOR "%zn",
                      file, line, func, &parameters_end);

  bool all_separators_found = result == 3;
  const char *actual_message;
  if (all_separators_found) {
    char file_arg[strlen(sd_code_file) + strlen(file) + 1];
    create_sd_journal_argument(file_arg, sd_code_file, file);
    char line_arg[strlen(sd_code_line) + strlen(line) + 1];
    create_sd_journal_argument(line_arg, sd_code_line, line);
    actual_message = message + parameters_end;

    result = sd_journal_send_with_location(
        file_arg,
        line_arg,
        func,
        "SDL_CATEGORY=%i", category,
        "PRIORITY=%i", sdl_log_priority_to_syslog_priority(priority),
        "MESSAGE=%s", actual_message,
        NULL
    );
    fprintf(
        log_file,
        "category=%i,priority=%i %s:%s in %s(): %s\n",
        category,
        priority,
        file,
        line,
        func,
        actual_message
    );
  } else {
    actual_message = message;
    result = sd_journal_send(
        "SDL_CATEGORY=%i", category,
        "PRIORITY=%i", sdl_log_priority_to_syslog_priority(priority),
        "MESSAGE=%s", message,
        NULL
    );
    fprintf(
        log_file,
        "category=%i,priority=%i %s\n",
        category,
        priority,
        actual_message
    );
  }
  if (result != 0) {
    fprintf(stderr,
            "Failed to write a message to systemd-journald. Error code: %i\n",
            result);
  }
  free(file);
  free(line);
  free(func);

  assert(defaults_initialized);
  default_output_function(default_userdata, category, priority, actual_message);
}

int main(void) {
  // Clear any previous logs.
  open_log("w");
  close_log();

  open_log("a");
  if (atexit(close_log) != 0) {
    fputs("Failed to register close_log() as an atexit funtion.\n", stderr);
    return 1;
  }
  SDL_LogGetOutputFunction(&default_output_function, &default_userdata);
  defaults_initialized = true;
  SDL_LogSetOutputFunction(CustomLogOutputFunction, NULL);

  for (SDL_LogPriority p = SDL_LOG_PRIORITY_VERBOSE; p < SDL_NUM_LOG_PRIORITIES; p++) {
    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, p, "Logging without line number.");
    LogWithLocation(p, "Logging with line number.");
  }

  return 0;
}
