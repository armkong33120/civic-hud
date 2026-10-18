## Android0.3.0 — launch/account entry update

Branded native vector splash now leads to login/signup choice for unauthenticated users. Google/LINE/Facebook/email remain pending activation; explicit offline entry opens the existing dashboard without fake login.68 unit tests and S22 entry-flow checks pass; mileage/history preserved. See `android/CHANGELOG.md` and `android/docs/RELEASE_PROCESS.md`. Every delivered update must include version/changelog and verified GitHub push/tag.

## Latest Android UX/account preparation — 18 October 2026

Android0.2.0 adds editable vehicle profiles, persisted appearance/reduced motion, modern Compose transitions and durable local feedback.68 unit tests and upgraded-S22 simulation/migration checks pass. Browser OIDC/PKCE account client and Google/LINE/Facebook/email setup requirements are prepared; providers/backend are NOT registered or live. Offline core works without an account. Read `android/docs/UX_UPGRADE_REPORT.md`, `android/docs/AUTH_SETUP.md`, `android/docs/AUTH_PROVIDER_ANALYSIS.md`. This supersedes old “no auth code” wording only for client preparation; no cloud provisioning/sync/live authentication is claimed.

# การตัดสินใจและขอบเขตของผู้ใช้

## ยืนยันแล้ว

1. จุดประสงค์หลักคือเลขไมล์/ระยะที่เชื่อถือได้ + แจ้งเตือนบำรุงรักษา ไม่ใช่เพิ่มเกจทุกชนิด
2. อ่านแผน/โค้ดเดิมก่อน ทำต่อจากที่หยุด ไม่เริ่มทุกอย่างใหม่ ตัดการทดสอบที่ไม่จำเป็นแต่ต้องตรวจสิ่งที่แก้จริง
3. บาร์ยาวหมายถึงใช้รอบไปมาก ใกล้เปลี่ยน สีและกระพริบตาม CURRENT_STATE
4. UI กลางวัน/กลางคืน ไม่มีไอคอนตกแต่ง ใช้ดำ/เทา/ขาว/เขียว/เหลือง/แดง ระดับละ8เฉดภายใต้จอ RGB565
5. Android สำหรับผู้ใช้ประมาณ20คน ใช้ฟรี แต่ละคนมี OBD2 ของตัวเอง ล็อกอินและเก็บข้อมูลบนคลาวด์ของเจ้าของโครงการ
6. ตอนบันทึก service อาจถามตำแหน่งที่เปลี่ยน พร้อมชื่อช่าง/เบอร์โทร/หมายเหตุ เป็นตัวเลือกข้ามได้
7. เก็บ code+context ใน GitHub และบทความใน Notion เพื่อให้ AI ตัวอื่นทำต่อได้

## พักไว้ / ต้องตัดสินใจภายหลัง

- Azure Education US$100 เป็นคำแจ้งของผู้ใช้ ยังไม่ได้ตรวจ balance/expiry/สิทธิ์ ห้ามอ้างว่า provision หรือ verify แล้ว
- ยังไม่เลือกผู้ให้บริการ auth/database/backend, ชื่อแอป, supported Android versions, วิธีแจกแอป หรือ policy หลายมือถือคันเดียว
- SD migration, RTC และการเปลี่ยนสายเชื่อมรถเป็น CAN/K-Line ยังไม่ใช่งานที่ให้ทำ
- Engine-off: ผู้ใช้บอกหยุดลองเชื่อมเพิ่ม บอร์ดมี auto-connect ตาม firmware แต่ไม่ส่งคำสั่ง active scan/connect จากเครื่องพัฒนา

## ลำดับต่อไปสำหรับ Android (ข้อเสนอ)

1. สรุปหน้าจอและข้อมูล, กำหนด adapter/ECU capability detection
2. สร้าง vertical slice offline: connect → valid ECU → distance → local checkpoint → maintenance countdown
3. ทดสอบขับจริง/ดับจอ/ขาด Bluetooth/เปิดใหม่/ECU reset โดยไม่ทำระยะซ้ำหรือหายเงียบ
4. เพิ่มบัญชีและซิงก์พร้อมการแยกเจ้าของข้อมูล จากนั้น optional location/history
5. เลือกและประเมิน Azure เมื่อผู้ใช้ให้กลับมาทำ ไม่ทำ load balancer/microservices ล่วงหน้าเพราะมี20คน
