from pathlib import Path
import os,subprocess
out=Path(__file__).resolve().parent
ini=out/"dkb.ini"
dll=out/"nvngx_dlss.dll"
total=0
env={k:v for k,v in os.environ.items() if not k.startswith(("DKB_","TOTK_"))}
def run(platform,initial,expect,mode="view",env_selected=None,after=None):
    global total
    ini.write_text(initial,encoding="ascii")
    e=env.copy()
    if env_selected:e["DKB_UPSCALER"]=env_selected
    subprocess.run([out/f"check-{platform}.exe",platform,expect,mode],env=e,check=True)
    if after is None:assert ini.read_text(encoding="ascii")==initial
    else:assert f"upscaler = {after}" in ini.read_text(encoding="ascii")
    total+=1
for present in (False,True):
    if present:dll.write_bytes(b"presence fixture")
    else:dll.unlink(missing_ok=True)
    for platform in ("windows","native"):
        for lang in ("en","ru"):
            base=f"device_profile = desktop\nmenu_lang = {lang}\n"
            for preset in ("3","4","5","recommended"):
                run(platform,base+f"preset = {preset}\n","dlss" if present and platform=="windows" else "fsr3")
            for value in ("dlss","fsr3","fsr4","fsr","bilinear"):
                run(platform,base+f"upscaler = {value}\n",value)
            for value in ("dlss","fsr","bilinear"):
                after=("fsr" if value=="dlss" else "fsr3") if platform=="native" else ("fsr3" if value=="dlss" else "dlss")
                run(platform,base+f"upscaler = {value}\n",value,"edit",after=after)
            run(platform,base+"upscaler = fsr\n","dlss","env","dlss")
dll.unlink(missing_ok=True)
print(f"{total} menu and preset cases passed")
