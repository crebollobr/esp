# Manda os quadros dos videos de queda (URFD) p/ a placa, que roda o
# modelo + quedas.c neles, e no fim mostra a estatistica.
# Uso:  python teste_videos.py 192.168.0.225
# So usa a biblioteca padrao do Python (nada p/ instalar).
import os, sys, time, urllib.request

IP = sys.argv[1] if len(sys.argv) > 1 else '192.168.0.225'
BASE = 'http://' + IP
PASTA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'quadros')
FPS = 30                      # os videos URFD sao 30 quadros/s


def pede(caminho, corpo=None):
    req = urllib.request.Request(BASE + caminho, data=corpo, method='POST' if corpo else 'GET')
    if corpo:
        req.add_header('Content-Type', 'image/jpeg')
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.read().decode()


videos = sorted(os.listdir(PASTA))
res = []                       # (video, e_queda, alarme, vmax)
csv = open('caixas_placa.csv', 'w')
csv.write('video,frame,t_ms,x1,y1,x2,y2,score\n')
t0 = time.time()
nq = 0
for num, v in enumerate(videos, 1):
    pede('/teste/inicio')
    t = 0
    for arq in sorted(os.listdir(os.path.join(PASTA, v))):
        quadro = int(arq[:-4])
        t = quadro * 1000 // FPS + 1000
        with open(os.path.join(PASTA, v, arq), 'rb') as f:
            caixas = pede('/teste/quadro?t=%d' % t, f.read())
        for linha in caixas.splitlines():
            csv.write('%s,%d,%d,%s\n' % (v, quadro, t, linha))
        nq += 1
    alarme, vmax = pede('/teste/fim?t=%d' % t).rsplit(';', 1)
    e_queda = v.startswith('fall')
    res.append((v, e_queda, alarme, float(vmax)))
    print('[%d/%d] %-8s %-6s vmax=%5s  %s' % (num, len(videos), v,
          'QUEDA' if e_queda else 'normal', vmax, alarme), flush=True)
pede('/teste/sair')
csv.close()

# ---- estatistica ----
quedas  = [r for r in res if r[1]]
normais = [r for r in res if not r[1]]
pegas   = [r for r in quedas if r[2] != '-']
falsos  = [r for r in normais if r[2] != '-']
dt = time.time() - t0
print()
print('================ ESTATISTICA ================')
print('quedas detectadas : %d de %d (%.0f%%)' % (len(pegas), len(quedas), 100 * len(pegas) / max(len(quedas), 1)))
print('alarmes falsos    : %d de %d (%.0f%%)' % (len(falsos), len(normais), 100 * len(falsos) / max(len(normais), 1)))
print('  pelo tombo rapido   :', sum('rapido' in r[2] for r in pegas))
print('  por tempo no chao   :', sum('tempo' in r[2] for r in pegas))
print('quedas perdidas   :', ' '.join(r[0] for r in quedas if r[2] == '-') or '-')
print('alarmes falsos em :', ' '.join(r[0] for r in falsos) or '-')
print('tempo: %.0f s, %.2f s por quadro' % (dt, dt / max(nq, 1)))
print('caixas de cada quadro salvas em caixas_placa.csv')
