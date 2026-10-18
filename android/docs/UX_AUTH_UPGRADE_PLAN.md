# UX, profile, identity and feedback upgrade — 18 Oct 2026

Owner approved: no Google/LINE/Meta/ExternalID applications registered yet; prepare integration code and setup guide, do not simulate successful sign-in or provision cloud silently.

## Research → design decisions

1. Android Material3 principles: clear hierarchy, theme/contrast, consistent navigation, edge-to-edge/insets and readable touch targets. Use a compact vehicle summary + odometer, connected status, two live metrics, urgency overview and maintenance cards; move technical/retry detail behind secondary emphasis.
2. Compose motion: AnimatedContent for navigation/odometer text transitions, animateColorAsState/animateFloatAsState for actual data-driven status/progress, animateContentSize for expanding details. No idle decorative looping effects. Short transitions~180–320ms; obey platform animation scale and explicit reduced-motion setting. Do not tween stale speed: show unavailable immediately.
3. First use: customizable vehicle profile, dashboard mileage calibration, then paired OBD selection; account creation optional for local core. No fixed Honda model for new installs. Keep old profiles/history through an explicit Room migration.
4. Identity: Azure ExternalID broker is preferred candidate for hosted email/Google/Facebook and LINE via customOIDC pending tenant interoperability spike. Provider registration, redirect URIs and signing certificates are still required. Stable appAccountId + verified issuer/subject mapping; linking needs fresh authentication and server ownership checks; no email-only automatic merging.
5. Feedback: form in settings with category, title/message, optional reply email and diagnostic consent. Durable local queue; only label received after server receipt. Until a verified endpoint exists, clearly show pending/local and allow user-selected sharing. No hidden GPS/OBD/history uploads.
6. Play readiness: account deletion path in app and public web route, privacy policy/data safety aligned with actual data use, provider branding/consent, production signing and permission review. These are release prerequisites, not proof this debug build can be sold immediately.

## Ownership boundaries / current contracts additions

Domain Vehicle append defaulted fields manufacturer:String="",model:String="",modelYear:Int?=null,registration:String="",fuelType:String="". Preserve existing constructor callsites.
VehicleProfileDraft(name:String,manufacturer:String="",model:String="",modelYear:Int?=null,registration:String="",fuelType:String="").
TrackerStore.saveVehicleProfile(draft). Never touch base mileage/checkpoint/history on profile edits.

FeedbackDraft(category:String,title:String,message:String,replyEmail:String="",includeDiagnostics:Boolean=false).
FeedbackEntry(id:Long,syncId:String,category:String,title:String,message:String,replyEmail:String,includeDiagnostics:Boolean,createdAt:Long,status:String,receiptId:String="").
Store feedback: feedback:Flow<List<FeedbackEntry>>, enqueueFeedback(draft):Long, pendingFeedback():List<FeedbackEntry>, markFeedbackSent(id,receiptId), deleteFeedback(id). Never mark sent without nonblank acknowledged receipt. Persist metadata syncId/updatedAt/isDirty.
Room migration1→2 explicit, preserving real and simulation data. Initial vehicle name รถของฉัน, no fake previous services.

UI root owns TrackerScreen, settings/profile/account/feedback dialogs + VM wiring. UI specialist owns Dashboard.kt, TrackerTheme.kt, HistoryScreen.kt and new visual/motion component files. Preserve all existing keytest tags (estimated_odometer,tracking_status,calibrate_odometer,select_adapter,stop_tracking,maintenance_id,service_id,edit_rule_id,add_rule). Dashboard function signature unchanged. Profile summary uses new actual Vehicle fields.
Root UI state will append themeMode:String="system",reduceMotion:Boolean=false,feedback:List<FeedbackEntry>,account:AccountUiState and actions callbacks (root-owned). Dashboard specialist must not edit TrackerUiState.kt or TrackerScreen.kt. Motion compositionlocal LocalReducedMotion root will consume when implementing navigation.

## Sources
- https://developer.android.com/design/ui/mobile
- https://developer.android.com/develop/ui/compose/animation/composables-modifiers
- https://developer.android.com/develop/ui/compose/animation/quick-guide
- https://developer.android.com/identity/sign-in/credential-manager-siwg
- https://developers.line.biz/en/docs/line-login-sdks/android-sdk/integrate-line-login/
- https://support.google.com/googleplay/android-developer/answer/13327111
- Provider analysis with additional primary references: AUTH_PROVIDER_ANALYSIS.md
