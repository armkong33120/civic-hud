# รายงานอัปเกรด 0.2.0 — 18 ตุลาคม 2026

## สิ่งที่ใช้งานได้

- หน้าหลักแบบ Material3: เลขไมล์เด่น สถานะรถ/ความเร็ว ระยะรอบนี้ สรุปความเร่งด่วน และบาร์บำรุงรักษาที่ใช้ข้อมูลจริง
- ภาพเคลื่อนไหวสั้นตอนเปลี่ยนหน้า/เลขไมล์/สีและความคืบหน้า รวมถึงรายละเอียดประวัติที่ขยายได้ ไม่มีการลากความเร็วเก่าต่อเมื่อข้อมูลขาด
- สว่าง/มืด/ตามระบบ และลดการเคลื่อนไหว เก็บค่าข้ามการเปิดแอป
- ผู้ใช้กรอกชื่อรถ ยี่ห้อ รุ่น ปี ทะเบียน และเชื้อเพลิงเองได้ ไม่ผูกติดกับ Honda สำหรับการติดตั้งใหม่
- Room1→2 ย้ายข้อมูลเดิมโดยไม่ล้างเลขไมล์หรือประวัติ การแก้โปรไฟล์ไม่แก้ฐานเลขไมล์
- แบบฟอร์มข้อเสนอแนะ: หมวด/หัวข้อ/ข้อความ/อีเมลติดต่อกลับไม่บังคับ/ยินยอมแนบเวอร์ชัน เก็บลงเครื่องเป็น PENDING และเลือกแชร์ผ่านแอปอื่นเองได้ ยังไม่อ้างว่าส่งถึงทีมงาน

## ระบบบัญชีที่เตรียมไว้

AppAuth browser Authorization Code + PKCE, callback/token exchange, HTTPS API verification `/me`, canonical account state, link-session/deletion-request client และหน้าจอที่เปิดเฉพาะเมื่อมี config ครบ ไม่มี client secret และไม่บันทึก token ลงฐานข้อมูล

**ยังไม่ได้เปิดใช้จริง:** เจ้าของยังไม่ลงทะเบียน Google/LINE/Meta/ExternalID และยังไม่มี backend `/me`/link/delete จึงยังไม่ผ่าน live OAuth, การผูกบัญชี หรือการลบข้อมูลจริง ไม่มี cloud sync ในรุ่นนี้ ตัวแอปใช้ออฟไลน์ต่อได้ อ่าน `AUTH_SETUP.md` และ `AUTH_PROVIDER_ANALYSIS.md` ก่อนตั้งค่า โดยเฉพาะการทดสอบ LINE กับ broker

## หลักฐานตรวจสอบ

- Unit tests68รายการ: domain39, data16, app protocol7+account configuration6; ไม่มี failure/error
- Debug และ unsigned release build ผ่าน รวม release vital lint ไม่มี Kotlin warning ใน build log
- อัปเกรด APK บน Samsung SM-S901E Android16 ผ่าน เปิดแอปได้
- เปรียบเทียบตารางรถจริงก่อน/หลัง Room1→2: ทุกคอลัมน์เดิมของรถ/รอบบำรุงรักษา/ประวัติ/notification reservation เท่าเดิมทั้งหมด
- ตรวจ migration บนสำเนาฐานข้อมูลเก่า3ชุด: ข้อมูลเดิมครบ schemaตรงกับ Roomv2 และ integrityผ่าน
- แก้ข้อมูลรถและสร้าง feedback ผ่าน UI ในฐานข้อมูลจำลอง เปิดใหม่ยังอยู่ feedbackเป็น PENDING/receiptว่าง
- จำลอง120กม./ชม.: เลขไมล์เพิ่มเท่ากับระยะคงเหลือที่ลดลง
- ขาด frame6วินาที/NO DATA: ระยะเพิ่ม0 ช่วงขาดข้อมูล; frameแรกเมื่อกลับมาไม่ชดเชยช่องว่าง
- ส่งแอป HOME และ make-uid-idle แล้วรับframeต่อ: serviceยังคำนวณได้; DISCONNECTเข้าสถานะ RECONNECT_WAIT
- ไม่พบ crash record ของ processแอปในการตรวจ crash buffer; ไม่ได้อ้างว่า Android ทั้งเครื่องไม่มีประวัติ crash
- การทดสอบระยะ/โปรไฟล์/feedbackใช้ฐานข้อมูลจำลองเท่านั้น สุดท้ายหยุด service และกลับโหมดรถจริง ไม่เปลี่ยนเลขไมล์จริงเป็นค่าทดสอบ

ผลรายละเอียดใน `verification/UX_UPGRADE_TEST_SUMMARY.json`, `ux-migration-preservation.json` และ `ux-appearance-tests.json` ภาพ/ฐานข้อมูล/บันทึกเครื่องดิบเก็บเฉพาะพื้นที่ทำงาน ไม่เผยแพร่ GitHub

## ขอบเขตที่ยังต้องทดสอบก่อนขาย

รถ/ELM327จริง, OEM background restrictions/deep Dozeเต็มรูปแบบ, GPSfixจริง, live provider loginและlink/delete, backend authorization/privacy/retention, production signing, Play policyและData Safety ยังเป็นงานเปิดบริการขั้นถัดไป การ build releaseผ่านไม่ได้หมายถึงพร้อมวางขายแล้ว

## การทำงานของทีม

Lead รวมหน้าจอ ระบบบัญชี และตรวจเครื่องจริง; Storage Agent ทำ Room/profile/outboxและทดสอบข้อมูล; Research Agent จัดทำรายงานผู้ให้บริการก่อนถึง usage limit; UI Agent เสนอแนวทางก่อนถึง usage limit จึงให้ Leadทำ implementationต่อ ไม่มีการอ้างว่า reviewerหลายรายอนุมัติเมื่อไม่มีหลักฐาน
