from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]).resolve()
fixture = Path(__file__).resolve().parent
src = root / "runtime"
imgui = src / "third_party" / "imgui"
cc, cxx = os.environ.get("CC", "clang"), os.environ.get("CXX", "clang++")
flags = ["-O1", "-ffunction-sections", "-fdata-sections", "-I"+str(src), "-I"+str(imgui)]
flags += shlex.split(os.environ.get("CPPFLAGS", ""))
with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp)
    menu = (src/"ui/menu.cpp").read_text()
    start = menu.index("    static const struct { const char *value; UiTextId label; } choices[] =")
    end = menu.index("    // The Window scaling filter", start)
    (work/"choices.inc").write_text(menu[start:end])
    (work/"test.cpp").write_text("#include <string>\n" + (fixture/"test.cpp").read_text())
    (work/"test_hw.c").write_text("""
    #include <stdint.h>
    uint32_t __wrap_plat_cpu_usable(void) { return 16; }
    void __wrap_plat_mem_info(float *used, float *total, float *proc) {
        *used = *proc = 0; *total = 32768;
    }
    """)
    objs = []
    def compile_one(compiler, lang, path):
        obj = work/(path.stem+".o")
        subprocess.run([compiler, "-std="+lang, *flags, "-c", str(path), "-o", str(obj)], check=True)
        objs.append(str(obj))
    for path in [src/"cfg.c", src/"ui_text.c", src/"platform/linux.c", src/"platform/common.c", work/"test_hw.c"]:
        compile_one(cc, "gnu11", path)
    for path in [work/"test.cpp", *(imgui/(name+".cpp") for name in ["imgui", "imgui_draw", "imgui_tables", "imgui_widgets"])]:
        compile_one(cxx, "c++17", path)
    subprocess.run([cxx, *objs, "-Wl,--gc-sections,--wrap=plat_cpu_usable,--wrap=plat_mem_info",
                    "-pthread", "-ldl", "-lm", "-o", str(work/"check-native.exe")], check=True)
    suite = (fixture/"run.py").read_text().replace('for platform in ("windows","native"):', 'for platform in ("native",):')
    (work/"run.py").write_text(suite)
    subprocess.run([sys.executable, str(work/"run.py")], check=True)
