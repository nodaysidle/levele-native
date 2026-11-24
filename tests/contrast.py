# Re-implements Theme::loadFrom (src/theme.cpp) as changed for F02/F18 and
# checks the same pair table Theme::contrastPairs() gives --theme-sweep, plus
# the two tokens the sweep does not derive (green/yellow/textStrong).
import os, re, glob, math, sys
def lin(v):
    v/=255.0; return v/12.92 if v<=0.04045 else ((v+0.055)/1.055)**2.4
def lum(c): return 0.2126*lin(c[0])+0.7152*lin(c[1])+0.0722*lin(c[2])
def cr(a,b):
    la,lb=lum(a),lum(b); return (max(la,lb)+0.05)/(min(la,lb)+0.05)
def qround(x): return int(math.floor(x+0.5)) if x>=0 else -int(math.floor(-x+0.5))
def mix(a,b,t):
    t=max(0,min(1,t)); return tuple(max(0,min(255,qround(a[i]+(b[i]-a[i])*t))) for i in range(3))
def hexc(s): return tuple(int(s[i:i+2],16) for i in (1,3,5))
def parse(p):
    out={}
    for line in open(p,encoding='utf-8',errors='replace'):
        line=line.strip()
        if not line or line.startswith('#'): continue
        eq=line.find('=')
        if eq<=0: continue
        k=line[:eq].strip(); v=line[eq+1:].strip()
        if len(v)>=2 and v[0]=='"' and v[-1]=='"': v=v[1:-1]
        if k and v: out[k]=v
    return out
HEX=re.compile(r'^#[0-9a-fA-F]{6}$')
def colour(m,keys,fb):
    for k in keys:
        if k in m and HEX.match(m[k]): return hexc(m[k])
    return fb
WHITE=(255,255,255); BLACK=(0,0,0)
def ok_on(c,panels,ratio): return all(cr(c,p)>=ratio for p in panels)
def readable(ink,panels,ratio):
    if ok_on(ink,panels,ratio): return ink
    panel_lum=sum(lum(p) for p in panels)/max(1,len(panels))
    tgt=WHITE if panel_lum<0.35 else BLACK
    for i in range(1,201):
        c=mix(ink,tgt,i/200.0)
        if ok_on(c,panels,ratio): return c
    return tgt
def best_ink_on(paper):
    return BLACK if cr(BLACK,paper)>=cr(WHITE,paper) else WHITE
def derive(m):
    dark=m.get('mode','').lower()!='light'
    bg=colour(m,['background','bg'],(0x0c,0x0d,0x0b) if dark else (0xf7,0xf5,0xf0))
    fg=colour(m,['foreground','fg','light_foreground'],(0xe8,0xe4,0xdc) if dark else (0x1d,0x1c,0x1a))
    surface=colour(m,['lighter_background','lighter_bg'],None) or mix(bg,fg,0.09 if dark else 0.06)
    surfaceAlt=mix(surface,fg,0.07 if dark else 0.05)
    chrome=colour(m,['dark_background','dark_bg','darker_bg'],None) or (mix(bg,BLACK,0.18) if dark else mix(bg,fg,0.05))
    border=colour(m,['line','border'],None) or mix(bg,fg,0.22 if dark else 0.28)
    accent=colour(m,['accent','blue','magenta'],colour(m,['bright_blue'],fg))
    t=dict(dark=dark,bg=bg,surface=surface,surfaceAlt=surfaceAlt,chrome=chrome)
    # --- mirrors the C++ derivation
    t['accent']=readable(accent,[surface,chrome],3.0)
    t['accentSoft']=mix(surface,t['accent'],0.24 if dark else 0.16)
    t['accentInk']=best_ink_on(t['accent'])
    panels=[surface,chrome,surfaceAlt,bg]
    t['border']=readable(border,[surface,chrome,bg],3.0)
    t['fg']=readable(fg,panels,4.5)
    tint=0.24 if dark else 0.16
    while tint>0 and cr(t['fg'],t['accentSoft'])<4.5:
        tint-=0.02; t['accentSoft']=mix(surface,t['accent'],max(0.0,tint))
    t['textStrong']=readable(colour(m,['bright_foreground'],fg),panels,4.5)
    t['muted']=readable(mix(fg,bg,0.42),panels,4.5)
    t['red']=readable(colour(m,['red'],(0xff,0x64,0x67)),panels,4.5)
    t['green']=readable(colour(m,['green'],(0x8e,0xe0,0x7a)),panels,4.5)
    t['yellow']=readable(colour(m,['yellow'],(0xf3,0xca,0x3e)),panels,4.5)
    return t
# (label, ink, paper, required, where) — same table as Theme::contrastPairs()
PAIRS=[
 ('fg/bg',          'fg','bg',4.5,'NumField/Combo/TextArea/tool tiles/shortcut keys'),
 ('fg/surface',     'fg','surface',4.5,'panels, rows, chips'),
 ('fg/chrome',      'fg','chrome',4.5,'window, top bar'),
 ('fg/surfaceAlt',  'fg','surfaceAlt',4.5,'hovered rows, chips'),
 ('muted/surfaceAlt','muted','surfaceAlt',4.5,'recent row meta on hover'),
 ('fg/accentSoft',  'fg','accentSoft',4.5,'selected layer name'),
 ('accentInk/accent','accentInk','accent',4.5,'accent Btn, active tool, align, combo highlight'),
 ('accent/surface', 'accent','surface',3.0,'focus ring, selected layer glyph, lock icon (1.4.11)'),
 ('accent/chrome',  'accent','chrome',3.0,'canvas selection outline + handles'),
 ('border/surface', 'border','surface',3.0,'NumField/Combo/ColorChip boundary (1.4.11)'),
 ('border/bg',      'border','bg',3.0,'input boundary on bg-filled fields'),
 ('red/surfaceAlt', 'red','surfaceAlt',4.5,'danger Btn hovered'),
]
roots=[]
cur=os.path.expanduser('~/.local/state/omarchy/current')
if os.path.isdir(cur): roots.append(('current',cur))
for base in [os.path.expanduser('~/.config/omarchy/themes'),'/usr/share/omarchy/themes']:
    for d in sorted(glob.glob(base+'/*')):
        roots.append((os.path.basename(d)+('@usr' if base.startswith('/usr') else '@cfg'),d))
fails={p[0]:[] for p in PAIRS}; n=0; skipped=[]; light=0
for name,d in roots:
    p=d+'/theme/colors.toml'
    if not os.path.exists(p): p=d+'/colors.toml'
    if not os.path.exists(p): skipped.append(name); continue
    t=derive(parse(p)); n+=1; light+= (not t['dark'])
    for lab,a,b,req,_ in PAIRS:
        r=cr(t[a],t[b])
        if r<req: fails[lab].append((name,round(r,2)))
print(f'themes checked: {n} (light: {light}); skipped (no colors.toml): {skipped}')
total=0
for lab,a,b,req,where in PAIRS:
    f=fails[lab]; total+=len(f)
    worst=sorted(f,key=lambda x:x[1])[:6]
    print(f'{lab:18s} need {req}: {len(f):3d} failing  worst={worst}  [{where}]')
print(f'TOTAL failing pairs: {total}')
sys.exit(0 if total==0 else 1)
