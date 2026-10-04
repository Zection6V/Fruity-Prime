"""Deploy a built Qt Android ABI for the repository's existing Gradle head."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--abi')
    parser.add_argument('--host', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--merge', type=Path, nargs='+')
    args = parser.parse_args()
    if args.merge:
        for source_root in args.merge:
            merge(source_root, args.output)
        return
    if not args.build_dir or not args.abi or not args.host:
        parser.error('--build-dir, --abi and --host are required when deploying')
    build = args.build_dir.resolve()
    stage = build / 'qt-deploy'
    libraries = stage / 'libs' / args.abi
    libraries.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / f'libFruityPrime_{args.abi}.so', libraries)
    deploy = args.host / 'bin' / ('androiddeployqt.exe' if (args.host / 'bin/androiddeployqt.exe').exists() else 'androiddeployqt')
    subprocess.run([str(deploy), '--input', str(build / 'android-fruity_mphread_native_android-deployment-settings.json'),
                    '--output', str(stage), '--aux-mode', '--release'], check=True)
    settings = json.loads((build / 'android-fruity_mphread_native_android-deployment-settings.json').read_text(encoding='utf-8'))
    suffix = '.exe' if settings['ndk-host'].startswith('windows') else ''
    strip = Path(settings['ndk']) / 'toolchains/llvm/prebuilt' / settings['ndk-host'] / 'bin' / ('llvm-strip' + suffix)
    # Aux deployment does not strip the application. Keep the original build
    # for diagnostics, and strip only its package copy before Gradle consumes it.
    subprocess.run([str(strip), '--strip-unneeded', str(libraries / f'libFruityPrime_{args.abi}.so')], check=True)
    merge(stage, args.output)

def merge(source_root, output):
    for directory in ['libs', 'assets', 'src']:
        if (source_root / directory).exists():
            shutil.copytree(source_root / directory, output / directory, dirs_exist_ok=True)
    # Qt's resource arrays contain ABI-qualified library names. Merge both
    # generated arrays, so the APK's loader can select either architecture.
    resources = source_root / 'res'
    for source in resources.rglob('*'):
        if not source.is_file():
            continue
        destination = output / 'res' / source.relative_to(resources)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if source.name == 'libs.xml' and destination.exists():
            existing = ET.parse(destination)
            incoming = ET.parse(source)
            for element in incoming.getroot():
                target = next((e for e in existing.getroot() if e.tag == element.tag and e.get('name') == element.get('name')), None)
                if target is None:
                    existing.getroot().append(element)
                elif element.tag == 'array':
                    present = {e.text for e in target}
                    for item in element:
                        if item.text not in present:
                            target.append(item)
            existing.write(destination, encoding='utf-8', xml_declaration=True)
        else:
            shutil.copy2(source, destination)


if __name__ == '__main__':
    main()
