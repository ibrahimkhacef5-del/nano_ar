/**************************************************************************
 *  nano-ar — محرر نصوص عربي خفيف يعمل على Termux و Linux              *
 *  الرخصة: MIT                                                          *
 **************************************************************************/

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <ncurses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>

#define APP_NAME     "nano-ar"
#define APP_VERSION  "1.0.0"
#define TAB_SIZE     4
#define MAX_LINE     4096
#define UNDO_MAX     200
#define COLOR_TAG    1
#define COLOR_ATTR   2
#define COLOR_STRING 3
#define COLOR_COMMENT 4
#define COLOR_MATCH  5

typedef struct Line {
    char *data;
    struct Line *prev;
    struct Line *next;
} Line;

typedef struct {
    Line *head, *tail, *current;
    int cx, cy;
    int rowoff, coloff;
    int numlines;
    char filename[512];
    bool modified;
} Buffer;

typedef struct {
    char *text;
    int cx, cy;
} UndoState;

typedef struct {
    UndoState stack[UNDO_MAX];
    int top;
    int count;
} UndoStack;

static Buffer B;
static UndoStack undo_stack;
static int screen_rows = 24, screen_cols = 80;
static char status_msg[512] = "";
static bool running = true;
static char *clipboard = NULL;

static char **dictionary = NULL;
static size_t dict_count = 0;
static size_t dict_cap = 0;
static bool spell_ready = false;
static bool hunspell_available = false;

static const char *html_tags[] = {
    "a","abbr","address","area","article","aside","audio",
    "b","base","bdi","bdo","blockquote","body","br","button",
    "canvas","caption","cite","code","col","colgroup",
    "data","datalist","dd","del","details","dfn","dialog","div","dl","dt",
    "em","embed","fieldset","figcaption","figure","footer","form",
    "h1","h2","h3","h4","h5","h6","head","header","hgroup","hr","html",
    "i","iframe","img","input","ins","kbd","label","legend","li","link",
    "main","map","mark","menu","meta","meter","nav","noscript",
    "object","ol","optgroup","option","output","p","param","picture",
    "pre","progress","q","rp","rt","ruby","s","samp","script","section",
    "select","slot","small","source","span","strong","style","sub",
    "summary","sup","table","tbody","td","template","textarea","tfoot",
    "th","thead","time","title","tr","track","u","ul","var","video","wbr"
};
#define NUM_HTML_TAGS (sizeof(html_tags)/sizeof(html_tags[0]))

static const char *void_tags[] = {
    "area","base","br","col","embed","hr","img","input",
    "link","meta","param","source","track","wbr"
};
#define NUM_VOID_TAGS (sizeof(void_tags)/sizeof(void_tags[0]))

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { endwin(); fprintf(stderr, "Out of memory\n"); exit(1); }
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) { endwin(); fprintf(stderr, "Out of memory\n"); exit(1); }
    return q;
}

static char *xstrdup(const char *s) {
    if (!s) s = "";
    char *p = xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static Line *line_new(const char *data) {
    Line *l = xmalloc(sizeof(Line));
    l->data = xstrdup(data ? data : "");
    l->prev = l->next = NULL;
    return l;
}

static void buffer_init(void) {
    B.head = B.tail = B.current = line_new("");
    B.cx = B.cy = 0;
    B.rowoff = B.coloff = 0;
    B.numlines = 1;
    B.filename[0] = '\0';
    B.modified = false;
}

static void buffer_free(void) {
    Line *l = B.head;
    while (l) {
        Line *n = l->next;
        free(l->data);
        free(l);
        l = n;
    }
    B.head = B.tail = B.current = NULL;
}

static char *buffer_serialize(void) {
    size_t total = 0;
    for (Line *l = B.head; l; l = l->next)
        total += strlen(l->data) + 2;

    char *buf = xmalloc(total + 64);
    buf[0] = '\0';

    for (Line *l = B.head; l; l = l->next) {
        strcat(buf, l->data);
        if (l->next) strcat(buf, "\n");
    }
    return buf;
}

static void undo_push(void) {
    if (undo_stack.count >= UNDO_MAX) {
        free(undo_stack.stack[0].text);
        memmove(&undo_stack.stack[0], &undo_stack.stack[1],
                sizeof(UndoState) * (UNDO_MAX - 1));
        undo_stack.count--;
        undo_stack.top--;
    }

    undo_stack.top = (undo_stack.top + 1) % UNDO_MAX;
    undo_stack.stack[undo_stack.top].text = buffer_serialize();
    undo_stack.stack[undo_stack.top].cx = B.cx;
    undo_stack.stack[undo_stack.top].cy = B.cy;
    undo_stack.count++;
}

static void buffer_deserialize(const char *text) {
    Line *l = B.head;
    while (l) {
        Line *n = l->next;
        free(l->data);
        free(l);
        l = n;
    }

    B.head = B.tail = NULL;
    B.numlines = 0;

    const char *p = text;
    Line *prev = NULL;

    while (1) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);

        char *tmp = xmalloc(len + 1);
        memcpy(tmp, p, len);
        tmp[len] = '\0';

        Line *newl = line_new(tmp);
        free(tmp);

        if (prev) { prev->next = newl; newl->prev = prev; }
        else B.head = newl;
        prev = newl;
        B.numlines++;

        if (!nl) break;
        p = nl + 1;
    }

    if (!B.head) { B.head = B.tail = line_new(""); B.numlines = 1; }
    else B.tail = prev;

    B.current = B.head;
    B.cy = 0;
    B.cx = 0;
}

static void do_undo(void) {
    if (undo_stack.count <= 0) {
        snprintf(status_msg, sizeof(status_msg), "لا يوجد شيء للتراجع");
        return;
    }

    UndoState *s = &undo_stack.stack[undo_stack.top];
    buffer_deserialize(s->text);
    B.cx = s->cx;
    B.cy = s->cy;

    Line *l = B.head;
    for (int i = 0; i < s->cy && l; i++) l = l->next;
    if (l) {
        B.current = l;
        B.cx = s->cx;
        int len = strlen(l->data);
        if (B.cx > len) B.cx = len;
    }

    free(s->text);
    s->text = NULL;
    undo_stack.top = (undo_stack.top - 1 + UNDO_MAX) % UNDO_MAX;
    undo_stack.count--;
    B.modified = true;
    snprintf(status_msg, sizeof(status_msg), "تم التراجع");
}

static void do_redo(void) {
    if (undo_stack.count >= UNDO_MAX) {
        snprintf(status_msg, sizeof(status_msg), "لا يوجد شيء للإعادة");
        return;
    }

    int next = (undo_stack.top + 1) % UNDO_MAX;
    if (undo_stack.stack[next].text == NULL) {
        snprintf(status_msg, sizeof(status_msg), "لا يوجد شيء للإعادة");
        return;
    }

    UndoState *s = &undo_stack.stack[next];
    buffer_deserialize(s->text);
    B.cx = s->cx;
    B.cy = s->cy;

    Line *l = B.head;
    for (int i = 0; i < s->cy && l; i++) l = l->next;
    if (l) {
        B.current = l;
        B.cx = s->cx;
        int len = strlen(l->data);
        if (B.cx > len) B.cx = len;
    }

    undo_stack.top = next;
    undo_stack.count++;
    B.modified = true;
    snprintf(status_msg, sizeof(status_msg), "تم الإعادة");
}

static void buffer_insert_char(int c) {
    if (c == '\n' || c == '\r') {
        Line *l = B.current;
        char *right = xstrdup(l->data + B.cx);
        l->data[B.cx] = '\0';

        Line *nl = line_new(right);
        free(right);

        nl->next = l->next;
        nl->prev = l;
        if (l->next) l->next->prev = nl;
        else B.tail = nl;
        l->next = nl;

        B.current = nl;
        B.cx = 0;
        B.cy++;
        B.numlines++;
        B.modified = true;
        undo_push();
        return;
    }
    if (c < 32 || c == 127) return;

    Line *l = B.current;
    int len = strlen(l->data);

    l->data = xrealloc(l->data, len + 2);
    memmove(l->data + B.cx + 1, l->data + B.cx, len - B.cx + 1);
    l->data[B.cx] = (char)c;
    B.cx++;
    B.modified = true;
}

static void buffer_insert_string(const char *s) {
    undo_push();
    while (*s) {
        buffer_insert_char((unsigned char)*s);
        s++;
    }
}

static void buffer_delete_char(void) {
    Line *l = B.current;
    int len = strlen(l->data);

    if (B.cx < len) {
        undo_push();
        memmove(l->data + B.cx, l->data + B.cx + 1, len - B.cx);
        B.modified = true;
    } else if (l->next) {
        undo_push();
        Line *n = l->next;
        int nlen = strlen(n->data);
        l->data = xrealloc(l->data, len + nlen + 1);
        strcpy(l->data + len, n->data);
        l->next = n->next;
        if (n->next) n->next->prev = l;
        else B.tail = l;
        free(n->data);
        free(n);
        B.numlines--;
        B.modified = true;
    }
}

static void buffer_backspace(void) {
    Line *l = B.current;

    if (B.cx > 0) {
        undo_push();
        memmove(l->data + B.cx - 1, l->data + B.cx,
                strlen(l->data) - B.cx + 1);
        B.cx--;
        B.modified = true;
    } else if (l->prev) {
        undo_push();
        Line *p = l->prev;
        int plen = strlen(p->data);
        B.cx = plen;
        p->data = xrealloc(p->data, plen + strlen(l->data) + 1);
        strcpy(p->data + plen, l->data);
        p->next = l->next;
        if (l->next) l->next->prev = p;
        else B.tail = p;
        free(l->data);
        free(l);
        B.current = p;
        B.cy--;
        B.numlines--;
        B.modified = true;
    }
}

static void buffer_load(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        snprintf(B.filename, sizeof(B.filename), "%s", filename);
        return;
    }

    Line *l = B.head;
    while (l) {
        Line *n = l->next;
        free(l->data);
        free(l);
        l = n;
    }

    char line[MAX_LINE];
    Line *prev = NULL;
    B.numlines = 0;

    while (fgets(line, sizeof(line), fp)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        Line *nl = line_new(line);
        if (prev) { prev->next = nl; nl->prev = prev; }
        else B.head = nl;
        prev = nl;
        B.numlines++;
    }

    if (!prev) { B.head = line_new(""); B.numlines = 1; }
    else B.tail = prev;

    B.current = B.head;
    B.cx = B.cy = 0;
    B.rowoff = B.coloff = 0;
    B.modified = false;
    snprintf(B.filename, sizeof(B.filename), "%s", filename);

    fclose(fp);
}

static void buffer_save(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        snprintf(status_msg, sizeof(status_msg), "خطأ: لا يمكن الحفظ في %s", filename);
        return;
    }

    for (Line *l = B.head; l; l = l->next) {
        fputs(l->data, fp);
        if (l->next) fputc('\n', fp);
    }

    fclose(fp);
    B.modified = false;
    snprintf(B.filename, sizeof(B.filename), "%s", filename);
    snprintf(status_msg, sizeof(status_msg),
             "✓ تم الحفظ: %s (%d سطر)", filename, B.numlines);
}

static bool spell_init(void) {
    FILE *fp = popen("command -v hunspell 2>/dev/null", "r");
    if (fp) {
        char buf[256];
        if (fgets(buf, sizeof(buf), fp)) hunspell_available = true;
        pclose(fp);
    }

    const char *paths[] = {
        "/usr/share/dict/words",
        "/usr/share/dict/american-english",
        "/data/data/com.termux/files/usr/share/dict/words",
        "/data/data/com.termux/files/usr/share/hunspell/en_US.dic",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        FILE *dict = fopen(paths[i], "r");
        if (!dict) continue;

        dict_cap = 1024;
        dictionary = xmalloc(dict_cap * sizeof(char *));

        char line[256];
        while (fgets(line, sizeof(line), dict)) {
            int len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (len == 0) continue;

            char *slash = strchr(line, '/');
            if (slash) *slash = '\0';

            if (dict_count >= dict_cap) {
                dict_cap *= 2;
                dictionary = xrealloc(dictionary, dict_cap * sizeof(char *));
            }
            dictionary[dict_count++] = xstrdup(line);
        }
        fclose(dict);
        break;
    }

    spell_ready = (dict_count > 0) || hunspell_available;
    return spell_ready;
}

static void spell_free(void) {
    for (size_t i = 0; i < dict_count; i++) free(dictionary[i]);
    free(dictionary);
    dictionary = NULL;
    dict_count = dict_cap = 0;
}

static bool spell_check_word(const char *word) {
    if (strlen(word) < 2) return true;
    for (size_t i = 0; i < dict_count; i++)
        if (strcasecmp(dictionary[i], word) == 0) return true;
    return false;
}

static char *spell_suggest(const char *word) {
    static char result[512];
    result[0] = '\0';

    if (!hunspell_available) return result;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "echo '%s' | hunspell -a 2>/dev/null | sed -n '2p'", word);

    FILE *fp = popen(cmd, "r");
    if (!fp) return result;

    char buf[512];
    if (fgets(buf, sizeof(buf), fp)) {
        char *colon = strchr(buf, ':');
        if (colon) {
            strncpy(result, colon + 2, sizeof(result) - 1);
            int len = strlen(result);
            while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r'))
                result[--len] = '\0';
        }
    }
    pclose(fp);
    return result;
}

static void do_spell_check(void) {
    if (!spell_ready) {
        snprintf(status_msg, sizeof(status_msg),
                 "التصحيح غير متاح — ثبّت: pkg install hunspell hunspell-en");
        return;
    }

    int errors = 0;
    char *text = buffer_serialize();
    char *p = text;
    char word[128];
    int wi = 0;
    char first_word[128] = "";
    char first_sugg[256] = "";

    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (isalpha(c) || c == '\'') {
            if (wi < 127) word[wi++] = *p;
        } else {
            if (wi > 0) {
                word[wi] = '\0';
                if (!spell_check_word(word)) {
                    errors++;
                    if (errors == 1) {
                        strncpy(first_word, word, sizeof(first_word) - 1);
                        char *s = spell_suggest(word);
                        if (s[0]) strncpy(first_sugg, s, sizeof(first_sugg) - 1);
                    }
                }
                wi = 0;
            }
        }
        p++;
    }

    if (errors == 0) {
        snprintf(status_msg, sizeof(status_msg), "✓ لا توجد أخطاء إملائية");
    } else if (first_sugg[0]) {
        snprintf(status_msg, sizeof(status_msg),
                 "أخطاء: %d | مثال: '%s' → %s",
                 errors, first_word, first_sugg);
    } else {
        snprintf(status_msg, sizeof(status_msg),
                 "أخطاء إملائية: %d | مثال: '%s'",
                 errors, first_word);
    }
    free(text);
}

static bool html_is_html_file(void) {
    const char *name = B.filename;
    const char *dot = strrchr(name, '.');
    if (dot) {
        if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm") ||
            !strcasecmp(dot, ".xhtml") || !strcasecmp(dot, ".php") ||
            !strcasecmp(dot, ".vue") || !strcasecmp(dot, ".jsx"))
            return true;
    }
    if (B.head && strstr(B.head->data, "<!DOCTYPE")) return true;
    if (B.head && strstr(B.head->data, "<html")) return true;
    return false;
}

static bool tag_is_void(const char *tag) {
    for (size_t i = 0; i < NUM_VOID_TAGS; i++)
        if (strcmp(void_tags[i], tag) == 0) return true;
    return false;
}

static bool html_complete(void) {
    if (!html_is_html_file()) return false;

    Line *l = B.current;
    int start = B.cx;

    while (start > 0 && (isalnum((unsigned char)l->data[start-1]) ||
                         l->data[start-1] == '-' ||
                         l->data[start-1] == '_'))
        start--;

    int fraglen = B.cx - start;
    if (fraglen == 0) return false;

    char frag[64];
    if (fraglen >= 63) return false;
    strncpy(frag, l->data + start, fraglen);
    frag[fraglen] = '\0';

    const char *matches[32];
    int count = 0;
    for (size_t i = 0; i < NUM_HTML_TAGS && count < 32; i++) {
        if (strncasecmp(html_tags[i], frag, fraglen) == 0)
            matches[count++] = html_tags[i];
    }

    if (count == 0) {
        snprintf(status_msg, sizeof(status_msg),
                 "لا يوجد وسم يبدأ بـ '%s'", frag);
        return true;
    }

    if (count == 1) {
        const char *tag = matches[0];
        int taglen = strlen(tag);

        undo_push();

        l->data = xrealloc(l->data, strlen(l->data) - fraglen + taglen + 1);
        memmove(l->data + start + taglen, l->data + B.cx,
                strlen(l->data + B.cx) + 1);
        memcpy(l->data + start, tag, taglen);
        B.cx = start + taglen;
        B.modified = true;

        if (!tag_is_void(tag)) {
            char close[64];
            snprintf(close, sizeof(close), "></%s>", tag);
            int clen = strlen(close);

            l->data = xrealloc(l->data, strlen(l->data) + clen + 1);
            memmove(l->data + B.cx + clen, l->data + B.cx,
                    strlen(l->data + B.cx) + 1);
            memcpy(l->data + B.cx, close, clen);

            B.cx += 1;
            snprintf(status_msg, sizeof(status_msg),
                     "<%s>…</%s> (المؤشر داخل الوسم)", tag, tag);
        } else {
            snprintf(status_msg, sizeof(status_msg), "اكتمل: <%s>", tag);
        }
        return true;
    }

    size_t common = strlen(matches[0]);
    for (int i = 1; i < count; i++) {
        size_t j = 0;
        while (j < common && matches[i][j] &&
               tolower(matches[0][j]) == tolower(matches[i][j]))
            j++;
        common = j;
    }

    if ((int)common > fraglen) {
        undo_push();
        const char *first = matches[0];
        l->data = xrealloc(l->data, strlen(l->data) - fraglen + common + 1);
        memmove(l->data + start + common, l->data + B.cx,
                strlen(l->data + B.cx) + 1);
        memcpy(l->data + start, first, common);
        B.cx = start + common;
        B.modified = true;
    }

    char list[256] = "";
    for (int i = 0; i < count && i < 6; i++) {
        if (i > 0) strcat(list, " ");
        strcat(list, matches[i]);
    }
    snprintf(status_msg, sizeof(status_msg), "خيارات: %s", list);
    return true;
}

static void html_insert_skeleton(void) {
    const char *skeleton =
        "<!DOCTYPE html>\n"
        "<html lang=\"ar\" dir=\"rtl\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>صفحة جديدة</title>\n"
        "</head>\n"
        "<body>\n"
        "    \n"
        "</body>\n"
        "</html>";

    buffer_insert_string(skeleton);
    snprintf(status_msg, sizeof(status_msg), "✓ تم إدراج هيكل HTML5");
}

static void draw_html_line(const char *data, int y, int xoff) {
    int x = -xoff;
    bool in_tag = false;
    bool in_string = false;
    char quote = 0;
    bool in_comment = false;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];

        if (x >= 0 && x < screen_cols) {
            int color = 0;

            if (!in_comment && data[i] == '<' && data[i+1] == '!' &&
                data[i+2] == '-' && data[i+3] == '-')
                in_comment = true;

            if (in_comment) {
                color = COLOR_COMMENT;
                if (c == '-' && data[i+1] == '-' && data[i+2] == '>')
                    in_comment = false;
            } else if (in_string) {
                color = COLOR_STRING;
                if (c == quote) in_string = false;
            } else if (c == '<') {
                in_tag = true;
                color = COLOR_TAG;
            } else if (c == '>' && in_tag) {
                color = COLOR_TAG;
                in_tag = false;
            } else if (in_tag) {
                if (c == '"' || c == '\'') {
                    in_string = true;
                    quote = c;
                    color = COLOR_STRING;
                } else if (isalpha(c) && (i == 0 || !isalpha((unsigned char)data[i-1]))) {
                    color = COLOR_ATTR;
                } else {
                    color = COLOR_TAG;
                }
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

static void ui_move_cursor(void);

static void ui_draw_status(void) {
    attron(A_REVERSE);
    mvhline(screen_rows - 2, 0, ' ', screen_cols);

    char info[512];
    snprintf(info, sizeof(info),
             " %s %s | %s%s | سطر %d/%d | عمود %d ",
             APP_NAME, APP_VERSION,
             B.filename[0] ? B.filename : "[بدون اسم]",
             B.modified ? " *" : "",
             B.cy + 1, B.numlines, B.cx + 1);

    mvprintw(screen_rows - 2, 0, "%.*s", screen_cols, info);
    attroff(A_REVERSE);
}

static void ui_draw_message(void) {
    attron(A_BOLD);
    mvhline(screen_rows - 1, 0, ' ', screen_cols);

    const char *keys = "^X خروج  ^O حفظ  ^W بحث  ^T تصحيح  ^G سطر  ^Z تراجع  ^Y إعادة  Tab إكمال";
    int klen = strlen(keys);

    if (status_msg[0]) {
        mvprintw(screen_rows - 1, 0, "%.*s", screen_cols, status_msg);
    } else if (klen < screen_cols) {
        mvprintw(screen_rows - 1, screen_cols - klen - 1, "%s", keys);
    }

    attroff(A_BOLD);
}

static void ui_draw(void) {
    getmaxyx(stdscr, screen_rows, screen_cols);
    erase();

    Line *l = B.head;
    for (int i = 0; i < B.rowoff && l; i++)
        l = l->next;

    int y = 0;
    bool is_html = html_is_html_file();

    while (l && y < screen_rows - 2) {
        if (is_html)
            draw_html_line(l->data, y, B.coloff);
        else {
            int len = strlen(l->data);
            int x = -B.coloff;
            for (int i = 0; i < len && x < screen_cols; i++) {
                if (x >= 0) mvaddch(y, x, l->data[i]);
                x++;
            }
        }
        l = l->next;
        y++;
    }

    ui_draw_status();
    ui_draw_message();
    ui_move_cursor();
    refresh();
}

static void ui_move_cursor(void) {
    int y = B.cy - B.rowoff;
    int x = B.cx - B.coloff;
    if (y < 0) y = 0;
    if (y >= screen_rows - 2) y = screen_rows - 3;
    if (x < 0) x = 0;
    if (x >= screen_cols) x = screen_cols - 1;
    move(y, x);
}

static void ui_scroll(void) {
    int visible = screen_rows - 2;

    if (B.cy < B.rowoff) B.rowoff = B.cy;
    if (B.cy >= B.rowoff + visible) B.rowoff = B.cy - visible + 1;

    if (B.cx < B.coloff) B.coloff = B.cx;
    if (B.cx >= B.coloff + screen_cols) B.coloff = B.cx - screen_cols + 1;
}

static void do_save(void) {
    char fname[512];

    if (B.filename[0]) {
        buffer_save(B.filename);
        return;
    }

    echo();
    curs_set(1);
    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "اسم الملف: ");
    getnstr(fname, sizeof(fname) - 1);
    noecho();
    curs_set(1);

    if (strlen(fname) > 0)
        buffer_save(fname);
}

static void do_exit(void) {
    if (B.modified) {
        move(screen_rows - 1, 0);
        clrtoeol();
        attron(A_REVERSE);
        mvprintw(screen_rows - 1, 0,
                 "الملف معدّل! ^O للحفظ، ^X مرة أخرى للخروج بدون حفظ، أي مفتاح آخر للإلغاء");
        attroff(A_REVERSE);
        refresh();

        int c = getch();
        if (c != 24) return;
    }
    running = false;
}

static void do_search(void) {
    char query[256];
    echo();
    curs_set(1);
    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "بحث: ");
    getnstr(query, sizeof(query) - 1);
    noecho();

    if (strlen(query) == 0) return;

    Line *start = B.current;
    Line *l = start;
    bool wrapped = false;
    int line_no = B.cy;
    int start_cx = B.cx;

    while (1) {
        int offset = (l == start) ? start_cx : 0;
        char *found = strstr(l->data + offset, query);
        if (found) {
            B.current = l;
            B.cx = found - l->data;
            B.cy = line_no;
            snprintf(status_msg, sizeof(status_msg), "وُجد في السطر %d", line_no + 1);
            ui_scroll();
            return;
        }
        l = l->next;
        line_no++;
        if (!l) {
            if (wrapped) break;
            l = B.head;
            line_no = 0;
            wrapped = true;
        }
        if (l == start && wrapped) break;
    }

    snprintf(status_msg, sizeof(status_msg), "لم يوجد: %s", query);
}

static void do_replace(void) {
    char find[128], repl[128];
    echo();
    curs_set(1);

    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "ابحث عن: ");
    getnstr(find, sizeof(find) - 1);

    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "استبدل بـ: ");
    getnstr(repl, sizeof(repl) - 1);
    noecho();

    if (strlen(find) == 0) return;

    int count = 0;
    int flen = strlen(find);
    int rlen = strlen(repl);

    undo_push();

    for (Line *l = B.head; l; l = l->next) {
        char *p;
        while ((p = strstr(l->data, find)) != NULL) {
            int len = strlen(l->data);
            l->data = xrealloc(l->data, len - flen + rlen + 1);
            memmove(p + rlen, p + flen, strlen(p + flen) + 1);
            memcpy(p, repl, rlen);
            count++;
        }
    }

    B.modified = (count > 0);
    snprintf(status_msg, sizeof(status_msg), "تم استبدال %d حالة", count);
}

static void do_goto_line(void) {
    char buf[32];
    echo();
    curs_set(1);
    move(screen_rows - 1, 0);
    clrtoeol();
    mvprintw(screen_rows - 1, 0, "اذهب إلى سطر: ");
    getnstr(buf, sizeof(buf) - 1);
    noecho();

    int target = atoi(buf) - 1;
    if (target < 0) target = 0;

    Line *l = B.head;
    int i = 0;
    while (l && i < target) { l = l->next; i++; }
    if (l) {
        B.current = l;
        B.cy = i;
        B.cx = 0;
        ui_scroll();
        snprintf(status_msg, sizeof(status_msg), "السطر %d", i + 1);
    }
}

static void do_help(void) {
    erase();
    attron(A_BOLD);
    mvprintw(1, 2, "=== %s %s — المساعدة ===", APP_NAME, APP_VERSION);
    attroff(A_BOLD);
    mvprintw(3, 2, "^X    خروج");
    mvprintw(4, 2, "^O    حفظ");
    mvprintw(5, 2, "^W    بحث");
    mvprintw(6, 2, "^\\    بحث واستبدال (Ctrl+\\)");
    mvprintw(7, 2, "^G    اذهب إلى سطر");
    mvprintw(8, 2, "^T    تصحيح إملائي");
    mvprintw(9, 2, "^Z    تراجع");
    mvprintw(10, 2, "^Y    إعادة");
    mvprintw(11, 2, "^K    قطع السطر");
    mvprintw(12, 2, "^U    لصق");
    mvprintw(13, 2, "^C    موقع المؤشر");
    mvprintw(14, 2, "Tab   إكمال وسم HTML (في ملفات .html)");
    mvprintw(15, 2, "F2    إدراج هيكل HTML5 كامل");
    mvprintw(17, 2, "اضغط أي مفتاح للعودة...");
    refresh();
    getch();
    flushinp();
}

static void do_cut_line(void) {
    free(clipboard);
    clipboard = xstrdup(B.current->data);
    undo_push();
    B.current->data[0] = '\0';
    B.cx = 0;
    B.modified = true;
    snprintf(status_msg, sizeof(status_msg), "✂ تم قطع السطر");
}

static void do_paste(void) {
    if (!clipboard) {
        snprintf(status_msg, sizeof(status_msg), "لا يوجد شيء للصق");
        return;
    }
    buffer_insert_string(clipboard);
    snprintf(status_msg, sizeof(status_msg), "✓ تم اللصق");
}

static void do_cursor_info(void) {
    snprintf(status_msg, sizeof(status_msg),
             "السطر %d، العمود %d، المجموع %d سطر",
             B.cy + 1, B.cx + 1, B.numlines);
}

static void process_key(int c) {
    switch (c) {
        case 24: do_exit(); break;
        case 15: do_save(); break;
        case 23: do_search(); break;
        case 28: do_replace(); break;
        case 7:  do_goto_line(); break;
        case 20: do_spell_check(); break;
        case 26: do_undo(); break;
        case 25: do_redo(); break;
        case 11: do_cut_line(); break;
        case 21: do_paste(); break;
        case 3:  do_cursor_info(); break;
        case '\t':
            if (!html_complete())
                for (int i = 0; i < TAB_SIZE; i++)
                    buffer_insert_char(' ');
            break;
        case KEY_F(1): do_help(); break;
        case KEY_F(2): html_insert_skeleton(); break;
        case KEY_UP:
            if (B.current->prev) {
                B.current = B.current->prev;
                B.cy--;
                int len = strlen(B.current->data);
                if (B.cx > len) B.cx = len;
            }
            break;
        case KEY_DOWN:
            if (B.current->next) {
                B.current = B.current->next;
                B.cy++;
                int len = strlen(B.current->data);
                if (B.cx > len) B.cx = len;
            }
            break;
        case KEY_LEFT:
            if (B.cx > 0) B.cx--;
            else if (B.current->prev) {
                B.current = B.current->prev;
                B.cy--;
                B.cx = strlen(B.current->data);
            }
            break;
        case KEY_RIGHT:
            if (B.cx < (int)strlen(B.current->data)) B.cx++;
            else if (B.current->next) {
                B.current = B.current->next;
                B.cy++;
                B.cx = 0;
            }
            break;
        case KEY_HOME: B.cx = 0; break;
        case KEY_END: B.cx = strlen(B.current->data); break;
        case KEY_PPAGE:
            for (int i = 0; i < screen_rows - 2 && B.current->prev; i++) {
                B.current = B.current->prev;
                B.cy--;
            }
            break;
        case KEY_NPAGE:
            for (int i = 0; i < screen_rows - 2 && B.current->next; i++) {
                B.current = B.current->next;
                B.cy++;
            }
            break;
        case KEY_BACKSPACE:
        case 127:
        case 8:
            buffer_backspace();
            break;
        case KEY_DC:
            buffer_delete_char();
            break;
        case KEY_RESIZE:
            getmaxyx(stdscr, screen_rows, screen_cols);
            break;
        default:
            if (c >= 32 && c < 127)
                buffer_insert_char(c);
            break;
    }

    ui_scroll();
}

static void handle_sigint(int sig) {
    (void)sig;
    running = false;
}

static void init_colors(void) {
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(COLOR_TAG,    COLOR_CYAN,   -1);
        init_pair(COLOR_ATTR,   COLOR_YELLOW, -1);
        init_pair(COLOR_STRING, COLOR_GREEN,  -1);
        init_pair(COLOR_COMMENT,COLOR_BLUE,   -1);
        init_pair(COLOR_MATCH,  COLOR_BLACK,  COLOR_YELLOW);
    }
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    signal(SIGINT, handle_sigint);

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    init_colors();
    getmaxyx(stdscr, screen_rows, screen_cols);

    buffer_init();
    undo_stack.top = -1;
    undo_stack.count = 0;
    memset(undo_stack.stack, 0, sizeof(undo_stack.stack));

    spell_init();

    if (argc > 1) {
        buffer_load(argv[1]);
    } else {
        B.filename[0] = '\0';
    }

    snprintf(status_msg, sizeof(status_msg),
             "%s %s — F1 للمساعدة", APP_NAME, APP_VERSION);

    while (running) {
        ui_draw();
        int c = getch();
        status_msg[0] = '\0';
        process_key(c);
    }

    endwin();
    buffer_free();
    spell_free();
    free(clipboard);

    return 0;
}
