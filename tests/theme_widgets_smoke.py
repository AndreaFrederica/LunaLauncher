# SPDX-License-Identifier: GPL-3.0-only
"""Verify full native widget/icon combinations, global references and restart state."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from api_sidecar_smoke import Sidecar
from theme_system_smoke import COLORS, wait_for_png


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--real-theme', type=Path, action='append', default=[])
    parser.add_argument('--native-tests', type=Path, help='Run native interaction and gallery tests after restarting with each real theme.')
    parser.add_argument('--online', action='store_true')
    args = parser.parse_args()
    executable = args.exe.resolve()
    output = args.output.resolve() if args.output else None
    if output:
        output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='luna-widget-integration-') as temporary:
        root = Path(temporary)
        capture = root/'preview.png'
        if output:
            os.environ['LUNA_THEME_PREVIEW_CAPTURE'] = str(capture)
        source = root/'fixture'
        contents = source/'global/contents'
        contents.mkdir(parents=True)
        (contents/'defaults').write_text('[kdeglobals][KDE]\nwidgetStyle=kvantum\n[kdeglobals][Icons]\nTheme=FixtureIcons\n')
        (contents.parent/'metadata.json').write_text(json.dumps({'KPlugin': {'Name': 'Complete native fixture'}}))
        widget = source/'widgets'
        widget.mkdir()
        (widget/'Fixture.kvconfig').write_text('[GeneralColors]\nwindow.color=#152637\ntext.color=#ffffff\nwindow.text.color=#ffffff\n[PanelButtonCommand]\nframe=false\ninterior=true\ninterior.element=button\n')
        (widget/'Fixture.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><rect id="button-normal" width="40" height="20" rx="8" fill="#12ab34"/></svg>')
        icons = source/'FixtureIcons'
        (icons/'16x16/actions').mkdir(parents=True)
        (icons/'index.theme').write_text('[Icon Theme]\nName=FixtureIcons\nDirectories=16x16/actions\n[16x16/actions]\nSize=16\nType=Fixed\nContext=Actions\n')
        (icons/'16x16/actions/configure.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><circle cx="8" cy="8" r="7" fill="#f9d000"/></svg>')
        s = Sidecar(executable, root/'profile')
        try:
            caps = s.ok('appearance.capabilities')
            assert 'Kvantum' in caps['engines'] and 'kvantum' in caps['formats']
            initial = s.ok('appearance.state')
            installed = s.ok('appearance.import', path=str(source))
            after = s.ok('appearance.state')
            assert after['effective'] == initial['effective'] and after['revision'] == initial['revision']
            combination = s.ok('appearance.package.configuration', id=installed['packageId'])
            assert combination['complete'], combination
            config = combination['configuration']
            assert config['theme'].startswith('managed-') and config['icons'].startswith('managed-') and config['palette'] == ''
            pending = s.ok('appearance.apply', configuration=config, baseRevision=after['revision'])
            assert pending['requiresRestart']
            s.close()
            s = Sidecar(executable, root/'profile')
            state = s.ok('appearance.state')
            assert state['effective'] == config and not state['requiresRestart'] and not state.get('error'), state.get('error') or state['effective']
            if output:
                s.ok('appearance.preview', configuration={})
                wait_for_png(capture)
                shutil.copy2(capture, output/'native-complete-package.png')
            missing = root/'missing/global/contents'
            missing.mkdir(parents=True)
            (missing/'defaults').write_text('[kdeglobals][KDE]\nwidgetStyle=UnavailableEngine\n[kdeglobals][Icons]\nTheme=UnavailableIcons\n[kdeglobals][General]\nColorScheme=UnavailablePalette\n')
            (missing.parent.parent/'OtherPalette.colors').write_text(COLORS)
            partial = s.ok('appearance.import', path=str(missing.parent.parent))
            report = s.ok('appearance.package.configuration', id=partial['packageId'])
            assert not report['complete'] and {i['kind'] for i in report['missing']} == {'widgets', 'icons', 'palette'}, report
            empty = root/'desktop-only/contents'
            empty.mkdir(parents=True)
            (empty/'defaults').write_text('[plasmarc][Theme]\nname=DesktopOnly\n')
            desktop = s.ok('appearance.import', path=str(empty.parent))
            report = s.ok('appearance.package.configuration', id=desktop['packageId'])
            assert not report['complete'] and report['missing'][0]['kind'] == 'application', report
            real_configurations = []
            for number, real in enumerate(args.real_theme):
                package = s.ok('appearance.import', path=str(real.resolve()))
                report = s.ok('appearance.package.configuration', id=package['packageId'])
                assert report['complete'], report
                widgets = [c for c in s.ok('appearance.state')['components'] if c.get('packageId') == package['packageId'] and c['kind'] == 'widgets']
                assert len(widgets) == 1 and widgets[0]['format'] == 'kvantum' and not widgets[0].get('unavailable'), widgets
                configuration = report['configuration']
                configuration['icons'] = ('breeze_dark', 'breeze_light', 'pe_colored')[number % 3]
                real_configurations.append((real.name, configuration))
                if output:
                    capture.unlink(missing_ok=True)
                    s.ok('appearance.preview', configuration=configuration)
                    wait_for_png(capture)
                    shutil.copy2(capture, output/(real.name+'.png'))
            evidence = []
            for name, configuration in real_configurations:
                state = s.ok('appearance.state')
                applied = s.ok('appearance.apply', configuration=configuration, baseRevision=state['revision'])
                assert applied['requiresRestart'], applied
                s.close()
                s = Sidecar(executable, root/'profile')
                state = s.ok('appearance.state')
                assert state['effective'] == configuration and not state['requiresRestart'] and not state.get('error'), {
                    'theme': name, 'expected': configuration, 'effective': state['effective'],
                    'requiresRestart': state['requiresRestart'], 'error': state.get('error')}
                if args.native_tests:
                    assert output, '--native-tests requires --output for captured evidence'
                    s.close()
                    s = None
                    native_output = output/name
                    native_output.mkdir(exist_ok=True)
                    result = subprocess.run([str(args.native_tests.resolve()), 'nativeControls', 'cardGallery', 'navigationAndTransactions',
                        '-o', str(native_output/'native-tests.txt')+',txt'], cwd=executable.parent, capture_output=True, timeout=60,
                        env={**os.environ, 'QT_QPA_PLATFORM': 'windows', 'LUNA_THEME_TEST_PROFILE': str(root/'profile'),
                            'LUNA_THEME_TEST_PREVIEWS': str(output), 'LUNA_THEME_TEST_OUTPUT': str(native_output)})
                    assert result.returncode == 0, name + '\n' + (native_output/'native-tests.txt').read_text(errors='replace')[-5000:]
                    s = Sidecar(executable, root/'profile')
                evidence.append({'theme': name, 'configuration': configuration, 'restart': 'passed',
                    'nativeControlsAndGallery': 'passed' if args.native_tests else 'not requested'})
                print('PASS: '+name+' applied, restarted'+(', native controls and gallery' if args.native_tests else ''))
            if output:
                (output/'verification.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
            if args.online:
                catalog = s.ok('appearance.query', provider='kde', category='widgets', refresh=True)
                assert catalog['items'] and all(i['installable'] for i in catalog['items'])
                files = s.ok('appearance.files', provider='kde', category='widgets', contentId='1294013', refresh=True)
                file = next(f for f in files['files'] if f['installable'] and f['name'].endswith('.tar.xz'))
                dependency = root/'dependent/global/contents'
                dependency.mkdir(parents=True)
                (dependency/'defaults').write_text('[kdeglobals][KDE]\nwidgetStyle=kvantum\n[kdeglobals][Icons]\nTheme=breeze\n')
                (dependency.parent/'metadata.json').write_text(json.dumps({'KPlugin': {'Name': 'Sweet native dependency'}, 'X-KPackage-Dependencies': ['kns://kvantum.knsrc/api.opendesktop.org/1294013']}))
                imported = s.ok('appearance.import', path=str(dependency.parent.parent), dependencyFiles={'1294013': file['id']})
                assert len(imported['dependencyPackages']) == 1 and not imported['unresolvedDependencies'], imported
                report = s.ok('appearance.package.configuration', id=imported['packageId'])
                assert report['complete'] or ('widgets' in report['choices'] and not report['missing']), report
                if report['choices']:
                    config = report['configuration']
                    config.update(mode='independent', theme=report['choices']['widgets'][0]['id'], palette='', colorMode='fixed')
                else:
                    config = report['configuration']
                if output:
                    capture.unlink(missing_ok=True)
                    s.ok('appearance.preview', configuration=config)
                    wait_for_png(capture)
                    shutil.copy2(capture, output/'Sweet-from-KDE-Store.png')
                print('PASS: real Kvantum catalog, declared KDE dependency download and full component resolution')
            print('PASS: native widgets + icons, no auto-apply, restart, global references, missing component reports, real theme previews')
        finally:
            if s:
                s.close()


if __name__ == '__main__':
    main()
