#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  nano-ar — سكريبت التحديث التلقائي
#  الاستخدام: ./update.sh  أو  nano-ar-update
# ============================================================

set -e

APP_NAME="nano-ar"
REPO_URL="https://github.com/ibrahimkhacef5-del/nano_ar.git"
BRANCH="main"

# الألوان
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'
BOLD='\033[1m'

# ============================================================
#  دوال مساعدة
# ============================================================

print_banner() {
    echo -e "${CYAN}${BOLD}"
    echo "╔══════════════════════════════════════════════╗"
    echo "║                                              ║"
    echo "║        🔄 nano-ar — التحديث التلقائي        ║"
    echo "║                                              ║"
    echo "╚══════════════════════════════════════════════╝"
    echo -e "${NC}"
}

info()  { echo -e "${BLUE}[ℹ]${NC} $1"; }
ok()    { echo -e "${GREEN}[✓]${NC} $1"; }
warn()  { echo -e "${YELLOW}[⚠]${NC} $1"; }
error() { echo -e "${RED}[✗]${NC} $1"; }

# ============================================================
#  كشف البيئة
# ============================================================

detect_env() {
    if [ -d "/data/data/com.termux/files/usr" ]; then
        ENV="termux"
        PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
    else
        ENV="linux"
        PREFIX="${PREFIX:-/usr/local}"
    fi
    info "البيئة: $ENV"
    info "PREFIX: $PREFIX"
}

# ============================================================
#  التحقق من git
# ============================================================

check_git() {
    if ! command -v git >/dev/null 2>&1; then
        warn "git غير مثبّت، جاري التثبيت..."
        if [ "$ENV" = "termux" ]; then
            pkg install -y git
        elif command -v apt >/dev/null 2>&1; then
            sudo apt install -y git
        elif command -v dnf >/dev/null 2>&1; then
            sudo dnf install -y git
        fi
    fi
    ok "git متاح: $(git --version)"
}

# ============================================================
#  تحديث المشروع
# ============================================================

update_repo() {
    info "جاري التحديث من GitHub..."
    echo

    # لو إحنا داخل مستودع git
    if [ -d ".git" ]; then
        info "المستودع موجود، جاري السحب (git pull)..."

        # احفظ التعديلات المحلية مؤقتًا
        if ! git diff --quiet 2>/dev/null; then
            warn "يوجد تعديلات محلية، جاري حفظها مؤقتًا..."
            git stash push -m "auto-stash-$(date +%s)" 2>/dev/null || true
            STASHED=true
        fi

        # اسحب التحديثات
        git fetch origin "$BRANCH" 2>/dev/null || {
            error "فشل الاتصال بـ GitHub"
            exit 1
        }

        LOCAL=$(git rev-parse HEAD)
        REMOTE=$(git rev-parse "origin/$BRANCH")

        if [ "$LOCAL" = "$REMOTE" ]; then
            ok "أنت على أحدث إصدار بالفعل"
            if [ "$STASHED" = true ]; then
                git stash pop 2>/dev/null || true
            fi
            return 0
        fi

        info "تحديثات جديدة متوفرة، جاري السحب..."
        git pull origin "$BRANCH" || {
            error "فشل git pull"
            exit 1
        }

        # استعد التعديلات المحلية
        if [ "$STASHED" = true ]; then
            info "استعادة التعديلات المحلية..."
            git stash pop 2>/dev/null || warn "تعارض في التعديلات المحلية"
        fi

        ok "تم التحديث بنجاح"

    else
        # لو مش مستودع git → استنسخ من جديد
        warn "ليس مستودع git، جاري الاستنساخ..."

        TEMP_DIR="/tmp/nano_ar_update_$$"
        git clone "$REPO_URL" "$TEMP_DIR" || {
            error "فشل استنساخ المستودع"
            exit 1
        }

        # انسخ الملفات (احتفظ بالـ .git)
        cp -f "$TEMP_DIR"/*.c . 2>/dev/null || true
        cp -f "$TEMP_DIR"/*.h . 2>/dev/null || true
        cp -f "$TEMP_DIR"/Makefile . 2>/dev/null || true
        cp -f "$TEMP_DIR"/install.sh . 2>/dev/null || true
        cp -f "$TEMP_DIR"/update.sh . 2>/dev/null || true
        cp -f "$TEMP_DIR"/README.md . 2>/dev/null || true
        cp -f "$TEMP_DIR"/LICENSE . 2>/dev/null || true
        cp -f "$TEMP_DIR"/.gitignore . 2>/dev/null || true

        rm -rf "$TEMP_DIR"
        ok "تم النسخ من GitHub"
    fi
}

# ============================================================
#  تثبيت الاعتماديات
# ============================================================

install_deps() {
    info "التحقق من الاعتماديات..."

    MISSING=""

    command -v clang >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1 || MISSING="$MISSING clang"
    command -v make  >/dev/null 2>&1 || MISSING="$MISSING make"

    if [ "$ENV" = "termux" ]; then
        # تحقق من ncurses
        if [ ! -f "$PREFIX/include/ncurses.h" ] && [ ! -f "$PREFIX/include/ncurses/ncurses.h" ]; then
            MISSING="$MISSING ncurses-dev"
        fi
    fi

    if [ -n "$MISSING" ]; then
        warn "اعتماديات ناقصة:$MISSING"
        info "جاري التثبيت..."

        if [ "$ENV" = "termux" ]; then
            pkg install -y clang make ncurses ncurses-dev
        elif command -v apt >/dev/null 2>&1; then
            sudo apt update -qq
            sudo apt install -y build-essential libncurses-dev
        elif command -v dnf >/dev/null 2>&1; then
            sudo dnf install -y gcc make ncurses-devel
        elif command -v pacman >/dev/null 2>&1; then
            sudo pacman -S --noconfirm base-devel ncurses
        fi
    fi

    # التصحيح (اختياري)
    if ! command -v hunspell >/dev/null 2>&1 && ! command -v aspell >/dev/null 2>&1; then
        warn "لا يوجد مدقق إملائي (hunspell/aspell) — التصحيح معطّل"
        info "لتفعيله: pkg install hunspell hunspell-en"
    else
        ok "المدقق الإملائي متاح"
    fi

    ok "جميع الاعتماديات جاهزة"
}

# ============================================================
#  البناء
# ============================================================

build_app() {
    info "جاري البناء..."

    # نظّف أولاً
    if [ -f "Makefile" ]; then
        make clean 2>/dev/null || true
        make 2>&1 || {
            error "فشل البناء"
            info "حاول يدويًا: make"
            exit 1
        }
    else
        # بناء مباشر بدون Makefile
        CC="${CC:-clang}"
        command -v $CC >/dev/null 2>&1 || CC="gcc"
        command -v $CC >/dev/null 2>&1 || CC="cc"

        $CC -Wall -O2 -std=c11 -o "$APP_NAME" nano_ar.c -lncurses || {
            error "فشل البناء"
            exit 1
        }
    fi

    if [ ! -f "$APP_NAME" ]; then
        error "لم يتم إنتاج الملف التنفيذي"
        exit 1
    fi

    ok "تم البناء بنجاح"
}

# ============================================================
#  التثبيت
# ============================================================

install_app() {
    info "جاري التثبيت في $PREFIX/bin..."

    mkdir -p "$PREFIX/bin"
    cp -f "$APP_NAME" "$PREFIX/bin/$APP_NAME"
    chmod +x "$PREFIX/bin/$APP_NAME"

    # ثبّت update.sh أيضًا
    if [ -f "update.sh" ]; then
        cp -f "update.sh" "$PREFIX/bin/nano-ar-update"
        chmod +x "$PREFIX/bin/nano-ar-update"
    fi

    ok "تم التثبيت: $PREFIX/bin/$APP_NAME"
    ok "أمر التحديث: nano-ar-update"
}

# ============================================================
#  التحقق النهائي
# ============================================================

verify() {
    info "التحقق..."

    if command -v "$APP_NAME" >/dev/null 2>&1; then
        ok "$APP_NAME جاهز للتشغيل"
    else
        warn "لم يُضف إلى PATH — استخدم: $PREFIX/bin/$APP_NAME"
    fi

    # اعرض الإصدار
    if [ -f "nano_ar.c" ]; then
        VERSION=$(grep '#define APP_VERSION' nano_ar.c | head -1 | sed 's/.*"\(.*\)".*/\1/')
        [ -n "$VERSION" ] && info "الإصدار: $VERSION"
    fi
}

# ============================================================
#  البرنامج الرئيسي
# ============================================================

main() {
    clear
    print_banner

    START_TIME=$(date +%s)

    detect_env
    echo

    check_git
    echo

    update_repo
    echo

    install_deps
    echo

    build_app
    echo

    install_app
    echo

    verify
    echo

    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))

    echo -e "${GREEN}${BOLD}"
    echo "╔══════════════════════════════════════════════╗"
    echo "║                                              ║"
    echo "║          ✅ اكتمل التحديث بنجاح!             ║"
    echo "║                                              ║"
    echo "╚══════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo -e "${CYAN}الوقت: ${DURATION} ثانية${NC}"
    echo
    echo -e "${BOLD}للتشغيل:${NC}"
    echo -e "  ${GREEN}$APP_NAME ملف.txt${NC}"
    echo -e "  ${GREEN}$APP_NAME صفحة.html${NC}"
    echo
    echo -e "${BOLD}للتحديث مستقبلًا:${NC}"
    echo -e "  ${GREEN}nano-ar-update${NC}"
    echo
}

main "$@"
