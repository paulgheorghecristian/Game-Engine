"""Visual regression tour: renders a set of viewpoints of the showcase maps at
several times of day and assembles a contact sheet per map (rows: viewpoints,
columns: times of day), so graphical bugs and regressions are easy to spot.

Each shot is a short benchmark run of the engine (GE_BENCH) with a fixed
camera (GE_START), a start time (GE_TIME_OF_DAY) and no overlays
(GE_CLEAN_SHOTS). The per-shot average frame time is printed as well.

usage: python3 tools/visual_tour.py OUTPUT_DIR [lagoon|canyon ...] [--size 1280x720]
"""
import os, subprocess, sys

from PIL import Image, ImageDraw

TIMES = [("dawn", 0.27), ("noon", 0.5), ("dusk", 0.745), ("night", 0.02)]

# name: (map, [(view, "x,y,z,yaw,pitch", frames)]) -- few frames for views high
# above the ground (the player falls once physics runs)
TOURS = {
    "lagoon": ("Game/lagoon.json", [
        ("spawn", "490,40,-60,0,-4", 40),
        ("path", "490,40,-200,0,-8", 40),
        ("shore", "640,40,-430,40,-4", 40),
        ("campfire", "700,55,-318,-27,-30", 2),
        ("lighthouse", "665,30,-1000,44,10", 40),
        ("hills", "1000,300,-1550,135,-18", 2),
        ("sky", "490,40,-100,17,28", 40),
    ]),
    "canyon": ("Game/canyon.json", [
        ("spawn", "40,25,820,-24,2", 40),
        ("valley", "60,25,300,10,2", 40),
        ("plaza", "0,30,-560,0,4", 40),
        ("pool", "0,30,-640,0,12", 40),
        ("colossus", "170,10,-930,62,14", 40),
        ("overlook", "110,10,-1040,10,-3", 40),
        ("sky", "40,25,500,23,25", 40),
    ]),
}


def shoot(map_path, start, frames, time_of_day, out, size):
    env = dict(os.environ, GE_SCENARIO="static", GE_BENCH=str(frames), GE_SIZE=size, GE_START=start,
               GE_TIME_OF_DAY=str(time_of_day), GE_CLEAN_SHOTS="1", GE_SCREENSHOT=out)
    result = subprocess.run(["./ge", map_path], env=env, capture_output=True, text=True, timeout=300)
    for line in result.stdout.splitlines():
        if line.startswith("BENCH"):
            return line.split("avg=")[1].split("ms")[0]
    return "?"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    size = "1280x720"
    if "--size" in sys.argv:
        size = sys.argv[sys.argv.index("--size") + 1]
        args = [a for a in args if a != size]
    if not args:
        print(__doc__)
        return 1
    out_dir, names = args[0], args[1:] or list(TOURS)
    os.makedirs(out_dir, exist_ok=True)
    tw, th = 480, 270
    for name in names:
        map_path, views = TOURS[name]
        sheet = Image.new("RGB", (tw * len(TIMES), th * len(views)), "black")
        draw = ImageDraw.Draw(sheet)
        for row, (view, start, frames) in enumerate(views):
            for col, (label, t) in enumerate(TIMES):
                path = os.path.join(out_dir, "%s_%s_%s.png" % (name, view, label))
                ms = shoot(map_path, start, frames, t, path, size)
                print("%-7s %-11s %-6s %s ms" % (name, view, label, ms), flush=True)
                if os.path.exists(path):
                    sheet.paste(Image.open(path).convert("RGB").resize((tw, th)), (col * tw, row * th))
                draw.text((col * tw + 6, row * th + 4), "%s / %s  %s ms" % (view, label, ms), fill="yellow")
        sheet_path = os.path.join(out_dir, "%s_sheet.png" % name)
        sheet.save(sheet_path)
        print("sheet:", sheet_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
