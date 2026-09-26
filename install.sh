#!/data/data/com.termux/files/usr/bin/bash
# سكريبت تثبيت nano-ar على Termux و Linux

set -e

APP="nano-ar"
SRC="nano_ar.c"

# كشف البيئة
if [ -d "/data/data/com.termux/files/usr" ]; then
    ENV="termux"
    PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
else
    ENV="linux"
    PREFIX="${PREFIX:-/usr/local}"
fi

echo "╔════════════════════════════════════════╗"
echo "║        تثبيت $APP                    ║"
echo "║        البيئة: $ENV                    "
echo "╚════════════════════════════════════════╝"
echo

# ===== 1. تثبيت الاعتماديات =====
echo "[1/4] تثبيت الاعتماديات..."

if [ "$ENV" = "termux" ]; then
    pkg install -y clang make ncurses ncurses-dev 2>/dev/null || true
    pkg install -y hunspell hunspell-en 2>/dev/null || true
    pkg install -y aspell aspell-en 2>/dev/null || true
elif command -v apt >/dev/null 2>&1; then
    sudo apt update -qq
    sudo apt install -y build-essential libncurses-dev hunspell hunspell-en-us wamerican 2>/dev/null || true
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y gcc make ncurses-devel hunspell hunspell-en 2>/dev/null || true
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --noconfirm base-devel ncurses hunspell hunspell-en 2>/dev/null || true
fi
echo "✓ الاعتماديات جاهزة"
echo

# ===== 2. البناء =====
echo "[2/4] بناء $APP..."

CC="${CC:-clang}"
command -v $CC >/dev/null 2>&1 || CC="gcc"
command -v $CC >/dev/null 2>&1 || CC="cc"

$CC -Wall -Wextra -O2 -std=c11 -o "$APP" "$SRC" -lncurses

if [ ! -f "$APP" ]; then
    echo "✗ فشل البناء"
    exit 1
fi
echo "✓ تم البناء: $APP"
echo

# ===== 3. التثبيت =====
echo "[3/4] تثبيت $APP في $PREFIX/bin..."

mkdir -p "$PREFIX/bin"
cp "$APP" "$PREFIX/bin/$APP"
chmod +x "$PREFIX/bin/$APP"
echo "✓ تم التثبيت"
echo

# ===== 4. التحقق =====
echo "[4/4] التحقق..."

if command -v "$APP" >/dev/null 2>&1; then
    echo "✓ $APP مثبّت وجاهز!"
    echo
    echo "للتشغيل:"
    echo "   $APP ملف.txt"
    echo "   $APP صفحة.html"
    echo
    echo "الأزرار:"
    echo "   ^X خروج | ^O حفظ | ^W بحث | ^\\ استبدال"
    echo "   ^T تصحيح | ^Z تراجع | ^Y إعادة | ^G سطر"
    echo "   Tab إكمال HTML | F2 هيكل HTML5 | F1 مساعدة"
else
    echo "⚠ لم يُضف إلى PATH"
    echo "أضف: export PATH=\"$PREFIX/bin:\$PATH\""
fi
