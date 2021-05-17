#!/usr/bin/env python3

import os
import subprocess
import sys

profile = sys.argv[1]

sys.stdout.write("/* This file is auto-generated. Do not edit. */\n")
sys.stdout.write("#pragma once\n")
sys.stdout.write("\n")
sys.stdout.write(f"#define PROFILE \"{profile}\"\n")

short_sha = os.environ.get('CI_COMMIT_SHORT_SHA')
if short_sha is not None:
    sys.stdout.write(f"#define VCS_TAG \"{short_sha}\"\n")
else:
    cmd = ["git", "rev-parse", "--short", "HEAD"]
    with subprocess.Popen(cmd, stdout=subprocess.PIPE) as p:
        short_sha = p.stdout.read().decode('utf-8').rstrip("\n")
        sys.stdout.write(f"#define VCS_TAG \"{short_sha}\"\n")
