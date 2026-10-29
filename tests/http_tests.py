#!/usr/bin/env python3
"""Run on a disposable local database. Python standard library only."""
import json, urllib.request, urllib.error, sys, math
from pathlib import Path
BASE=sys.argv[1] if len(sys.argv)>1 else 'http://localhost:8080'
rows=[]
def call(method,path,body=None,token=None,expected=200,label=None):
    raw=body if isinstance(body,str) else (json.dumps(body) if body is not None else None)
    headers={'Content-Type':'application/json'}
    if token: headers['Authorization']='Bearer '+token
    req=urllib.request.Request(BASE+path,data=raw.encode() if raw is not None else None,headers=headers,method=method)
    try: res=urllib.request.urlopen(req)
    except urllib.error.HTTPError as e: res=e
    content=json.loads(res.read() or 'null');ok=res.status==expected
    rows.append(dict(name=label or method+' '+path,expected=expected,actual=res.status,passed=ok))
    assert ok,rows[-1]
    return content
A='/api/v1.1';auth=A+'/auth';route=A+'/works/calc-individual'
call('GET','/health');call('GET',A+'/counters');call('GET',A+'/history')
tokens=[call('POST',auth+'/login',dict(username=x,password=x))['token'] for x in ['user1','admin1','owner1']]
u,a,o=tokens
for t in tokens:
    call('GET',auth+'/profile',token=t);call('POST',auth+'/refresh',token=t)
sample=dict(total_vehicles=20,active_vehicles_day=15,total_fuel_consumed=300,total_km_driven=3000,average_fuel_price=1.5)
result=call('POST',route,sample,u)
assert all(math.isclose(result[k],v,rel_tol=1e-12) for k,v in zip(result,[75,10,.15]))
for field,val in [('total_vehicles',0),('total_vehicles',20.5),('total_vehicles',2147483648),('active_vehicles_day',0),('active_vehicles_day',21),('total_fuel_consumed',0),('total_fuel_consumed',-1),('total_km_driven',0),('total_km_driven',22500.1),('average_fuel_price',0),('average_fuel_price','1.5')]:
    r=call('POST',route,sample|{field:val},u,400,field+'='+str(val));assert r['error']!='Request failed'
for raw in ['{}','[]','null','{"total_vehicles":20,}',json.dumps(sample)+'x',json.dumps(sample)[:-1]+',"total_vehicles":20}',json.dumps(sample).replace('300,','NaN,'),json.dumps(sample).replace('300,','1e999,')]:call('POST',route,raw,u,400,'Invalid JSON: '+raw[:50])
for field in sample:
    bad=sample.copy();del bad[field];call('POST',route,bad,u,400,'Missing '+field)
call('POST',route,sample,None,401);call('POST',route,sample,'invalid',401)
call('POST',route,sample|{'total_km_driven':22500},u)
hist=call('GET',A+'/works/history-individual',token=u);assert hist[0]['input']['total_km_driven']==22500
assert call('GET',A+'/works/history-individual',token=a)==[]
call('POST',A+'/works/calc',{'data':[1,2,3]},u);call('GET',A+'/works/history',token=u)
for path in [auth+'/users',A+'/admin/stats',A+'/admin/run-tests',A+'/admin/run-tests-individual']:
    call('GET',path,expected=401);call('GET',path,token=u,expected=403);call('GET',path,token=a)
for path in ['/admin/run-tests','/admin/run-tests-individual']:
    r=call('GET',A+path,token=o);assert r['passed']==20 and r['failed']==0
    Path('tests/'+('fleet' if path.endswith('individual') else 'builtin')+'_results.json').write_text(json.dumps(r,indent=2))
call('PUT',auth+'/users/password',dict(username='owner1',new_password='forbidden'),a,403)
call('DELETE',auth+'/users',dict(username='owner1'),a,400)
call('PUT',auth+'/users/password',dict(username='absent_test_user',new_password='pass123'),o,404)
call('POST',auth+'/register',dict(username='fleet_test_user',password='pass123'))
call('POST',auth+'/register',dict(username='fleet_test_user',password='pass123'),expected=409)
call('PUT',auth+'/users/password',dict(username='fleet_test_user',new_password='new123'),a)
call('DELETE',auth+'/users',dict(username='fleet_test_user'),a)
call('POST',auth+'/login',dict(username="' OR 1=1 --",password='x'),expected=401)
call('OPTIONS',route,expected=204);call('GET','/no-such-route',expected=404)
call('POST',auth+'/logout',token=u);call('GET',auth+'/profile',token=u,expected=401)
Path('tests/http_results.json').write_text(json.dumps(dict(passed=len(rows),failed=0,cases=rows),ensure_ascii=False,indent=2))
print(f'HTTP integration: {len(rows)}/{len(rows)} passed')
