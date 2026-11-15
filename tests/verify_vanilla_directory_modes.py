import sys,os,json,tempfile,subprocess
from pathlib import Path
sys.path.insert(0,str(Path.cwd()))
from scripts.pixi_meson import task_env
r=Path(tempfile.mkdtemp(prefix='luna-mode-test-')); data=r/'data'; data.mkdir(); root=r/'store/.minecraft'; root.mkdir(parents=True)
env=task_env(Path.cwd(),'release');env['QT_QPA_PLATFORM']='offscreen';env['QT_PLUGIN_PATH']=env['LUNA_QT_PLUGIN_DIR']
env['PATH']=os.pathsep.join([str(p.parent.resolve()) for p in Path('build-meson-release/subprojects').rglob('*.dll')]+[env['PATH']])
exe=Path('build-meson-release/launcher/lunalauncher-cli.exe').resolve()
(data/'instance-stores.json').write_text(json.dumps({'formatVersion':1,'InstanceStores':[{'id':'mode-test','name':'mode-test','path':str(root.parent),'format':'vanilla-compatible','origin':'user','enabled':True,'writable':True,'defaultForNewInstances':True}]}))
def put(path,obj):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(obj),encoding='utf-8')
for name in ['empty','hmcl-shared','hmcl-isolated','pcl-shared','pcl-isolated','global']:
 p=root/'versions'/name;put(p/(name+'.json'),{'id':name,'type':'release','mainClass':'net.minecraft.client.main.Main','minecraftArguments':'--gameDir ${game_directory}','libraries':[]})
put(root/'versions/hmcl-shared/hmclversion.cfg',{'gameDirType':0})
put(root/'versions/hmcl-isolated/hmclversion.cfg',{'gameDirType':1})
for n,v in [('pcl-shared','False'),('pcl-isolated','True')]:
 p=root/'versions'/n/'PCL/Setup.ini';p.parent.mkdir();p.write_text('VersionArgumentIndieV2:'+v)
seq=0
def api(op,params,ok=True):
 global seq;seq+=1
 proc=subprocess.run([str(exe),'--cli','--json','--non-interactive','-d',str(data),'api',op,json.dumps(params)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
 (r/f'{seq}.stderr').write_bytes(proc.stderr)
 lines=[l for l in proc.stdout.decode('utf-8',errors='replace').splitlines() if l.startswith('{')]
 assert lines,(op,proc.returncode,proc.stderr[-1500:]); result=json.loads(lines[-1]);assert result['ok']==ok,result
 return result.get('data')
items=api('instance.list',{})
print('ROOT',r)
if isinstance(items,dict): items=items.get('instances',items.get('entries',[]))
ids={i['name']:i['id'] for i in items}
assert len(ids)==6,ids
for name in ids:
 info=api('instance.info',{'instance':ids[name]})
 expected=str(root if name.endswith('shared') else root/'versions'/name).replace('\\','/')
 assert info['gameRoot'].replace('\\','/')==expected,(name,info['gameRoot'],expected)
for mode in ['shared','isolated','auto']:
 api('settings.set',{'scope':'instance','instance':ids['empty'],'key':'VanillaGameDirectoryMode','value':mode})
 info=api('instance.info',{'instance':ids['empty']});assert info['isolated']==(mode!='shared'),info
api('settings.set',{'scope':'instance','instance':ids['empty'],'key':'VanillaGameDirectoryMode','value':'invalid'},False)
put(root.parent/'hmcl.json',{'configurations':{'test':{'gameDir':'.minecraft','global':{'usesGlobal':True,'gameDirType':0}}}})
info=api('instance.info',{'instance':ids['global']});assert not info['isolated'],info
assert not list(root.rglob('instance.cfg')) and not list(root.rglob('mmc-pack.json'))
print('PASS: six mixed profiles, default isolation, HMCL/PCL modes, global inheritance, persisted switching, validation, no MMC pollution')
