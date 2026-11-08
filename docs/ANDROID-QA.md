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
  directly to Evidence Inspector. The Scope tab remains accessible.
- Treat **touch release with <14 logical pixels of movement** as a tap.
  Moving more than the threshold scrolls without activating the row.
- Fit button captions to available width rather than spilling outside them.

## Physical Android QA (required before production claims)

1. Uninstall an earlier debug APK first: each CI build uses a different
   temporary signing key, so installing over it generally fails.
2. Install the PR's Android-arm64 build artifact. Record device, build and
   actual screen size. Launch and capture a screenshot.
3. Verify the app is portrait, full-screen, no right-side cropping, no large
   black unpainted area below its footer, and all three tabs are visible.
4. Verify **Atlas** loads four local sample operations; tap one route to open
   **Evidence**, with a readable path, method, source and SHA-256.
5. Return to **Scope**, vertically swipe through the whole form without
   accidentally activating a button. Tap a button deliberately and confirm
   exactly one action is handled.
6. Verify no target request is made by loading an offline OpenAPI file and
   exporting OPS 42; the resulting brief must say REVIEW_REQUIRED and
   target_execution=false.
7. Rotate the phone. The activity should remain portrait (deliberate for
   this candidate). Check system navigation and returning from the launcher.
8. Inspect the runtime log for native crashes or graphics errors.
9. Repeat on a second Android device with a different screen aspect ratio.

Build and debug-signing in CI do **not** substitute for these device checks.
Dynamic sensor rotation and Android soft-keyboard/file-picker support remain
separate work items.
