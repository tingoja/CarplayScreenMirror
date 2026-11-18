import os
import shutil
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
WORKSHOP_UNIV = os.path.join(ROOT, "mod", "Steam_Workshop_Hazirlik", "universal")
DIST_DIR = os.path.join(ROOT, "dist")
RELEASE_DLL = os.path.join(ROOT, "src", "x64", "Release", "CarplayScreenMirror.dll")

os.makedirs(DIST_DIR, exist_ok=True)

# 1. Copy compiled DLL to dist/ and root (for GitHub repo compatibility)
if os.path.exists(RELEASE_DLL):
    shutil.copy2(RELEASE_DLL, os.path.join(DIST_DIR, "CarplayScreenMirror.dll"))
    shutil.copy2(RELEASE_DLL, os.path.join(ROOT, "CarplayScreenMirror.dll"))
    print("Copied CarplayScreenMirror.dll to dist/ and root")

# 2. Build CarplayScreenMirror.scs from mod/Steam_Workshop_Hazirlik/universal/
scs_path = os.path.join(DIST_DIR, "CarplayScreenMirror.scs")
with zipfile.ZipFile(scs_path, "w", zipfile.ZIP_STORED) as zf:
    for folder, _, files in os.walk(WORKSHOP_UNIV):
        for fn in sorted(files):
            full_p = os.path.join(folder, fn)
            rel_p = os.path.relpath(full_p, WORKSHOP_UNIV).replace("\\", "/")
            zf.write(full_p, rel_p)

print(f"Created {scs_path} ({os.path.getsize(scs_path)} bytes)")

# 3. Build GitHub Release ZIP (CarplayScreenMirror_v1.4.0.zip)
zip_path = os.path.join(DIST_DIR, "CarplayScreenMirror_v1.4.0.zip")
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    zf.write(os.path.join(DIST_DIR, "CarplayScreenMirror.dll"), "CarplayScreenMirror.dll")
    zf.write(os.path.join(DIST_DIR, "CarplayScreenMirror.scs"), "CarplayScreenMirror.scs")
    zf.write(os.path.join(DIST_DIR, "Kurulum_ve_Kullanim_Rehberi.txt"), "Kurulum_ve_Kullanim_Rehberi.txt")

print(f"Created {zip_path} ({os.path.getsize(zip_path)} bytes)")
