import sys
import subprocess


versionPath = "src/version.h"

if len(sys.argv) > 1 and sys.argv[1][0] == 'v':
    versionNumber = sys.argv[1]
else:
    # NOTE: upstream also ran `git pull` here, but that pulls from origin on every
    # local build (merging upstream into your work). CI passes the version as argv[1]
    # and skips this branch, so dropping the pull only affects local builds.
    result = subprocess.run(['git', 'describe', '--abbrev=0', '--tags'], stdout=subprocess.PIPE)
    versionNumber = result.stdout.decode('utf-8').replace("\n", "")
  
with open(versionPath, "w") as file: 
    file.write('#define SW_VER "' + versionNumber + '"') 