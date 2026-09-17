"""Generate disasm/shapes_preview.html (plus build/vector_tables.json and
build/cam_scripts.json): a single static page in the style of Space Duel's
shapes_preview.html showing

  1. every labelled vector picture in the vector ROM ($3000-$3FFF) with name,
     address, colours (wave-1 palette) and description
  2. the program-ROM 'between two points' pictures (flipper, claw, pulsar)
  3. the character set (VGMSGA) and the PICLO picture table
  4. all 30 messages in English, French, German and Spanish
  5. the CAM enemy-motion scripts, decoded, with an animated flipper that
     runs the chosen script (schematic well, player at lane 0)

The renderer follows JSRL/JMPL, applies SCAL and tracks the Tempest colour
STAT (see avg.py / vrender.py).
"""
import html, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import avg, vrender, vtables, cam
from vrender import clean

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "shapes_preview.html")
BUILD = os.path.join(HERE, "build")


def fmt(v):
    return "%g" % round(float(v), 3)


def segjs(segs):
    return "[" + ",".join("[%s,%s,%s,%s,%d,%d]" % (fmt(a), fmt(b), fmt(c), fmt(d),
                                                   -1 if col is None else col, br)
                          for a, b, c, d, col, br in segs) + "]"


def emit():
    M = vrender.Model()
    mem = M.mem
    tables = vtables.build(M)
    camd = cam.decode(mem)
    os.makedirs(BUILD, exist_ok=True)
    json.dump(tables, open(os.path.join(BUILD, "vector_tables.json"), "w", encoding="utf-8"),
              indent=1, ensure_ascii=False)
    json.dump(camd, open(cam.OUT, "w"), indent=1)
    pal = vrender.palette(mem, 0)
    E = html.escape

    # ---------------------------------------------------------------- 1. vector ROM pictures
    cards, data = [], []
    npics = 0
    for a in sorted(M.label_at):
        if not (0x3000 <= a <= 0x3FFF):
            continue
        names = M.label_at[a]
        nm = names[0]
        if nm in vrender.DATA_LABELS or nm == "VGMSGA":
            continue
        segs, cols, info = vrender.render(mem, a, one_op=nm in vrender.WORD_LABELS)
        key = "V%04X" % a
        npics += 1
        notes = []
        if not any(br > 0 for *_x, br in segs):
            notes.append("no lit vectors")
        if info["vram_calls"]:
            notes.append("calls vector RAM")
        colnote = ", ".join(vrender.COLOR_NAMES.get(c, str(c)).split(" ")[0] for c in cols) or "inherits colour"
        alias = (" = " + ", ".join(clean(n) for n in names[1:])) if len(names) > 1 else ""
        desc = vrender.describe(nm)
        users = sorted({r for (t, _src), r in M.routine_refs.items() if t == a})
        if users:
            notes.append("used by " + ", ".join(users))
        cards.append("<div class='s' title='%s'><canvas id='c%s' width='104' height='104'></canvas>"
                     "<br>%s%s<br>%04X<br><span class='d'>%s%s</span></div>"
                     % (E(desc), key, E(clean(nm)), E(alias), a, E(colnote),
                        ("<br>" + E("; ".join(notes))) if notes else ""))
        data.append("S['%s']=%s;" % (key, segjs(segs)))

    # ---------------------------------------------------------------- 2. between-points pictures
    bp = tables["between_points"]
    bcards = []
    bcol = {"INVA1": 3, "NCRS": 1, "PULS": 4}
    for p in bp["pictures"]:
        x = y = 0
        segs = []
        col = bcol.get(p["name"], bcol.get(p["name"][:4]))
        for v in p["vectors"]:
            nx, ny = x + v["unit"], y + v["perp"]
            br = 0 if v["bright"] == 0 else (12 if v["bright"] in (1, 0x10) else min(15, v["bright"] >> 4))
            segs.append((x, y, nx, ny, col, br))
            x, y = nx, ny
        key = "B%s" % p["name"]
        bcards.append("<div class='s' title='%s'><canvas id='c%s' width='104' height='104'></canvas>"
                      "<br>%s<br>%s<br><span class='d'>%s</span></div>"
                      % (E(p["desc"]), key, p["name"], p["addr"][1:], E(p["desc"])))
        data.append("S['%s']=%s;" % (key, segjs(segs)))
    flip_vecs = json.dumps([[v["unit"], v["perp"], v["bright"]] for v in bp["pictures"][0]["vectors"]])
    claw_vecs = json.dumps([[v["unit"], v["perp"], v["bright"]] for v in bp["pictures"][1]["vectors"]])

    # ---------------------------------------------------------------- 3. character set + PICLO
    glyphs, ccards = [], []
    for c in tables["characters"]["entries"]:
        t = int(c["target"][1:], 16)
        st = {"x": 0.0, "y": 0.0, "scale": 1.0, "color": None, "intensity": 12}
        segs, _cols, _i = vrender.render(mem, t, state=st)
        glyphs.append("{s:%s,a:%s}" % (segjs(segs), fmt(st["x"])))
        ccards.append("<div class='s c'><canvas id='g%d' width='52' height='60'></canvas><br>%d %s<br>"
                      "<span class='d'>code %s<br>%s</span></div>"
                      % (c["index"], c["index"], E(c["char"]) if c["char"].strip() else "&nbsp;",
                         c["code"], E(clean(c["target_name"] or c["target"]))))
    picrows = "".join("<tr><td>$%02X</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>"
                      % (e["code"], e["code_name"] or "", e["addr"], E(clean(e["target_name"] or "")),
                         E(e["comment"] or "")) for e in tables["csect"]["PICLO"]["entries"])

    # ---------------------------------------------------------------- 4. messages
    mrows, mdata = [], []
    langs = vtables.LANGS
    for i, m in enumerate(tables["messages"]["messages"]):
        cells = []
        for k, lg in enumerate(langs):
            t = m["texts"][lg]
            cid = "m%d_%d" % (i, k)
            cells.append("<td><canvas id='%s' width='260' height='26'></canvas><br>"
                         "<span class='d'>%s x=%d</span> %s</td>"
                         % (cid, t["addr"], t["x"], E(t["text"])))
            mdata.append("M.push(['%s',%s,%d,%d]);" % (cid, json.dumps(t["codes"]), m["color"], m["scale"]))
        mrows.append("<tr><td>%s<br><span class='d'>#%d %s<br>colour %d %s, scale %d, y=%d</span></td>%s</tr>"
                     % (m["name"], m["number"], E(m["comment"]), m["color"], m["color_name"], m["scale"],
                        m["y"], "".join(cells)))

    # ---------------------------------------------------------------- 5. CAM scripts
    ophtml = "".join("<tr><td>$%02X</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>"
                     % (o["code"], o["name"], o["handler"], o["handler_addr"], o["operand"] or "",
                        E(o["doc"])) for o in camd["opcodes"])
    scr = []
    for sc in camd["scripts"]:
        rows = []
        for o in sc["ops"]:
            lab = ",".join(o["labels"])
            rows.append("<span class='d'>%s  %-5s</span> %-8s %-7s %-16s <span class='d'>;%s</span>"
                        % (o["addr"], o["bytes"], (lab + ":") if lab else "", o["op"],
                           E(o["operand"] or ""), E(o["comment"] or "")))
        scr.append("<div class='scr'><b>%s</b> <span class='d'>%s (CAM+$%02X, %d bytes)</span><br>%s<pre>%s</pre></div>"
                   % (sc["name"], sc["addr"], sc["offset"], sc["size"], E(sc["doc"]), "\n".join(rows)))
    users = camd["users"]
    camwav = ", ".join("%d:%s" % (k + 1, n) for k, n in enumerate(users.get("CAMWAV", {}).get("entries", [])))
    # compact program for the JS interpreter
    prog = {}
    camaddr = int(camd["cam_addr"][1:], 16)
    for sc in camd["scripts"]:
        for o in sc["ops"]:
            prog[o["offset"]] = [o["op"], o["target_offset"] if o["target_offset"] is not None else o["value"],
                                 o["operand"] or ""]
    cam_bytes = list(mem[camaddr:int(camd["cam_end"][1:], 16)])
    entries = {sc["name"]: sc["offset"] for sc in camd["scripts"]}
    ops_by_code = {o["code"]: [o["name"], o["macro"]] for o in camd["opcodes"]}

    page = TEMPLATE
    subs = {
        "%NPICS%": str(npics),
        "%CARDS%": "\n".join(cards),
        "%BCARDS%": "\n".join(bcards),
        "%CCARDS%": "\n".join(ccards),
        "%PICROWS%": picrows,
        "%MROWS%": "\n".join(mrows),
        "%OPROWS%": ophtml,
        "%SCRIPTS%": "\n".join(scr),
        "%NSCRIPTS%": str(len(camd["scripts"])),
        "%CAMWAV%": E(camwav),
        "%CAMADDR%": camd["cam_addr"], "%CAMEND%": camd["cam_end"], "%TABJSR%": camd["tabjsr_addr"],
        "%VGMSGA%": tables["characters"]["VGMSGA"], "%PICLO%": tables["csect"]["PICLO"]["addr"],
        "%MSG%": "ENGMSG %s, FREMSG %s, GERMSG %s, SPAMSG %s, MSGLBS %s, LNGTAB %s" % tuple(
            tables["messages"][k] for k in ("ENGMSG", "FREMSG", "GERMSG", "SPAMSG", "MSGLBS", "LNGTAB")),
        "%BP%": "PCOUNT %s, PINDEX %s, VBASE %s" % (bp["PCOUNT"], bp["PINDEX"], bp["VBASE"]),
        "%NMSG%": str(tables["messages"]["count"]),
        "%DATA%": "\n".join(data),
        "%GLYPHS%": "[" + ",\n".join(glyphs) + "]",
        "%MDATA%": "\n".join(mdata),
        "%PAL%": json.dumps(pal),
        "%FLIP%": flip_vecs, "%CLAW%": claw_vecs,
        "%CAMBYTES%": json.dumps(cam_bytes),
        "%OPS%": json.dumps(ops_by_code),
        "%ENTRIES%": json.dumps(entries),
    }
    for k, v in subs.items():
        page = page.replace(k, v)
    open(OUT, "w", encoding="utf-8").write(page)
    return npics, len(bcards), len(ccards), tables["messages"]["count"], len(camd["scripts"])


TEMPLATE = r"""<!doctype html><meta charset='utf-8'><title>Tempest vector objects</title>
<style>body{background:#000;color:#0f0;font:12px monospace;margin:8px 16px}
.s{display:inline-block;margin:4px;text-align:center;width:110px;vertical-align:top}
.s.c{width:60px}
canvas{border:1px solid #333;background:#000}h3{color:#6f6}h2{color:#6f6;border-bottom:1px solid #030;margin-top:28px}
.d{color:#666;font-size:10px}table{border-collapse:collapse}td,th{border:1px solid #131;padding:2px 5px;vertical-align:top}
th{color:#6f6}.wrap{overflow-x:auto}.scr{display:inline-block;vertical-align:top;margin:6px 14px 6px 0}
pre{margin:4px 0;color:#0f0}select,button{background:#010;color:#0f0;border:1px solid #060;font:12px monospace}
a{color:#6f6}</style>
<h3>Tempest vector objects &mdash; %NPICS% labelled vector ROM pictures ($3000-$3FFF), 14 between-point pictures,
41 characters, %NMSG% messages &times; 4 languages, %NSCRIPTS% CAM motion scripts</h3>
<p class='d'>Generated by disasm/emit_shapes.py from the rev-3 image. Names are Atari's (ALVROM.MAC / ANVGAN.MAC / ALDIS2.MAC /
ALLANG.MAC / ALWELG.MAC, '.' shown as '_'). Colours use the wave-1 palette (COLTAB); PDIWHI/PDIYEL/PDIRED rotate at run time.
Sections: <a href='#vrom'>vector ROM</a> &middot; <a href='#bp'>between-point pictures</a> &middot; <a href='#chars'>characters</a> &middot;
<a href='#msgs'>messages</a> &middot; <a href='#cam'>CAM scripts</a></p>

<h2 id='vrom'>Vector ROM pictures</h2>
%CARDS%

<h2 id='bp'>Program-ROM pictures drawn between two lane points (ALDIS2 %BP%)</h2>
<p class='d'>Not AVG data: each vector is unit&times;U + perp&times;P, U = lane point 1&rarr;2 (scaled), P = U rotated 90&deg;. Shown with U = (1,0).</p>
%BCARDS%

<h2 id='chars'>Character set (VGMSGA %VGMSGA%)</h2>
%CCARDS%
<h3>Picture table PICLO %PICLO% (ALVROM .CSECT)</h3>
<div class='wrap'><table><tr><th>code</th><th>PT name</th><th>word</th><th>picture</th><th>source comment</th></tr>%PICROWS%</table></div>

<h2 id='msgs'>Messages (ALLANG: %MSG%)</h2>
<div class='wrap'><table><tr><th>message</th><th>English</th><th>French</th><th>German</th><th>Spanish</th></tr>
%MROWS%
</table></div>

<h2 id='cam'>CAM enemy motion scripts (ALWELG CAM %CAMADDR%-%CAMEND%, TABJSR %TABJSR%)</h2>
<p class='d'>Byte code run by MOVINV once per enemy per frame until VEXIT. 2-byte branch operands hold target-CAM-1.
Flipper script per wave (CAMWAV, wave-1 &amp; 15): %CAMWAV%</p>
<div>
<canvas id='well' width='360' height='360'></canvas>
<div style='display:inline-block;vertical-align:top;margin-left:10px;width:420px'>
Script: <select id='scsel'></select> <button id='rst'>restart</button> <button id='pause'>pause</button><br>
<span class='d'>Schematic: circular well, player claw (NCRS1) on lane 0 at the rim, flipper drawn with INVA1 between the two lane
points. VSMOVE moves up, VJUMPS/VJUMPM flip over the pivot corner (8 steps), VEXIT ends the frame.</span>
<pre id='trace'></pre></div></div>
<h3>Opcodes (TABJSR)</h3>
<div class='wrap'><table><tr><th>code</th><th>name</th><th>handler</th><th>addr</th><th>operand</th><th>meaning</th></tr>%OPROWS%</table></div>
<h3>Scripts</h3>
%SCRIPTS%

<script>
var PAL=%PAL%;
var S={};
%DATA%
var G=%GLYPHS%;
var M=[];
%MDATA%
function col(c,br){var p=(c<0)?'#FFFFFF':PAL[c];var a=br>=15?1:0.4+0.6*br/15;return [p,a];}
function drawSegs(cv,sg,pad){
  var g=cv.getContext('2d'),W=cv.width,H=cv.height;var xs=[],ys=[];
  for(var i=0;i<sg.length;i++){if(sg[i][5]<=0)continue;xs.push(sg[i][0],sg[i][2]);ys.push(sg[i][1],sg[i][3]);}
  if(!xs.length)return;
  var x0=Math.min.apply(null,xs),x1=Math.max.apply(null,xs),y0=Math.min.apply(null,ys),y1=Math.max.apply(null,ys);
  var w=Math.max(1,x1-x0),h=Math.max(1,y1-y0);var sc=Math.min((W-2*pad)/w,(H-2*pad)/h);
  var ox=(W-w*sc)/2,oy=(H-h*sc)/2;g.lineWidth=1;
  for(var i=0;i<sg.length;i++){var s=sg[i];if(s[5]<=0)continue;var ca=col(s[4],s[5]);
    g.strokeStyle=ca[0];g.fillStyle=ca[0];g.globalAlpha=ca[1];
    var ax=ox+(s[0]-x0)*sc,ay=H-oy-(s[1]-y0)*sc,bx=ox+(s[2]-x0)*sc,by=H-oy-(s[3]-y0)*sc;
    if(Math.abs(ax-bx)<0.5&&Math.abs(ay-by)<0.5){g.fillRect(ax-1,ay-1,2,2);}
    else{g.beginPath();g.moveTo(ax,ay);g.lineTo(bx,by);g.stroke();}}
  g.globalAlpha=1;
}
for(var k in S){var cv=document.getElementById('c'+k);if(cv)drawSegs(cv,S[k],6);}
for(var i=0;i<G.length;i++){var cv=document.getElementById('g'+i);if(!cv)continue;
  var sg=G[i].s.slice();sg.push([0,-4,0,-4,-1,0]);sg.push([24,28,24,28,-1,0]);
  var g=cv.getContext('2d');var sc=40/28;g.strokeStyle='#0f0';g.fillStyle='#0f0';
  for(var j=0;j<G[i].s.length;j++){var s=G[i].s[j];if(s[5]<=0)continue;
    var ax=8+s[0]*sc,ay=52-s[1]*sc,bx=8+s[2]*sc,by=52-s[3]*sc;
    if(ax==bx&&ay==by){g.fillRect(ax-1,ay-1,2,2);}else{g.beginPath();g.moveTo(ax,ay);g.lineTo(bx,by);g.stroke();}}}
for(var i=0;i<M.length;i++){var e=M[i],cv=document.getElementById(e[0]);if(!cv)continue;
  var g=cv.getContext('2d'),adv=0;for(var j=0;j<e[1].length;j++){var gl=G[e[1][j]];adv+=gl?gl.a:24;}
  var sc=Math.min((cv.width-6)/Math.max(adv,1),18/28);var x=3;g.strokeStyle=PAL[e[2]];g.fillStyle=PAL[e[2]];
  for(var j=0;j<e[1].length;j++){var gl=G[e[1][j]];if(!gl)continue;
    for(var q=0;q<gl.s.length;q++){var s=gl.s[q];if(s[5]<=0)continue;var ax=x+s[0]*sc,ay=22-s[1]*sc,bx=x+s[2]*sc,by=22-s[3]*sc;
      if(ax==bx&&ay==by){g.fillRect(ax-1,ay-1,2,2);}else{g.beginPath();g.moveTo(ax,ay);g.lineTo(bx,by);g.stroke();}}
    x+=gl.a*sc;}}

/* ---------------- CAM animation ---------------- */
var CAM=%CAMBYTES%, OPS=%OPS%, ENTRY=%ENTRIES%, FLIP=%FLIP%, CLAW=%CLAW%;
var sel=document.getElementById('scsel');
for(var n in ENTRY){var o=document.createElement('option');o.value=n;o.text=n;if(n=='SPIRCH')o.selected=true;sel.appendChild(o);}
var st,paused=false,log=[];
function reset(){st={pc:ENTRY[sel.value],lane:5,y:0,jump:false,step:0,dir:1,loop:0,camsta:1,top:false,frame:0};log=[];}
function opname(pc){var c=OPS[CAM[pc]];return c?c[0]:'?';}
function run(){
  var n=0,exit=false,ops=[];
  while(!exit&&n++<60){
    var pc=st.pc,code=CAM[pc],o=OPS[code];if(!o){st.pc=ENTRY[sel.value];break;}
    var name=o[0],arg=CAM[pc+1];ops.push(name);
    switch(name){
      case 'VEXIT':exit=true;break;
      case 'VSLOOP':st.pc++;st.loop=arg;break;
      case 'VSLOPB':st.pc++;st.loop=(arg==0xB2)?6:2;break;
      case 'VSKIP0':if(st.camsta==0)st.pc+=2;break;
      case 'VSETPC':st.pc=CAM[st.pc+1];break;
      case 'VELOOP':st.loop--;if(st.loop<=0)st.pc++;else st.pc=CAM[st.pc+1];break;
      case 'VBR0PC':st.pc++;if(st.camsta==0)st.pc=CAM[st.pc];break;
      case 'VSMOVE':case 'VSPUMO':case 'VSFUSE':
        if(!st.top){st.y+=0.025;if(st.y>=1){st.y=1;st.top=true;st.pc=ENTRY['TOPPER']-1;st.dir=(st.lane<8)?-1:1;}}break;
      case 'VJUMPS':st.jump=true;st.step=0;break;
      case 'VJUMPM':if(!st.jump){st.jump=true;st.step=0;}st.step++;
        if(st.step>=8){st.lane=(st.lane+st.dir+16)%16;st.jump=false;st.step=0;st.camsta=0;}else st.camsta=1;break;
      case 'VCHROT':st.dir=-st.dir;break;
      case 'VCHPLA':st.dir=(st.lane<8)?-1:1;break;
      case 'VCHKPU':st.camsta=0;break;
      case 'VELTST':st.camsta=1;break;
      case 'VSTRAI':st.camsta=1;break;
      default:break;
    }
    st.pc++;
  }
  st.frame++;log.unshift('frame '+st.frame+': '+ops.join(' '));if(log.length>14)log.pop();
  document.getElementById('trace').textContent='lane '+st.lane+'  depth '+st.y.toFixed(2)+(st.jump?'  flipping '+st.step+'/8':'')+
    (st.top?'  (at rim: TOPPER)':'')+'\n'+log.join('\n');
}
function lanePt(k,y){var a=2*Math.PI*k/16,r=30+140*y*y;return [180+r*Math.cos(a),180-r*Math.sin(a)];}
function drawPic(g,P,Q,vecs,color){
  var ux=(Q[0]-P[0])/8,uy=(Q[1]-P[1])/8,px=-uy,py=ux,x=P[0],y=P[1];g.strokeStyle=color;
  for(var i=0;i<vecs.length;i++){var v=vecs[i],nx=x+v[0]*ux+v[1]*px,ny=y+v[0]*uy+v[1]*py;
    if(v[2]!=0){g.beginPath();g.moveTo(x,y);g.lineTo(nx,ny);g.stroke();}x=nx;y=ny;}
}
function draw(){
  var cv=document.getElementById('well'),g=cv.getContext('2d');g.clearRect(0,0,360,360);g.lineWidth=1;
  g.strokeStyle=PAL[6];
  for(var k=0;k<16;k++){var a=lanePt(k,0),b=lanePt(k,1),c=lanePt(k+1,1),d=lanePt(k+1,0);
    g.beginPath();g.moveTo(a[0],a[1]);g.lineTo(b[0],b[1]);g.lineTo(c[0],c[1]);g.stroke();
    g.beginPath();g.moveTo(a[0],a[1]);g.lineTo(d[0],d[1]);g.stroke();}
  g.lineWidth=1.5;drawPic(g,lanePt(1,1),lanePt(0,1),CLAW,PAL[1]);
  var y=st.y,L=st.lane,P=lanePt(L,y),Q=lanePt(L+1,y);
  if(st.jump){ /* rotate about the pivot corner, over the outside of the well */
    var piv=st.dir>0?Q:P, far=st.dir>0?P:Q, dest=st.dir>0?lanePt(L+2,y):lanePt(L-1,y);
    var a0=Math.atan2(far[1]-piv[1],far[0]-piv[0]),a1=Math.atan2(dest[1]-piv[1],dest[0]-piv[0]);
    var d1=a1-a0;while(d1<=-Math.PI)d1+=2*Math.PI;while(d1>Math.PI)d1-=2*Math.PI;
    var mid=a0+d1/2,cx=piv[0]-180,cy=piv[1]-180;
    if(Math.cos(mid)*cx+Math.sin(mid)*cy<0){d1=d1>0?d1-2*Math.PI:d1+2*Math.PI;}
    var ang=a0+d1*st.step/8,len=Math.hypot(far[0]-piv[0],far[1]-piv[1]);
    var nf=[piv[0]+len*Math.cos(ang),piv[1]+len*Math.sin(ang)];
    if(st.dir>0){P=nf;}else{Q=nf;}
  }
  drawPic(g,P,Q,FLIP,PAL[3]);g.lineWidth=1;
}
reset();draw();
document.getElementById('rst').onclick=function(){reset();draw();};
sel.onchange=function(){reset();draw();};
document.getElementById('pause').onclick=function(){paused=!paused;};
setInterval(function(){if(paused)return;run();draw();},110);
</script>
"""

if __name__ == "__main__":
    r = emit()
    print("shapes_preview.html: %d vector ROM pictures, %d between-point pictures, %d characters, "
          "%d messages x 4 languages, %d CAM scripts" % r)
    print("wrote", OUT)
