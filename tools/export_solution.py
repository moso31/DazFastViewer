"""Export a CMake Visual Studio solution to the checkout root with valid paths."""
import argparse
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PROJECT = re.compile(r'^(Project\("\{[^}]+\}"\) = "[^"]+", ")([^"]+)(", "\{[^}]+\}"\r?$)', re.MULTILINE)
SOLUTION_FOLDER = '{2150E333-8FDC-42A3-9474-1A3956D46DE8}'


def export_solution(build_dir, output):
    source = Path(build_dir).resolve() / 'DazFastViewer.sln'
    output = Path(output).resolve()
    if source == output:
        raise ValueError('The exported solution must be outside the CMake build directory.')
    content = source.read_text(encoding='utf-8-sig')

    def rebase(match):
        if SOLUTION_FOLDER in match[1].upper():
            return match[0]
        project = (source.parent / match[2]).resolve()
        if not project.is_file():
            raise FileNotFoundError(f'Solution project is missing: {project}')
        try:
            path = os.path.relpath(project, output.parent)
        except ValueError:  # A custom build directory may be on another drive.
            path = str(project)
        return match[1] + path.replace('/', '\\') + match[3]

    content = PROJECT.sub(rebase, content)
    if not output.exists() or output.read_text(encoding='utf-8-sig') != content:
        output.write_text(content, encoding='utf-8-sig', newline='\r\n')
    print(f'Visual Studio solution: {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'DazFastViewer.sln')
    args = parser.parse_args()
    export_solution(args.build_dir, args.output)
