# OPS 42 / Android viewport and interaction acceptance

Status: **implementation candidate, not physical-device verified**. NativeActivity is
an offline C11/raylib preview, not a live crawler.

## Field failure reproduced from screenshots

The prior Android build launched raylib at the desktop framebuffer `1350x820`.
On a handset that produces a landscape-sized internal canvas, clipped in
portrait and vertically truncated when rotated. Extra in-app Android scaling
compounded that mismatch. Buttons were wider than the visible phone screen.

## Changes in this candidate

- Initialize Android raylib at a **432x960 portrait logical framebuffer**,
  while leaving the desktop's resizable `1350x820` window unchanged.
- Let raylib/Android handle device framebuffer scaling. The application uses
  `UI_SCALE=1` for both interaction and scissor coordinates on Android.
- Lock Android NativeActivity orientation to portrait until a tested, dynamic
  native resize lifecycle is implemented.
- Prefer a readable Android Roboto/NotoSans system font when accessible;
  retain a bundled raylib fallback.
- Open the populated Atlas by default on Android. Tapping a route jumps
  directly to Evidence Inspector. The Desk tab provides genuine offline actions; disabled network controls are not presented as working Android features.
- Treat **touch release with <14 logical pixels of movement** as a tap.
  Moving more than the threshold scrolls without activating the row.
- Fit button captions to available width rather than spilling outside them.

## Screen evidence / 9 November 2026

A user-provided portrait screenshot shows the Alpha 9 route list rendering all
four Petstore fixture operations with readable GET/POST/DELETE pills and
working active selection. The screenshots exposed an Android-only defect:
the footer notice was placed under the OS navigation buttons.

The following follow-up is **implemented but not yet device-confirmed**:

- A conservative 64-logical-pixel bottom safe area keeps footer notices and
  the interactive panel above three-button Android navigation.
- For small catalogs (1–6 operations), the unused Atlas space now shows
  the selected route and its recorded source metadata. A visible button jumps
  to the Evidence inspector. For larger catalogs, the scrollable route list
  retains all available space.
- Re-test footer placement and tap targets on both three-button and gesture
  navigation, and on a device with a different aspect ratio.

## Physical Android QA (required before production claims)

1. Uninstall an earlier debug APK first: each CI build uses a different
   temporary signing key, so installing over it generally fails.
2. Install the PR's Android-arm64 build artifact. Record device, build and
   actual screen size. Launch and capture a screenshot.
3. Verify the app is portrait, full-screen, no right-side cropping, no large
   black unpainted area below its footer, and all three tabs are visible.
4. Verify **Atlas** loads four local sample operations; tap one route to open
   **Evidence**, with a readable path, method, source and SHA-256.
5. Open **Desk** and confirm the local source and route counts match the
   imported sample. Test **OPEN INCLUDED SAMPLE**, **EXPLORE DOCUMENTED ROUTES**
   and **INSPECT SOURCE EVIDENCE**; confirm each tap triggers only one action.
   Swipe the Atlas list without inadvertently selecting a route.
6. Verify Android presents no live target execution or misleading approval
   controls. For the exported OPS 42 v2 report, use the offline **desktop CLI**:
   `restmore-cli --import examples/openapi-petstore.json --ops-brief --out evidence`.
   Confirm `mission_state=REVIEW_REQUIRED`, `target_execution=false`,
   at least one source, and that the source's local import status is 0.
7. Rotate the phone. The activity should remain portrait (deliberate for
   this candidate). Check system navigation and returning from the launcher.
8. Inspect the runtime log for native crashes or graphics errors.
9. Repeat on a second Android device with a different screen aspect ratio.

Build and debug-signing in CI do **not** substitute for these device checks.
Dynamic sensor rotation and Android soft-keyboard/file-picker/storage-export
support remain separate work items. A physical screenshot must confirm the
source digest and response contract fit without clipping.
