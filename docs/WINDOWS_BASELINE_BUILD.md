# Isolated Windows ROM-backed build

**Purpose:** Rebuild the authenticated historical Japanese-ROM host from the CV3DCJP checkout without touching the working `C:\NESDev\AkumajouDensetsu\build-5h8f` installation.

Run the repository script `tools/windows/Build-CV3DCJP-IsolatedBaseline.ps1` from PowerShell, with a Visual Studio C++ toolchain, CMake, the pinned external NESRecomp checkout, and the owner's original Japanese ROM.

## Required inputs

- Repository: `C:\NESDev\CV3DCJP-Repo` (after pulling the current `main`)
- ROM: `C:\NESDev\AkumajouDensetsu\Akumajou Densetsu (Japan).nes`, expected size 393232 bytes, SHA-256 `958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28`
- NESRecomp: `C:\NESDev\nesrecomp` containing `runner\cyc\project.cmake`
- MSVC: the historical build used the CMake generator `Visual Studio 18 2026`. Set `-Generator` explicitly if your installed generator differs.
- ROM and generated private assets stay on the owner's machine; never commit them.

## Commands

```powershell
Set-Location C:\NESDev\CV3DCJP-Repo
git switch main
git pull --ff-only origin main

$script = '.\tools\windows\Build-CV3DCJP-IsolatedBaseline.ps1'
& $script -Action Validate
& $script -Action Build
```

Build outputs are created only under `C:\NESDev\CV3DCJP-Builds\5h8f-original` (or a separately supplied safe `-BuildDir`). The script refuses an unmarked existing directory and protects the previous game, ROM source tree and engine checkout. The original game build and private recomp-ui checkout are not modified. The UI is deliberately disabled for this first original-ROM baseline check.

On a successful build, the script runs CTest and reports results. Compilation is **not** evidence of live original gameplay or native graphical parity.

To run the isolated executable **only after successful Build**, use:

```powershell
& $script -Action Launch
```

The launch explicitly requests `--edition original --room-view 0` to avoid enhanced display effects. Validate original gameplay separately: title screen, Trevor movement, attacks, collision, items, enemies, room transitions and audio. Attach actual Windows output before declaring the baseline accepted.

## Owner confirmation (2026-10-31)

The owner reported that the Japanese ROM **loads and plays as natively intended** after following the isolated build/launch workflow. Record this as **user-observed original-ROM gameplay functionality**, not as an independent pixel-perfect parity result or a verified widescreen/native renderer. No individual build/CTest console log or complete-stage gameplay checklist is stored in this repository yet.

The validated scope is the working ROM-backed original game on the owner's PC. Preserve it as the functional reference; keep later native-renderer experiments isolated. Future rebuilds must retain the ROM SHA gate and avoid modifying either the existing game installation or the private UI checkout.

## Important limits

This script has repository-level static tests but has **not** itself been run with MSVC or an actual Windows game window during CI. No code from W8–W13 has been imported as verified gameplay. If CMake fails, capture the first error without touching the previous working build.
