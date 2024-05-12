#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <SDL_log.h>

// Thanks to Dan (<https://stackoverflow.com/users/27816/dan>) for this idea:
// <https://stackoverflow.com/a/240370/7593853>
#define TO_STRING_HELPER(value) #value
#define TO_STRING(value) TO_STRING_HELPER(value)

bool defaults_initialized = false;
SDL_LogOutputFunction default_output_function;
void *default_userdata;

/* A future change will make logs get outputted to systemd-journald [1]. The
 * signature for this function was chosen to make it easier to integrate with
 * systemd-journald. Specifically, the signature is very similar to
 * sd_journal_send_with_location()’s signature [2].
 *
 * [1]: <https://www.freedesktop.org/software/systemd/man/latest/systemd-journald.html>
 * [2]: <https://www.freedesktop.org/software/systemd/man/latest/sd_journal_print.html>
 */
void log_with_location_implementation(const char *file, const char *line,
                                      const char *func, const char *message) {
  SDL_Log("File: %s, Line: %s, Function: %s, Message: %s", file, line, func,
          message);
}

#define LogWithLocation(message)                                               \
  log_with_location_implementation(__FILE__, TO_STRING(__LINE__),              \
                                   __FUNCTION__, message)

void CustomLogOutputFunction(void *userdata, int category,
                             SDL_LogPriority priority, const char *message) {
  assert(defaults_initialized);
  default_output_function(default_userdata, category, priority, message);
}

int main(void) {
  SDL_LogGetOutputFunction(&default_output_function, &default_userdata);
  defaults_initialized = true;
  SDL_LogSetOutputFunction(CustomLogOutputFunction, NULL);

  LogWithLocation("Hello, world!");
  return 0;
}
