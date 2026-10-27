#!/usr/bin/env python3
"""Generate README acknowledgements from Git history and verified GitHub identities."""
import html
import json
import os
import re
import subprocess
import urllib.error
import urllib.request
from pathlib import Path

CONFIG = Path('.github/contributors.json')
README = Path('README.md')
START, END = '<!-- contributors:start -->', '<!-- contributors:end -->'
LOGIN = re.compile(r'^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$')
NOREPLY = re.compile(r'^(?:\d+\+)?([A-Za-z0-9-]+)@users\.noreply\.github\.com$', re.I)

def git(*args):
    return subprocess.check_output(['git', *args]).decode('utf-8', 'replace')

def records(*args):
    # One representative SHA per name/email, used for GitHub's commit association.
    out = git('log', '--no-merges', '--format=%H%x00%aN%x00%aE%x00', *args)
    fields = out.replace('\n', '').split('\0')
    result = {}
    for i in range(0, len(fields) - 2, 3):
        sha, name, email = fields[i:i+3]
        if name.strip():
            result.setdefault((name.strip(), email.strip()), sha.strip())
    return result

def resolve(representatives, repository):
    # Queries are bounded batches, no one-request-per-commit polling.
    token = os.environ.get('GH_TOKEN')
    if not token:
        raise SystemExit('GH_TOKEN is required to resolve GitHub commit authors')
    owner, repo = repository.split('/', 1)
    shas = sorted(set(representatives.values()))
    found = {}
    for start in range(0, len(shas), 40):
        batch = shas[start:start+40]
        selections = ' '.join(f'c{i}: object(oid:"{sha}") {{ ... on Commit {{ author {{ user {{ login }} }} }} }}' for i, sha in enumerate(batch))
        query = 'query { repository(owner:' + json.dumps(owner) + ',name:' + json.dumps(repo) + ') { ' + selections + ' } }'
        request = urllib.request.Request('https://api.github.com/graphql', data=json.dumps({'query': query}).encode(), headers={'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json', 'User-Agent': 'Luna-contributors'}, method='POST')
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                data = json.load(response)
        except (urllib.error.URLError, TimeoutError) as e:
            raise SystemExit(f'GitHub identity lookup failed: {e}') from e
        if data.get('errors') or not data.get('data', {}).get('repository'):
            raise SystemExit('GitHub GraphQL error: ' + json.dumps(data.get('errors')))
        objects = data['data']['repository']
        for i, sha in enumerate(batch):
            item = objects.get(f'c{i}') or {}
            login = ((item.get('author') or {}).get('user') or {}).get('login')
            if login and LOGIN.fullmatch(login):
                found[sha] = login
    return found

def build_groups(upstream, luna, config, identity):
    aliases = {k.casefold(): v for k, v in config.get('aliases', {}).items()}
    excluded = {x.casefold() for x in config.get('exclude', [])}
    by_email = {}
    for (name, email), sha in list(upstream.items()) + list(luna.items()):
        login = aliases.get(email.casefold()) or aliases.get(name.casefold()) or identity.get(sha)
        if login:
            by_email[email.casefold()] = login

    def person(name, email='', sha='', explicit=None):
        noreply = NOREPLY.fullmatch(email)
        login = explicit or aliases.get(email.casefold()) or aliases.get(name.casefold()) or identity.get(sha) or by_email.get(email.casefold()) or (noreply.group(1) if noreply else None)
        if login and not LOGIN.fullmatch(login):
            raise ValueError(f'Invalid GitHub username: {login!r}')
        if name.casefold() in excluded or (login and login.casefold() in excluded):
            return None
        # Account login is the stable visible label; aliases never produce extra cards.
        return (login or name, login)

    def collect(records, extra):
        people = {}
        entries = [person(n, e, sha) for (n, e), sha in records.items()]
        for item in extra:
            entries.append(person(item.get('name') or item['github'], explicit=item.get('github')))
        for item in entries:
            if item:
                name, login = item
                key = ('github' if login else 'name', (login or name).casefold())
                people[key] = item
        return people

    manual = config.get('manual', {})
    u = collect(upstream, manual.get('upstream', []))
    l = collect(luna, manual.get('luna', []))
    shared = u.keys() & l.keys()
    sort = lambda values: sorted(values, key=lambda x: x[0].casefold())
    return [
        ('Luna contributors', 'The people building and improving Luna Launcher.', sort(l[k] for k in l.keys() - shared)),
        ('Co-contributors', 'Contributors to both Luna Launcher and Prism Launcher.', sort(l[k] for k in shared)),
        ('Upstream contributors', 'Thank you to the Prism Launcher community, whose work Luna builds on.', sort(u[k] for k in u.keys() - shared)),
    ]

def render_people(people):
    # Inline, equally sized images wrap naturally within each group panel.
    # No table-per-person cells or varying-height name captions.
    if not people:
        return '<p><em>No contributors in this group yet.</em></p>'
    avatars, unlinked = [], []
    for name, login in people:
        label = html.escape(name, quote=True)
        if login:
            account = html.escape(login, quote=True)
            avatars.append(
                f'<a href="https://github.com/{account}" title="{label}">'
                f'<img src="https://github.com/{account}.png?size=112" '
                f'width="56" height="56" alt="{label}" title="{label}" /></a>'
            )
        else:
            unlinked.append(label)
    blocks = []
    if avatars:
        blocks.append('<p>' + '\n'.join(avatars) + '</p>')
    # Keep contributors without a verified account visible and accessible.
    if unlinked:
        blocks.append('<p>' + ' · '.join(unlinked) + '</p>')
    return '\n'.join(blocks)

def render(groups):
    sections = [START, '## Contributors', '',
        'Built by the Luna community, on the foundations of Prism Launcher and MultiMC. Thank you to everyone who has contributed.', '']
    for title, description, people in groups:
        sections.extend([
            '<table width="100%">', '<tbody>', '<tr>', '<td>',
            f'<h3>{html.escape(title)}</h3>',
            f'<p>{html.escape(description)}</p>',
            render_people(people), '</td>', '</tr>', '</tbody>', '</table>', ''
        ])
    sections.append(END)
    return '\n'.join(sections)

def update_readme(text, content):
    if text.count(START) != 1 or text.count(END) != 1 or text.index(START) > text.index(END):
        raise ValueError('README must contain exactly one ordered pair of contributor markers')
    before, rest = text.split(START, 1)
    _, after = rest.split(END, 1)
    return before + content + after

def main():
    config = json.loads(CONFIG.read_text(encoding='utf-8'))
    upstream = records('prism/default')
    luna = records('--cherry-pick', '--right-only', 'prism/default...HEAD')
    # Resolve upstream SHAs in Prism, including commits Luna has not merged.
    identities = resolve(upstream, 'PrismLauncher/PrismLauncher')
    identities.update(resolve(luna, os.environ.get('GITHUB_REPOSITORY', 'AndreaFrederica/LunaLauncher')))
    groups = build_groups(upstream, luna, config, identities)
    text = README.read_text(encoding='utf-8')
    README.write_text(update_readme(text, render(groups)), encoding='utf-8')
    for title, _, people in groups:
        print(f'{title}: {len(people)}')
    unresolved = sorted({n for _, _, people in groups for n, login in people if not login})
    # Names only: never leak historical commit email addresses into logs or README.
    if unresolved:
        print('Names without a GitHub association (add an alias if known): ' + ', '.join(unresolved))

if __name__ == '__main__':
    main()
