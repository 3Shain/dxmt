#!/bin/bash
# Fake winebuild -- just copy the input to output for postproc
cp "$2" "$2.postproc" 2>/dev/null
exit 0
