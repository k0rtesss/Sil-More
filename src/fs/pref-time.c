/* File: fs/pref-time.c */

#include "angband.h"
#include "externs.h"

#include "fs/file.h"
#include "fs/path.h"
#include "fs/pref-time.h"

#include <time.h>

#ifdef CHECK_TIME
static char days[7][29] = { "SUN:XXXXXXXXXXXXXXXXXXXXXXXX",
    "MON:XXXXXXXX.........XXXXXXX", "TUE:XXXXXXXX.........XXXXXXX",
    "WED:XXXXXXXX.........XXXXXXX", "THU:XXXXXXXX.........XXXXXXX",
    "FRI:XXXXXXXX.........XXXXXXX", "SAT:XXXXXXXXXXXXXXXXXXXXXXXX" };

static bool check_time_flag = false;
#endif /* CHECK_TIME */

errr check_time(void)
{
#ifdef CHECK_TIME
    time_t c;
    struct tm* tp;

    if (!check_time_flag)
        return 0;

    c = time((time_t*)0);
    tp = localtime(&c);

    if (days[tp->tm_wday][tp->tm_hour + 4] != 'X')
        return 1;
#endif /* CHECK_TIME */

    return 0;
}

errr check_time_init(void)
{
#ifdef CHECK_TIME
    ang_file* fp;
    char buf[1024];

    path_build(buf, sizeof(buf), ANGBAND_DIR_FILE, "time.txt");

    fp = ang_file_open(buf, "r");

    if (!fp)
        return 0;

    check_time_flag = true;

    while (0 == sdl_fgets(fp, buf, sizeof(buf)))
    {
        if (!buf[0] || (buf[0] == '#'))
            continue;

        buf[sizeof(days[0]) - 1] = '\0';

        if (prefix(buf, "SUN:"))
            SDL_strlcpy(days[0], buf, sizeof(days[0]));
        if (prefix(buf, "MON:"))
            SDL_strlcpy(days[1], buf, sizeof(days[1]));
        if (prefix(buf, "TUE:"))
            SDL_strlcpy(days[2], buf, sizeof(days[2]));
        if (prefix(buf, "WED:"))
            SDL_strlcpy(days[3], buf, sizeof(days[3]));
        if (prefix(buf, "THU:"))
            SDL_strlcpy(days[4], buf, sizeof(days[4]));
        if (prefix(buf, "FRI:"))
            SDL_strlcpy(days[5], buf, sizeof(days[5]));
        if (prefix(buf, "SAT:"))
            SDL_strlcpy(days[6], buf, sizeof(days[6]));
    }

    ang_file_close(fp);
#endif /* CHECK_TIME */

    return 0;
}
