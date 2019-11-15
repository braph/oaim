#!/usr/bin/python3

import re, os
from argparse import ArgumentParser
import subprocess

argp = ArgumentParser()
argp.add_argument('-f', '--fps', default=60, type=int, dest='FPS', help='Set FPS')
argp.add_argument('-s', '--shoot-time', default=0, type=int, dest='SHOOT_TIME', help='Set shoot duration in milli seconds')
argp.add_argument('-X', '--crosshair', default=1, choices=(1,2,3), type=int, dest='CROSSHAIR_NUM', help='Select crosshair')
argp.add_argument('-G', '--shoot-on-green', default=0, action='store_const', const=1, dest='SHOOT_ON_GREEN', help='Shoot on green color')
argp.add_argument('-W', '--shoot-on-white', default=0, action='store_const', const=1, dest='SHOOT_ON_WHITE', help='Shoot on white color')
argp.set_defaults(GAME_WINDOW_ID=0, SHOOT_DELAY_MIN=0, SHOOT_DELAY_MAX=0)
args = argp.parse_args()

p = subprocess.Popen(['xwininfo', '-root', '-all'], stdout=subprocess.PIPE, encoding='UTF-8')
for line in p.stdout:
    line_lower = line.lower()
    if 'openarena' in line_lower or 'yuoa.' in line_lower:
        print(line_lower)
        args.GAME_WINDOW_ID = re.search('0x[0-9a-fA-F]+', line_lower)[0]
        break
if args.GAME_WINDOW_ID == 0:
    print("Warning: Falling back on root window")

ret = os.spawnvp(os.P_WAIT,
    'gcc', ['gcc'] + ['-D%s=%s' % (k,v) for k,v in args.__dict__.items()] + [
            '-lX11', '-lXtst', '-Ofast', 'oaim.static.c', '-o', 'oaim' ] )

if not ret:
    os.execv('oaim', ['oaim'])

#          if (6 != sscanf(optarg, "%hu-%hu,%hu-%hu,%hu-%hu", &cr.red[0], &cr.red[1],
#                &cr.green[0], &cr.green[1], &cr.blue[0], &cr.blue[1]))
#            errx(1, "Invalid custom color format: %s", optarg);
#          printf("- Shooting on custom color: %s\n", optarg);
#ADD_COLOR_RANGE:
#          for (unsigned i = 0; i <= 1; ++i) {
#            if (cr.red[i] > 255) errx(1, "Invalid value for red: %u", cr.red[i]);
#            if (cr.blue[i] > 255) errx(1, "Invalid value for blue: %u", cr.blue[i]);
#            if (cr.green[i] > 255) errx(1, "Invalid value for green: %u", cr.green[i]);
#            cr.red[i] *= 256;
#            cr.blue[i] *= 256;
#            cr.green[i] *= 256;
#          }
#          color_ranges = realloc(color_ranges, ++color_ranges_count);
#          color_ranges[color_ranges_count-1] = cr;
