#!/usr/bin/env python3
"""schema.json (gen_save_schema.py) -> engine/src/formats/SaveSchema.inc. Aufruf: gen_save_schema_cpp.py schema.json out.inc"""
import json, sys
d = json.load(open(sys.argv[1]))
# mconstph (PlaceholderFeature, Vtable-Slot 38 ist ein leerer Stumpf): in den Karten PrjID + die Felder eines Planeten (GameObject)
if 'mconstph' not in d:
    pass
if 'mconstph' not in d:
    pl = next(v for v in d.values() if v['class'] == 'Planet')['variants'][0]
    pre = [['prjid', 'buildClass', 8]]
    d['mconstph'] = {'class': 'PlaceholderFeature', 'label': 'constPlaceholder', 'variants': [{'count': 1, 'mission': pre + pl['mission'], 'full': pre + pl['full']}]}
out = ['// Erzeugt von tools/gen_save_schema_cpp.py - nicht von Hand aendern.',
       '// {odf, Klasse, Klassenbezeichnung, Variante, Anzahl Karten-Objekte, Kartenfelder, Spielstandfelder}; Feld = art:name:groesse']
for odf in sorted(d):
    v = d[odf]
    for i, x in enumerate(sorted(v['variants'], key=lambda z: -z['count'])):
        m = ';'.join('%s:%s:%d' % (f[0],f[1],f[2] or 0) for f in x['mission'])
        f = ';'.join('%s:%s:%d' % (f[0],f[1],f[2] or 0) for f in x['full'])
        out.append('{"%s","%s","%s",%d,%d,"%s","%s"},' % (odf.lower(), v['class'], v.get('label', '').lower(), i, x['count'], m, f))
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
