#!/usr/bin/env python3
"""Read-only lexical audit of Fruity Prime ZIP; no source modifications."""
from __future__ import annotations
import re,csv,hashlib,json,collections,os,sys,math
from pathlib import Path

ROOT=Path('/mnt/data/_fruity_audit/Fruity-Prime-main')
MODS=ROOT/'src/MphRead.Native/Mods'
OUT=Path('/mnt/data/_fruity_audit/evidence');OUT.mkdir(exist_ok=True)
allfiles=sorted(x for x in ROOT.rglob('*') if x.is_file())
cppfiles=sorted(x for x in MODS.rglob('*') if x.suffix in ('.cpp','.hpp','.h'))
cppsources=sorted(x for x in MODS.rglob('*.cpp'))

# strip literal and comments while retaining offsets/newlines for position references
# comments are not interpreted as code; preserve punctuations of source.
def scrub(s):
 a=list(s); i=0;n=len(s); mode='code'; delim=''
 while i<n:
  c=s[i];nx=s[i+1] if i+1<n else ''
  if mode=='code':
   if c=='/' and nx=='/':
    a[i]=' ';a[i+1]=' ';i+=2;mode='line';continue
   if c=='/' and nx=='*':
    a[i]=' ';a[i+1]=' ';i+=2;mode='block';continue
   if c in ('\"',"'"):
    a[i]=' ';mode=c;i+=1;continue
   # raw strings are handled conservatively as string content
  elif mode=='line':
   if c=='\n':mode='code'
   else:a[i]=' '
  elif mode=='block':
   if c=='*' and nx=='/':a[i]=' ';a[i+1]=' ';i+=2;mode='code';continue
   if c!='\n':a[i]=' '
  elif mode in ('\"',"'"):
   if c=='\\':
    a[i]=' '; i+=1
    if i<n and s[i]!='\n':a[i]=' '
    i+=1;continue
   if c==mode: mode='code'
   if c!='\n':a[i]=' '
  i+=1
 return ''.join(a)

def write(name,header,rows):
 p=OUT/name
 with p.open('w',newline='',encoding='utf-8') as f:
  w=csv.writer(f,delimiter='\t',quoting=csv.QUOTE_MINIMAL);w.writerow(header);w.writerows(rows)
 return p

rel=lambda p:p.relative_to(ROOT).as_posix()
raw={p:p.read_text(encoding='utf-8',errors='replace') for p in cppfiles}
clean={p:scrub(s) for p,s in raw.items()}
filetexts={p:raw[p].splitlines() for p in cppfiles}

inventory=[]; groups=collections.Counter(); bytegroups=collections.Counter()
for p in sorted(x for x in MODS.rglob('*') if x.is_file()):
 b=p.read_bytes();sub=p.relative_to(MODS).parts;cat=sub[0] if len(sub)>1 else '(root)'; groups[cat]+=1;bytegroups[cat]+=len(b)
 type_='source' if p.suffix=='.cpp' else 'header' if p.suffix in ('.hpp','.h') else 'asset'
 inventory.append([rel(p),type_,cat,len(b),b.count(b'\n')+(0 if not b or b.endswith(b'\n') else 1),hashlib.sha256(b).hexdigest()])
write('mods_file_inventory.tsv',['path','kind','area','bytes','lines','sha256'],inventory)

# C++ quoted include edges; choose matching physical files only. Distinguish dependency ownership.
roots=[ROOT/'src/MphRead.Native',ROOT/'src/MphRead.Native.Qt',ROOT/'src',ROOT]
inc_edges=[]; unresolved=[];direct=collections.Counter(); fanout=collections.Counter()
for p,s in raw.items():
 for num,line in enumerate(s.splitlines(),1):
  m=re.match(r'^\s*#\s*include\s*["<]([^">]+)[">]',line)
  if not m:continue
  inc=m.group(1)
  if not re.match(r'^\s*#\s*include\s*"',line): continue
  choices=[p.parent/inc]+[r/inc for r in roots]
  target=next((c.resolve() for c in choices if c.is_file()),None)
  if target is None:
   unresolved.append((rel(p),num,inc))
   continue
  group='Mods' if target.is_relative_to(MODS) else 'NativeQt' if target.is_relative_to(ROOT/'src/MphRead.Native.Qt') else 'ExternalInRepo'
  inc_edges.append([rel(p),num,inc,rel(target),group]);direct[(rel(p),rel(target))]+=1;fanout[rel(p)]+=1
write('mods_dependency_edges.tsv',['source','line','include','resolved_to','scope'],inc_edges)
write('mods_unresolved_includes.tsv',['source','line','include'],unresolved)

# An extract of syntactic function definitions, lexical analysis (NOT AST). External qualified definitions more reliable.
name_rx=re.compile(r'(?P<name>(?:~?[A-Za-z_]\w*::)*~?[A-Za-z_]\w*|operator\s*\S+?)\s*\(')
control={'if','else','while','for','switch','catch','return','sizeof','alignof','decltype','noexcept','throw','requires','static_assert','co_await','co_return'}
meta=[];funks=[];fn_byfile=collections.defaultdict(list)
for p in cppfiles:
 s=clean[p]; orig=raw[p];line_starts=[0]+[m.end() for m in re.finditer('\n',s)]
 # scan code segments at beginning of physical line (declarations); permit attributes and templates
 import bisect
 cursor=0
 for lineno,ln in enumerate(s.splitlines(keepends=True),1):
  line_offset=cursor;cursor+=len(ln)
  if len(ln)>420 or '(' not in ln or '{' in ln and 'return ' in ln:continue
  st=ln.strip()
  if not st or st.startswith(('#','//','*',':',',','if ','for ','while ','return ','else ','case ','switch ','catch ','static_assert','using ','typedef ','throw ','co_return ','&&','||','!','.')):continue
  # Find all identifier + open-paren pairs; take the first sensible signature.
  candidates=list(re.finditer(r'([A-Za-z_~]\w*(?:::[A-Za-z_~]\w*)*)\s*\(',ln))
  if not candidates:continue
  m=candidates[0];name=m.group(1)
  if name in control or name.startswith('std::'):continue
  before=ln[:m.start()].strip()
  if '::' not in name and not before:continue
  if '=' in before or ('[' in before and ']]' not in before) or before.count('<')>before.count('>'):continue
  if not re.match(r'^(?:[\w:<>,*&~]+(?:\s+[\w:<>,*&~]+)*)$',before) and before:continue
  start=line_offset+m.start();paren=line_offset+m.end()-1;depth=1;i=paren+1
  while i<len(s) and depth:
   if s[i]=='(':depth+=1
   elif s[i]==')':depth-=1
   i+=1
  if depth:continue
  post=s[i:i+370]
  openidx=post.find('{'); semicolon=post.find(';')
  if openidx<0 or (semicolon>=0 and semicolon<openidx):continue
  brace=i+openidx
  suffix=s[i:brace]
  if re.search(r'\]\s*\(',suffix) or re.search(r'\b(if|while|for|switch)\s*\(',suffix):continue
  dep=1;j=brace+1
  while j<len(s) and dep:
   if s[j]=='{':dep+=1
   elif s[j]=='}':dep-=1
   j+=1
  if dep:continue
  line=lineno; endline=bisect.bisect_right(line_starts,j)
  if (endline-line)>6000:continue
  origbody=orig[brace:j];cleanbody=s[brace:j]
  if p.suffix!='.cpp' and endline-line>500:continue
  toks=re.findall(r'\w+|[^\s\w]',cleanbody)
  n=name.split('::')[-1];low=name.lower()
  cat=('TEST' if any(a in rel(p).lower() for a in ('/testing/','/diagnostics/','check.cpp','tests.cpp')) or n.startswith('Test') else
       'PRESENT' if any(a in low for a in ('render','draw','sample','compose','menu','color','glyph','font','hover','layout')) else
       'AUTH' if any(a in low for a in ('judge','arbitrat','settle','grant','verdict','validate','damage','claim')) else
       'CODEC' if any(a in low for a in ('parse','serialize','deserialize','read','write','pack','decode','encode')) else
       'INPUT' if any(a in low for a in ('input','touch','key','pad','mouse','gamepad','pen','stick')) else
       'STATE' if any(a in low for a in ('reset','clear','update','set','get','start','stop','activate','remember','forget','remove','insert')) else
       'OBS' if any(a in low for a in ('log','trace','debug','capturestack','describe','metric')) else
       'RULE' if any(a in low for a in ('rules','compute','convert','calculate','clamp','lerp','plausible','is','has','find','max','min')) else 'OTHER')
  rec={'path':rel(p),'line':line,'end_line':endline,'name':name,'category_heuristic':cat,'chars':len(cleanbody),'body':cleanbody,'text':origbody,'tokens':toks,'scope':'qualified' if '::' in name else 'lexical','calls':[]}
  funks.append(rec);fn_byfile[rel(p)].append(rec)

funks.sort(key=lambda a:(a['path'],a['line']))
write('mods_function_index.tsv',['path','line','end_line','symbol','category_hint','body_chars','recognition'],[[f[k] for k in ('path','line','end_line','name','category_heuristic','chars','scope')] for f in funks])

# Approximate symbol call references: names are not disambiguated; mark heuristic.
symbols=collections.defaultdict(set)
for f in funks:symbols[f['name'].split('::')[-1]].add(f['path'])
allsource={rel(p):clean[p] for p in cppfiles}; allraw={rel(p):raw[p] for p in cppfiles}
reference_counts=collections.Counter();refs_by=collections.defaultdict(list)
for path,txt in allsource.items():
 counts=collections.Counter(re.findall(r'\b([A-Za-z_]\w*)\s*\(',txt))
 for n,cnt in counts.items():
  if n in symbols:
   reference_counts[n]+=cnt
   refs_by[n].append((path,cnt))
# Possible dead symbols only file static/anonymous namespace might be safe to inspect; never automatic delete.
unused=[]
for f in funks:
 name=f['name'].split('::')[-1]
 if len(name)<3:continue
 cnt=reference_counts[name]
 if cnt<=1 and f['path'].endswith('.cpp'):
  unused.append([f['path'],f['line'],f['name'],cnt,'MAYBE: lexical references across Mods only; dynamic callbacks / exports / non-Mods users not checked'])
write('mods_dead_code_evidence.tsv',['path','line','symbol','mods_lexical_reference_count','conclusion'],unused)

# Focused mutable static inventory; candidate only, global/function/member lifetimes not yet proven.
state=[]
for p,source in raw.items():
 for num,line in enumerate(source.splitlines(),1):
  st=line.strip()
  if not st or st.startswith(('#','//','/*','*','using ','return ','if ','else ','case ',':','enum ','class ','struct ','namespace ')):continue
  if not (';' in st or '=' in st or '{}' in st):continue
  type_=''
  if re.match(r'^(?:(?:inline|thread_local|mutable)\s+)*static\s+(?!(?:constexpr|consteval|assert)\b)(?:[\w:<>*&]+\s+){1,7}[A-Za-z_]\w*\s*(?:[=;{])',st):
   if '(' not in st and not re.search(r'\bconst\b',st):type_='static_declaration_candidate'
  elif re.match(r'^(?:(?:inline|thread_local)\s+)?(?:[\w:<>*&]+\s+){1,6}[A-Za-z_]\w*::[_A-Za-z]\w*\s*(?:[=;{])',st):
   if '(' not in st and 'const ' not in st and not st.startswith(('constexpr ','const ')):type_='static_member_definition_candidate'
  if type_:state.append([rel(p),num,type_,st[:320]])
write('mods_mutable_state.tsv',['path','line','kind_heuristic','declaration'],state)

# Exact large function body normalized without comments / whitespace; note that names may vary; this is lexical.
normalized=collections.defaultdict(list)
for f in funks:
 if f['chars']<190 or f['end_line']-f['line']<4 or re.sub(r'\s+','',f['body'])=='{}':continue
 x=re.sub(r'\s+','',f['body']);normalized[hashlib.sha256(x.encode()).hexdigest()].append(f)
dup=[]
for h,arr in normalized.items():
 if len(arr)<2:continue
 for a in arr:dup.append([a['path'],a['line'],a['name'],'exact-body',h,', '.join(f"{b['path']}:{b['line']}" for b in arr if b is not a)[:450]])
write('mods_duplicate_candidates.tsv',['path','line','symbol','match_kind','normalized_body_sha256','other_locations'],dup)

# Strongly connected components on per-file include graph (Tarjan)
adj=collections.defaultdict(set)
for _,_,_,t,_ in inc_edges:pass
for src,_,_,dst,_ in inc_edges:
 if src.startswith('src/MphRead.Native/Mods/') and dst.startswith('src/MphRead.Native/Mods/'):
  adj[src].add(dst)
idx=0;stack=[];on=set();ids={};low={};components=[]
def visit(v):
 global idx
 ids[v]=idx;low[v]=idx;idx+=1;stack.append(v);on.add(v)
 for w in adj[v]:
  if w not in ids:visit(w);low[v]=min(low[v],low[w])
  elif w in on:low[v]=min(low[v],ids[w])
 if low[v]==ids[v]:
  c=[]
  while True:
   w=stack.pop();on.remove(w);c.append(w)
   if w==v:break
  if len(c)>1:components.append(sorted(c))
for n in sorted(adj):
 if n not in ids:visit(n)
components.sort(key=lambda c:-len(c))
write('mods_include_cycles.tsv',['scc_id','members','sample'],[[i+1,len(c),' | '.join(c[:22])] for i,c in enumerate(components)])

summary={'files':len(inventory),'cpp':len(cppsources),'hpp':sum(1 for a in cppfiles if a.suffix=='.hpp'),'other':sum(1 for x in inventory if x[1]=='asset'), 'file_groups':dict(groups),'bytes_groups':dict(bytegroups), 'functions_lexical':len(funks),'qualified_functions':sum('::' in f['name'] for f in funks),'defs_per_area':dict(collections.Counter(f['path'].split('/')[3] if len(f['path'].split('/'))>4 else '(root)' for f in funks)),'include_edges':len(inc_edges),'include_unresolved':len(unresolved),'include_cycles':len(components),'largest_cycles':[[len(c),c[:8]] for c in components[:15]],'mutable_candidates':len(state),'dead_candidates':len(unused),'duplicate_exact':len(dup),'largest_functions':[[f['path'],f['line'],f['name'],f['end_line']-f['line']] for f in sorted(funks,key=lambda f:-(f['end_line']-f['line']))[:35]],'most_included':[list(x) for x in collections.Counter(t for _,_,_,t,_ in inc_edges).most_common(25)],'most_definitions':[[p,len(v)] for p,v in sorted(fn_byfile.items(),key=lambda x:-len(x[1]))[:30]],'duplicate_examples':dup[:35]}
(OUT/'audit_summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k not in ('largest_functions','largest_cycles','file_groups','bytes_groups','defs_per_area','most_included','most_definitions','duplicate_examples')},indent=2,ensure_ascii=False))
