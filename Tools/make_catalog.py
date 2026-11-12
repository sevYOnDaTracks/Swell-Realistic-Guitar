"""Build the embedded filename map from the user's eight Kontakt WAV exports."""
from pathlib import Path
import re
import sys
from collections import Counter

TUNING = {"e6": 40, "a": 45, "d": 50, "g": 55, "h": 59, "e1": 64}
STR = r"(e6|a|d|g|h|e1)"
NOTE = r"([a-h]#?-?\d+)"
NOTE_VALUES = dict(c=0, d=2, e=4, f=5, g=7, a=9, h=11, b=11)


def pitch(token):
    m = re.fullmatch(r"([a-h])(#?)(-?\d+)", token)
    return 12 * (int(m[3]) + 2) + NOTE_VALUES[m[1]] + bool(m[2])


def classify(filename, model):
    name = re.sub(r"^(sg_|tg_|lp_|tc_)", "", filename.lower()[:-4])
    m = re.fullmatch(r"rel_(?:\d+_)?" + NOTE + r"[_-](\d+)-?", name)
    if m:
        return 9, pitch(m[1]), 0, int(m[2])
    m = re.match(STR + r"_(flis|flagis|flag|fgl|fl)_(?:\d+_)?" + NOTE + r"(?:_|$)", name)
    if m:
        return 2, pitch(m[3]), list(TUNING).index(m[1]), int(re.search(r"_d(\d+)", name)[1]) if re.search(r"_d(\d+)", name) else 1
    if model == "LP" and re.match(r"(?:e6|a|d|g|h|e1)_\d+_", name):
        name = re.sub(r"^([^_]+)_", r"\1_mute_", name)
    if model == "LP" and re.match(r"\d+_[a-h]#?\d+_", name):
        name = "e1_mute_" + name
    if name.startswith("mute_"):
        parts = name.split("_", 2)
        name = (parts[1] + "_mute_" + parts[2]) if parts[1] in TUNING else ("e1_" + name)
    m = re.match(STR + r"_(sus|mute|ham-on|hammer-on|hammer|p-off|pull-off|pulll?|sl-[12]|slide[12]|flide2|gl_up|gl_dn|gliss_up|gliss_down|vib)_?(\d+)(?:_|$)", name)
    if not m:
        return None
    string, gesture, fret = m.groups()
    art = next(i for i, terms in enumerate([
        ("sus",), ("mute",), (), ("ham-on", "hammer-on", "hammer"), ("p-off", "pull-off", "pull", "pulll"),
        ("sl-1", "sl-2", "slide1", "slide2", "flide2"), ("vib",), ("gl_up", "gliss_up"), ("gl_dn", "gliss_down")
    ]) if gesture in terms)
    # The six open strings and fret number disambiguate incomplete octave labels in the source files.
    note = TUNING[string] + int(fret)
    tail = name[m.end():]
    last = re.search(r"(?:^|_)(?:d-|[ldt])?(\d+)(?:x|-)?$", tail)
    layer = int(last[1]) if last else (2 if tail.endswith("_s") else 1)
    return art, note, list(TUNING).index(string), layer


def main():
    root = Path(sys.argv[1])
    out = Path(__file__).resolve().parents[1] / "Source" / "Catalog.inc"
    with out.open("w") as stream:
        stream.write('R"SWELLCATALOG(\n')
        for model_index, model in enumerate(("AG", "NG", "LP", "TC")):
            for mode in (0, 1):
                directory = root / model / (model + (" Strum" if mode else "") + " Samples")
                counts = Counter()
                for path in sorted(directory.iterdir()):
                    if path.name.startswith(".") or path.suffix.lower() != ".wav":
                        continue
                    mapped = classify(path.name, model)
                    if mapped:
                        art, note, string, layer = mapped
                        if not 0 <= note <= 127:
                            raise ValueError(path)
                        stream.write(f"{model_index * 2 + mode}\t{art}\t{note}\t{string}\t{layer}\t{path.name}\n")
                        counts[art] += 1
                print(directory.name, dict(sorted(counts.items())))
        stream.write(')SWELLCATALOG"\n')


if __name__ == "__main__":
    main()
