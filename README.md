# nano-ar 📝

محرر نصوص عربي خفيف يعمل على **Termux** و **Linux**، مستوحى من `nano` مع مميزات إضافية.

## ✨ المميزات

- 🎨 **تلوين HTML** — وسوم، سمات، نصوص، تعليقات
- ⚡ **إكمال تلقائي** لوسوم HTML (Tab)
- 🔄 **إغلاق تلقائي** `<div>` → `<div></div>`
- 🔤 **تصحيح إملائي** (hunspell / aspell / قاموس محلي)
- ↩️ **Undo / Redo** (`^Z` / `^Y`)
- 🔍 **بحث واستبدال** (`^W` / `^\`)
- 🇸🇦 **دعم كامل للعربية** و UTF-8
- ⌨️ **أزرار مثل nano** — نفس الاختصارات

## 📦 التثبيت

### على Termux

```bash
pkg install git clang make ncurses-dev hunspell hunspell-en
git clone https://github.com/USERNAME/nano-ar.git
cd nano-ar
chmod +x install.sh
./install.sh
```

### على Linux

```bash
git clone https://github.com/USERNAME/nano-ar.git
cd nano-ar
chmod +x install.sh
./install.sh
```

### بناء يدوي

```bash
make
./nano-ar ملف.txt
```

## 🎮 الأزرار

| المفتاح | الوظيفة |
|---|---|
| `^X` | خروج |
| `^O` | حفظ |
| `^W` | بحث |
| `^\` | بحث واستبدال |
| `^G` | اذهب إلى سطر |
| `^T` | تصحيح إملائي |
| `^Z` | تراجع |
| `^Y` | إعادة |
| `^K` | قطع السطر |
| `^U` | لصق |
| `^C` | موقع المؤشر |
| `Tab` | إكمال وسم HTML |
| `F1` | مساعدة |
| `F2` | إدراج هيكل HTML5 |

## 🖼️ أمثلة

### إكمال HTML
```
تكتب: h
تضغط: Tab
النتيجة: <head></head>  (أو قائمة خيارات)
```

### هيكل HTML5
```
تضغط: F2
النتيجة:
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
    <meta charset="UTF-8">
    ...
</head>
<body>
    ...
</body>
</html>
```

### تصحيح إملائي
```
تكتب: helo wrld
تضغط: ^T
النتيجة: أخطاء: 2 | مثال: 'helo' → hello
```

## 🛠️ المتطلبات

- `ncurses`
- `hunspell` أو `aspell` (للتصحيح)
- قاموس في `/usr/share/dict/words`

## 📁 هيكل المشروع

```
nano-ar/
├── nano_ar.c      ← الكود (ملف واحد)
├── Makefile       ← للبناء
├── install.sh     ← تثبيت تلقائي
├── README.md      ← هذا الملف
└── LICENSE        ← MIT
```

## 📜 الرخصة

MIT — استخدم وعدّل بحرية.

## 🤝 المساهمة

افتح Issue أو Pull Request على GitHub.

---

صُنع بـ ❤️ للمجتمع العربي
