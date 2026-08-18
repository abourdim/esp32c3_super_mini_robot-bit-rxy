"""Install an Arrange-mode export as the b3 expert layout.

    python3 install_export.py <exported-layout.json>

Positions are never touched — the user placed them. Only two things happen:

  1. group boxes grow UPWARD where the header chip would collide with the first
     widget inside (needs ~50px; exports routinely have 12-40),
  2. fields the app's applyWidgetDefaults() re-fills are dropped, because the
     CFG is base64'd twice on the way to the browser so every redundant byte
     costs ~1.33 bytes of flash and transfer time.

Then it validates, writes layout_expert.json into the firmware directory, and
splices both layout blobs into 03_bit-rxy.cpp.
"""
import json, base64, itertools, os, re, sys, textwrap

# Resolved from this file's own location, so the tool travels with the repo.
APP = os.path.dirname(os.path.abspath(__file__))
CPP = f"{APP}/01_src/03_bit-rxy.cpp"
TITLE_GAP, GROUP_SEP = 50, 8

# Values applyWidgetDefaults() re-creates on the app side, per widget type.
DEFAULTS = {
    "group":     {"model": "panel", "padding": 18},
    "separator": {"model": "subtle", "thickness": 1},
    "slider":    {"model": "track", "min": 0, "step": 1},
    "gauge":     {"warn": None, "danger": None},
    "graph":     {"model": "grid", "autoScale": True, "showLegend": True,
                  "series": 1, "windowSec": 30, "min": 0},
    "led":       {"colorOff": "#2a2a3a"},
    "button":    {"model": "neo"},
    "joystick":  {"model": "classic"},
}

def box(w): return (w["x"], w["y"], w["x"] + w["w"], w["y"] + w["h"])
def ov(a, b):
    ax1, ay1, ax2, ay2 = box(a); bx1, by1, bx2, by2 = box(b)
    return not (ax2 <= bx1 or bx2 <= ax1 or ay2 <= by1 or by2 <= ay1)

def repair(cfg):
    W = cfg["widgets"]; by = {w["id"]: w for w in W}
    groups = [w for w in W if w["t"] == "group"]
    notes = []
    for g in sorted(groups, key=lambda g: g["y"]):
        kids = [by[c] for c in g["children"] if c in by]
        if not kids: continue
        gap = min(k["y"] for k in kids) - g["y"]
        if gap >= TITLE_GAP: continue
        need = TITLE_GAP - gap; room = need
        for o in groups:
            if o is g: continue
            ox1, oy1, ox2, oy2 = box(o); gx1, gy1, gx2, gy2 = box(g)
            if not (gx2 <= ox1 or ox2 <= gx1) and oy2 <= gy1:
                room = min(room, max(0, gy1 - oy2 - GROUP_SEP))
        if room > 0:
            g["y"] -= room; g["h"] += room
            notes.append(f"{g['label']} header +{room}px (gap {gap}->{gap+room})")
        else:
            notes.append(f"{g['label']} header still tight at {gap}px (no room above)")
    return notes

def slim(cfg):
    n = 0
    for w in cfg["widgets"]:
        if w.pop("props", None) is not None: n += 1
        for k, v in DEFAULTS.get(w["t"], {}).items():
            if k in w and w[k] == v: w.pop(k); n += 1
        if w["t"] == "separator":
            derived = "vertical" if w["h"] > w["w"] else "horizontal"
            if w.get("orientation") == derived: w.pop("orientation"); n += 1
    if cfg.pop("configRevision", None) is not None: n += 1
    return n

NUDGE_MAX = 14   # overlaps deeper than this are a real design problem, not a slip

def denudge(cfg):
    """Resolve *tiny* control overlaps by shifting one widget clear.

    Dragging in Arrange mode routinely leaves a couple of pixels of overlap,
    which is invisible on screen but means two touch targets share pixels. Only
    shallow collisions are fixed, and only along the axis of least penetration,
    so a widget never jumps somewhere unexpected. Anything deeper is left for
    validate() to reject, because it means the arrangement itself is wrong.
    """
    W = cfg["widgets"]; by = {w["id"]: w for w in W}
    ctrls = [w for w in W if w["t"] not in ("group", "separator")]
    notes = []
    for a, b in itertools.combinations(ctrls, 2):
        if not ov(a, b): continue
        ax1, ay1, ax2, ay2 = box(a); bx1, by1, bx2, by2 = box(b)
        dx = min(ax2 - bx1, bx2 - ax1)      # horizontal penetration
        dy = min(ay2 - by1, by2 - ay1)      # vertical penetration
        if min(dx, dy) > NUDGE_MAX: continue
        # move whichever of the two sits further right/down, so the leading
        # widget keeps the position the user chose for it
        mover, other = (b, a) if (bx1, by1) >= (ax1, ay1) else (a, b)
        g = by.get(mover.get("groupId"))
        if dx <= dy:
            shift = dx + 2
            mover["x"] += shift; axis = "x"
        else:
            shift = dy + 2
            mover["y"] += shift; axis = "y"
        # if that pushed it out of its group, grow the group to match
        if g:
            gx1, gy1, gx2, gy2 = box(g); mx1, my1, mx2, my2 = box(mover)
            if mx2 > gx2 - 18: g["w"] += (mx2 + 18) - gx2
            if my2 > gy2 - 18: g["h"] += (my2 + 18) - gy2
        notes.append(f"{mover['id']} nudged {axis}+{shift} clear of {other['id']} "
                     f"({min(dx,dy)}px overlap)")
    return notes

def validate(cfg):
    W = cfg["widgets"]; by = {w["id"]: w for w in W}
    groups = [w for w in W if w["t"] == "group"]
    ctrls  = [w for w in W if w["t"] not in ("group", "separator")]
    errs = []
    for g in groups:
        gx1, gy1, gx2, gy2 = box(g)
        for c in g["children"]:
            if c not in by: errs.append(f"{g['label']} child {c} missing"); continue
            x1, y1, x2, y2 = box(by[c])
            if x1 < gx1 or y1 < gy1 or x2 > gx2 or y2 > gy2:
                errs.append(f"{c} escapes {g['label']}")
    for a, b in itertools.combinations(ctrls, 2):
        if ov(a, b): errs.append(f"control overlap {a['id']}~{b['id']}")
    for a, b in itertools.combinations(groups, 2):
        if ov(a, b): errs.append(f"group overlap {a['label']}~{b['label']}")
    if not any(c["id"] == "level" for c in ctrls):
        errs.append("no `level` select — switching would be one-way")
    clipped = [w["id"] for w in ctrls if w["t"] == "gauge" and w["h"] < 175]
    return errs, clipped

def splice(var, jf):
    cfg  = json.load(open(jf))
    mini = json.dumps(cfg, separators=(",", ":"))
    b64  = base64.b64encode(mini.encode()).decode()
    body = "\n".join('  "%s"' % c for c in textwrap.wrap(b64, 72))
    src  = open(CPP, encoding="utf-8").read()
    src, n = re.subn(rf'static const char\* {var} =.*?;',
                     f"static const char* {var} =\n{body};", src, count=1, flags=re.S)
    assert n == 1, f"{var} not found in {CPP}"
    open(CPP, "w", encoding="utf-8", newline="\n").write(src)
    return len(mini), len(b64)

def main(path):
    cfg = json.load(open(path, encoding="utf-8"))
    before = len(json.dumps(cfg, separators=(",", ":")))
    print(f"source: {path}")
    print(f"  title={cfg.get('title')}  widgets={len(cfg['widgets'])}")
    for note in repair(cfg): print("  fix:", note)
    for note in denudge(cfg): print("  fix:", note)
    removed = slim(cfg)
    cfg["canvas"] = {"w": max(w["x"]+w["w"] for w in cfg["widgets"]) + 46,
                     "h": max(w["y"]+w["h"] for w in cfg["widgets"]) + 46}
    errs, clipped = validate(cfg)
    after = len(json.dumps(cfg, separators=(",", ":")))
    print(f"  slimmed: {removed} fields, {before} -> {after} B")
    if clipped: print(f"  NOTE gauges under 175px may clip their label: {clipped}")
    if errs:
        print("  FAILED:"); [print("   -", e) for e in errs]; return 1
    json.dump(cfg, open(f"{APP}/layout_expert.json", "w"), indent=1)
    print("  wrote layout_expert.json")
    for var, jf in (("LAYOUT_CFG_BEGINNER_BASE64", f"{APP}/layout_beginner.json"),
                    ("LAYOUT_CFG_EXPERT_BASE64",   f"{APP}/layout_expert.json")):
        j, b = splice(var, jf)
        for mtu in (135, 247):
            ch = min(180, max(18, mtu - 8)); n = -(-b // ch)
            if mtu == 135: t135 = (n, 0.3 + n*0.05)
            else:          t247 = (n, 0.3 + n*0.05)
        print(f"  {var:30} {j:5}B -> {b:5}B  "
              f"MTU135: {t135[0]:3}ch {t135[1]:.2f}s | MTU247: {t247[0]:3}ch {t247[1]:.2f}s")
    print("  spliced both blobs into 03_bit-rxy.cpp")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
