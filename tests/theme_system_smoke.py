# SPDX-License-Identifier: GPL-3.0-only
"""End-to-end Classic appearance transactions using isolated data directories."""
import argparse
import io
import json
import os
import shutil
from pathlib import Path
import tarfile
import tempfile
import time
import zipfile
from api_sidecar_smoke import Sidecar

COLORS = """[General]
Name=Luna test palette
[Colors:Window]
BackgroundNormal=30,31,32
ForegroundNormal=240,241,242
[Colors:View]
BackgroundNormal=20,21,22
ForegroundNormal=230,231,232
[Colors:Selection]
BackgroundNormal=10,120,230
ForegroundNormal=255,255,255
"""

def wait_for_png(path):
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
        if path.exists():
            data=path.read_bytes()
            if data.startswith(b'\x89PNG') and data.endswith(b'IEND\xaeB`\x82'):return
        time.sleep(0.1)
    raise AssertionError(f'Preview did not finish writing {path}')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--online',action='store_true')
    parser.add_argument('--preview',action='store_true',help='Exercise a native preview subprocess (requires a desktop).')
    parser.add_argument('--output',type=Path,help='Keep native preview screenshots in this directory.')
    args=parser.parse_args()
    args.exe=args.exe.resolve()
    with tempfile.TemporaryDirectory(prefix='luna-theme-integration-') as temporary:
        root=Path(temporary)
        if args.preview:os.environ['LUNA_THEME_PREVIEW_CAPTURE']=str(root/'preview.png')
        source=root/'fixture.colors';source.write_text(COLORS,encoding='utf-8')
        s=Sidecar(args.exe,root)
        try:
            initial=s.ok('appearance.state')
            sub=s.ok('appearance.subscribe')['subscriptionId']
            assert not initial['requiresRestart']
            caps=s.ok('appearance.capabilities');assert 'kde-colors' in caps['formats']
            result=s.ok('appearance.import',path=str(source))
            events=s.ok('event.poll',subscriptionId=sub)['events']
            assert any(e['kind']=='appearance.catalog' for e in events)
            assert any(e['kind']=='appearance.task' and e['finished'] and e['succeeded'] for e in events)
            assert [e['sequence'] for e in events]==sorted({e['sequence'] for e in events})
            s.ok('event.unsubscribe',subscriptionId=sub)
            package=result['packageId']
            after=s.ok('appearance.state');assert after['effective']==initial['effective']
            palette=next(c['id'] for c in after['components'] if c.get('packageId')==package and c['kind']=='palette')
            assert next(c for c in after['components'] if c['id']==palette)['name']=='Luna test palette'
            duplicate=s.ok('appearance.import',path=str(source));assert duplicate['alreadyInstalled']
            invalid=s.execute('appearance.apply',baseRevision=after['revision'],configuration={'theme':'missing'})
            assert not invalid['ok'];assert s.ok('appearance.state')['requested']==after['requested']
            pending=s.ok('appearance.apply',baseRevision=after['revision'],configuration={'mode':'independent','theme':'Fusion','palette':palette})
            assert pending['requiresRestart'];assert pending['effective']==initial['effective']
            assert not s.execute('appearance.apply',baseRevision=after['revision'],configuration={'icons':'pe_dark'})['ok']
            assert not s.execute('appearance.package.remove',id=package,confirm=True)['ok']
            s.close();s=Sidecar(args.exe,root)
            active=s.ok('appearance.state');assert not active['requiresRestart'];assert active['effective']['palette']==palette
            hot=s.ok('appearance.apply',baseRevision=active['revision'],configuration={'icons':'pe_light'})
            assert not hot['requiresRestart'];assert hot['effective']['icons']=='pe_light'
            if args.preview:
                saved=(root/'classic-appearance.json').read_bytes()
                accounts=root/'accounts.json';original=accounts.read_bytes() if accounts.exists() else None
                s.ok('appearance.preview',configuration={})
                wait_for_png(root/'preview.png')
                assert (root/'classic-appearance.json').read_bytes()==saved
                assert (accounts.read_bytes() if accounts.exists() else None)==original
                print('PASS: real isolated preview, saved configuration and accounts unchanged while backend retains its data lock')
            pending=s.ok('appearance.apply',baseRevision=hot['revision'],configuration={'theme':'dark','palette':''})
            assert pending['requiresRestart']
            undo=s.ok('appearance.undo',baseRevision=pending['revision']);assert not undo['requiresRestart'];assert undo['requested']==hot['effective']
            assert not s.execute('appearance.package.remove',id=package,confirm=True)['ok']
            s.ok('appearance.select',theme='system')
            s.close();s=Sidecar(args.exe,root)
            assert not s.ok('appearance.state')['requiresRestart']
            s.ok('appearance.package.remove',id=package,confirm=True)
            assert not (root/'theme-packages'/package).exists()
            # The old API and importer share extraction validation.
            icon='[Icon Theme]\nName=Fixture icons\nDirectories=\nInherits=hicolor\n'
            legacy=root/'legacy-icons.zip'
            with zipfile.ZipFile(legacy,'w') as z:z.writestr('icons/legacy-fixture/index.theme',icon)
            assert s.ok('appearance.install',kind='icons',path=str(legacy))['installed']==['legacy-fixture']
            s.ok('appearance.select',icons='legacy-fixture')
            assert not s.execute('appearance.install',kind='icons',path=str(legacy),overwrite=True)['ok']
            assert not s.execute('appearance.remove',kind='icons',id='legacy-fixture',confirm=True)['ok']
            child=root/'child-icons.zip'
            with zipfile.ZipFile(child,'w') as z:z.writestr('icons/child-fixture/index.theme',icon.replace('Inherits=hicolor','Inherits=legacy-fixture'))
            s.ok('appearance.install',kind='icons',path=str(child));s.ok('appearance.select',icons='child-fixture')
            assert not s.execute('appearance.remove',kind='icons',id='legacy-fixture',confirm=True)['ok']
            s.ok('appearance.select',icons='pe_colored')
            s.ok('appearance.remove',kind='icons',id='child-fixture',confirm=True)
            s.ok('appearance.remove',kind='icons',id='legacy-fixture',confirm=True)
            for name in ['../escaped','icons/../../escaped','C:/escaped','icons/CON.svg','icons/bad./a','icons/a\\..\\..\\escaped']:
                unsafe=root/'unsafe.zip'
                with zipfile.ZipFile(unsafe,'w') as z:
                    z.writestr('icons/fixture/index.theme',icon);z.writestr(name,'x')
                assert not s.execute('appearance.import',path=str(unsafe))['ok'],name
            assert not (root/'escaped').exists()
            # Safe internal file and directory aliases are materialized; no privileges required.
            archive=root/'aliases.tar'
            with tarfile.open(archive,'w') as tar:
                for name,data in [('fixture/index.theme',icon.encode()),('fixture/real/a.svg',b'<svg/>')]:
                    entry=tarfile.TarInfo(name);entry.size=len(data);tar.addfile(entry,io.BytesIO(data))
                entry=tarfile.TarInfo('fixture/alias');entry.type=tarfile.SYMTYPE;entry.linkname='real';tar.addfile(entry)
                entry=tarfile.TarInfo('fixture/file.svg');entry.type=tarfile.SYMTYPE;entry.linkname='alias/a.svg';tar.addfile(entry)
            aliases=s.ok('appearance.import',path=str(archive));installed=root/'theme-packages'/aliases['packageId']
            assert (installed/'fixture/alias/a.svg').read_bytes()==b'<svg/>'
            assert (installed/'fixture/file.svg').read_bytes()==b'<svg/>'
            assert not (installed/'fixture/alias').is_symlink()
            for target in ['../../outside','real/../../outside','/outside','alias']:
                with tarfile.open(archive,'w') as tar:
                    entry=tarfile.TarInfo('fixture/index.theme');data=icon.encode();entry.size=len(data);tar.addfile(entry,io.BytesIO(data))
                    entry=tarfile.TarInfo('fixture/alias');entry.type=tarfile.SYMTYPE;entry.linkname=target;tar.addfile(entry)
                assert not s.execute('appearance.import',path=str(archive))['ok'],target
            # Luna bundles advertise their independent components, including decorations.
            bundle=root/'bundle.zip'
            with zipfile.ZipFile(bundle,'w') as z:
                z.writestr('theme.json',json.dumps({'schemaVersion':2,'name':'Luna fixture','components':{
                    'widgets':{'qss':'skin.qss','engine':'Fusion','paletteBinding':'semantic'},
                    'palettes':{'dark':{'format':'kde-colors','file':'test.colors'}},
                    'decoration':{'renderer':'luna-widgets','config':'decoration.json'}}}))
                z.writestr('skin.qss','QPushButton { padding: 5px; }')
                z.writestr('test.colors',COLORS)
                z.writestr('decoration.json','{"height":36,"boldTitle":true}')
            first=s.ok('appearance.import',path=str(bundle),components=['palette'])
            state=s.ok('appearance.state');assert len([c for c in state['components'] if c.get('packageId')==first['packageId']])==1
            expanded=s.ok('appearance.import',path=str(bundle),components=['widgets','decoration'])
            assert expanded['alreadyInstalled'] and expanded['packageId']==first['packageId']
            state=s.ok('appearance.state');assert len([c for c in state['components'] if c.get('packageId')==first['packageId']])==3
            assert s.ok('appearance.tasks')[-1]['succeeded']
            # Exercise the shipped example through import, apply and a fresh process.
            sample=s.ok('appearance.import',path=str(Path(__file__).resolve().parents[1]/'examples/themes/luna-sample'))
            state=s.ok('appearance.state')
            sample_components={c['kind']:c['id'] for c in state['components'] if c.get('packageId')==sample['packageId']}
            config={'mode':'independent','theme':sample_components['widgets'],'palette':sample_components['palette']}
            if caps['customDecoration']:config.update(decoration='luna',decorationTheme=sample_components['decoration'])
            pending=s.ok('appearance.apply',baseRevision=state['revision'],configuration=config)
            assert pending['requiresRestart']
            s.close();s=Sidecar(args.exe,root)
            applied=s.ok('appearance.state');assert applied['effective']['theme']==sample_components['widgets'] and not applied['requiresRestart']
            if args.preview:
                (root/'preview.png').unlink()
                s.ok('appearance.preview',configuration={})
                wait_for_png(root/'preview.png')
                if args.output:
                    args.output.mkdir(parents=True,exist_ok=True)
                    shutil.copy2(root/'preview.png',args.output/'theme-preview.png')
            if args.online:
                catalog=s.ok('appearance.query',provider='kde',category='palette',refresh=True)
                assert catalog['items'];assert catalog['pageSize']>0
                files=s.ok('appearance.files',provider='kde',category='palette',contentId='2379809',refresh=True)
                assert len(files['files'])>=2
                file=next(f for f in files['files'] if f['name'].endswith('Dark.colors'))
                installed=s.ok('appearance.import',provider='kde',category='palette',contentId='2379809',fileId=file['id'],name=file['name'])
                assert installed['packageId']=='d7c24d60ee19478785012b4855e7e1b27b301fee33e12509533058a89b12f4e6'
                github=s.ok('appearance.query',provider='github',source='https://github.com/PrismLauncher/Themes',refresh=True)
                assert github['items']
                item=min((i for i in github['items'] if i['category']=='widgets'),key=lambda i:i['file']['size'])
                prism=s.ok('appearance.import',provider='github',source='https://github.com/PrismLauncher/Themes',contentId=item['id'],fileId=item['file']['id'],name=item['file']['name'])
                state=s.ok('appearance.state')
                widget=next(c for c in state['components'] if c.get('packageId')==prism['packageId'] and c['kind']=='widgets')
                assert not widget.get('unavailable'),widget
                pending=s.ok('appearance.apply',baseRevision=state['revision'],configuration={'mode':'independent','theme':widget['id'],'palette':''})
                assert pending['requiresRestart']
                s.ok('appearance.undo',baseRevision=pending['revision'])
                print('PASS: real KDE catalog, multiple files, signed resolver, checksum/import; real Prism GitHub package install')
            s.close();s=None
            # Incomplete theme initialization restores the last good appearance.
            state_path=root/'classic-appearance.json';saved=json.loads(state_path.read_text())
            saved['initializing']=True;saved['requested']['theme']='missing-fixture'
            state_path.write_text(json.dumps(saved),encoding='utf-8')
            s=Sidecar(args.exe,root);restored=s.ok('appearance.state');assert restored['requested']['theme']!='missing-fixture';assert restored['error']
            print('PASS: migration, import, duplicate, pending/restart, hot swap, revision conflict, undo, protected removal, legacy downloader, malicious archives, icon aliases, Luna bundle, recovery')
        finally:
            if s:s.close()

if __name__=='__main__':main()
