#!/usr/bin/env python3

"""
binconv: Convert a bit stream to binary data
"""

import sys

def isdec(ch: str):
    if ch == '':
        return False
    return ch >= '0' and ch <= '9'

def ishex(ch: str):
    if ch == '':
        return False
    return (
        isdec(ch) or
        (ch >= 'a' and ch <= 'f') or
        (ch >= 'A' and ch <= 'F')
    )

s = ""
while True:
    ch = sys.stdin.read(1)
    if ch == '':
        break

    if ch == ';':
        while True:
            ch = sys.stdin.read(1)
            if ch == '' or ch == '\n':
                break
        continue

    if ch == 'x':
        assert s == ""
        while True:
            ch = sys.stdin.read(1)
            if not ishex(ch):
                break
            s += ch
        assert s != ""
        sys.stdout.buffer.write(bytes([int(s, 16)]))
        s = ""
        continue

    if ch == '$':
        assert s == ""
        while True:
            ch = sys.stdin.read(1)
            if not isdec(ch):
                break
            s += ch
        assert s != ""
        sys.stdout.buffer.write(bytes([int(s, 10)]))
        s = ""
        continue

    if ch != '0' and ch != '1':
        continue

    s += ch
    if len(s) == 8:
        num = int(s, 2)
        sys.stdout.buffer.write(bytes([num]))
        s = ""
