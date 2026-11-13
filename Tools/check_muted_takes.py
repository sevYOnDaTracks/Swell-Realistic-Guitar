"""Check electric palm-mute WAVs for a sustained periodic tail at their mapped pitch."""
from collections import defaultdict
from pathlib import Path
import sys
import wave
import numpy as np

def pitch_score(path, note):
    with wave.open(str(path)) as wav:
        rate = wav.getframerate()
        width = wav.getsampwidth()
        data = wav.readframes(int(.20 * rate))
        if width == 3:
            b = np.frombuffer(data, np.uint8).reshape(-1, 3).astype(np.int32)
            x = ((b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16) << 8).astype(float)
        else:
            x = np.frombuffer(data, '<i' + str(width)).astype(float)
        x = x.reshape(-1, wav.getnchannels())[:, 0]
    peak = np.max(np.abs(x))
    if not peak:
        return 0.
    x = x[np.flatnonzero(np.abs(x) > peak * .05)[0]:] / peak
    period = rate / (440 * 2 ** ((note - 69) / 12))
    scores = []
    for t in (.025, .065, .105):
        y = x[int(t * rate):int((t + .045) * rate):2]
        if len(y) < .019 * rate or np.mean(y*y) < .00015:
            scores.append(0.)
            continue
        y = y - y.mean()
        best = 0.
        for lag in range(max(1, round(period * .47)), round(period * .53) + 1):
            a, b = y[:-lag], y[lag:]
            best = max(best, float(np.dot(a,b) / max(1e-15,np.linalg.norm(a)*np.linalg.norm(b))))
        scores.append(best)
    return sorted(scores)[1]

def main():
    root = Path(sys.argv[1])
    src = Path(__file__).resolve().parents[1] / 'Source'
    groups = defaultdict(list)
    for line in (src/'Catalog.inc').read_text().splitlines():
        fields = line.split('\t')
        if len(fields) != 6: continue
        sel, art, note, string, layer = map(int,fields[:5])
        if sel < 4 or art != 1: continue
        model = 'LP' if sel < 6 else 'TC'
        name = fields[-1]
        path = root/model/(model + (' Strum' if sel%2 else '') + ' Samples')/name
        groups[sel,note,string].append((pitch_score(path,note), name,layer))
    excluded = []
    for (sel,note,string), takes in groups.items():
        top = max(t[0] for t in takes)
        # Retain at least the best pitched take for every recorded string and fret.
        for score,name,layer in takes:
            if score < min(.72,top) or (score < top and name.lower().endswith(('x.wav','x-.wav'))):
                excluded.append((sel,name))
    for sel in range(4,8):
        print(sel, 'excluded',sum(s==sel for s,_ in excluded),'groups',sum(k[0]==sel for k in groups))
    (src/'MutedTakeExclusions.inc').write_text('R"MUTES(\n'+''.join(f'{s}\\t{n}\n'.replace('\\t','\t') for s,n in sorted(excluded))+')MUTES"\n')

if __name__ == '__main__':
    main()
