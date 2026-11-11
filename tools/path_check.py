import sys,json
for sc in json.load(open(sys.argv[1])):
    print(len(sc['fields']),*[v for p in sc['fields'] for v in p],*sc['start'],*sc['goal'],len(sc['pts']),*[v for p in sc['pts'] for v in p])
