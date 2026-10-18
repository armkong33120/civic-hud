# LuckRot brand assets — 0.3.1

Owner request: rename the app LuckRot and replace the mark with an original cartoon mechanic standing with folded arms, holding a wrench.

## Files and usage

- `luckrot-mascot-standing.png`: generated full-body source mascot; transparent PNG. Marketing/reference source, not bundled in the APK.
- `luckrot-mascot-icon-master.png`: generated compact same-character waist-up rendition for small-icon readability; transparent PNG. Source, not bundled in the APK.
- `../app/src/main/res/drawable-nodpi/luckrot_mascot.png`: mechanically downsampled512px runtime export, preserving aspect and alpha. Used in launcher, native splash and welcome screen. APK does not bundle the large masters.
- Launcher: adaptive forest-green background + inset mascot foreground, preserving circular-mask safe space. System native splash uses the same mascot and existing short fade exit; no forced loading timer.

Generated with the built-in `imagegen` tool; no CLI/API fallback. The second tool call edits/reframes the first generated mascot. No external reference character or third-party brand logo was supplied. Original subject, framing and transparent background were checked visually. PNG alpha and exported size were checked locally.

## Exact prompt 1 — full mascot

Use case: logo-brand. Asset type: Android app icon mascot for an automotive maintenance app named LuckRot. Primary request: one original cartoon auto mechanic standing confidently with arms folded across the chest, one hand visibly gripping a silver open-ended wrench that projects upward beside the shoulder. Show the entire standing character, including boots, in a compact centered silhouette. Friendly, capable adult mechanic with a small smile, short dark hair, clean work overalls and simple shirt. Style: polished modern 2D cartoon mascot, bold clean outlines, simple broad color shapes, restrained shading, no tiny texture. Designed to remain recognizable at small launcher-icon sizes. Composition: single isolated full-body mascot centered, subject fills about 82 percent of canvas height, all body parts and wrench fully inside canvas with clear safe margins. True transparent background. No text, no letters, no wordmark, no scenery, no badge, no border, no watermark, no extra people, no extra limbs. Arms visibly crossed while holding the wrench, not two separate unrelated poses. Colors should work on the app's existing deep forest-green background; use contrasting light gray/cream workwear details and metal wrench.

## Exact prompt 2 — app icon

Use case: logo-brand. Edit target: the attached mechanic mascot. Create the compact square Android app-icon rendition of EXACTLY this same original mechanic, preserving his facial identity, hairstyle, cream shirt, forest-green overalls, friendly expression, crossed arms and silver wrench held clearly upright. Reframe from the waist upward, centered and front-facing in his standing crossed-arm pose. Keep both folded arms, hands, head and entire wrench fully visible. Strong clean cartoon outline and broad readable shapes for tiny launcher icons; simplify small clothing textures. No text, no letter, no badge, no border, no scenery, no vignette or glow. True transparent background. Square canvas with generous transparent safe space on all sides for Android circular masking. The head/arms/wrench silhouette should fit inside a centered circular safe zone, no clipping at corners.
