#!/usr/bin/env python3

import argparse

def calibrate():
    r1 = (u1 * (u0 - u2) * rl1 + u2 * (u1 - u0) * rl2) / u0 / (u2 - u1)
    r2 = u2 * u1 / u0 / (u2 - u1) * (rl1 - rl2)
    print('r1={}, r2={}'.format(r1, r2))
    print('check: u1/u0*(r1+r2+rl1) - r2 = {}, u2/u0*(r1+r2+rl2) - r2 = {}'.format(
        u1 / u0 * (r1+r2+rl1) - r2,
        u2 / u0 * (r1+r2+rl2) - r2))


def reverse_calibrate():
    rl1 = (u0/u1 - 1) * r2 - r1
    rl2 = (u0/u2 - 1) * r2 - r1
    print('rl1={}, rl2={}'.format(rl1, rl2))

def substitute():
    u = r2 * u0 / (r1 + r2 + rl)
    print('u={}'.format(u))

parser = argparse.ArgumentParser()
parser.add_argument('-u0', type=float, required=True)
subparsers = parser.add_subparsers()

parser_calibrate = subparsers.add_parser('calibrate', aliases=['c'])
parser_calibrate.add_argument('-u1', type=float)
parser_calibrate.add_argument('-u2', type=float)
parser_calibrate.add_argument('-rl1', type=float)
parser_calibrate.add_argument('-rl2', type=float)
parser_calibrate.set_defaults(func=calibrate)

parser_reverse = subparsers.add_parser('reverse_calibrate', aliases=['r'])
parser_reverse.add_argument('-u1', type=float)
parser_reverse.add_argument('-u2', type=float)
parser_reverse.add_argument('-r1', type=float)
parser_reverse.add_argument('-r2', type=float)
parser_reverse.set_defaults(func=reverse_calibrate)

parser_substitute = subparsers.add_parser('substitute', aliases=['s'])
parser_substitute.add_argument('-r1', type=float)
parser_substitute.add_argument('-r2', type=float)
parser_substitute.add_argument('-rl', type=float)
parser_substitute.set_defaults(func=substitute)

args = parser.parse_args()
globals().update(args.__dict__)
args.func()
