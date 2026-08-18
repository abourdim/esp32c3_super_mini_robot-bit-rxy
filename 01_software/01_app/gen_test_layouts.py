"""Generate the child-facing test-mode layouts.

One panel per subsystem, served by the same firmware and chosen from the Level
selector -- so a child never reflashes to move between exercises.

Every panel carries the `level` select. Losing it would strand the robot on
that panel until someone reflashed, which is exactly the situation these are
meant to avoid.

Panels are deliberately small: few widgets, large touch targets, and a CFG
under ~1KB so the transfer is about a second even on a poor MTU.
"""
import json, base64, itertools, os, textwrap

APP   = os.path.dirname(os.path.abspath(__file__))
PAD, TITLE = 24, 34
MODES = "Beginner,Expert,Motors,Distance,Lights,Sound,Display,Power"

def W(id, t, x, y, w, h, label, **kw):
    d = dict(id=id, t=t, x=x, y=y, w=w, h=h, label=label); d.update(kw); return d

def panel(title, label, color, members, hint):
    """members are positioned relative to the group's inner origin (80,100)."""
    members = list(members)
    # every panel gets the way back
    members.append(W("level","select", 80, max(m["y"]+m["h"] for m in members)+40,
                     200, 70, "Test", options=MODES))
    x1 = min(m["x"] for m in members); y1 = min(m["y"] for m in members)
    x2 = max(m["x"]+m["w"] for m in members); y2 = max(m["y"]+m["h"] for m in members)
    grp = dict(id="grp_test", t="group", label=label, color=color,
               x=x1-PAD, y=y1-PAD-TITLE, w=(x2+PAD)-(x1-PAD), h=(y2+PAD)-(y1-PAD-TITLE),
               children=[m["id"] for m in members])
    for m in members: m["groupId"] = "grp_test"
    # The hint must go in `value`, not only `label`. The runtime renders a label
    # as `val || label`, and getRuntimeWidgetValue() falls back to the STRING "0"
    # when nothing has been sent -- which is truthy, so the label fallback never
    # fires. lbl_hint never receives an UPD, so it would read "0" forever.
    note = W("lbl_hint","label", x1, y2+40, x2-x1, 60, hint, value=hint, model="card")
    widgets = [grp] + members + [note]
    cfg = {"schemaVersion":1, "title":title,
           "canvas":{"w":max(w["x"]+w["w"] for w in widgets)+56,
                     "h":max(w["y"]+w["h"] for w in widgets)+56},
           "widgets":widgets}
    # validate
    box=lambda w:(w["x"],w["y"],w["x"]+w["w"],w["y"]+w["h"])
    def ov(a,b):
        a1,b1,a2,b2=box(a); c1,d1,c2,d2=box(b)
        return not (a2<=c1 or c2<=a1 or b2<=d1 or d2<=b1)
    ctrls=[w for w in widgets if w["t"] not in ("group","separator")]
    errs=[f"{a['id']}~{b['id']}" for a,b in itertools.combinations(ctrls,2) if ov(a,b)]
    gx1,gy1,gx2,gy2=box(grp)
    for m in members:
        mx1,my1,mx2,my2=box(m)
        if mx1<gx1+PAD or mx2>gx2-PAD or my1<gy1+TITLE or my2>gy2-PAD:
            errs.append(f"{m['id']} escapes group")
    assert not errs, f"{label}: {errs}"
    return cfg

P = {}

P["motors"] = panel("b3 - Motors test", "MOTORS", "#00d4ff", [
    W("dpad_drive","dpad",      80,100,300,300,"Drive", model="classic"),
    W("spd","slider",          420,100, 90,200,"Speed", max=100, value=100),
    W("btn_stop","button",     420,330,120,120,"STOP", model="flat"),
    W("gauge_speed","gauge",   570,100,150,190,"Speed", max=100, units="%", decimals=0),
], "Press an arrow. The wheels should turn that way.")

P["distance"] = panel("b3 - Distance test", "DISTANCE", "#ffb020", [
    W("gauge_distance","gauge", 80,100,150,190,"Distance", max=200, units="cm", decimals=0),
    W("alert","notification",  260,110,110,110,"Obstacle"),
    W("graph_dist","graph",     80,320,380,200,"Distance cm", max=200),
], "Move your hand in front of the sensor.")

P["lights"] = panel("b3 - Lights test", "LIGHTS", "#7c5cff", [
    W("toggle_led_r","toggle",   80,100,110,110,"Red LED", model="pill"),
    W("toggle_led_g","toggle",  210,100,110,110,"Green LED", model="pill"),
    W("led_r_state","led",      350,110, 90, 90,"Red is", model="dot",
      colorOn="#ff5252", colorOff="#2a2a3a"),
    W("led_g_state","led",      460,110, 90, 90,"Green is", model="dot",
      colorOn="#00ff88", colorOff="#2a2a3a"),
    W("toggle_np","toggle",      80,250,110,110,"Strip", model="pill"),
    W("np_effect","select",     210,270,190, 70,"Effect",
      options="Solid,Rainbow,Knight Rider,Duel eye,French flag"),
    W("np_r","slider",           80,400, 80,190,"R", max=255, value=255),
    W("np_g","slider",          180,400, 80,190,"G", max=255, value=0),
    W("np_b","slider",          280,400, 80,190,"B", max=255, value=0),
    W("np_bright","slider",     390,400,100,190,"Bright", max=255, step=5, value=15),
], "Switch the lights on, then mix a colour.")

P["sound"] = panel("b3 - Sound test", "SOUND", "#ff5c8a", [
    W("btn_horn","button",  80,100,120,120,"Horn", model="neo"),
    W("btn_buzz","button", 230,100,120,120,"Buzz", model="neo"),
    W("sound_alert","sound",380,105,110,110,"Alert"),
], "Each button makes a different sound.")

P["display"] = panel("b3 - Display test", "DISPLAY", "#00e5ff", [
    W("oled_text","editfield", 80,100,300,80,"Write here",
      placeholder="Type your name..."),
    W("lbl_oled","label",     410,105,300,70,"Screen shows", model="card"),
], "Type, then look at the robot's little screen.")

P["power"] = panel("b3 - Power test", "POWER", "#3ddc97", [
    W("battery_level","battery", 80,100, 90,120,"Battery", model="vertical"),
    W("lbl_vbat","label",       200,120,220, 60,"Volts", model="card"),
    W("gauge_rssi","gauge",     450,100,150,190,"Signal", min=-100, max=-30,
      units="dBm", decimals=0),
    W("btn_buzz","button",       80,270,120,120,"Buzz", model="neo"),
], "Buzz uses the same pin as the battery sensor -- watch the volts.")

order = ["motors","distance","lights","sound","display","power"]
decls = []
for name in order:
    cfg  = P[name]
    mini = json.dumps(cfg, separators=(",",":"))
    b64  = base64.b64encode(mini.encode()).decode()
    var  = f"LAYOUT_CFG_TEST_{name.upper()}_BASE64"
    decls.append(f"static const char* {var} =\n" +
                 "\n".join('  "%s"' % c for c in textwrap.wrap(b64, 72)) + ";")
    json.dump(cfg, open(f"{APP}/layout_test_{name}.json","w"), indent=1)
    n = -(-len(b64)//127)
    print(f"  {name:9} {len(cfg['widgets']):2} widgets  {len(mini):4}B -> {len(b64):4}B  "
          f"{n:2} chunks  {0.3+n*0.05:.2f}s")
open(f"{APP}/test_decls.h","w",newline="\n").write("\n\n".join(decls)+"\n")
print(f"\nwrote {len(order)} layouts + test_decls.h")
