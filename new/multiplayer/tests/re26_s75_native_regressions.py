#!/usr/bin/env python3
"""Sweep 75: explicit run of all 24 native multiplayer tests (no CTest registration).
Uses a private Xvfb server for the real one-window X11 test and requires the
user-provided retail Jaguar ROM for ROM-dependent coverage. No downloads.
"""
import argparse, os, pathlib, subprocess, time

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--build',type=pathlib.Path,required=True)
    ap.add_argument('--rom',type=pathlib.Path,required=True)
    ap.add_argument('--display',default=':143')
    ap.add_argument('--log-dir',type=pathlib.Path)
    a=ap.parse_args()
    root=a.build/'new/multiplayer'; a.log_dir=a.log_dir or a.build/'sweep75-tests'; a.log_dir.mkdir(parents=True,exist_ok=True)
    all_tests=sorted(p for p in root.glob('test_*') if p.is_file() and os.access(p,os.X_OK))
    xvf=subprocess.Popen(['Xvfb',a.display,'-screen','0','800x600x24','-nolisten','tcp'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        time.sleep(.45)
        results=[]
        for t in all_tests:
            args=[str(t)]
            if t.name=='test_local_mp_retail_rom':args.append(str(a.rom.resolve()))
            env={**os.environ,'DISPLAY':a.display}
            try:
                r=subprocess.run(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=25)
                success=r.returncode==0;output=r.stdout
            except subprocess.TimeoutExpired as e:
                success=False;output='TIMEOUT: '+str(e)
            (a.log_dir/f'{t.name}.log').write_text(output)
            results.append((t.name,success))
            print(('PASS' if success else 'FAIL'),t.name,flush=True)
        (a.log_dir/'SUMMARY.txt').write_text('\n'.join(f'{"PASS" if passed else "FAIL"} {name}' for name,passed in results)+'\n')
        print(f'SUMMARY {sum(success for _,success in results)}/{len(results)} PASS')
        if len(results)!=24 or not all(success for _,success in results):raise SystemExit(1)
    finally:
        xvf.terminate();xvf.wait(timeout=5)
if __name__=='__main__':main()
