# BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
# Renders docs/MANUAL.md (a small Markdown subset) to an A4 PDF with PIL: headings, paragraphs with **bold** and
# `code`, lists, two-column tables, PNG images. SVG images are left out (no renderer on the Brick).
# usage: python3 -I mkpdf.py MANUAL.md OUT.pdf
import sys, os, re
from PIL import Image, ImageDraw, ImageFont
SRC, OUT = sys.argv[1], sys.argv[2]; BASE = os.path.dirname(os.path.abspath(SRC))
W, H, M = 1000, 1414, 72; CW = W - 2 * M
FD = '/usr/share/fonts/dejavu/'
F = {'r': ImageFont.truetype(FD + 'DejaVuSans.ttf', 17), 'b': ImageFont.truetype(FD + 'DejaVuSans-Bold.ttf', 17),
     'c': ImageFont.truetype(FD + 'DejaVuSansMono.ttf', 16), 'h1': ImageFont.truetype(FD + 'DejaVuSans-Bold.ttf', 34),
     'h2': ImageFont.truetype(FD + 'DejaVuSans-Bold.ttf', 25), 'h3': ImageFont.truetype(FD + 'DejaVuSans-Bold.ttf', 20),
     's': ImageFont.truetype(FD + 'DejaVuSans.ttf', 13)}
INK, DIM, ACC, LINE, CELL = (24, 26, 32), (98, 104, 119), (20, 150, 140), (210, 214, 222), (244, 245, 248)
LH = 25
pages = []; pg = None; d = None; y = 0
def new_page():
    global pg, d, y
    pg = Image.new('RGB', (W, H), 'white'); d = ImageDraw.Draw(pg); pages.append(pg); y = M
    d.text((M, H - 46), 'BrickPad 1.0 beta · manual', font=F['s'], fill=DIM)
    d.text((W - M, H - 46), str(len(pages)), font=F['s'], fill=DIM, anchor='ra')
def need(h):
    if y + h > H - M - 10: new_page()
def runs(text):                          # [(style, word-with-trailing-space)]
    out = []
    for i, part in enumerate(re.split(r'(\*\*[^*]+\*\*|`[^`]+`)', text)):
        if not part: continue
        st = 'b' if part.startswith('**') else 'c' if part.startswith('`') else 'r'
        t = part.strip('*') if st == 'b' else part.strip('`') if st == 'c' else part
        t = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', t)
        for w in re.findall(r'\S+\s*|\s+', t): out.append((st, w))
    return out
def layout(text, width):                 # lines of runs
    lines, cur, cw = [], [], 0
    for st, w in runs(text):
        ww = F[st].getlength(w)
        if cw + F[st].getlength(w.rstrip()) > width and cur: lines.append(cur); cur, cw = [], 0; w = w.lstrip()
        if not w: continue
        cur.append((st, w)); cw += F[st].getlength(w)
    if cur: lines.append(cur)
    return lines
def draw_lines(lines, x, y0):
    yy = y0
    for ln in lines:
        xx = x
        for st, w in ln:
            if st == 'c': d.rectangle((xx - 1, yy - 1, xx + F['c'].getlength(w.rstrip()) + 1, yy + 20), fill=CELL)
            d.text((xx, yy), w, font=F[st], fill=INK); xx += F[st].getlength(w)
        yy += LH
    return yy
def para(text, indent=0, bullet=None):
    global y
    lines = layout(text, CW - indent)
    for i, ln in enumerate(lines):
        need(LH)
        if i == 0 and bullet: d.text((M + indent - 24, y), bullet, font=F['r'], fill=ACC)
        y = draw_lines([ln], M + indent, y)
    y += 6
def img_h(path):
    if path.lower().endswith('.svg'): return 0
    im = Image.open(os.path.join(BASE, path)); return int(CW * 0.82) * im.height // im.width + 20
def heading(text, lv, extra=0):
    global y
    f = F['h%d' % lv]; gap = {1: 0, 2: 26, 3: 14}[lv]
    need(gap + 60 + max(extra, 120 if lv < 3 else 60)); y += gap
    d.text((M, y), text, font=f, fill=INK if lv != 3 else ACC); y += {1: 50, 2: 38, 3: 30}[lv]
    if lv == 2: d.rectangle((M, y - 6, M + CW, y - 5), fill=LINE); y += 6
def image(path):
    global y
    if path.lower().endswith('.svg'): return
    im = Image.open(os.path.join(BASE, path)).convert('RGB'); w = int(CW * 0.82); h = im.height * w // im.width
    im = im.resize((w, h), Image.LANCZOS); need(h + 20)
    x = M + (CW - w) // 2; d.rectangle((x - 1, y - 1, x + w, y + h), outline=LINE); pg.paste(im, (x, y)); y += h + 18
def table(rows):
    global y
    c1 = max(min(max(F['b'].getlength(re.sub(r'[*`]', '', r[0])) for r in rows) + 24, 300), 120)
    for i, r in enumerate(rows):
        a = layout('**%s**' % r[0].strip('*') if i else r[0], c1 - 16); b = layout(r[1], CW - c1 - 16); h = max(len(a), len(b)) * LH + 12
        need(h)
        if i == 0: d.rectangle((M, y, M + CW, y + h), fill=CELL)
        draw_lines(a, M + 8, y + 6); draw_lines(b, M + c1, y + 6)
        y += h; d.rectangle((M, y - 1, M + CW, y - 1), fill=LINE)
    y += 12
new_page()
lines = open(SRC, encoding='utf-8').read().split('\n'); i = 0; buf = []; kind = None
def flush():
    global buf, kind
    if buf:
        t = ' '.join(s.strip() for s in buf)
        if kind == '-': para(t, 28, '•')
        elif kind and kind[0].isdigit(): para(t, 28, kind)
        else: para(t)
    buf, kind = [], None
while i < len(lines):
    s = lines[i]
    if s.startswith('#'):
        flush(); lv = len(s) - len(s.lstrip('#')); j = i + 1
        while j < len(lines) and not lines[j].strip(): j += 1
        ex = img_h(re.search(r'\(([^)]+)\)', lines[j]).group(1)) if j < len(lines) and lines[j].startswith('![') else 0
        heading(s.lstrip('#').strip(), lv, ex)
    elif s.startswith('!['):
        flush(); image(re.search(r'\(([^)]+)\)', s).group(1))
    elif s.startswith('|'):
        flush(); rows = []
        while i < len(lines) and lines[i].startswith('|'):
            c = [x.strip() for x in lines[i].strip().strip('|').split('|')]
            if not set(''.join(c)) <= set('-: '): rows.append(c)
            i += 1
        table(rows); continue
    elif s.startswith('- ') or re.match(r'\d+\. ', s):
        flush(); kind = '-' if s.startswith('- ') else s.split(' ')[0]; buf = [s[len(kind) + 1:] if kind != '-' else s[2:]]
    elif s.startswith('  ') and buf and kind:
        buf.append(s)
    elif s.strip() == '---':
        flush(); need(30); y += 10; d.rectangle((M, y, M + CW, y), fill=LINE); y += 16
    elif not s.strip():
        flush()
    else:
        if kind: flush()
        buf.append(s)
    i += 1
flush()
pages[0].save(OUT, 'PDF', resolution=121, save_all=True, append_images=pages[1:], quality=88)
print('pages', len(pages), os.path.getsize(OUT))
