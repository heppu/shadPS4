#!/usr/bin/env python3
"""Consolidate project tooling, preserving emulator patches and the exact final tree."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--apply', action='store_true', required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
base = '4788f0b108a6faaae5105f8ac88ef8e1935e2b22'
branch = 'refs/heads/bo4-dev'

def git(*command, env=None, data=None):
    return subprocess.check_output(['git', *command], env=env, input=data)

def value(*command, env=None):
    return git(*command, env=env).decode().strip()

if value('symbolic-ref', 'HEAD') != branch or value('status', '--porcelain'):
    raise SystemExit('Requires the clean bo4-dev checkout')
old = value('rev-parse', 'HEAD')
remote = value('ls-remote', 'origin', branch).split()[0]
if remote != old:
    raise SystemExit('Push the reviewed checkpoint before reorganizing history')
commits = value('rev-list', '--reverse', base + '..' + old).splitlines()
if value('rev-list', '--merges', base + '..' + old):
    raise SystemExit('Expected a linear project history')
archive = 'archive/bo4-dev-before-tooling-flatten-' + datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
git('tag', archive, old)
git('push', 'origin', 'refs/tags/' + archive)
args.output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='bo4-index-', dir=args.output.parent) as directory:
    env = dict(os.environ, GIT_INDEX_FILE=str(Path(directory).resolve() / 'index'))
    git('read-tree', base, env=env)
    git('restore', '--source=' + old, '--staged', '--', 'bo4-dev', 'AGENTS.md', env=env)

    def commit(parent, original, message):
        identity = git('show', '-s', '--format=%an%x00%ae%x00%aI', original).decode().strip().split('\0')
        author_env = dict(env, GIT_AUTHOR_NAME=identity[0], GIT_AUTHOR_EMAIL=identity[1], GIT_AUTHOR_DATE=identity[2])
        tree = value('write-tree', env=env)
        return git('commit-tree', tree, '-p', parent, env=author_env, data=message).decode().strip()

    current = commit(base, commits[0], b'Add custom development tooling and project workflow\n')
    first = current
    mapping = []
    for original in commits:
        patch = git('diff', '--binary', '--full-index', original + '^', original,
                    '--', '.', ':!bo4-dev', ':!AGENTS.md')
        if not patch:
            mapping.append({'old': original, 'new': first, 'tooling_only': True})
            continue
        git('apply', '--cached', '--whitespace=nowarn', '-', env=env, data=patch)
        message = git('show', '-s', '--format=%B', original)
        current = commit(current, original, message)
        mapping.append({'old': original, 'new': current, 'tooling_only': False})
    if value('rev-parse', current + '^{tree}') != value('rev-parse', old + '^{tree}'):
        raise SystemExit('Final tree mismatch; branch left unchanged')
    record = {'base': base, 'old': old, 'new': current, 'tooling': first, 'archive': archive,
              'identical_tree': True, 'mapping': mapping}
    args.output.write_text(json.dumps(record, indent=2) + '\n')
    git('update-ref', branch, current, old)
    git('push', '--force-with-lease=' + branch + ':' + old, 'origin', branch)
    print(json.dumps({k: v for k, v in record.items() if k != 'mapping'}, indent=2))
