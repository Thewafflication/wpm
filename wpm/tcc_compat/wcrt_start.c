#include <windows.h>

#define WPM_MAX_COMMAND_LINE 32768
#define WPM_MAX_ARGUMENTS 4096

int main(int argc, char **argv);

static char command_line[WPM_MAX_COMMAND_LINE];
static char *arguments[WPM_MAX_ARGUMENTS];

static int parse_command_line(const char *source)
{
    char *destination = command_line;
    int argument_count = 0;

    while (*source != '\0') {
        int quoted = 0;
        while (*source == ' ' || *source == '\t') source++;
        if (*source == '\0') break;
        if (argument_count + 1 >= WPM_MAX_ARGUMENTS) break;
        arguments[argument_count++] = destination;

        while (*source != '\0') {
            unsigned int slash_count = 0;
            while (*source == '\\') {
                slash_count++;
                source++;
            }
            if (*source == '"') {
                while (slash_count >= 2) {
                    *destination++ = '\\';
                    slash_count -= 2;
                }
                if (slash_count == 1) {
                    *destination++ = '"';
                    source++;
                } else {
                    quoted = !quoted;
                    source++;
                }
                continue;
            }
            while (slash_count-- > 0) *destination++ = '\\';
            if (*source == '\0' || (!quoted && (*source == ' ' || *source == '\t'))) break;
            *destination++ = *source++;
        }
        *destination++ = '\0';
        while (*source == ' ' || *source == '\t') source++;
    }

    arguments[argument_count] = NULL;
    return argument_count;
}

void _start(void)
{
    int argument_count = parse_command_line(GetCommandLineA());
    ExitProcess((UINT)main(argument_count, arguments));
}
