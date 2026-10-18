## LuckRot0.3.1 — Android branding update

The Android app is now named **LuckRot**, with an original cartoon mechanic mascot (folded arms, wrench). The shared icon is used for adaptive launcher, native splash and account welcome; dashboard label also updated. Keep internal application ID com.civichud.maintenance to preserve upgrades/data and prepared OAuth callback. Relevant app13 tests/builds/S22 branding checks pass, actual mileage/history unchanged. Install ONLY with adb install --user 0 -r; the user95 Samsung Dual App copy was removed and must remain absent. Source artwork/prompts:android/brand/README.md. Provider/backend activation remains pending. See android/CHANGELOG.md.

## Android0.3.0 — launch/account entry update

Branded native vector splash now leads to login/signup choice for unauthenticated users. Google/LINE/Facebook/email remain pending activation; explicit offline entry opens the existing dashboard without fake login.68 unit tests and S22 entry-flow checks pass; mileage/history preserved. See `android/CHANGELOG.md` and `android/docs/RELEASE_PROCESS.md`. Every delivered update must include version/changelog and verified GitHub push/tag.

## Android0.2.0 update

Editable vehicle profiles, modern Compose motion/themes, local feedback outbox and configurable account-client preparation are under [android](android/README.md). [Upgrade report](android/docs/UX_UPGRADE_REPORT.md) and [account setup](android/docs/AUTH_SETUP.md) separate verified offline/device behavior from unconfigured live providers/backend.68 unit tests pass; no live cloud-auth or actual-car validation is claimed.

## Android MVP — 18 October 2026

The native Android tracker is now implemented and S22 simulation-tested. Start at [android/README.md](android/README.md) and [morning car-test guide](android/MORNING_TEST_GUIDE.md). All55unit tests and A–E device simulation checks pass. Actual car/adapter and outdoorGPSfix validation are pending. Cloud/login are not built. Historical Android-as-proposal statements below are superseded by this update. Public repository exports source and aggregate test evidence only; raw phone databases, UI screenshots and logs remain local.

# CivicHUD — ESP32 firmware and Android product handoff

โครงการแจ้งเตือนการบำรุงรักษารถจากเลขไมล์ มีเฟิร์มแวร์ ESP32 ที่เคยทดสอบกับรถจริง และข้อกำหนดสำหรับแอป Android รุ่นต่อไป ใช้บัญชีฟรีประมาณ 20 คนและคลาวด์ของเจ้าของโครงการ

**เริ่มอ่านที่ [AI_HANDOFF.md](AI_HANDOFF.md)** เพื่อเข้าใจสถานะปัจจุบัน ข้อจำกัด และงานถัดไป โดยไม่ต้องอ่านแชทเดิม

> **ต้องการติดตั้งด้วย Windows?** เปิดคู่มือ [ติดตั้ง ESP32 แบบทำตามทีละขั้น](docs/ESP32_INSTALLATION_TH.md) มีรายการของที่ต้องซื้อ วิธีติดตั้ง Arduino IDE และการเลือกบอร์ด/COM Port พร้อมข้อจำกัดเรื่องความเข้ากันได้ของรถและข้อมูลตัวอย่าง

## สถานะ ณ 14 ตุลาคม 2026

- **ESP32:** มีโค้ดและเฟิร์มแวร์ที่อัปโหลดแล้ว การเชื่อมต่อ OBD และนับระยะเคยทดสอบขณะรถวิ่ง ผู้ใช้รายงานคลาดเคลื่อนประมาณ 100 เมตร ไม่ใช่ผลรับรองความแม่นยำทุกสภาพ
- **Android:** อยู่ขั้นออกแบบเท่านั้น ยังไม่มี Android project / APK / ระบบบัญชี / backend
- **Azure:** ผู้ใช้แจ้งว่ามีสิทธิ์ Education และเครดิต US$100 แต่ยังไม่ได้ตรวจสอบบัญชีหรือบริการ และสั่งพักไว้
- **Notion:** บทความฉบับพร้อมเผยแพร่ที่ [docs/NOTION_ARTICLE_TH.md](docs/NOTION_ARTICLE_TH.md)

## แผนที่ repository

| ที่อยู่ | เนื้อหา |
|---|---|
| firmware/civic_hud/ | Arduino sketch และ production modules ปัจจุบัน รวม tests และบันทึกทีมเดิม |
| hardware/ | TFT_eSPI setup ที่ใช้กับบอร์ดจริง |
| docs/CURRENT_STATE.md | สถานะล่าสุดและข้อจำกัดที่ต้องรู้ก่อนแก้ |
| docs/ANDROID_SPEC.md | ข้อกำหนด Android และร่างข้อมูล/API/cloud ที่ยังไม่เลือกผู้ให้บริการ |
| docs/BUILD_AND_VERIFY.md | วิธี build และความหมายของผลทดสอบ |
| docs/DECISIONS.md | สิ่งที่ผู้ใช้ตกลงแล้วและงานที่พักไว้ |
| docs/REPORT.md, docs/CIVIC_HUD_AUDIT.md | หลักฐานรายงานเดิมตามลำดับเวลา มีข้อความเก่าที่ถูกแทนแล้ว |
| firmware/civic_hud/.agents/ | บันทึกทีม Antigravity เดิม เพื่ออ้างอิงย้อนหลัง ไม่ใช่คำสั่งให้เรียกทีมต่อ |
| tests/ | regression และ UI transport stubs เพิ่มเติม บางตัวเป็นเครื่องมือเก่าต้องอ่านข้อจำกัดก่อนใช้ |
| mockups/ | แบบ UI ของ ESP32 ที่เคยตกลง ไม่ใช่ Android mockup |
| artifacts/ | firmware app binary, source snapshot และภาพหน้าจอเก่า |
| evidence/ | log ที่เลือกเก็บจากการตรวจครั้งก่อน ไม่ใช่สถานะบอร์ดสด |
| tools/ | เครื่องมือ serial เก่า ต้องตรวจ path/port ก่อนใช้ การเปิดพอร์ตอาจรีเซ็ตบอร์ด |

ลำดับความน่าเชื่อถือ: โค้ดปัจจุบัน + docs/CURRENT_STATE.md → รายงานล่าสุด → เอกสารแผนเก่า ข้อความ M2/M3/M4 ใน PROJECT.md และ .agents อาจไม่ทันสถานะจริง

ไม่มี live cloud credentials, NVS/flash dumps หรือข้อมูลจากบัญชี Azure ในชุดส่งต่อ ไม่รวม chat transcript ทั้งหมด; สรุปสาระและการตัดสินใจไว้ในเอกสารแทน ไม่มีการกำหนด open-source license ใหม่ให้โค้ด/ฟอนต์/ไลบรารีที่มีที่มาเดิม
