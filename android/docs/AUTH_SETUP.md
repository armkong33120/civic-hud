## 0.3.0 entry flow

Native splash → login/signup welcome for unauthenticated sessions. Provider-specific cards are availability indicators and remain disabled until provider routing is registered and verified. If the existing broker config is complete, the welcome screen can open its hosted account flow (which must expose its verified sign-up/sign-in options). No invented provider hints or fake email form is used. Explicit offline entry is a guest session, not successful authentication. Account tokens remain memory-only as described below; process restart therefore requires fresh login once providers are enabled.

# ระบบบัญชี — สิ่งที่เตรียมแล้วและขั้นตอนเปิดใช้

18 ตุลาคม 2026 — เจ้าของยังไม่ได้ลงทะเบียนแอปกับผู้ให้บริการ จึงเตรียม client และสัญญา API โดยยังไม่เปิดล็อกอินจริง

## โค้ดที่มีแล้ว

`identity/AccountClient.kt` ใช้ AppAuth Android 0.11.1 เปิดเบราว์เซอร์ของระบบ ทำ Authorization Code + PKCE รับ callback และแลก code โดยไม่ฝัง client secret ใน APK จากนั้นเรียก backend `/me` ให้เซิร์ฟเวอร์ตรวจ access token ก่อนยอมรับ canonical `accountId` ไม่ใช้เพียงการถอดข้อความ JWT หรือการที่อีเมลเหมือนกันเป็นหลักฐานเข้าสู่ระบบ

หน้าตั้งค่าแสดงสถานะรอการตั้งค่า ใช้แอปออฟไลน์ต่อได้ การสร้างบัญชี/เลือกผู้ให้บริการจะอยู่ใน hosted user flow ของ broker ไม่ใช่ปุ่มแยกที่อ้างว่าเชื่อมผู้ให้บริการสำเร็จแล้ว

Token เก็บในหน่วยความจำเท่านั้น ไม่เขียนลง Room/SharedPreferences/บันทึกระบบ ต้องเข้าสู่ระบบใหม่หลัง process ถูกปิด ยังไม่มี persistent session/refresh หรือ cloud sync ในรุ่นนี้ กรณี token หมดอายุให้เข้าสู่ระบบใหม่ การออกจากระบบล้าง session บนอุปกรณ์ ไม่อ้างว่าล้าง session เบราว์เซอร์ของผู้ให้บริการ

## ตั้งค่า Entra External ID

1. สร้าง customer external tenant และ app registration แบบ public/native client แยกจาก tenant นักศึกษาเดิมตามความเหมาะสม ตรวจสิทธิ์/ค่าใช้จ่ายก่อนเปิดบริการจริง
2. ลงทะเบียน redirect URI `com.civichud.maintenance:/oauth2redirect` ให้ตรงทุกตัวอักษร ถ้า broker ไม่ยอมรับ public-client redirect นี้ ต้องเลือก client/broker หรือ verified App Link ที่รองรับ แล้วแก้ manifest/config ให้ตรง ไม่ใช้ client secret ในมือถือ
3. สร้าง hosted sign-up/sign-in user flow เปิด email/password และผู้ให้บริการที่ลงทะเบียนสำเร็จเท่านั้น ผูกแอปกับ flow
4. ลงทะเบียน protected API และ scope สำหรับบัญชีของแอป Backend ต้องตรวจ JWT signature จาก JWKS, issuer, audience, expiry, scope และปฏิเสธ token ของ API อื่น
5. กรอกค่าที่ไม่ใช่ secret ใน `~/.gradle/gradle.properties` หรือ CI environment-to-property mapping:

```properties
AUTH_DISCOVERY_URL=<HTTPS discovery URL ของ user flow ที่ตรวจสอบแล้ว>
AUTH_CLIENT_ID=<public client application ID>
AUTH_SCOPE=<API delegated scope ที่ /me ยอมรับ>
ACCOUNT_API_URL=<HTTPS API base URL>
```

ค่าที่เว้นว่างทำให้ปุ่มล็อกอินไม่เปิดใช้งาน ไม่ต้องใส่ค่าเดาใน source control BuildConfig ถูกฝังใน APK จึงห้ามใส่ secret แม้เป็นไฟล์ local

## ลงทะเบียนผู้ให้บริการ

- **Google**: สร้าง OAuth consent screen/credentials ที่ broker ต้องการ ตั้ง redirect URI ของ broker ไม่ใช่คาดเดาว่าใช้ callback ของ Android เหมือนกัน หากเปลี่ยนไปใช้ native Credential Manager ภายหลัง ต้องลงทะเบียน Android package + debug/release/Play App Signing certificates และ web client ตามคู่มือแยกต่างหาก
- **Facebook**: สร้าง Meta app เพิ่ม Facebook Login ตั้ง valid OAuth redirect URI ของ broker ทำ app review/โหมด live/สิทธิ์ข้อมูลให้ครบก่อนให้บุคคลภายนอกใช้ เก็บ app secret ใน broker เท่านั้น
- **LINE**: สร้าง LINE Login channel เปิด OpenID Connect ตั้ง callback ของ broker และขอ email permission ถ้าต้องใช้ ทดสอบ custom OIDC interoperability จริง โดยเฉพาะ ES256 และ claims/email กับ Entra ก่อนถือว่ารองรับสำเร็จ ดู `AUTH_PROVIDER_ANALYSIS.md` หากผ่านไม่ได้ให้เลือก broker/bridge ที่ผ่านการทดสอบ ไม่ใส่ channel secret ใน APK
- **อีเมล**: ใช้ hosted user flow ที่ทำ verification, password reset, abuse protection และ rate limit ไม่เก็บรหัสผ่านในแอปหรือฐานข้อมูลรถ

## สัญญา API ที่ client เรียกจริงเมื่อมีการตั้งค่า

Backend ยังไม่ถูกสร้าง/เปิดบริการ คู่มือนี้คือข้อกำหนดสำหรับขั้นต่อไป ไม่ใช่ผลทดสอบเซิร์ฟเวอร์

- `GET /me` + Bearer API access token → `{"accountId":"stable-server-id","displayName":"...","linkedProviders":["google"]}`. Mapping หลักคือ issuer + subject → canonical accountId ไม่ใช่อีเมล
- `POST /account/link-sessions` + Bearer → `{"authorizationUrl":"https://..."}`. URL ต้องเป็น host ของ broker discovery หรือ API ที่ตั้งค่าไว้ Server สร้าง one-time, short-lived link ticket ผูกกับบัญชีเดิม ต้องยืนยันตัวตนเดิมแบบ recent authentication และพิสูจน์บัญชีใหม่, ตรวจ state/nonce/PKCE, transaction/unique issuer-sub mapping และป้องกันการย้าย identity ที่ถูกผูกกับคนอื่น URL ต้องไม่มี access token
- `DELETE /account` + Bearer → รับคำขอแล้วส่ง `{"receiptId":"..."}`. Server ต้อง reauthenticate สำหรับงานสำคัญ, ลบบัญชี/ข้อมูลที่เกี่ยวข้องจริงตามนโยบาย และมีวิธีติดตามคำขอ ไม่ใช้ receipt เป็นหลักฐานว่าลบเสร็จแล้ว ตัวแอปแจ้งเพียงรับคำขอ ข้อมูลในเครื่องยังอยู่
- 401/403 → ไม่ดำเนินการ ให้ผู้ใช้เข้าสู่ระบบ/ยืนยันตัวตนใหม่ ไม่มีการ fallback เป็นสำเร็จ

## ข้อเสนอแนะ

แบบฟอร์มใช้งานได้แล้วและเก็บใน Room เป็น `PENDING` พร้อม syncId/เวลา ผู้ใช้เลือกแชร์ข้อความผ่าน Android Sharesheet ได้ แอปไม่เปลี่ยนเป็น SENT เพียงเพราะเปิด Sharesheet เพราะตรวจการส่งถึงผู้รับไม่ได้ รุ่นนี้ยังไม่ส่งข้อมูลอัตโนมัติขึ้นคลาวด์

หากเพิ่ม endpoint ภายหลัง ให้ใช้ HTTPS + authentication/rate limit, idempotency จาก syncId, explicit diagnostic consent และ receipt ที่ไม่ว่างก่อนเรียก `markFeedbackSent` ห้ามแนบ GPS/ประวัติรถโดยปริยาย

## ก่อนเผยแพร่ Google Play

- ทดสอบ Google/LINE/Facebook/email จริงครบทุก flow: sign-up, cancel, error, network loss, expired session, linking conflict, unlink ที่เหลือช่องทางล็อกอินอย่างน้อยหนึ่งทาง
- ทำ in-app account deletion และหน้าเว็บสาธารณะที่รับคำขอลบบัญชีได้จริง พร้อม privacy policy/data safety ให้ตรงกับพฤติกรรมแอป
- กำหนด retention, ลบข้อมูลที่เชื่อมบัญชี, ตรวจ authorization ข้ามผู้ใช้ และตรวจ backend security ก่อนเปิด endpoint
- ลงทะเบียน release/Play signing certificate, application ID/ชื่อ/ไอคอนแบรนด์จริง และทดสอบ permission/background service กับเครื่องจริง
- การทดสอบจำลอง OBD ไม่ใช่หลักฐานว่า OAuth หรือรถจริงผ่านแล้ว

## แหล่งอ้างอิง

- [AppAuth Android — browser flow / PKCE](https://github.com/openid/AppAuth-Android)
- [Google sign-in และการตั้งค่า](https://developer.android.com/identity/sign-in/credential-manager-siwg)
- [LINE Android integration](https://developers.line.biz/en/docs/line-login-sdks/android-sdk/integrate-line-login/)
- [Google Play account deletion](https://support.google.com/googleplay/android-developer/answer/13327111)
- รายละเอียด Entra และการเปรียบเทียบ: `AUTH_PROVIDER_ANALYSIS.md`
