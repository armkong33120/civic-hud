import time,sys,json
sys.path.insert(0,"/Users/arm/.platformio/penv/lib/python3.13/site-packages")
import serial
from pathlib import Path
s=serial.Serial()
s.port='/dev/cu.usbserial-11220';s.baudrate=115200;s.timeout=0.2;s.dtr=False;s.rts=False;s.open();time.sleep(3)
def command(line,seconds=2):
 s.write((line+'\n').encode());s.flush();end=time.monotonic()+seconds;b=bytearray()
 while time.monotonic()<end:
  b.extend(s.read(s.in_waiting or 1))
 return bytes(b)
if sys.argv[1]=='capture':
 out=Path(sys.argv[2]);s.reset_input_buffer();s.write(b'CMD:SNAPSHOT\n');s.flush()
 header=b'';limit=time.monotonic()+8
 while time.monotonic()<limit:
  header=s.readline()
  if header.startswith(b'SNAPSHOT_RGB888 '):break
 else:raise RuntimeError('snapshot header missing '+repr(header))
 n=int(header.split()[3]);data=bytearray();limit=time.monotonic()+45
 while len(data)<n and time.monotonic()<limit:data.extend(s.read(n-len(data)))
 if len(data)!=n:raise RuntimeError('short snapshot '+str(len(data)))
 out.with_suffix('.rgb').write_bytes(data)
 from PIL import Image
 Image.frombytes('RGB',(240,320),bytes(data)).save(out)
 print(str(out))
else:
 duration=float(sys.argv[3]) if len(sys.argv)>3 else 3
 result=command(sys.argv[1],duration)
 Path(sys.argv[2]).write_bytes(result)
 print(result.decode('utf-8',errors='replace')[:3000])
s.close()
