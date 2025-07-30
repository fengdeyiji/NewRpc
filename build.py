import os
import argparse

parser = argparse.ArgumentParser(description='compile script')
parser.add_argument('--compile', default=None, choices=['config', 'debug', 'release'], type=str, help='compile')
parser.add_argument('--compiler', default='clang', choices=['gcc', 'clang'], type=str, help='compile')
parser.add_argument('--run', default=False, action ='store_true', help='run')
args = parser.parse_args()

def assert_do(action : str):
  print(f'[CMD]{action}')
  assert(0 == os.system(action))

CURRENT_DIR = os.getcwd()

if args.compile:
  compile_type = ''
  if args.compile == "config":
    assert_do("xmake project -k compile_commands")
    exit(0)
  else:
    # os.system("lsof +D build 2>/dev/null | awk '{print $2}' | xargs kill -9 >/dev/null 2>&1")
    # assert_do("xmake clean")
    # assert_do("rm -rf ./build_debug")
    # assert_do("rm -rf ./build_release")
    assert_do(f"xmake f -m {args.compile} --toolchain={args.compiler}")
    assert_do("xmake -v -D")