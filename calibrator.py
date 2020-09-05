#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser()
parser.add_argument('-u0', type=float, required=True)
parser.add_argument('-u1', type=float, required=True)
parser.add_argument('-u2', type=float, required=True)
parser.add_argument('-rl1', type=float)
parser.add_argument('-rl2', type=float)
parser.add_argument('-r1', type=float)
parser.add_argument('-r2', type=float)

args = parser.parse_args()
locals().update(args.__dict__)

if rl1 and rl2:
    assert not r1 and not r2
    r1 = (u1 * (u0 - u2) * rl1 + u2 * (u1 - u0) * rl2) / u0 / (u2 - u1)
    r2 = u2 * u1 / u0 / (u2 - u1) * (rl1 - rl2)
    print('r1={}, r2={}'.format(r1, r2))
    print('check: u1/u0*(r1+r2+rl1) - r2 = {}, u2/u0*(r1+r2+rl2) - r2 = {}'.format(
        u1 / u0 * (r1+r2+rl1) - r2,
        u2 / u0 * (r1+r2+rl2) - r2))
elif r1 and r2:
    assert not rl1 and not rl2
    rl1 = (u0/u1 - 1) * r2 - r1
    rl2 = (u0/u2 - 1) * r2 - r1
    print('rl1={}, rl2={}'.format(rl1, rl2))
else:
    assert False

