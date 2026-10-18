"""ADB helpers for the debug-only, isolated CivicHUD simulation. Never locks the device."""
import subprocess,shlex,time,json,re,xml.etree.ElementTree as ET
from pathlib import Path
PKG='com.civichud.maintenance'
ACTION=PKG+'.DEBUG_OBD_INJECT'
OUT=Path(__file__).resolve().parents[1]/'verification'
OUT.mkdir(exist_ok=True)
def adb(*args,raw=False):
 r=subprocess.run(['adb',*args],stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
 return r.stdout if raw else r.stdout.decode(errors='replace')
def shell(*args):return adb('shell',shlex.join(map(str,args)))
def broadcast(command='FRAME',**extras):
 args=['am','broadcast','--receiver-foreground','-a',ACTION,'-n',PKG+'/.debug.DebugObdReceiver','--es','command',command]
 for k,v in extras.items():args+=['--es',k,str(v)]
 return shell(*args)
def dump(name='window'):
 shell('uiautomator','dump','/sdcard/civic-hil.xml')
 data=adb('exec-out','cat','/sdcard/civic-hil.xml')
 (OUT/(name+'.xml')).write_text(data)
 return ET.fromstring(data)
def find(root,tag=None,text=None):
 for n in root.iter('node'):
  if (tag is not None and n.get('resource-id')==tag) or (text is not None and n.get('text')==text):return n
 raise AssertionError('Missing UI element '+str(tag or text))
def tap_node(n):
 x1,y1,x2,y2=map(int,re.findall(r'\d+',n.get('bounds')))
 shell('input','tap',(x1+x2)//2,(y1+y2)//2)
def tap(tag=None,text=None,name='window'):tap_node(find(dump(name),tag,text))
def enter(tag,value):
 tap(tag);shell('input','keyevent','KEYCODE_MOVE_END');shell('input','keyevent','--longpress','KEYCODE_DEL');shell('input','text',str(value))
def state(name):
 broadcast('STATE');time.sleep(.15)
 lines=adb('logcat','-d','-t','400','-s','CivicTracker:I')
 values=[]
 for line in lines.splitlines():
  at=line.find('{')
  if at>=0:
   try:
    obj=json.loads(line[at:])
    if obj.get('event')=='DEBUG_STATE':values.append(obj)
   except ValueError:pass
 assert values,'No DEBUG_STATE found'
 result=values[-1];(OUT/(name+'.json')).write_text(json.dumps(result,indent=2));return result
def inject_for(raw,seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  began=time.monotonic();broadcast(raw=raw);time.sleep(max(0,.5-(time.monotonic()-began)))
def screenshot(name):
 (OUT/(name+'.png')).write_bytes(adb('exec-out','screencap','-p',raw=True))
if __name__=='__main__':
 import sys
 if sys.argv[1]=='dump':
  for n in dump(sys.argv[2] if len(sys.argv)>2 else 'window').iter('node'):
   if n.get('text') or n.get('resource-id'):print(n.get('resource-id'),n.get('text'),n.get('bounds'))
 elif sys.argv[1]=='state':print(state(sys.argv[2]))
 elif sys.argv[1]=='broadcast':print(broadcast(sys.argv[2],**dict(a.split('=',1) for a in sys.argv[3:])))
