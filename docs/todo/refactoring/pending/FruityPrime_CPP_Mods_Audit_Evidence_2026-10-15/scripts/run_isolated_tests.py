#!/usr/bin/env python3
import subprocess, csv, re
from pathlib import Path
ROOT=Path('/mnt/data/_fruity_audit/Fruity-Prime-main'); BIN=Path('/mnt/data/_fruity_audit/test-bin'); BIN.mkdir(exist_ok=True)
OUT=Path('/mnt/data/_fruity_audit/evidence'); OUT.mkdir(exist_ok=True)
files={
 'session_authority_state': ['Testing/TestSessionAuthorityState.cpp','Mods/Network/Session/SessionAuthorityState.cpp'],
 'hit_claim_rate_limiter':['Testing/TestHitClaimRateLimiter.cpp','Mods/Network/Gameplay/Claims/Hit/HitClaimRateLimiter.cpp'],
 'hit_claim_metrics':['Testing/TestHitClaimMetrics.cpp','Mods/Network/Gameplay/Claims/Hit/HitClaimMetrics.cpp'],
 'hit_claim_death_state':['Testing/TestHitClaimDeathState.cpp','Mods/Network/Gameplay/Claims/Hit/HitClaimDeathState.cpp'],
 'rescued_hit_ledger':['Testing/TestRescuedHitLedger.cpp','Mods/Network/Gameplay/Claims/Hit/RescuedHitLedger.cpp'],
 'hit_claim_outbox':['Testing/TestHitClaimOutbox.cpp','Mods/Network/Gameplay/Claims/Hit/HitClaimOutbox.cpp'],
 'server_slot_identity':['Testing/TestServerSlotIdentity.cpp','Mods/Network/Server/Core/ServerSlotIdentity.cpp'],
 'shot_ray_validation':['Testing/TestShotRayValidation.cpp','Mods/Network/Gameplay/Claims/Hit/ShotRayValidation.cpp'],
 'client_connection_health':['Testing/TestClientConnectionHealth.cpp','Mods/Network/Session/ClientConnectionHealth.cpp'],
 'session_traffic_metrics':['Testing/TestSessionTrafficMetrics.cpp','Mods/Network/Session/SessionTrafficMetrics.cpp'],
 'session_match_identity':['Testing/TestSessionMatchIdentity.cpp','Mods/Network/Session/SessionMatchIdentity.cpp'],
}
rows=[]
for label,srcs in files.items():
 sources=[ROOT/'src/MphRead.Native'/p for p in srcs]
 missing=[str(s.relative_to(ROOT)) for s in sources if not s.is_file()]
 if missing:
  rows.append([label,'not-attempted','missing source in proposed isolated target: '+','.join(missing),'']);continue
 cmd=['clang++','-std=c++20','-O1','-I',str(ROOT/'src/MphRead.Native'),*[str(s) for s in sources],'-o',str(BIN/label)]
 try:
  comp=subprocess.run(cmd,capture_output=True,text=True,timeout=35)
 except subprocess.TimeoutExpired:
  rows.append([label,'compile-timeout','compile exceeded 35 seconds','']);continue
 if comp.returncode:
  err=re.sub(r'/mnt/data/_fruity_audit/Fruity-Prime-main/','',comp.stderr)
  rows.append([label,'isolated-link-failed',err[-700:].replace('\n',' | '),'']);continue
 try:res=subprocess.run([str(BIN/label)],capture_output=True,text=True,timeout=15)
 except subprocess.TimeoutExpired:
  rows.append([label,'runtime-timeout','runtime exceeded 15 seconds','']);continue
 rows.append([label,'pass' if res.returncode==0 else 'fail',res.stderr[-650:].replace('\n',' | '),str(res.returncode)])
with open(OUT/'isolated_tests.tsv','w',newline='',encoding='utf-8') as f:
 w=csv.writer(f,delimiter='\t');w.writerow(['test','status','notes','exit_code']);w.writerows(rows)
for row in rows:print(row[0],row[1],row[2][:120])
