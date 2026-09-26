/**************************************************************************
 *  syntax.c — تلوين متعدد اللغات لـ nano-ar                             *
 *  الرخصة: MIT                                                          *
 **************************************************************************/

#include "nano_ar.h"

/* ============================================================ */
/*                    أنواع اللغات                              */
/* ============================================================ */

typedef enum {
    LANG_NONE = 0,
    LANG_HTML,
    LANG_CSS,
    LANG_JS,
    LANG_PYTHON,
    LANG_C,
    LANG_MARKDOWN,
    LANG_JSON,
    LANG_XML
} Language;

/* ============================================================ */
/*                    كشف اللغة                                */
/* ============================================================ */

Language detect_language(const char *filename) {
    if (!filename || !filename[0]) return LANG_NONE;

    const char *dot = strrchr(filename, '.');
    if (!dot) return LANG_NONE;

    /* HTML */
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm") ||
        !strcasecmp(dot, ".xhtml") || !strcasecmp(dot, ".vue"))
        return LANG_HTML;

    /* CSS */
    if (!strcasecmp(dot, ".css") || !strcasecmp(dot, ".scss") ||
        !strcasecmp(dot, ".sass") || !strcasecmp(dot, ".less"))
        return LANG_CSS;

    /* JavaScript */
    if (!strcasecmp(dot, ".js") || !strcasecmp(dot, ".jsx") ||
        !strcasecmp(dot, ".ts") || !strcasecmp(dot, ".tsx") ||
        !strcasecmp(dot, ".mjs"))
        return LANG_JS;

    /* Python */
    if (!strcasecmp(dot, ".py") || !strcasecmp(dot, ".pyw"))
        return LANG_PYTHON;

    /* C/C++ */
    if (!strcasecmp(dot, ".c") || !strcasecmp(dot, ".h") ||
        !strcasecmp(dot, ".cpp") || !strcasecmp(dot, ".hpp") ||
        !strcasecmp(dot, ".cc"))
        return LANG_C;

    /* Markdown */
    if (!strcasecmp(dot, ".md") || !strcasecmp(dot, ".markdown"))
        return LANG_MARKDOWN;

    /* JSON */
    if (!strcasecmp(dot, ".json"))
        return LANG_JSON;

    /* XML */
    if (!strcasecmp(dot, ".xml") || !strcasecmp(dot, ".svg"))
        return LANG_XML;

    return LANG_NONE;
}

/* ============================================================ */
/*                    رسم HTML                                  */
/* ============================================================ */

void draw_html(const char *data, int y, int xoff) {
    int x = -xoff;
    bool in_tag = false, in_string = false, in_comment = false;
    char quote = 0;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            if (!in_comment && data[i]=='<' && data[i+1]=='!' &&
                data[i+2]=='-' && data[i+3]=='-')
                in_comment = true;

            if (in_comment) {
                color = C_COMMENT;
                if (c=='-' && data[i+1]=='-' && data[i+2]=='>')
                    in_comment = false;
            } else if (in_string) {
                color = C_STRING;
                if (c == quote) in_string = false;
            } else if (c == '<') {
                in_tag = true;
                color = C_TAG;
            } else if (c == '>' && in_tag) {
                color = C_TAG;
                in_tag = false;
            } else if (in_tag) {
                if (c=='"' || c=='\'') {
                    in_string = true;
                    quote = c;
                    color = C_STRING;
                } else if (isalpha(c) && (i==0 || !isalpha((unsigned char)data[i-1]))) {
                    color = C_ATTR;
                } else {
                    color = C_TAG;
                }
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    رسم CSS                                   */
/* ============================================================ */

void draw_css(const char *data, int y, int xoff) {
    int x = -xoff;
    bool in_comment = false;
    bool in_string = false;
    char quote = 0;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            if (!in_comment && c=='/' && data[i+1]=='*') {
                in_comment = true;
                color = C_COMMENT;
            } else if (in_comment) {
                color = C_COMMENT;
                if (c=='*' && data[i+1]=='/') in_comment = false;
            } else if (in_string) {
                color = C_STRING;
                if (c == quote) in_string = false;
            } else if (c=='"' || c=='\'') {
                in_string = true;
                quote = c;
                color = C_STRING;
            } else if (c == '{' || c == '}') {
                color = C_ATTR;
            } else if (c == ':' || c == ';') {
                color = C_TAG;
            } else if (c == '#') {
                color = C_TAG;
            } else if (isalpha(c) && (i==0 || !isalnum((unsigned char)data[i-1]))) {
                /* اسم الخاصية */
                color = C_ATTR;
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    رسم JavaScript                            */
/* ============================================================ */

void draw_js(const char *data, int y, int xoff) {
    static const char *keywords[] = {
        "var","let","const","function","return","if","else","for",
        "while","do","switch","case","break","continue","new","this",
        "true","false","null","undefined","typeof","instanceof",
        "try","catch","finally","throw","class","extends","super",
        "import","export","from","default","async","await","yield",
        "of","in","delete","void","with","debugger",NULL
    };

    int x = -xoff;
    bool in_string = false, in_comment = false, in_line_comment = false;
    char quote = 0;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            if (!in_comment && !in_line_comment &&
                c=='/' && data[i+1]=='*') {
                in_comment = true;
                color = C_COMMENT;
            } else if (in_comment) {
                color = C_COMMENT;
                if (c=='*' && data[i+1]=='/') in_comment = false;
            } else if (!in_line_comment && c=='/' && data[i+1]=='/') {
                in_line_comment = true;
                color = C_COMMENT;
            } else if (in_line_comment) {
                color = C_COMMENT;
            } else if (in_string) {
                color = C_STRING;
                if (c == quote && data[i-1] != '\\') in_string = false;
            } else if (c=='"' || c=='\'' || c=='`') {
                in_string = true;
                quote = c;
                color = C_STRING;
            } else if (isdigit(c)) {
                color = C_LINE_NUM;
            } else if (isalpha(c) && (i==0 || !isalnum((unsigned char)data[i-1]))) {
                /* فحص كلمة مفتاحية */
                int word_len = 0;
                while (isalnum((unsigned char)data[i+word_len]) ||
                       data[i+word_len] == '_') word_len++;

                char word[32];
                if (word_len < 32) {
                    strncpy(word, data+i, word_len);
                    word[word_len] = '\0';

                    for (int k = 0; keywords[k]; k++) {
                        if (strcmp(word, keywords[k]) == 0) {
                            color = C_TAG;
                            break;
                        }
                    }
                }
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    رسم Python                                */
/* ============================================================ */

void draw_python(const char *data, int y, int xoff) {
    static const char *keywords[] = {
        "def","class","return","if","elif","else","for","while",
        "break","continue","pass","import","from","as","try",
        "except","finally","raise","with","lambda","yield","global",
        "nonlocal","assert","del","in","is","not","and","or",
        "True","False","None","async","await",NULL
    };

    int x = -xoff;
    bool in_string = false, in_comment = false;
    char quote = 0;
    int triple = 0;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            /* تعليق */
            if (!in_string && c == '#') {
                in_comment = true;
                color = C_COMMENT;
            } else if (in_comment) {
                color = C_COMMENT;
            } else if (in_string) {
                color = C_STRING;
                if (c == quote) {
                    if (triple == 3 &&
                        data[i+1]==quote && data[i+2]==quote) {
                        triple = 0;
                        in_string = false;
                    } else if (triple == 1) {
                        in_string = false;
                    }
                }
            } else if (c=='"' || c=='\'') {
                /* فحص ثلاث علامات */
                if (data[i+1]==c && data[i+2]==c) {
                    triple = 3;
                    in_string = true;
                    quote = c;
                    color = C_STRING;
                } else {
                    triple = 1;
                    in_string = true;
                    quote = c;
                    color = C_STRING;
                }
            } else if (isdigit(c)) {
                color = C_LINE_NUM;
            } else if (isalpha(c) && (i==0 || !isalnum((unsigned char)data[i-1]))) {
                int word_len = 0;
                while (isalnum((unsigned char)data[i+word_len]) ||
                       data[i+word_len] == '_') word_len++;

                char word[32];
                if (word_len < 32) {
                    strncpy(word, data+i, word_len);
                    word[word_len] = '\0';

                    for (int k = 0; keywords[k]; k++) {
                        if (strcmp(word, keywords[k]) == 0) {
                            color = C_TAG;
                            break;
                        }
                    }
                }
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    رسم C/C++                                 */
/* ============================================================ */

void draw_c(const char *data, int y, int xoff) {
    static const char *keywords[] = {
        "int","char","float","double","void","long","short","unsigned",
        "signed","const","static","extern","volatile","register",
        "if","else","for","while","do","switch","case","break",
        "continue","return","goto","struct","union","enum","typedef",
        "sizeof","include","define","ifdef","ifndef","endif",
        "class","public","private","protected","virtual","template",
        "namespace","using","new","delete","this","try","catch",
        "throw","bool","true","false",NULL
    };

    int x = -xoff;
    bool in_string = false, in_char = false;
    bool in_comment = false, in_line_comment = false;
    char quote = 0;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            if (!in_comment && !in_line_comment && !in_string && !in_char &&
                c=='/' && data[i+1]=='*') {
                in_comment = true;
                color = C_COMMENT;
            } else if (in_comment) {
                color = C_COMMENT;
                if (c=='*' && data[i+1]=='/') in_comment = false;
            } else if (!in_line_comment && !in_string && !in_char &&
                       c=='/' && data[i+1]=='/') {
                in_line_comment = true;
                color = C_COMMENT;
            } else if (in_line_comment) {
                color = C_COMMENT;
            } else if (in_string || in_char) {
                color = C_STRING;
                if (c == quote && data[i-1] != '\\')
                    in_string = in_char = false;
            } else if (c == '"') {
                in_string = true;
                quote = c;
                color = C_STRING;
            } else if (c == '\'') {
                in_char = true;
                quote = c;
                color = C_STRING;
            } else if (c == '#') {
                color = C_COMMENT;
                in_line_comment = true;
            } else if (isdigit(c)) {
                color = C_LINE_NUM;
            } else if (isalpha(c) && (i==0 || !isalnum((unsigned char)data[i-1]))) {
                int word_len = 0;
                while (isalnum((unsigned char)data[i+word_len]) ||
                       data[i+word_len] == '_') word_len++;

                char word[32];
                if (word_len < 32) {
                    strncpy(word, data+i, word_len);
                    word[word_len] = '\0';

                    for (int k = 0; keywords[k]; k++) {
                        if (strcmp(word, keywords[k]) == 0) {
                            color = C_TAG;
                            break;
                        }
                    }
                }
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    رسم Markdown                              */
/* ============================================================ */

void draw_markdown(const char *data, int y, int xoff) {
    int x = -xoff;
    bool in_code = false;

    /* عنوان؟ */
    bool is_header = false;
    int header_level = 0;
    for (int i = 0; data[i] == '#' && i < 6; i++) {
        header_level++;
        is_header = true;
    }

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            if (c == '`') {
                in_code = !in_code;
                color = C_STRING;
            } else if (in_code) {
                color = C_STRING;
            } else if (is_header) {
                color = C_TAG | A_BOLD;
            } else if (c == '*' || c == '_') {
                color = C_ATTR;
            } else if (c == '[' || c == ']' || c == '(' || c == ')') {
                color = C_LINE_NUM;
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    رسم JSON                                  */
/* ============================================================ */

void draw_json(const char *data, int y, int xoff) {
    int x = -xoff;
    bool in_string = false;
    bool is_key = false;
    bool in_escape = false;

    for (int i = 0; data[i]; i++) {
        unsigned char c = data[i];
        if (x >= 0 && x < COLS) {
            int color = 0;

            if (in_escape) {
                in_escape = false;
                color = C_STRING;
            } else if (in_string) {
                if (c == '\\') {
                    in_escape = true;
                    color = C_STRING;
                } else if (c == '"') {
                    in_string = false;
                    /* فحص: هل التالي ':' ؟ */
                    int j = i + 1;
                    while (data[j] == ' ' || data[j] == '\t') j++;
                    color = (data[j] == ':') ? C_ATTR : C_STRING;
                } else {
                    color = is_key ? C_ATTR : C_STRING;
                }
            } else if (c == '"') {
                in_string = true;
                /* فحص إذا كان مفتاحًا */
                int j = i + 1;
                is_key = false;
                while (data[j] && data[j] != '"') j++;
                if (data[j] == '"') {
                    j++;
                    while (data[j] == ' ' || data[j] == '\t') j++;
                    if (data[j] == ':') is_key = true;
                }
                color = is_key ? C_ATTR : C_STRING;
            } else if (isdigit(c) || c == '-') {
                color = C_LINE_NUM;
            } else if (c=='{' || c=='}' || c=='[' || c==']' || c==':' || c==',') {
                color = C_TAG;
            } else if (c=='t' || c=='f' || c=='n') {
                /* true, false, null */
                color = C_TAG;
            }

            if (color) attron(COLOR_PAIR(color));
            mvaddch(y, x, c);
            if (color) attroff(COLOR_PAIR(color));
        }
        x++;
    }
}

/* ============================================================ */
/*                    دالة رئيسية للتلوين                       */
/* ============================================================ */

void draw_syntax_line(const char *data, int y, int xoff,
                      const char *filename) {
    Language lang = detect_language(filename);

    switch (lang) {
        case LANG_HTML:     draw_html(data, y, xoff); break;
        case LANG_CSS:      draw_css(data, y, xoff); break;
        case LANG_JS:       draw_js(data, y, xoff); break;
        case LANG_PYTHON:   draw_python(data, y, xoff); break;
        case LANG_C:        draw_c(data, y, xoff); break;
        case LANG_MARKDOWN: draw_markdown(data, y, xoff); break;
        case LANG_JSON:     draw_json(data, y, xoff); break;
        case LANG_XML:      draw_html(data, y, xoff); break;
        default:
            /* نص عادي */
            for (int i = 0; data[i] && i + xoff < COLS; i++)
                if (i >= xoff) mvaddch(y, i - xoff, data[i]);
            break;
    }
}
