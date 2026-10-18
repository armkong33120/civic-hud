## Android update — 18 October 2026

See `../android/README.md` for the implemented Android MVP and physical S22 simulation results. This page below records the historical ESP32 snapshot. Android cloud/auth remain unimplemented; car testing remains pending.

# สถานะปัจจุบันที่ใช้ส่งต่อ — 14 ตุลาคม 2026

## Hardware

ESP32-2432S028R (CYD), ESP32-D0WD-V3 dual core 240 MHz, SRAM รวม 520 KB, flash 4 MB, ไม่มี PSRAM/GPU แยก จอ ILI9341 2.8 นิ้ว 240×320 RGB565 (65,536 สี) และ XPT2046 resistive touch ไม่ใช่ ESP32-C6

## การอ่านรถและเลขไมล์

ทดสอบก่อนหน้านี้กับ Civic FC 1.8 และ ELM327 v1.5 ชื่อ OBDII ใช้ Classic Bluetooth SPP, PIN default 1234 พบ protocol A7: ISO15765-4 CAN 29-bit / 500 kbps, functional header 18DB33F1, ECU source 0E

อ่าน 010D speed, 010C RPM, 0105 coolant และ 0131 distance since DTC clear ได้จริงในช่วงทดสอบ รถสตาร์ทเคยอ่าน ATRV ราว 14.2–14.6 V แยก btConnected กับ ECU-valid ไว้คนละสถานะ

ฐานหน้าปัดที่ผู้ใช้ให้สำหรับรอบขับทดสอบคือ 298,785 กม. จับคู่กับ PID31 4,182 กม. หลังขับผู้ใช้รายงานว่าคลาดเคลื่อนประมาณ 100 เมตร ± ไม่ใช่การสอบเทียบทุกคันและไม่รับประกันช่วงที่มือถือ/อุปกรณ์ขาดการเชื่อมต่อ จุดตรวจล่าสุดในรายงานคือ 298,958 กม.; เป็นข้อมูลเก่า ไม่ใช่เลขไมล์ปัจจุบัน

แก้ command + CR เป็นการส่งชุดเดียว แก้ baseline เมื่อ NO DATA/reconnect ป้องกันระยะซ้ำ ใช้ NVS checkpoint คู่พร้อม CRC และมี speed integration fallback ต้องอ่าน implementation ก่อนย้ายไป Android

## UI ที่ใช้อยู่

- ไม่มี emoji/ไอคอนตกแต่งในชื่อรายการบนจอ ผ่าน plainName โดยไม่แก้ stored history
- day/night: แตะหัวจอสลับ เริ่มต้นกลางคืนทุก boot ไม่มี auto-theme ตามค่าความสว่าง
- LDR raw ประมาณ 300–3600 ปรับ PWM ภายในธีม: night 30–90, day 130–255 ไม่ใช่ lux
- 4-bit partial sprite 240×48 = 5,760 bytes, วาด header/card/footer/modal เฉพาะเมื่อเปลี่ยน
- บาร์ 24 เฉดใน RGB565 วาดตรงเฉพาะบาร์: เขียวเข้ม→อ่อน 8 / เหลืองอ่อน→เข้ม 8 / แดงอ่อน→เข้ม 8
- ช่วง used: 0–<60% เขียว; 60–<85% เหลือง; 85–95% ไล่แดง; ≥95% แดงเข้ม
- intervalKm > 0 และ kmLeft ≤500: แดงเข้มกระพริบทุก 500 ms รวม overdue จนบันทึก service ใหม่; time-only ไม่ใช้เกณฑ์ 500 กม.
- แก้ปุ่มกลับ/ปิด modal และ slider แตะลากถึงอย่างน้อย 85% แล้วปล่อยยืนยัน
- ค่า last-service เก่าหลายรายการยังไม่ได้รับการยืนยันจากเจ้าของรถ จึงอาจเห็น overdue มหาศาล ไม่มีการสร้าง service ปลอมเพื่อแก้ภาพ

## Hotspot และ monitor

แตะกลาง footer เข้า CPU/RAM เปิด hotspot ผ่านปุ่มในหน้านี้ Worker หยุด BT ก่อนเปิด AP เพื่อลดการชน RAM ปิด AP จึงให้ BT ทำงานต่อ SSID CivicHUD, IP192.168.1.1, ไม่มี password ตาม config ปัจจุบัน จึงควรใช้ในพื้นที่ควบคุม และประเมิน authentication ก่อนนำรูปแบบนี้ไปเผยแพร่

Hardware log ยืนยันเปิด AP state2/paused1 และปิดกลับได้; ยังไม่ได้ทดสอบ HTTP ผ่านโทรศัพท์จริง เปิด AP เคย free heap108,204bytes; หลังปิด AP เหลือ42,564bytes (~81.6% used) มีหน่วยความจำ network คงอยู่จน reboot ไม่ได้พิสูจน์ว่าเป็น leak

Monitor sample RAM1s/CPU5s ไม่มี task/log/network เพิ่ม แสดง RAM เป็นเปอร์เซ็นต์ของ allocatable heap (~231KB) ไม่ใช่ 520KB SRAM ทั้งชิป ค่า min/max เป็นช่วงหลัง boot เพราะการเปิด serial อาจ reset

## Snapshot ล่าสุด

รายงาน compile: โปรแกรม2,090,420bytes (~66% ของ app partition3MB), static globals71,188bytes (~21.7% ของ327,680byte link budget) ขนาดไฟล์ .bin อาจมี padding จึงไม่เท่าตัวเลขโปรแกรม หลัง bootราว21s: heap used73.2%, free61,844bytes, CPU1ประมาณ0.6–2.3%, BT1/ECU0 สอดคล้องผู้ใช้ดับเครื่องแล้ว ไม่ใช่หลักฐาน ECU ผ่านในรอบนี้

Firmware binary เก็บใน artifacts; เป็น app image ไม่ใช่ merged full-flash image ห้ามเขียนที่ address0 โดยเดาว่าเป็นทั้ง flash ใช้ build/upload ผ่าน Arduino CLI ตามคู่มือ

## สิ่งที่ยังไม่เสร็จ/ข้อจำกัด

- Android/login/cloud ยังไม่มี implementation; Azure รอผู้ใช้ให้กลับมาตรวจ
- ยังไม่ได้ยืนยันประวัติ service จริงทุกชิ้น, HTTP ผ่านลูกข่ายจริง, ความเสถียรทุกสภาพรถ และความแม่นยำทุก adapter
- RTC แบบใส่ถ่านและ K-Line transceiver เป็นการหารือ ไม่ได้ติดตั้ง Civic ที่ทดสอบใช้ CAN; vendor K-Line firmware สำหรับ Honda motorcycle ไม่ใช่ drop-in
- SD32GB ผู้ใช้เสียบและแจ้งไว้ แต่สั่งพักงาน SD ยังไม่ย้าย code/data ไปการ์ด
- อย่ารายงานสถานะจากบอร์ดแบบสดโดยอ้าง log เก่า
