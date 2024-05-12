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

char *create_sd_journal_argument(char *prefix, const char *full_message,
                                 size_t start, size_t end) {
  size_t suffix_length = end - start;
  size_t return_value_size = strlen(prefix) + suffix_length + 1;
  char *return_value = malloc(return_value_size);
  if (return_value == NULL) {
    fputs("Failed to allocate memory for log message.\n", stderr);
    exit(1);
  }
  sprintf(return_value, "%s%.*s", prefix, suffix_length, full_message + start);
  return return_value;
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
  const size_t total_separators = 3;
  size_t separator_indexes[total_separators];
  size_t separator_length = strlen(LOG_PARAM_SEPARATOR);

  size_t i = 0;
  const char *current_position = message;
  bool all_separators_found = true;
  while (i < total_separators) {
    char *new_position = strstr(current_position, LOG_PARAM_SEPARATOR);
    if (new_position == NULL) {
      all_separators_found = false;
      break;
    } else {
      separator_indexes[i] = new_position - message;
      current_position = new_position + separator_length;
    }
    i++;
  }

  int result;
  const char *actual_message;
  if (all_separators_found) {
    actual_message = message + separator_indexes[2] + separator_length;
    char *file = create_sd_journal_argument("CODE_FILE=", message, 0, separator_indexes[0]);
    char *line = create_sd_journal_argument("CODE_LINE=", message, separator_indexes[0] + separator_length, separator_indexes[1]);
    char *func = create_sd_journal_argument("", message, separator_indexes[1] + separator_length, separator_indexes[2]);
    result = sd_journal_send_with_location(
        file,
        line,
        func,
        "SDL_CATEGORY=%i", category,
        "PRIORITY=%i", sdl_log_priority_to_syslog_priority(priority),
        "MESSAGE=%s", actual_message,
        NULL
    );
    free(file);
    free(line);
    free(func);

    fprintf(
        log_file,
        "category=%i,priority=%i %.*s:%.*s in %.*s(): %s\n",
        category,
        priority,
        separator_indexes[0],
        message,
        separator_indexes[1] - separator_indexes[0] - separator_length,
        message + separator_indexes[0] + separator_length,
        separator_indexes[2] - separator_indexes[1] - separator_length,
        message + separator_indexes[1] + separator_length,
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
