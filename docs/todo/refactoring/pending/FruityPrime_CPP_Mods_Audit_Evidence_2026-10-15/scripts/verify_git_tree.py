import zipfile,hashlib,collections,json
z=zipfile.ZipFile('/mnt/data/Fruity-Prime-main(1).zip')
tree=collections.defaultdict(list)
def gitsha(type_,payload):return hashlib.sha1(f'{type_} {len(payload)}\0'.encode()+payload).digest()
for zi in z.infolist():
 if zi.is_dir():continue
 path=zi.filename.split('/')[1:]
 for i in range(len(path)):tree[tuple(path[:i])]
 mode='100755' if zi.external_attr>>16&0o111 else '100644'
 sha=gitsha('blob',z.read(zi));tree[tuple(path[:-1])].append((path[-1],mode,sha))
allpaths=sorted(tree,key=len,reverse=True)
for path in allpaths:
 items=sorted(tree[path],key=lambda o:o[0].encode('utf-8')+(b'/' if o[1]=='40000' else b''))
 payload=b''.join(mode.encode()+b' '+name.encode('utf-8')+b'\0'+sha for name,mode,sha in items)
 sha=gitsha('tree',payload)
 if len(path)==4 and path[:3]==('src','MphRead.Native','Mods') or path==('src','MphRead.Native','Mods'):print(path,sha.hex())
 if path:
  tree[path[:-1]].append((path[-1],'40000',sha))
 else:
  print('ZIP reconstructed tree sha:',sha.hex());print('expected: 2ee29a13cba4a76c8d32e235a21cdd76158ebd07');print('match:',sha.hex()=='2ee29a13cba4a76c8d32e235a21cdd76158ebd07')
