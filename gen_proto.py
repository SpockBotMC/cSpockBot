import sys
version = sys.argv[1]
import mcd2c
mcd2c.run(version.replace('_', '.'))
import shutil
shutil.move(f'{version}_proto.c', f'src/{version}_proto.c')
shutil.move(f'{version}_proto.h', f'include/{version}_proto.h')
