#include <stdio.h>
#include <fotax.h>

static void fotax_event(fotax_t *fotax, fotax_event_e event, const char *json)
{
    switch (event) {
        case FOTAX_EVENT_VERSION:
            printf("%s,%d,%s\n", __func__, __LINE__, json);
            break;

        case FOTAX_EVENT_DOWNLOAD:
            printf("%s,%d,%s\n", __func__, __LINE__, json);
            break;

        case FOTAX_EVENT_END:
            printf("%s,%d,%s\n", __func__, __LINE__, json);
            break;

        case FOTAX_EVENT_RESTART:
            printf("%s,%d,%s\n", __func__, __LINE__, json);
            break;

        default: break;
    }
}

static fotax_t g_fotax;
int main(int argc, char **argv)
{
    while(1) {
        char ch = getchar();
        if (ch == 's') {
            fotax_start(&g_fotax);
            g_fotax.fotax_event_cb = fotax_event;
        } else if (ch == 'p') {
            fotax_stop(&g_fotax);
        } else if (ch == 'v') {
            fotax_version_check(&g_fotax);
        } else if (ch == 'd') {
            fotax_download(&g_fotax);
        } else if (ch == 'r') {
            fotax_restart(&g_fotax, 5000);
        } else if (ch == 'x') {
            fotax_restart(&g_fotax, 0);
        } else if (ch == 'g') {
            fotax_get_state(&g_fotax);
        }
    }


    return 0;
}