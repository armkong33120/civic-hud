import time,sys,re
from pathlib import Path
sys.path.insert(0,"/Users/arm/.platformio/penv/lib/python3.13/site-packages")
import serial
from PIL import Image
s=serial.Serial();s.port='/dev/cu.usbserial-11220';s.baudrate=115200;s.timeout=.2;s.exclusive=True;s.dtr=False;s.rts=False;s.open();time.sleep(3)
log=bytearray()
def cmd(v,seconds=1):
 s.write((v+'\n').encode());s.flush();end=time.monotonic()+seconds;b=bytearray()
 while time.monotonic()<end:b.extend(s.read(s.in_waiting or 1))
 log.extend(b);return bytes(b)
def capture(name):
 s.reset_input_buffer();s.write(b'CMD:SNAPSHOT\n');s.flush();limit=time.monotonic()+10
 while time.monotonic()<limit:
  h=s.readline()
  if h.startswith(b'SNAPSHOT_RGB888 '):break
 else:raise RuntimeError('No header')
 n=int(h.split()[3]);data=bytearray();limit=time.monotonic()+45
 while len(data)<n and time.monotonic()<limit:data.extend(s.read(n-len(data)))
 if len(data)!=n:raise RuntimeError('Short capture')
 Image.frombytes('RGB',(240,320),bytes(data)).save('outputs/'+name+'.png');print(name,flush=True)
 log.extend(s.readline());log.extend(s.readline())
page=cmd('CMD:WEB_PAGE',3);Path('work/device_web_page.log').write_bytes(page)
cmd('CMD:UI_PAGE 1');capture('device_page2')
cmd('CMD:UI_OPEN oil');capture('device_modal')
cmd('CMD:TOUCH 36 250 1');cmd('CMD:TOUCH 120 250 1');capture('device_slider')
cmd('CMD:TOUCH 120 250 0');cmd('CMD:TOUCH 205 250 1');cmd('CMD:TOUCH 205 250 0')
state=cmd('CMD:STATE');Path('work/no_false_confirm.log').write_bytes(state)
# Actual full gesture drives production controller, then production service handler.
cmd('CMD:TOUCH 36 250 1');cmd('CMD:TOUCH 205 250 1');cmd('CMD:TOUCH 205 250 0');capture('device_confirmed')
cmd('CMD:WEB_SAVE odo=98540&km_oil=15000&vis_cvt=0&new_name=%E0%B8%97%E0%B8%94%E0%B8%AA%E0%B8%AD%E0%B8%9A&new_km=12345')
state=cmd('CMD:STATE');Path('work/persistence_before_reset.log').write_bytes(state)
cmd('CMD:DUMP_STORAGE');Path('work/device_sequence.log').write_bytes(log)
s.close()
